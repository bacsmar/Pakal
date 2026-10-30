"""Builds the Android APKs of Pakal's games (arm64-v8a, OpenGL ES 3): one per game, each its own app (its package, its
name on the launcher), so they are all installed side by side.

    python android/build_apk.py [--game zombies|brawler|dungeon|<a game of games/>|all] [--skip-native] [--raw-textures]

Needs the Android NDK/SDK/JDK (point ANDROID_NDK, ANDROID_SDK and JAVA_HOME at them, or write them in
android/local_paths.json as {"ANDROID_NDK": ..., "ANDROID_SDK": ..., "JAVA_HOME": ...}, a file Git ignores) and the vcpkg
arm64-android packages:

    vcpkg install --triplet arm64-android --x-manifest-root=android/vcpkg --x-install-root=build-android/vcpkg_installed

The games' data comes from the desktop build (build-win64/bin/Assets): build it first, and for Zombies generate the levels
(tools/zombies_convert/build_downtown.py). Output: build-android/pakal-<game>.apk (signed with a debug key).
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path
from urllib.parse import unquote

import pack_textures

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / 'build-android'
DESKTOP_ASSETS = ROOT / 'build-win64' / 'bin' / 'Assets'
# bgfx's texture tool (external/bgfx/.build/projects/vs2022/texturec.vcxproj, Release x64)
TEXTUREC = ROOT / 'external' / 'bgfx' / '.build' / 'win64_vs2022' / 'bin' / 'texturecRelease.exe'


# Each game: its app's package and name, its library (android/CMakeLists.txt) and what else its manifest needs
GAMES = {
    'zombies': {'package': 'org.pakalengine.zombies', 'label': 'Zombies', 'library': 'pakal_zombies', 'permissions': []},
    'brawler': {'package': 'org.pakalengine.brawler', 'label': 'Pakal Brawler', 'library': 'pakal_brawler', 'permissions': []},
    # online: the network, and the Wi-Fi's broadcasts (the games on the network answer the menu's question)
    'dungeon': {'package': 'org.pakalengine.dungeon', 'label': 'Pakal Dungeon', 'library': 'pakal_dungeon',
                'permissions': ['android.permission.INTERNET', 'android.permission.ACCESS_NETWORK_STATE',
                                'android.permission.ACCESS_WIFI_STATE', 'android.permission.CHANGE_WIFI_MULTICAST_STATE']},
}
# The games kept outside the engine's examples (games/<name>/, a folder only some checkouts have): each one says what it
# is in its android.json (package, label, library, permissions; an icon and a stager, files of its folder). Its library
# comes from its android.cmake (android/CMakeLists.txt includes it). "staging": "zombies" packs it as the Zombies module
# (its levels, with its own Assets/<name>); a "stager" (a Python file with stage(stage, raw_textures, apk)) packs it its
# own way.
for recipe in sorted((ROOT / 'games').glob('*/android.json')):
    entry = json.loads(recipe.read_text(encoding='utf-8'))
    entry.setdefault('permissions', [])
    for key in ('icon', 'stager'):
        if key in entry:
            entry[key] = recipe.parent / entry[key]
    GAMES[recipe.parent.name] = entry


def local_paths():
    """android/local_paths.json: this machine's tool folders (not in Git)."""
    path = ROOT / 'android' / 'local_paths.json'
    return json.loads(path.read_text(encoding='utf-8')) if path.is_file() else {}


def tool_dir(env):
    value = os.environ.get(env) or local_paths().get(env)
    if not value:
        sys.exit(f'{env} is not set (the environment or android/local_paths.json)')
    path = Path(value)
    if not path.exists():
        sys.exit(f'{env} not found ({path})')
    return path


NDK = tool_dir('ANDROID_NDK')
SDK = tool_dir('ANDROID_SDK')
JDK = tool_dir('JAVA_HOME')
BUILD_TOOLS = sorted((SDK / 'build-tools').iterdir())[-1]
PLATFORM = sorted((SDK / 'platforms').iterdir())[0]  # the lowest installed platform is enough for linking
CMAKE_BIN = next((SDK / 'cmake').iterdir()) / 'bin'


def run(command, **kwargs):
    print('>', ' '.join(str(part) for part in command))
    subprocess.check_call([str(part) for part in command], **kwargs)


