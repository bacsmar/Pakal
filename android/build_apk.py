"""Builds the Android APKs of Pakal's games (arm64-v8a, OpenGL ES 3): one per game, each its own app (its package, its
name on the launcher), so they are all installed side by side.

    python android/build_apk.py [--game zombies|brawler|dungeon|<a game of games/>|all] [--skip-native] [--raw-textures]

Needs the Android NDK/SDK/JDK (point ANDROID_NDK, ANDROID_SDK and JAVA_HOME at them, or write them in
android/local_paths.json as {"ANDROID_NDK": ..., "ANDROID_SDK": ..., "JAVA_HOME": ...}, a file Git ignores) and the vcpkg
arm64-android packages, which it installs the first time with VCPKG_ROOT's vcpkg (PAKAL_VCPKG_ANDROID: another
install tree to use instead):

    vcpkg install --triplet arm64-android --x-manifest-root=android/vcpkg --x-install-root=build-android/vcpkg_installed

The games' data comes from the desktop build (build-win64/bin/Assets): build it first, and for Zombies generate the levels
(tools/zombies_convert/build_downtown.py). Output: build-android/pakal-<game>.apk (signed with a debug key).

For a store: --bundle makes the Android App Bundle (build-android/pakal-<game>-<version>.aab, with bundletool) and
--release signs it with the upload key (PAKAL_KEYSTORE, PAKAL_KEY_ALIAS; its passwords are asked at the terminal of the person
running it, never read from a file or the environment, and a run with no console is refused); the version and its code
come from the game's version file (android.json "version_file"). "asset_pack": true in a game's android.json puts its assets
in an install-time asset pack instead of the base module (Google Play caps the base's download at 200 MB). --install-bundle
SERIAL installs the bundle on that phone as the store would (bundletool build-apks for the device, then install-apks).
A release seals the game's art: its android.json names a "pack", or says why not ("pack": false, "pack_reason").

A flavor (--flavor, a game's android.json "flavors") is the game as another app, a demo for instance: its own package and
name, and settings its code reads from pakal_flavor.h (--define NAME=VALUE; the certificates its copies may carry, "signers"
and --signer). A flavor is a release: it is signed with the upload key. Next to each package, <package>.build.json holds the
build's identity (its number, commit, branch, version).
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path, PurePosixPath
from xml.etree import ElementTree
from urllib.parse import unquote

import pack_textures

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build-android'
DESKTOP_ASSETS = ROOT / 'build-win64' / 'bin' / 'Assets'
ENGINE_LOGO = ROOT / 'assets' / 'engine' / 'pakal_splash.png'   # SplashState's logo when a game names none
# bgfx's texture tool (external/bgfx/.build/projects/vs2022/texturec.vcxproj, Release x64)
HOST_WINDOWS = os.name == 'nt'
EXE = '.exe' if HOST_WINDOWS else ''
SCRIPT = '.bat' if HOST_WINDOWS else ''
TEXTUREC = ROOT / 'external' / 'bgfx' / '.build' / ('win64_vs2022' if HOST_WINDOWS else 'linux64_gcc') / 'bin' / ('texturecRelease' + EXE)


EXAMPLE = "an engine example: its art is free (Quaternius, CC0) and its sources are public"
# Each game: its app's package and name, its library (android/CMakeLists.txt) and what else its manifest needs
GAMES = {
    'zombies': {'package': 'org.pakalengine.zombies', 'label': 'Zombies', 'library': 'pakal_zombies', 'permissions': [],
                'pack': False, 'pack_reason': EXAMPLE},
    'brawler': {'package': 'org.pakalengine.brawler', 'label': 'Pakal Brawler', 'library': 'pakal_brawler', 'permissions': [],
                'pack': False, 'pack_reason': EXAMPLE},
    # online: the network, and the Wi-Fi's broadcasts (the games on the network answer the menu's question)
    'dungeon': {'package': 'org.pakalengine.dungeon', 'label': 'Pakal Dungeon', 'library': 'pakal_dungeon',
                'permissions': ['android.permission.INTERNET', 'android.permission.ACCESS_NETWORK_STATE',
                                'android.permission.ACCESS_WIFI_STATE', 'android.permission.CHANGE_WIFI_MULTICAST_STATE'],
                'pack': False, 'pack_reason': EXAMPLE},
}
# The games kept outside the engine's examples (games/<name>/, a folder only some checkouts have): each one says what it
# is in its android.json (package, label, library, permissions; an icon and a stager, files of its folder). Its library
# comes from its android.cmake (android/CMakeLists.txt includes it). "staging": "zombies" packs it as the Zombies module
# (its levels, with its own Assets/<name>); a "stager" (a Python file with stage(stage, raw_textures, apk)) packs it its
# own way.
for recipe in sorted((ROOT / 'games').glob('*/android.json')):
    entry = json.loads(recipe.read_text(encoding='utf-8'))
    entry.setdefault('permissions', [])
    for key in ('icon', 'icon_foreground', 'stager', 'version_file', 'desktop_data'):
        if key in entry:
            entry[key] = recipe.parent / entry[key]
    if entry.get('pack'):   # its data sealed in one pack inside the APK (seal_pack): the key's file is of its folder too
        entry['pack'] = dict(entry['pack'], key=recipe.parent / entry['pack']['key'])
    GAMES[recipe.parent.name] = entry


def local_paths():
    """android/local_paths.json: this machine's tool folders (not in Git)."""
    path = ROOT / 'android' / 'local_paths.json'
    return json.loads(path.read_text(encoding='utf-8')) if path.is_file() else {}


def setting(env):
    """A machine setting (a folder, the upload key's file and alias; never a password): the environment or local_paths.json."""
    value = os.environ.get(env) or local_paths().get(env)
    if not value:
        sys.exit(f'{env} is not set (the environment or android/local_paths.json)')
    return value


def tool_dir(env):
    path = Path(setting(env))
    if not path.exists():
        sys.exit(f'{env} not found ({path})')
    return path


NDK = tool_dir('ANDROID_NDK')
SDK = tool_dir('ANDROID_SDK')
JDK = tool_dir('JAVA_HOME')
BUILD_TOOLS = sorted((SDK / 'build-tools').iterdir())[-1]
# The API level the apps target (Google Play asks for a recent one), linked against that platform's android.jar
TARGET_SDK = 36
MIN_SDK = 26
PLATFORM = SDK / 'platforms' / f'android-{TARGET_SDK}'
if not PLATFORM.is_dir():
    sys.exit(f'the Android {TARGET_SDK} platform is not installed (sdkmanager "platforms;android-{TARGET_SDK}")')
CMAKE_BIN = next((SDK / 'cmake').iterdir()) / 'bin'


def run(command, hidden=(), **kwargs):
    """Runs a command, printed first; the parts in 'hidden' (a secret it is given: a pack's key) print as ***."""
    print('>', ' '.join('***' if str(part) in hidden else str(part) for part in command))
    subprocess.check_call([str(part) for part in command], **kwargs)


def install_vcpkg_android():
    """The arm64-android libraries (Lua, Assimp, RmlUi...) into build-android/vcpkg_installed, the first time: vcpkg
    builds them with the NDK. A tree already installed (or PAKAL_VCPKG_ANDROID pointing to another) is left alone."""
    installed = BUILD / 'vcpkg_installed'
    if os.environ.get('PAKAL_VCPKG_ANDROID') or (installed / 'arm64-android' / 'lib' / 'liblua.a').is_file():
        return
    vcpkg = Path(os.environ.get('VCPKG_ROOT', '')) / ('vcpkg' + EXE)
    if not os.environ.get('VCPKG_ROOT') or not vcpkg.is_file():
        sys.exit('the arm64-android libraries are not installed and VCPKG_ROOT does not point to vcpkg: set VCPKG_ROOT '
                 '(the folder of vcpkg' + EXE + '), or install them yourself:\n    vcpkg install --triplet arm64-android '
                 '--x-manifest-root=android/vcpkg --x-install-root=build-android/vcpkg_installed')
    run([vcpkg, 'install', '--triplet', 'arm64-android', f'--x-manifest-root={ROOT / "android" / "vcpkg"}',
         f'--x-install-root={installed}'], env=dict(os.environ, ANDROID_NDK_HOME=str(NDK)))


def build_native(games, cheats=False, tools=False, flavor_dir=None):
    install_vcpkg_android()
    env = dict(os.environ, PATH=str(CMAKE_BIN) + os.pathsep + os.environ['PATH'])
    run([CMAKE_BIN / ('cmake' + EXE), '-S', ROOT / 'android', '-B', BUILD, '-G', 'Ninja',
         f'-DPAKAL_GAME_CHEATS={"ON" if cheats else "OFF"}',   # a game's cheats (its android.cmake reads it)
         f'-DPAKAL_ENGINE_TOOLS={"ON" if tools else "OFF"}',   # the engine's MCP tools (docs/PROFILING.md)
         f'-DPAKAL_FLAVOR_DIR={flavor_dir or ""}',   # a flavor's pakal_flavor.h (apply_flavor); empty clears an earlier one
         f'-DCMAKE_TOOLCHAIN_FILE={NDK / "build/cmake/android.toolchain.cmake"}',
         '-DANDROID_ABI=arm64-v8a', f'-DANDROID_PLATFORM=android-{MIN_SDK}', '-DANDROID_STL=c++_static',
         '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON', '-DCMAKE_BUILD_TYPE=Release',
         *([f'-DPAKAL_BOX2D_SRC={os.environ["PAKAL_BOX2D_SRC"]}'] if os.environ.get('PAKAL_BOX2D_SRC') else []),
         *([f'-DPAKAL_VCPKG_ANDROID={os.environ["PAKAL_VCPKG_ANDROID"]}'] if os.environ.get('PAKAL_VCPKG_ANDROID') else []),
         f'-DCMAKE_MAKE_PROGRAM={CMAKE_BIN / ("ninja" + EXE)}'], env=env)
    targets = [arg for game in games for arg in ('--target', GAMES[game]['library'])]
    run([CMAKE_BIN / ('cmake' + EXE), '--build', BUILD, *targets, '--target', 'PakalShadersEssl'], env=env)


def sidecar_textures(sidecar, model_dir):
    """The textures a model's .materials.xml names: its ground's mix layers ("mix", "mix2": a texture, then numbers), found
    as MeshComponent3D_Bgfx resolves them (the path beside the model, then its file name there)."""
    try:
        root = ElementTree.fromstring(sidecar.read_bytes())
    except ElementTree.ParseError:
        return set()
    found = set()
    for material in root.iter('material'):
        for attribute in ('mix', 'mix2'):
            words = material.get(attribute, '').split()
            if not words:
                continue
            name = PurePosixPath(words[0].replace('\\', '/'))
            for candidate in (model_dir / name.as_posix().lstrip('/'), model_dir / name.name):
                if candidate.is_file():
                    found.add(candidate.resolve())
                    break
    return found


def referenced_files(level_dirs, shared_roots, documents=()):
    """The files under the shared folders (the models of ../downtown, the lightmap atlases) that the levels use: every
    string of their JSON files that names an existing file there, and what those models name in turn (a glTF's buffers and
    textures). 'documents': more JSON files read the same way (a game's data: the models its gore throws). The rest (old
    copies, bake caches, unused models) stays out of the package."""
    roots = [r.resolve() for r in shared_roots]
    found = set()

    def under_root(path):
        return any(path == r or r in path.parents for r in roots)

    def visit(value, base):
        if isinstance(value, dict):
            for item in value.values():
                visit(item, base)
        elif isinstance(value, list):
            for item in value:
                visit(item, base)
        elif isinstance(value, str) and ('/' in value or '\\' in value) and len(value) < 400:
            # next to the document, or from the game's folder ("Assets/...": the games' rules name models so)
            for path in ((base / value).resolve(), (DESKTOP_ASSETS.parent / value).resolve()):
                try:
                    is_file = path.is_file()
                except OSError:   # (a sentence with a '/', no path: a part longer than the system's names, ENAMETOOLONG)
                    continue
                if is_file and under_root(path):
                    found.add(path)

    for level in level_dirs:
        # the level and its mission layers (missions/*.json: LevelLayers)
        for document in list(level.glob('*.json')) + list(level.glob('missions/*.json')):
            try:
                visit(json.loads(document.read_text(encoding='utf-8')), document.parent)
            except (ValueError, UnicodeDecodeError):
                continue
    for document in documents:
        try:
            visit(json.loads(document.read_text(encoding='utf-8')), document.parent)
        except (ValueError, UnicodeDecodeError):
            continue
    for model in [f for f in found if f.suffix.lower() == '.gltf']:
        # its material overrides beside it (<model>.gltf.materials.xml: water that flows, wet sand, blends), which no
        # document names
        sidecar = model.with_name(model.name + '.materials.xml')
        if sidecar.is_file():
            found.add(sidecar.resolve())
            found.update(t for t in sidecar_textures(sidecar, model.parent) if under_root(t))
        doc = json.loads(model.read_text(encoding='utf-8'))
        for entry in doc.get('buffers', []) + doc.get('images', []):
            uri = entry.get('uri', '')
            if uri and not uri.startswith('data:'):
                path = (model.parent / unquote(uri)).resolve()
                if path.is_file():
                    found.add(path)
    return found


def game_files(game):
    """What a game other than Zombies packs: (source, target in the APK) for its folder (Assets/<game>: rules, interface,
    its own models), the interface's shared files (fonts, styles), the shaders, and the models of the Zombies example
    its rules name (only those)."""
    wanted = [(DESKTOP_ASSETS / game, f'Assets/{game}'),
              (BUILD / 'apk_assets' / 'Assets' / 'shaders', 'Assets/shaders'),
              (DESKTOP_ASSETS / 'ui', 'Assets/ui'),
              (ENGINE_LOGO, 'Assets/pakal/splash.png')]
    downtown = DESKTOP_ASSETS / 'zombies' / 'downtown'   # (it may be a link to another checkout's)
    for model in sorted(referenced_files([DESKTOP_ASSETS / game], [downtown])):
        wanted.append((model, (Path('Assets/zombies/downtown') / model.relative_to(downtown.resolve())).as_posix()))
    return wanted


def check_desktop_data(game):
    """The APK packs the desktop build's copy of a game's data (build-win64/bin/Assets/<game>), not its sources: a copy left
    from another branch, or from before the last commit, would ship old data with new code. Every file of the sources
    (examples/<game>/data, or for a game of games/ the folder its android.json names as "desktop_data", the one its
    pakal_add_game_module copies to Assets/<game>) must be in that copy with the same bytes; otherwise it stops and names
    them (build the desktop again, or PAKAL_APK_STALE_DATA=1 to pack it anyway)."""
    source = GAMES[game].get('desktop_data') or ROOT / 'examples' / game / 'data'
    if not source.is_dir() or os.environ.get('PAKAL_APK_STALE_DATA') == '1':
        return
    copy = DESKTOP_ASSETS / game
    stale = []
    for file in sorted(f for f in source.rglob('*') if f.is_file()):
        other = copy / file.relative_to(source)
        if not other.is_file() or other.stat().st_size != file.stat().st_size or other.read_bytes() != file.read_bytes():
            stale.append(file.relative_to(source).as_posix())
    if stale:
        shown = '\n  '.join(stale[:10]) + (f'\n  ... and {len(stale) - 10} more' if len(stale) > 10 else '')
        sys.exit(f'{copy} is not {source.relative_to(ROOT).as_posix()} ({len(stale)} files differ or are missing):\n  {shown}\n'
                 f'build the desktop again (or PAKAL_APK_STALE_DATA=1 to pack it as it is)')