def build_native(games, cheats=False):
    env = dict(os.environ, PATH=str(CMAKE_BIN) + os.pathsep + os.environ['PATH'])
    run([CMAKE_BIN / 'cmake.exe', '-S', ROOT / 'android', '-B', BUILD, '-G', 'Ninja',
         f'-DPAKAL_GAME_CHEATS={"ON" if cheats else "OFF"}',   # a game's cheats (its android.cmake reads it)
         f'-DCMAKE_TOOLCHAIN_FILE={NDK / "build/cmake/android.toolchain.cmake"}',
         '-DANDROID_ABI=arm64-v8a', '-DANDROID_PLATFORM=android-26', '-DANDROID_STL=c++_static',
         '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON', '-DCMAKE_BUILD_TYPE=Release',
         f'-DCMAKE_MAKE_PROGRAM={CMAKE_BIN / "ninja.exe"}'], env=env)
    targets = [arg for game in games for arg in ('--target', GAMES[game]['library'])]
    run([CMAKE_BIN / 'cmake.exe', '--build', BUILD, *targets, '--target', 'PakalShadersEssl'], env=env)


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
                if path.is_file() and under_root(path):
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
              (DESKTOP_ASSETS / 'ui', 'Assets/ui')]
    downtown = DESKTOP_ASSETS / 'zombies' / 'downtown'   # (it may be a link to another checkout's)
    for model in sorted(referenced_files([DESKTOP_ASSETS / game], [downtown])):
        wanted.append((model, (Path('Assets/zombies/downtown') / model.relative_to(downtown.resolve())).as_posix()))
    return wanted


def stage_assets(stage, game='zombies', raw_textures=False):
    """The files the game needs, with assets.list (the game extracts them on its first run). The other games' are listed by
    game_files. For Zombies, the levels: the original one (hollywood_s01t01), Downtown's missions and the
    level builder's (the kit's models in ../downtown). The first level also holds the effects (muzzle, bullet, casings)
    that Downtown names. The game opens on its menu."""
    if stage.exists():
        shutil.rmtree(stage)
    stager = GAMES[game].get('stager')
    if stager:
        import importlib.util
        spec = importlib.util.spec_from_file_location(f'{game}_stager', stager)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        module.stage(stage, raw_textures, sys.modules[__name__])
        return
    if game != 'zombies' and GAMES[game].get('staging') != 'zombies':
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


def finish_stage(stage, raw_textures):
    """The textures compressed (unless raw), the sounds at a phone's rate, and the list of what is staged."""
    if not raw_textures:
        count, before, after = pack_textures.compress_textures(stage, BUILD / 'texture_cache', TEXTUREC)
        print(f'{count} textures compressed ({pack_textures.FORMAT} with mips): {before / 1e6:.0f} MB -> {after / 1e6:.0f} MB')
    count, before, after = downsample_sounds(stage)
    if count:
        print(f'{count} sounds at 22 kHz: {before / 1e6:.0f} MB -> {after / 1e6:.0f} MB')
    # one line per file, path TAB md5: the game extracts only the files whose line is new or changed
    files = [f for f in stage.rglob('*') if f.is_file() and f.name != 'assets.list']
    names = sorted(f'{file.relative_to(stage).as_posix()}\t{hashlib.md5(file.read_bytes()).hexdigest()}' for file in files)
    (stage / 'assets.list').write_text('\n'.join(names) + '\n')
    print(f'{len(names)} asset files staged')


def build_java():
    """The activity (android/java: NativeActivity full screen) compiled to classes.dex. Returns its path."""
    out = BUILD / 'java'
    if out.exists():
        shutil.rmtree(out)
    (out / 'classes').mkdir(parents=True)
    sources = sorted(str(p) for p in (ROOT / 'android' / 'java').rglob('*.java'))
    run([JDK / 'bin/javac.exe', '-source', '8', '-target', '8', '-Xlint:-options', '-bootclasspath', PLATFORM / 'android.jar',
         '-d', out / 'classes', *sources])
    classes = sorted(str(p) for p in (out / 'classes').rglob('*.class'))
    run([BUILD_TOOLS / 'd8.bat', '--min-api', '26', '--lib', PLATFORM / 'android.jar', '--output', out, *classes],
        env=dict(os.environ, JAVA_HOME=str(JDK)))
    return out / 'classes.dex'