RELEASE = {'on': False}   # (make_apk's --release, for what is staged)


def write_env_defaults(stage, game):
    """The game's android.json "env" (KEY: VALUE) as pakal_env.txt at the APK's root: the app's environment before the device's own
    pakal_env.txt (which wins). PAKAL_NO_3D=1 for a 2D game, so that a phone does not compile the 3D programs. A release (--release, what
    a store gets) adds PAKAL_RELEASE=1: a game may do what only the players' copies should (send its telemetry)."""
    env = dict(GAMES[game].get('env') or {})
    if RELEASE['on']:
        env['PAKAL_RELEASE'] = '1'
    if env:
        (stage / 'pakal_env.txt').write_text(''.join(f'{k}={v}\n' for k, v in env.items()), encoding='utf-8', newline='\n')
        print('the game\'s environment:', ', '.join(f'{k}={v}' for k, v in env.items()))


def stage_assets(stage, game='zombies', raw_textures=False):
    """The files the game needs, with assets.list (the game extracts them on its first run). The other games' are listed by
    game_files. For Zombies, the levels: the original one (hollywood_s01t01), Downtown's missions and the
    level builder's (the kit's models in ../downtown). The first level also holds the effects (muzzle, bullet, casings)
    that Downtown names. The game opens on its menu."""
    if stage.exists():
        shutil.rmtree(stage)
    stager = GAMES[game].get('stager')
    if stager:
        check_desktop_data(game)   # (a game of games/ only when its android.json names its "desktop_data")
        import importlib.util
        spec = importlib.util.spec_from_file_location(f'{game}_stager', stager)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.stage(stage, raw_textures, sys.modules[__name__])
        drop_secrets(stage)   # (a stager may not write the asset list through this script)
        write_env_defaults(stage, game)
        return
    if game != 'zombies' and GAMES[game].get('staging') != 'zombies':
        check_desktop_data(game)
        for source, target in game_files(game):
            if not source.exists():
                sys.exit(f'missing {source}')
            files = [source] if source.is_file() else [f for f in source.rglob('*') if f.is_file()]
            for file in files:
                destination = stage / target if source.is_file() else stage / target / file.relative_to(source)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(file, destination)
        finish_stage(stage, raw_textures)
        return
    zombies = DESKTOP_ASSETS / 'zombies'
    skip_dirs = set()
    skip_names = {'level_map.svg', 'combat-fixture.json'}
    wanted = [(zombies / 'hollywood_s01t01', 'Assets/zombies/hollywood_s01t01'),
              (zombies / 'downtown_survival_01', 'Assets/zombies/downtown_survival_01'),
              (zombies / 'downtown', 'Assets/zombies/downtown'),
              (BUILD / 'apk_assets' / 'Assets' / 'shaders', 'Assets/shaders')]
    if game == 'zombies':
        wanted += [(DESKTOP_ASSETS / 'ui', 'Assets/ui'), (DESKTOP_ASSETS / 'game', 'Assets/game')]
    else:   # its own interface and campaign
        wanted.append((DESKTOP_ASSETS / game, f'Assets/{game}'))
    # the other Downtown missions and the levels of the level builder (tools/levelbuilder), when they were built
    for extra in sorted(zombies.glob('downtown_survival_*')) + sorted(zombies.glob('lb_*')):
        if extra.name != 'downtown_survival_01' and (extra / 'level.json').is_file():
            wanted.append((extra, f'Assets/zombies/{extra.name}'))
    if (zombies / 'lightmaps').is_dir():   # their baked light (lightmap atlases)
        wanted.append((zombies / 'lightmaps', 'Assets/zombies/lightmaps'))
    # the shared folders go in distilled: only what the packed levels name
    shared = [zombies / 'downtown', zombies / 'lightmaps']
    levels = [source for source, _ in wanted if (source / 'level.json').is_file()]
    used = referenced_files(levels, [s for s in shared if s.is_dir()])
    for source, target in wanted:
        if not source.exists():
            sys.exit(f'missing {source}')
        distilled = source in shared
        for file in source.rglob('*'):
            relative = file.relative_to(source)
            if not file.is_file() or file.name in skip_names or relative.parts[0] in skip_dirs:
                continue
            if distilled and file.resolve() not in used:
                continue
            destination = stage / target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(file, destination)
    logo = stage / 'Assets' / 'pakal' / 'splash.png'   # the engine's logo (SplashState's default)
    logo.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ENGINE_LOGO, logo)
    finish_stage(stage, raw_textures)


def downsample_sounds(stage, rate=22050):
    """The 16-bit WAVs over 'rate' resampled to it (a phone's speaker loses nothing a sound effect has above 11 kHz, and
    the package halves what they weigh); PAKAL_APK_FULL_AUDIO=1 leaves them as they are. Returns (count, before, after)."""
    import wave
    import numpy as np
    if os.environ.get('PAKAL_APK_FULL_AUDIO') == '1':
        return 0, 0, 0
    count = before = after = 0
    for path in stage.rglob('*.wav'):
        try:
            with wave.open(str(path), 'rb') as source:
                channels, width, src_rate, frames = source.getnchannels(), source.getsampwidth(), source.getframerate(), source.getnframes()
                if width != 2 or src_rate <= rate:
                    continue
                samples = np.frombuffer(source.readframes(frames), dtype='<i2').reshape(-1, channels).astype(np.float32)
        except (wave.Error, EOFError):
            continue
        size = path.stat().st_size
        length = max(1, int(round(len(samples) * rate / src_rate)))
        at = np.linspace(0, len(samples) - 1, length)
        resampled = np.stack([np.interp(at, np.arange(len(samples)), samples[:, c]) for c in range(channels)], axis=1)
        with wave.open(str(path), 'wb') as out:
            out.setnchannels(channels)
            out.setsampwidth(2)
            out.setframerate(rate)
            out.writeframes(np.clip(np.round(resampled), -32768, 32767).astype('<i2').tobytes())
        count += 1
        before += size
        after += path.stat().st_size
    return count, before, after


def ogg_to_wav(stage, rate=22050):
    """The platform's mixer plays WAV sound effects only (SoundManagerAAudio; music is decoded from OGG): each OGG becomes a 16-bit WAV beside it, and the JSON
    documents that name it (a game's audio.json) name the WAV. Returns the number of files converted."""
    sounds = [f for f in stage.rglob('*') if f.is_file() and f.suffix.lower() == '.ogg' and 'music' not in f.parts]   # (a music track stays OGG: the engine decodes it)
    if not sounds:
        return 0
    import soundfile
    import numpy as np
    for path in sounds:
        samples, source_rate = soundfile.read(str(path), dtype='float32', always_2d=True)
        if source_rate > rate:
            length = max(1, int(round(len(samples) * rate / source_rate)))
            at = np.linspace(0, len(samples) - 1, length)
            samples = np.stack([np.interp(at, np.arange(len(samples)), samples[:, c]) for c in range(samples.shape[1])], axis=1)
            source_rate = rate
        soundfile.write(str(path.with_suffix('.wav')), samples, source_rate, subtype='PCM_16')
        path.unlink()
    for document in stage.rglob('*.json'):
        text = document.read_text(encoding='utf-8')
        changed = re.sub(r'"([^"]*?)\.ogg"', lambda m: m.group(0) if 'music' in m.group(1).lower().replace(chr(92), '/').split('/') else '"%s.wav"' % m.group(1), text)
        if changed != text:
            document.write_text(changed, encoding='utf-8')
    return len(sounds)


def version_code(version):
    """A store's version code from a version 'MAJOR.MINOR.PATCH[-beta.N|-rc.N]': each release above the one before it, and a
    release above its own betas and candidates (1.0.0-beta.1 -> 1000021, 1.0.0-rc.2 -> 1000062, 1.0.0 -> 1000099)."""
    match = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+))?', version)
    if not match:
        sys.exit(f'the version "{version}" is not MAJOR.MINOR.PATCH[-alpha.N|-beta.N|-rc.N]')
    major, minor, patch, stage, number = match.groups()
    if int(minor) > 99 or int(patch) > 99 or (number and not 0 < int(number) < 20):
        sys.exit(f'the version "{version}" does not fit the version code (minor and patch under 100, a pre-release 1-19)')
    step = {'alpha': 0, 'beta': 20, 'rc': 60, None: 99}[stage] + (int(number) if number else 0)
    return int(major) * 1000000 + int(minor) * 10000 + int(patch) * 100 + step


def build_identity(game=None):
    """Which build this is: a number that rises with each build (build-android/build_number.txt), and the commit it was
    made from (PAKAL_BUILD_COMMIT/BRANCH/DIRTY from the host's build script; git itself when it runs here). A game with a
    version (android.json "version_file": a JSON file with "version") is published under it: its version code comes from it
    (version_code), not from the counter."""
    counter = BUILD / 'build_number.txt'
    BUILD.mkdir(parents=True, exist_ok=True)
    number = (int(counter.read_text().strip() or 0) if counter.is_file() else 0) + 1
    counter.write_text(f'{number}\n')

    def git(*args):
        try:
            return subprocess.run(['git', *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()
        except (OSError, subprocess.CalledProcessError):
            return ''
    commit = os.environ.get('PAKAL_BUILD_COMMIT') or git('rev-parse', '--short', 'HEAD') or 'unknown'
    branch = os.environ.get('PAKAL_BUILD_BRANCH') or git('rev-parse', '--abbrev-ref', 'HEAD') or 'unknown'
    dirty = (os.environ.get('PAKAL_BUILD_DIRTY') == '1') if 'PAKAL_BUILD_DIRTY' in os.environ else bool(git('status', '--porcelain', '--untracked-files=no', '--ignore-submodules=all'))
    name = f'0.1+{number}.{commit}' + ('.dirty' if dirty else '')
    identity = {'build': number, 'commit': commit, 'branch': branch, 'dirty': dirty, 'version': name, 'code': number}
    version_file = GAMES.get(game, {}).get('version_file')
    if version_file:
        version = json.loads(Path(version_file).read_text(encoding='utf-8'))['version']
        identity.update(version=version, code=version_code(version))
    return identity



def build_channel(game, release, tester):
    """The channel the build label shows (docs/BUILD_IDENTITY.md), set here and never typed: "store" for a release (any of the
    store's tracks), "qa" for a tester's copy (--tools, --cheats or --side), "dev" for a developer's. A flavor is the
    edition (its android.json "edition", else its name). A store build carries the game's "label_flag_url" (android.json):
    the engine asks it at start whether this bundle was promoted to production, and hides the label then (docs/BUILD_IDENTITY.md)."""
    fields = {'channel': 'store' if release else 'qa' if tester else 'dev'}
    if FLAVOR.get('name'):
        info = GAMES[game].get('flavors', {}).get(FLAVOR['name'], {})
        fields['edition'] = info.get('edition', FLAVOR['name'])
    if release and GAMES[game].get('label_flag_url'):
        fields['label_flag_url'] = GAMES[game]['label_flag_url']
    return fields

IDENTITY = {}


def finish_stage(stage, raw_textures, sprite_dirs=()):
    """The textures compressed (unless raw), the sounds at a phone's rate, and the list of what is staged. 'sprite_dirs':
    the folders of a 2D game (see pack_textures)."""
    if IDENTITY:   # the game reads it (Assets/build.json) to tell its profile which build it is
        (stage / 'Assets').mkdir(parents=True, exist_ok=True)
        (stage / 'Assets' / 'build.json').write_text(json.dumps(IDENTITY, indent=1) + '\n')
    if not raw_textures:
        if not TEXTUREC.is_file():
            sys.exit(f"bgfx's texture tool is missing ({TEXTUREC}): configure the desktop build again (cmake --preset "
                     f"win64 builds it with bgfx), or pass --raw-textures to pack the textures uncompressed")
        count, before, after = pack_textures.compress_textures(stage, BUILD / 'texture_cache', TEXTUREC, sprite_dirs)
        print(f'{count} textures compressed ({pack_textures.FORMAT} with mips): {before / 1e6:.0f} MB -> {after / 1e6:.0f} MB')
    converted = ogg_to_wav(stage)
    if converted:
        print(f'{converted} OGG sounds converted to WAV')
    count, before, after = downsample_sounds(stage)
    if count:
        print(f'{count} sounds at 22 kHz: {before / 1e6:.0f} MB -> {after / 1e6:.0f} MB')
    write_asset_list(stage)


# Files that never go in a package, wherever they were left: secrets (a service's config with its keys, signing keys). A game's
# folder next to the player is copied whole, and a build or a tool may have left one there.
SECRET_NAMES = {'forge.json', '.env', 'local_paths.json'}
SECRET_SUFFIXES = {'.keystore', '.jks', '.p12', '.pfx', '.pem', '.key'}


def drop_secrets(stage):
    """Removes the secret files from the stage, saying which (a package must not carry them)."""
    for file in [f for f in stage.rglob('*') if f.is_file()]:
        if file.name.lower() in SECRET_NAMES or file.suffix.lower() in SECRET_SUFFIXES:
            print(f'WARNING: {file.relative_to(stage).as_posix()} is a secret: left out of the package (remove it from the game folder)')
            file.unlink()


def write_asset_list(stage):
    """assets.list: one line per staged file, path TAB md5 (the game's index of the package; PAKAL_EXTRACT=1 extracts only the
    files whose line is new or changed). Secrets are dropped first (drop_secrets)."""
    drop_secrets(stage)
    files = [f for f in stage.rglob('*') if f.is_file() and f.name != 'assets.list']
    names = sorted(f'{file.relative_to(stage).as_posix()}\t{hashlib.md5(file.read_bytes()).hexdigest()}' for file in files)
    (stage / 'assets.list').write_text('\n'.join(names) + '\n')
    print(f'{len(names)} asset files staged')


def check_sealed(game):
    """A store's build (--release) seals the game's art: its android.json names a "pack" (seal_pack), or says why not with
    "pack": false and a "pack_reason" (art anyone may take, for instance). Asked before the long build."""
    info = GAMES[game]
    if info.get('pack'):
        return
    if info.get('pack') is False and str(info.get('pack_reason', '')).strip():
        print(f'{game}: no sealed pack ({info["pack_reason"]})')
        return
    sys.exit(f'{game}: a release seals its art, and its android.json names no "pack" ({{"file": "<name>.pak", "folders": '
             f'[...under Assets/], "key": "<a header with the key>"}}, docs/ANDROID.md). Without one, anyone who unzips the '
             f'package takes its art. If that is meant, say so: "pack": false, "pack_reason": "<why>".')


def report_loose(stage, game):
    """A release: the folders of Assets/ still loose in the package after seal_pack, with their sizes (what opening the
    package hands out)."""
    assets = stage / 'Assets'
    if not assets.is_dir():
        return
    loose = []
    for folder in sorted(p for p in assets.iterdir() if p.is_dir()):
        size = sum(f.stat().st_size for f in folder.rglob('*') if f.is_file())
        loose.append(f'{folder.name} ({size / 1e6:.1f} MB)')
    if loose:
        sealed = GAMES[game].get('pack')
        print(f'{game}: loose in the package: {", ".join(loose)}' + (f' (sealed in {sealed["file"]}: {", ".join(sealed["folders"])})' if sealed else ''))


def seal_pack(stage, game):
    """The game's android.json "pack" ({"file": "<name>.pak", "folders": [...under Assets/], "key": "<a header with the key>"}):
    those folders, as they were staged for the phone (its textures, its sounds), sealed in one pack (tools/pakal_pack.py, the
    Windows package's format) at the APK's root, and taken out of it loose: the game's art cannot be had by opening the APK. The
    pack is kept uncompressed in the package, so that the game reads it in place (AndroidAssetSource.map_resource), without
    copying it out or holding it in memory. The game mounts it as on Windows (ZipSource with the key)."""
    pack = GAMES[game].get('pack')
    if not pack:
        return
    found = re.search(r'return "([^"]+)"', Path(pack['key']).read_text(encoding='utf-8'))
    if not found:
        sys.exit(f'no key in {pack["key"]}')
    sealed = BUILD / f'pack_stage_{game}'
    if sealed.exists():
        shutil.rmtree(sealed)
    sealed.mkdir(parents=True)
    for folder in pack['folders']:
        if (stage / 'Assets' / folder).is_dir():
            shutil.move(str(stage / 'Assets' / folder), str(sealed / folder))
    # sealed in this process (pakal_pack.seal_folder): a child's command line with the key would show in any process listing
    sys.path.insert(0, str(ROOT / 'tools'))
    import pakal_pack
    count = pakal_pack.seal_folder(str(sealed), str(stage / pack['file']), found.group(1))
    print(f'> pakal_pack.seal_folder {sealed} {stage / pack["file"]} ***: {count} files')
    shutil.rmtree(sealed)
    write_asset_list(stage)


def build_java():
    """The activity (android/java: NativeActivity full screen) compiled to classes.dex. Returns its path."""
    out = BUILD / 'java'
    if out.exists():
        shutil.rmtree(out)
    (out / 'classes').mkdir(parents=True)
    sources = sorted(str(p) for p in (ROOT / 'android' / 'java').rglob('*.java'))
    run([JDK / 'bin' / ('javac' + EXE), '-source', '8', '-target', '8', '-Xlint:-options', '-bootclasspath', PLATFORM / 'android.jar',
         '-d', out / 'classes', *sources])
    classes = sorted(str(p) for p in (out / 'classes').rglob('*.class'))
    run([BUILD_TOOLS / ('d8' + SCRIPT), '--min-api', str(MIN_SDK), '--lib', PLATFORM / 'android.jar', '--output', out, *classes],
        env=dict(os.environ, JAVA_HOME=str(JDK)))
    return out / 'classes.dex'


def write_manifest(game, tools=False):
    """android/AndroidManifest.xml with the game's package, name, library and permissions (a socket, even on 127.0.0.1,
    needs INTERNET: a build with the engine's tools asks for it)."""
    info = GAMES[game]
    text = (ROOT / 'android' / 'AndroidManifest.xml').read_text(encoding='utf-8')
    wanted = list(info['permissions']) + (['android.permission.INTERNET'] if tools and 'android.permission.INTERNET' not in info['permissions'] else [])
    permissions = ''.join(f'    <uses-permission android:name="{name}" />\n' for name in wanted)
    text = text.replace('android:versionCode="1"', f'android:versionCode="{IDENTITY.get("code", 1)}"')
    text = text.replace('android:versionName="0.1"', f'android:versionName="{IDENTITY.get("version", "0.1")}"')
    text = (text.replace('@PACKAGE@', info['package']).replace('@LABEL@', info['label'])
            .replace('@LIBRARY@', info['library']).replace('    @PERMISSIONS@\n', permissions))
    if info.get('icon'):
        text = text.replace('<application\n', '<application\n        android:icon="@mipmap/ic_launcher"\n', 1)
    # links that open the game (android.json "links": [{"scheme": "mygame", "host": "invite"}]): a VIEW filter each, and one
    # activity for them all (singleTask: a link while it runs reaches it, PakalActivity.onNewIntent; OSManager::take_link)
    links = info.get('links', [])
    if links:
        filters = ''.join('            <intent-filter>\n'
                          '                <action android:name="android.intent.action.VIEW" />\n'
                          '                <category android:name="android.intent.category.DEFAULT" />\n'
                          '                <category android:name="android.intent.category.BROWSABLE" />\n'
                          f'                <data android:scheme="{link["scheme"]}"' + (f' android:host="{link["host"]}"' if link.get('host') else '') + ' />\n'
                          '            </intent-filter>\n' for link in links)
        text = text.replace('        </activity>\n', filters + '        </activity>\n', 1)
        if 'android:launchMode=' not in text:   # (the template's activity is singleTask already)
            text = text.replace('android:exported="true"\n', 'android:exported="true"\n            android:launchMode="singleTask"\n', 1)
    manifest = BUILD / f'AndroidManifest-{game}.xml'
    manifest.write_text(text, encoding='utf-8')
    return manifest


# The launcher icon's size per screen density (dp 48)
ICON_SIZES = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}


def compile_icon(game):
    """The game's icon (GAMES[game]['icon'], a square image) at every density, compiled by aapt2. Returns the compiled
    resources' archive, or None for a game without an icon. With 'icon_foreground' (a square image, its subject inside the
    middle 66%: the launcher crops the rest to its own shape) and 'icon_background' (a color, #RRGGBB) the icon is also
    adaptive (Android 8 on), the square one staying for the launchers that cannot."""
    source = GAMES[game].get('icon')
    if not source:
        return None
    from PIL import Image
    res = BUILD / f'res_{game}'
    if res.exists():
        shutil.rmtree(res)
    image = Image.open(source).convert('RGBA')
    for density, size in ICON_SIZES.items():
        folder = res / f'mipmap-{density}'
        folder.mkdir(parents=True)
        image.resize((size, size), Image.LANCZOS).save(folder / 'ic_launcher.png')
    foreground = GAMES[game].get('icon_foreground')
    if foreground:
        layer = Image.open(foreground).convert('RGBA')
        for density, size in ICON_SIZES.items():   # (an adaptive layer is 108 dp: 2.25 times the 48 of the icon)
            layer.resize((size * 9 // 4, size * 9 // 4), Image.LANCZOS).save(res / f'mipmap-{density}' / 'ic_launcher_foreground.png')
        (res / 'values').mkdir()
        (res / 'values' / 'ic_launcher_background.xml').write_text(
            '<?xml version="1.0" encoding="utf-8"?>\n<resources>\n    <color name="ic_launcher_background">'
            f'{GAMES[game].get("icon_background", "#000000")}</color>\n</resources>\n', encoding='utf-8')
        (res / 'mipmap-anydpi-v26').mkdir()
        (res / 'mipmap-anydpi-v26' / 'ic_launcher.xml').write_text(
            '<?xml version="1.0" encoding="utf-8"?>\n'
            '<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">\n'
            '    <background android:drawable="@color/ic_launcher_background" />\n'
            '    <foreground android:drawable="@mipmap/ic_launcher_foreground" />\n'
            '</adaptive-icon>\n', encoding='utf-8')
    compiled = BUILD / f'res_{game}.zip'
    run([BUILD_TOOLS / ('aapt2' + EXE), 'compile', '--dir', res, '-o', compiled])
    return compiled


def has_console():
    """Whether a person is at this run's terminal. On Windows isatty() also takes NUL for a console, so the check is
    GetConsoleMode on stdin's handle."""
    if os.name == 'nt':
        import ctypes
        import msvcrt
        try:
            handle = msvcrt.get_osfhandle(sys.stdin.fileno())
        except (OSError, ValueError, AttributeError):
            return False
        mode = ctypes.c_uint32()
        return bool(ctypes.windll.kernel32.GetConsoleMode(ctypes.c_void_p(handle), ctypes.byref(mode)))
    try:
        return sys.stdin is not None and sys.stdin.isatty()
    except (OSError, ValueError):
        return False


PASSWORDS = {}   # the upload key's passwords for this run: asked once, kept only in this process's memory


def signing_passwords(release):
    """{'PAKAL_KEYSTORE_PASS': ..., 'PAKAL_KEY_PASS': ...}: what a signing tool's own environment gets (apksigner env:,
    jarsigner :env), never this process's environment. A release's are asked with getpass at the terminal of the person
    running it (only the person releasing signs), never read from a file, a config or the environment, and a run with no
    console is refused. The debug key's are 'android'."""
    if not release:
        return {'PAKAL_KEYSTORE_PASS': 'android', 'PAKAL_KEY_PASS': 'android'}
    if not PASSWORDS:
        if not has_console():
            sys.exit('a release is signed by the person at the terminal: its password is asked there, and this run has no console')
        import getpass
        store = getpass.getpass('upload keystore password: ')
        key = getpass.getpass("upload key password (Enter if it is the keystore's): ") or store
        if not store:
            sys.exit('no password given')
        PASSWORDS.update(PAKAL_KEYSTORE_PASS=store, PAKAL_KEY_PASS=key)
    return dict(PASSWORDS)


def signing_key(release):
    """(keystore, alias) to sign with. A release is signed with the store's upload key: PAKAL_KEYSTORE (the .jks file) and
    PAKAL_KEY_ALIAS (the environment or android/local_paths.json: where the key is, not what opens it); its passwords come
    from signing_passwords. Otherwise the debug key of build-android (made the first time)."""
    if release:
        keystore = Path(setting('PAKAL_KEYSTORE'))
        if not keystore.is_file():
            sys.exit(f'PAKAL_KEYSTORE not found ({keystore})')
        return keystore, setting('PAKAL_KEY_ALIAS')
    keystore = BUILD / 'debug.keystore'
    if not keystore.exists():
        run([JDK / 'bin' / ('keytool' + EXE), '-genkeypair', '-keystore', keystore, '-storepass', 'android', '-keypass', 'android',
             '-alias', 'debug', '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000', '-dname', 'CN=Pakal Debug'])
    return keystore, 'debug'


FLAVOR = {}   # the flavor being built: {'name': ...} (main)


def output_stem(game):
    """pakal-<game>[-<flavor>]: the packages' names."""
    return f'pakal-{game}' + (f'-{FLAVOR["name"]}' if FLAVOR.get('name') else '')


def write_identity(package):
    """<package>.build.json: the build's identity (its number, commit, branch, version, code), which the package's name does not
    say (every build of a release has the same version): the performance suite reads it."""
    package.with_suffix('.build.json').write_text(json.dumps(IDENTITY, indent=1) + '\n', encoding='utf-8')


def write_flavor_header(defines):
    """build-android/flavor/pakal_flavor.h: a #define per setting (a number as it is, anything else a string). Written only
    when it changes (the files that include it compile again). Returns its folder."""
    folder = BUILD / 'flavor'
    folder.mkdir(parents=True, exist_ok=True)
    lines = ["// Written by android/build_apk.py (--flavor, --release): the build's settings", '#pragma once']
    for name, value in sorted(defines.items()):
        text = str(value)
        literal = text if text.lstrip('-').isdigit() else json.dumps(text, ensure_ascii=False)
        lines.append(f'#define {name} {literal}')
    header = folder / 'pakal_flavor.h'
    text = '\n'.join(lines) + '\n'
    if not header.is_file() or header.read_text(encoding='utf-8') != text:
        header.write_text(text, encoding='utf-8')
    return folder


def apply_flavor(game, flavor, defines, signers):
    """A game's flavor (its android.json "flavors": {"<name>": {"package", "label", "defines", "required", "signers"}}): the
    app's package and name replaced, and its settings in pakal_flavor.h: the flavor's defines, then the command line's, and
    PAKAL_FLAVOR_NAME and PAKAL_APP_SIGNERS (every one of "signers" and --signer: the SHA-256 of the certificates its copies
    may carry, the upload key's and the store's app signing key's; OSManager::app_signers is compared with them). No key is
    opened for it: a certificate's SHA-256 is public (keytool -printcert -jarfile on a package it signed). Returns the
    header's folder."""
    flavors = GAMES[game].get('flavors', {})
    if flavor not in flavors:
        sys.exit(f'{game} has no flavor "{flavor}" (its android.json: {", ".join(flavors) or "none"})')
    info = flavors[flavor]
    GAMES[game] = {**GAMES[game], **{k: info[k] for k in ('package', 'label') if k in info}}
    settings = {'PAKAL_LOCKED': 1, **info.get('defines', {}), **defines}   # a flavor is a release: locked too
    missing = [name for name in info.get('required', []) if not str(settings.get(name, '')).strip()]
    if missing:
        sys.exit(f'the flavor "{flavor}" needs ' + ', '.join(f'--define {name}=...' for name in missing))
    digests = [d.lower().replace(':', '') for d in [*info.get('signers', []), *signers]]
    if not digests:
        sys.exit(f'the flavor "{flavor}" lists no "signers": its own copy would never play')
    settings['PAKAL_FLAVOR_NAME'] = flavor
    settings['PAKAL_APP_SIGNERS'] = ','.join(dict.fromkeys(digests))
    FLAVOR['name'] = flavor
    print(f'flavor {flavor}: {GAMES[game]["package"]}, ' + ', '.join(f'{k}={v}' for k, v in sorted(settings.items()) if k != 'PAKAL_APP_SIGNERS')
          + f', {len(settings["PAKAL_APP_SIGNERS"].split(","))} signer(s)')
    return write_flavor_header(settings)


ASSET_PACK = 'gamedata'
ASSET_PACK_MANIFEST = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<manifest xmlns:android="http://schemas.android.com/apk/res/android" xmlns:dist="http://schemas.android.com/apk/distribution"\n'
    '    package="@PACKAGE@" split="@PACK@">\n'
    '    <dist:module dist:type="asset-pack">\n'
    '        <dist:fusing dist:include="true" />\n'
    '        <dist:delivery><dist:install-time /></dist:delivery>\n'
    '    </dist:module>\n'
    '</manifest>\n')


def bundletool():
    """bundletool's jar: BUNDLETOOL (the environment or android/local_paths.json), else build-android/bundletool.jar."""
    value = os.environ.get('BUNDLETOOL') or local_paths().get('BUNDLETOOL')
    path = Path(value) if value else BUILD / 'bundletool.jar'
    if not path.is_file():
        sys.exit(f'bundletool not found ({path}): download bundletool-all-<version>.jar from '
                 'https://github.com/google/bundletool/releases and point BUNDLETOOL at it')
    return path


def make_apk(game='zombies', raw_textures=False, cheats=False, tools=False, release=False, bundle=False, side=False):
    """The game's package: an APK to install (build-android/pakal-<game>.apk), or with 'bundle' the Android App Bundle a
    store takes (build-android/pakal-<game>-<version>.aab). 'release': signed with the upload key (signing_key)."""
    if release and (cheats or tools):
        sys.exit('a release carries neither the cheats nor the tools')
    if release:
        check_sealed(game)
    keystore, alias = signing_key(release)
    RELEASE['on'] = release
    IDENTITY.clear()
    IDENTITY.update(build_identity(game))
    IDENTITY.update(build_channel(game, release, tools or cheats or side))
    if release and IDENTITY['dirty']:
        print('WARNING: a release from a working tree with changes not committed')
    print(f'build {IDENTITY["version"]} (code {IDENTITY["code"]}, {IDENTITY["branch"]}, channel {IDENTITY["channel"]})')
    stage = BUILD / f'apk_stage_{game}'
    stage_assets(stage, game, raw_textures)
    if not cheats:
        # a release carries none of the tester's cheats: not their panel either
        for panel in stage.rglob('cheats.rml'):
            panel.unlink()
    seal_pack(stage, game)
    if release:
        report_loose(stage, game)

    unsigned = BUILD / 'pakal-unsigned.apk'
    icon = compile_icon(game)
    # (a bundle's resources are in protocol buffers: --proto-format)
    run([BUILD_TOOLS / ('aapt2' + EXE), 'link', '-o', unsigned, '--manifest', write_manifest(game, tools),
         '-I', PLATFORM / 'android.jar', '-A', stage, '-0', 'pak', '--min-sdk-version', str(MIN_SDK), '--target-sdk-version', str(TARGET_SDK),
         *(['--proto-format'] if bundle else []), *([icon] if icon else [])])

    name = GAMES[game]['library']
    library = BUILD / f'lib{name}-stripped.so'
    run([NDK / 'toolchains/llvm/prebuilt' / ('windows-x86_64' if HOST_WINDOWS else 'linux-x86_64') / 'bin' / ('llvm-strip' + EXE), '-o', library, BUILD / f'lib{name}.so'])
    dex = build_java()
    env = dict(os.environ, JAVA_HOME=str(JDK))
    sign_env = dict(env, **signing_passwords(release))   # only the signing tool's environment carries the passwords
    java = JDK / 'bin' / ('java' + EXE)
    if bundle:
        return make_bundle(game, unsigned, library, dex, keystore, alias, env, sign_env, java)
    add_to_apk(unsigned, [(library, f'lib/arm64-v8a/lib{name}.so', zipfile.ZIP_STORED), (dex, 'classes.dex', zipfile.ZIP_DEFLATED)])

    aligned = BUILD / 'pakal-aligned.apk'
    if aligned.exists():
        aligned.unlink()
    run([BUILD_TOOLS / ('zipalign' + EXE), '-P', '16', '-f', '4', unsigned, aligned])

    final = BUILD / f'{output_stem(game)}.apk'
    run([BUILD_TOOLS / ('apksigner' + SCRIPT), 'sign', '--ks', keystore, '--ks-pass', 'env:PAKAL_KEYSTORE_PASS',
         '--key-pass', 'env:PAKAL_KEY_PASS', '--ks-key-alias', alias, '--out', final, aligned], env=sign_env)
    run([BUILD_TOOLS / ('apksigner' + SCRIPT), 'verify', '--verbose', final], env=env)
    for step in (unsigned, aligned):   # only the signed one stays (an earlier step looks installable and is not)
        step.unlink(missing_ok=True)
    write_identity(final)
    print(f'{final} ({final.stat().st_size / 1e6:.1f} MB)')


def add_to_apk(apk, files):
    """Writes the APK again with 'files' added ((source, name in the APK, compression)). aapt2 on Windows names the assets
    of subfolders with backslashes (assets/Assets\\ui\\x.png) in the zip's headers; written whole here, every entry has the
    same name, with forward slashes, in its own header and in the directory. Appending instead left the two different (the
    directory rewritten with '/', the headers with '\\': zipalign's "header mismatch" for each, and an APK that only
    worked because Android reads the directory)."""
    rebuilt = apk.with_name(apk.stem + '-rebuilt' + apk.suffix)
    with zipfile.ZipFile(apk) as source, zipfile.ZipFile(rebuilt, 'w') as out:
        for item in source.infolist():
            entry = zipfile.ZipInfo(item.orig_filename.replace('\\', '/'), date_time=item.date_time)
            entry.compress_type = item.compress_type
            entry.external_attr = item.external_attr
            with source.open(item) as data, out.open(entry, 'w', force_zip64=item.file_size >= zipfile.ZIP64_LIMIT) as target:
                shutil.copyfileobj(data, target, 1 << 20)
        for path, name, compression in files:
            out.write(path, name, compress_type=compression)
    rebuilt.replace(apk)


def make_bundle(game, linked, library, dex, keystore, alias, env, sign_env, java):
    """The Android App Bundle: the linked resources (proto format) laid out as bundletool's base module (manifest/, dex/,
    lib/<abi>/, assets/, res/, resources.pb), built into an .aab and signed with jarsigner (the store signs what it hands
    out with its own key: Play App Signing). With the game's "asset_pack", its assets go in an install-time asset pack
    (ASSET_PACK), which the store installs with the app and the game reads as its own assets."""
    module = BUILD / f'bundle_base_{game}.zip'
    modules = [module]
    pack = None
    if GAMES[game].get('asset_pack'):
        pack_manifest = BUILD / f'bundle_pack_{game}.xml'
        pack_manifest.write_text(ASSET_PACK_MANIFEST.replace('@PACKAGE@', GAMES[game]['package']).replace('@PACK@', ASSET_PACK), encoding='utf-8')
        pack_proto = BUILD / f'bundle_pack_{game}.apk'
        run([BUILD_TOOLS / ('aapt2' + EXE), 'link', '--proto-format', '-o', pack_proto, '--manifest', pack_manifest, '-I', PLATFORM / 'android.jar'])
        pack = zipfile.ZipFile(BUILD / f'bundle_{ASSET_PACK}_{game}.zip', 'w', zipfile.ZIP_DEFLATED)
        with zipfile.ZipFile(pack_proto) as source:
            pack.writestr('manifest/AndroidManifest.xml', source.read('AndroidManifest.xml'))
        modules.append(Path(pack.filename))
        for step in (pack_manifest, pack_proto):
            step.unlink(missing_ok=True)
    with zipfile.ZipFile(linked) as source, zipfile.ZipFile(module, 'w', zipfile.ZIP_DEFLATED) as base:
        for item in source.infolist():
            name = item.filename.replace('\\', '/')
            target = 'manifest/AndroidManifest.xml' if name == 'AndroidManifest.xml' else name
            (pack if pack and name.startswith('assets/') else base).writestr(target, source.read(item.filename))
        base.write(dex, 'dex/classes.dex')
        base.write(library, f'lib/arm64-v8a/lib{GAMES[game]["library"]}.so')
    if pack:
        pack.close()
    final = BUILD / f'{output_stem(game)}-{IDENTITY["version"]}.aab'
    final.unlink(missing_ok=True)
    # a sealed pack stays uncompressed in what the store hands out (the game reads it in place: seal_pack)
    config = BUILD / 'bundle_config.json'
    config.write_text(json.dumps({'compression': {'uncompressedGlob': ['assets/*.pak', 'assets/**/*.pak']}}))
    run([java, '-jar', bundletool(), 'build-bundle', '--modules=' + ','.join(str(m) for m in modules), f'--output={final}', f'--config={config}'], env=env)
    run([JDK / 'bin' / ('jarsigner' + EXE), '-keystore', keystore, '-storepass:env', 'PAKAL_KEYSTORE_PASS',
         '-keypass:env', 'PAKAL_KEY_PASS', '-sigalg', 'SHA256withRSA', '-digestalg', 'SHA-256', final, alias], env=sign_env)
    run([java, '-jar', bundletool(), 'validate', f'--bundle={final}'], env=env)
    # the native library's symbols, for the store to read its crashes (Play Console: the version's App bundle explorer > upload
    # debug symbols): the library before it was stripped, at its ABI's path in a zip
    symbols = BUILD / f'{output_stem(game)}-{IDENTITY["version"]}-native-debug-symbols.zip'
    with zipfile.ZipFile(symbols, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(BUILD / f'lib{GAMES[game]["library"]}.so', f'arm64-v8a/lib{GAMES[game]["library"]}.so')
    print(f'{symbols} ({symbols.stat().st_size / 1e6:.1f} MB: the native debug symbols)')
    for step in (linked, *modules):
        step.unlink(missing_ok=True)
    write_identity(final)
    print(f'{final} ({final.stat().st_size / 1e6:.1f} MB, version {IDENTITY["version"]}, code {IDENTITY["code"]})')
    return final


def install_bundle(bundle, serial, release):
    """A bundle installed on a connected phone as the store would install it: bundletool build-apks for that device, signed with
    the key the bundle was (signing_key; for a release, the passwords typed at the terminal: signing_passwords), then install-apks.
    bundletool takes a password only as text on its command line (which other processes can read) or in a file. A release's go in
    files of a private folder of this run's own (the user's temporary folder), deleted as soon as build-apks ends, whether it
    worked or not; a run killed before that leaves the folder, and the next run deletes it (remove_stale_password_folders)."""
    keystore, alias = signing_key(release)
    adb = SDK / 'platform-tools' / ('adb' + EXE)
    apks = bundle.with_suffix('.apks')
    java = JDK / 'bin' / ('java' + EXE)
    command = [java, '-jar', bundletool(), 'build-apks', f'--bundle={bundle}', f'--output={apks}', '--overwrite', f'--ks={keystore}',
               f'--ks-key-alias={alias}', '--connected-device', f'--device-id={serial}', f'--adb={adb}']
    if not release:   # the debug key's password is no secret
        run(command + ['--ks-pass=pass:android', '--key-pass=pass:android'])
    else:
        passwords = signing_passwords(True)
        folder = Path(tempfile.mkdtemp(prefix=PASSWORD_FOLDER_PREFIX))   # only this user can open it (0700; the user's TEMP on Windows)
        try:
            store_pass, key_pass = folder / 'store', folder / 'key'
            store_pass.write_text(passwords['PAKAL_KEYSTORE_PASS'], encoding='utf-8')
            key_pass.write_text(passwords['PAKAL_KEY_PASS'], encoding='utf-8')
            run(command + [f'--ks-pass=file:{store_pass}', f'--key-pass=file:{key_pass}'])
        finally:
            shutil.rmtree(folder, ignore_errors=True)
            if folder.exists():
                print(f'WARNING: could not delete {folder}, which holds the upload key\'s passwords: delete it now')
    run([java, '-jar', bundletool(), 'install-apks', f'--apks={apks}', f'--device-id={serial}', f'--adb={adb}'])
    apks.unlink(missing_ok=True)


PASSWORD_FOLDER_PREFIX = 'pakal-sign-'


def remove_stale_password_folders():
    """The password folders an earlier run left behind (killed during install_bundle's build-apks): deleted."""
    for folder in Path(tempfile.gettempdir()).glob(PASSWORD_FOLDER_PREFIX + '*'):
        shutil.rmtree(folder, ignore_errors=True)
        print(f'deleted {folder}, left by an earlier run')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', choices=[*GAMES, 'all'], default='all', help='the game to pack (all: one APK each)')
    parser.add_argument('--skip-native', action='store_true', help='pack without rebuilding the libraries')
    parser.add_argument('--raw-textures', action='store_true', help='keep the PNG/TGA textures as they are (no ASTC)')
    parser.add_argument('--cheats', action='store_true', help="with the game's cheats (its tester's builds; never a release)")
    parser.add_argument('--tools', action='store_true',
                        help="with the engine's MCP tools, to profile and tune it through adb forward (docs/PROFILING.md; never a release)")
    parser.add_argument('--release', action='store_true',
                        help='signed with the upload key (PAKAL_KEYSTORE, PAKAL_KEY_ALIAS; its passwords asked at the terminal), for a store')
    parser.add_argument('--bundle', action='store_true', help='an Android App Bundle (.aab, what Google Play takes) instead of an APK')
    parser.add_argument('--side', action='store_true',
                        help="a tester's copy beside the store's: its package ends in .dev and its name in (dev), so it installs next to "
                             "the one from the store (signed with another key, it could not replace it) with its own saved games")
    parser.add_argument('--flavor', help="one of the game's flavors (its android.json \"flavors\": a demo...), its own app; a release")
    parser.add_argument('--define', action='append', default=[], metavar='NAME=VALUE', help="a flavor's setting (pakal_flavor.h)")
    parser.add_argument('--signer', action='append', default=[], metavar='SHA256',
                        help="a certificate a flavor's copies may carry, besides its \"signers\" (the store's app signing key)")
    parser.add_argument('--install-bundle', metavar='SERIAL', help='the bundle installed on that phone, as the store would')
    args = parser.parse_args()
    if args.side and (args.release or args.bundle):
        sys.exit('--side is a local copy: not with --release or --bundle')
    if args.install_bundle and not args.bundle:
        sys.exit('--install-bundle installs the --bundle being made')
    games = list(GAMES) if args.game == 'all' else [args.game]
    flavor_dir = None
    if args.flavor:
        if len(games) != 1:
            sys.exit('--flavor is for one game (--game)')
        if args.cheats or args.tools or args.side:
            sys.exit('a flavor carries no cheats and no tools, and is no side copy')
        if not args.release:
            sys.exit('a flavor is signed with the upload key: add --release')
        defines = dict(item.split('=', 1) for item in args.define)
        flavor_dir = apply_flavor(games[0], args.flavor, defines, args.signer)
    if args.release and not args.flavor:
        # a release is locked (pakal_flavor.h PAKAL_LOCKED): the device's pakal_env.txt is not read, so no debug switch can be
        # turned on in a player's copy; the package's own defaults (android.json "env", PAKAL_RELEASE) still are
        if args.cheats or args.tools or args.side:
            sys.exit('a release carries no cheats and no tools, and is no side copy')
        flavor_dir = write_flavor_header({'PAKAL_LOCKED': 1})
    if args.release and args.skip_native:
        sys.exit('a release builds its own native code (locked): leave out --skip-native')
    if args.side:
        for game in games:
            GAMES[game] = dict(GAMES[game], package=GAMES[game]['package'] + '.dev', label=GAMES[game]['label'] + ' (dev)')
    remove_stale_password_folders()
    if args.release:   # asked before the long build, so they are typed once and the build can run unattended
        for game in games:
            check_sealed(game)
        signing_key(True)
        signing_passwords(True)
    if not args.skip_native:
        build_native(games, args.cheats, args.tools, flavor_dir)
    for game in games:
        package = make_apk(game, args.raw_textures, args.cheats, args.tools, args.release, args.bundle, args.side)
        if args.install_bundle:
            install_bundle(package, args.install_bundle, args.release)


if __name__ == '__main__':
    main()