def write_manifest(game):
    """android/AndroidManifest.xml with the game's package, name, library and permissions."""
    info = GAMES[game]
    text = (ROOT / 'android' / 'AndroidManifest.xml').read_text(encoding='utf-8')
    permissions = ''.join(f'    <uses-permission android:name="{name}" />\n' for name in info['permissions'])
    text = (text.replace('@PACKAGE@', info['package']).replace('@LABEL@', info['label'])
            .replace('@LIBRARY@', info['library']).replace('    @PERMISSIONS@\n', permissions))
    if info.get('icon'):
        text = text.replace('<application\n', '<application\n        android:icon="@mipmap/ic_launcher"\n', 1)
    manifest = BUILD / f'AndroidManifest-{game}.xml'
    manifest.write_text(text, encoding='utf-8')
    return manifest


# The launcher icon's size per screen density (dp 48)
ICON_SIZES = {'mdpi': 48, 'hdpi': 72, 'xhdpi': 96, 'xxhdpi': 144, 'xxxhdpi': 192}


def compile_icon(game):
    """The game's icon (GAMES[game]['icon'], a square image) at every density, compiled by aapt2. Returns the compiled
    resources' archive, or None for a game without an icon."""
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
    compiled = BUILD / f'res_{game}.zip'
    run([BUILD_TOOLS / 'aapt2.exe', 'compile', '--dir', res, '-o', compiled])
    return compiled


def make_apk(game='zombies', raw_textures=False, cheats=False):
    stage = BUILD / f'apk_stage_{game}'
    stage_assets(stage, game, raw_textures)
    if not cheats:
        # a release carries none of the tester's cheats: not their panel either
        for panel in stage.rglob('cheats.rml'):
            panel.unlink()

    unsigned = BUILD / 'pakal-unsigned.apk'
    icon = compile_icon(game)
    run([BUILD_TOOLS / 'aapt2.exe', 'link', '-o', unsigned, '--manifest', write_manifest(game),
         '-I', PLATFORM / 'android.jar', '-A', stage, '--min-sdk-version', '26', '--target-sdk-version', '34',
         *([icon] if icon else [])])

    name = GAMES[game]['library']
    library = BUILD / f'lib{name}-stripped.so'
    run([NDK / 'toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe', '-o', library, BUILD / f'lib{name}.so'])
    dex = build_java()
    with zipfile.ZipFile(unsigned, 'a') as apk:
        apk.write(library, f'lib/arm64-v8a/lib{name}.so', compress_type=zipfile.ZIP_STORED)
        apk.write(dex, 'classes.dex', compress_type=zipfile.ZIP_DEFLATED)

    aligned = BUILD / 'pakal-aligned.apk'
    if aligned.exists():
        aligned.unlink()
    run([BUILD_TOOLS / 'zipalign.exe', '-P', '16', '-f', '4', unsigned, aligned])

    keystore = BUILD / 'debug.keystore'
    if not keystore.exists():
        run([JDK / 'bin/keytool.exe', '-genkeypair', '-keystore', keystore, '-storepass', 'android', '-keypass', 'android',
             '-alias', 'debug', '-keyalg', 'RSA', '-keysize', '2048', '-validity', '10000', '-dname', 'CN=Pakal Debug'])
    final = BUILD / f'pakal-{game}.apk'
    env = dict(os.environ, JAVA_HOME=str(JDK))
    run([BUILD_TOOLS / 'apksigner.bat', 'sign', '--ks', keystore, '--ks-pass', 'pass:android', '--key-pass', 'pass:android',
         '--ks-key-alias', 'debug', '--out', final, aligned], env=env)
    run([BUILD_TOOLS / 'apksigner.bat', 'verify', '--verbose', final], env=env)
    for step in (unsigned, aligned):   # only the signed one stays (an earlier step looks installable and is not)
        step.unlink(missing_ok=True)
    print(f'{final} ({final.stat().st_size / 1e6:.1f} MB)')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--game', choices=[*GAMES, 'all'], default='all', help='the game to pack (all: one APK each)')
    parser.add_argument('--skip-native', action='store_true', help='pack without rebuilding the libraries')
    parser.add_argument('--raw-textures', action='store_true', help='keep the PNG/TGA textures as they are (no ASTC)')
    parser.add_argument('--cheats', action='store_true', help="with the game's cheats (its tester's builds; never a release)")
    args = parser.parse_args()
    games = list(GAMES) if args.game == 'all' else [args.game]
    if not args.skip_native:
        build_native(games, args.cheats)
    for game in games:
        make_apk(game, args.raw_textures, args.cheats)


if __name__ == '__main__':
    main()
