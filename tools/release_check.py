"""Checks a game's finished Android package (an APK or an App Bundle) before it is released: the engine-wide checks of the
release checklist (docs/RELEASE_CHECKLIST.md: C-BLD-1/2/4/6/8/11, C-PRF-5), then the game's own. docs/RELEASE_CHECK.md explains every check.

    python tools/release_check.py <package.apk|package.aab> [--game <name>] [--channel store|qa] [--last-code N] [--json]

It reads only the package (and <package>.build.json beside it, the game's android.json and release_check.json). It never
opens a keystore or a secret: the certificate is checked by comparing the SHA-256 the package declares with the ones the
game's config lists. Each check is reported as PASS, FAIL, WARN, SKIP or WAIVED with its requirement ID; the exit code is
1 when any check fails.

A game adds its own checks without editing this file:
- <game folder>/release_check.json: the expected certificates, size limits, strings that must not be in the native
  library, files with player-facing text, exemptions (see docs/RELEASE_CHECK.md);
- <game folder>/tools/release_checks.py: a plugin with run(ctx), which reports more results through ctx.report.
"""
import argparse
import fnmatch
import hashlib
import importlib.util
import io
import json
import re
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

STATUSES = ('PASS', 'FAIL', 'WARN', 'SKIP', 'WAIVED')
CHANNELS = ('store', 'qa')

# Files that never go in a package (the same lists as android/build_apk.py's drop_secrets)
SECRET_NAMES = {'forge.json', '.env', 'local_paths.json'}
SECRET_SUFFIXES = {'.keystore', '.jks', '.p12', '.pfx', '.pem', '.key'}
# What a secret looks like inside a file. Only the pattern's name is ever reported, never what matched.
SECRET_PATTERNS = {
    'private key': r'-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----',
    'AWS access key': r'\bAKIA[0-9A-Z]{16}\b',
    'keystore password variable': r'PAKAL_(?:KEYSTORE|KEY)_PASS\s*[=:]\s*\S',
    'password or secret in JSON': r'"(?:password|passwd|secret|client_secret|private_key|api_secret)"\s*:\s*"[^"]{6,}"',
}
# Files read as text by the secret, dev-content and dev-word scans (the native libraries are searched as bytes)
TEXT_SUFFIXES = {'.json', '.txt', '.xml', '.lua', '.rml', '.rcss', '.csv', '.ini', '.cfg', '.properties', '.yaml', '.yml',
                 '.md', '.list', '.env'}
TEXT_LIMIT = 8 * 1024 * 1024   # larger files are not scanned as text
# Strings in the native library that only a build with the engine's MCP tools has (build_apk.py --tools)
ENGINE_NATIVE_FORBIDDEN = ['[McpServer] listening']
# The log line only a locked build has (android/main_android.cpp: PAKAL_LOCKED, pakal_env.txt not read)
LOCKED_MARKER = b'a locked build: pakal_env.txt is not read'
# The words of C-BLD-8 that must not be in player-facing text (TODO only in capitals: Spanish "todo" means "all")
DEV_WORDS = [r'(?i)\bdev\b', r'(?i)\bdebug\b', r'(?i)\btest\b', r'(?i)\bwip\b', r'(?i)\bcheats?\b', r'\bTODO\b',
             r'\bF[1-9]\b', r'(?i)\bdev panel\b']
# Google Play's limit for an App Bundle's base module (C-PRF-5)
BASE_MODULE_LIMIT_MB = 200
# Certificates that only a debug key has
DEBUG_CERT_NAMES = (b'Android Debug', b'Pakal Debug')

ANDROID_ATTRIBUTE_IDS = {0x0101000f: 'debuggable', 0x0101021b: 'versionCode', 0x0101021c: 'versionName'}


# ----------------------------------------------------------------------------------------------------------------------
# Reading a package
# ----------------------------------------------------------------------------------------------------------------------

def _axml_strings(data, offset):
    """The strings of a binary XML string pool chunk."""
    header_size = struct.unpack_from('<H', data, offset + 2)[0]
    count, _styles, flags, strings_start, _ = struct.unpack_from('<5I', data, offset + 8)
    offsets = struct.unpack_from(f'<{count}I', data, offset + header_size)
    base = offset + strings_start
    utf8 = flags & 0x100
    strings = []
    for start in offsets:
        p = base + start
        if utf8:
            for _ in range(2):   # the length in UTF-16 units, then in bytes
                n = data[p]
                if n & 0x80:
                    n = ((n & 0x7f) << 8) | data[p + 1]
                    p += 2
                else:
                    p += 1
            strings.append(data[p:p + n].decode('utf-8', 'replace'))
        else:
            n = struct.unpack_from('<H', data, p)[0]
            if n & 0x8000:
                n = ((n & 0x7fff) << 16) | struct.unpack_from('<H', data, p + 2)[0]
                p += 4
            else:
                p += 2
            strings.append(data[p:p + 2 * n].decode('utf-16-le', 'replace'))
    return strings


def parse_axml(data):
    """An APK's compiled AndroidManifest.xml (Android's binary XML) as {'tag', 'attrs', 'children'}."""
    strings, resource_ids = [], []
    root, stack = None, []
    offset = struct.unpack_from('<H', data, 2)[0]
    while offset + 8 <= len(data):
        kind, header_size, size = struct.unpack_from('<HHI', data, offset)
        if size < 8:
            break
        if kind == 0x0001:
            strings = _axml_strings(data, offset)
        elif kind == 0x0180:
            resource_ids = list(struct.unpack_from(f'<{(size - header_size) // 4}I', data, offset + header_size))
        elif kind == 0x0102:
            ext = offset + header_size
            _ns, name, attr_start, attr_size, attr_count = struct.unpack_from('<IIHHH', data, ext)
            attrs = {}
            for i in range(attr_count):
                a = ext + attr_start + i * attr_size
                _ans, aname, raw, _vsize, _res0, vtype, vdata = struct.unpack_from('<IIIHBBI', data, a)
                key = strings[aname] if aname < len(strings) else ''
                if not key and aname < len(resource_ids):
                    key = ANDROID_ATTRIBUTE_IDS.get(resource_ids[aname], '')
                if raw != 0xFFFFFFFF and raw < len(strings):
                    value = strings[raw]
                elif vtype == 0x03:
                    value = strings[vdata] if vdata < len(strings) else ''
                elif vtype == 0x12:
                    value = vdata != 0
                elif vtype == 0x10:
                    value = struct.unpack('<i', struct.pack('<I', vdata))[0]
                else:
                    value = vdata
                attrs[key] = value
            node = {'tag': strings[name] if name < len(strings) else '', 'attrs': attrs, 'children': []}
            if stack:
                stack[-1]['children'].append(node)
            else:
                root = node
            stack.append(node)
        elif kind == 0x0103 and stack:
            stack.pop()
        offset += size
    return root


def _varint(buf, i):
    value = shift = 0
    while True:
        b = buf[i]
        i += 1
        value |= (b & 0x7f) << shift
        shift += 7
        if not b & 0x80:
            return value, i


def _proto_fields(buf):
    """A protocol buffer message as {field number: [values]} (bytes for length-delimited fields, ints for varints)."""
    fields, i = {}, 0
    while i < len(buf):
        key, i = _varint(buf, i)
        number, wire = key >> 3, key & 7
        if wire == 0:
            value, i = _varint(buf, i)
        elif wire == 1:
            value, i = buf[i:i + 8], i + 8
        elif wire == 2:
            n, i = _varint(buf, i)
            value, i = buf[i:i + n], i + n
        elif wire == 5:
            value, i = buf[i:i + 4], i + 4
        else:
            raise ValueError(f'protocol buffer wire type {wire}')
        fields.setdefault(number, []).append(value)
    return fields


def parse_proto_manifest(data):
    """An App Bundle's AndroidManifest.xml (aapt2's protocol buffer XmlNode) as {'tag', 'attrs', 'children'}."""
    def element(buf):
        f = _proto_fields(buf)
        attrs = {}
        for raw in f.get(4, []):
            a = _proto_fields(raw)
            name = a.get(2, [b''])[0].decode('utf-8', 'replace')
            if not name and a.get(5):
                name = ANDROID_ATTRIBUTE_IDS.get(a[5][0], '')
            value = a.get(3, [b''])[0].decode('utf-8', 'replace')
            if not value and a.get(6):   # the compiled value: Item.prim (boolean 8, int 6, hex 7)
                prim = _proto_fields(_proto_fields(a[6][0]).get(7, [b''])[0])
                if 8 in prim:
                    value = bool(prim[8][0])
                elif 6 in prim or 7 in prim:
                    value = (prim.get(6) or prim.get(7))[0]
            attrs[name] = value
        children = [element(_proto_fields(node)[1][0]) for node in f.get(5, []) if 1 in _proto_fields(node)]
        return {'tag': f.get(3, [b''])[0].decode('utf-8', 'replace'), 'attrs': attrs, 'children': children}
    return element(_proto_fields(data)[1][0])


def parse_text_manifest(data):
    """A plain-text AndroidManifest.xml, the namespaces dropped from the attributes' names."""
    def convert(e):
        return {'tag': e.tag.split('}')[-1], 'attrs': {k.split('}')[-1]: v for k, v in e.attrib.items()},
                'children': [convert(c) for c in e]}
    return convert(ET.fromstring(data))


def parse_manifest(data):
    if data[:2] == b'\x03\x00':
        return parse_axml(data)
    if data.lstrip()[:1] == b'<':
        return parse_text_manifest(data)
    return parse_proto_manifest(data)


def _der(buf, i):
    """One DER element at i: (tag, start of its content, end of the element)."""
    tag = buf[i]
    n = buf[i + 1]
    i += 2
    if n & 0x80:
        count = n & 0x7f
        n = int.from_bytes(buf[i:i + count], 'big')
        i += count
    return tag, i, i + n


def pkcs7_certificates(data):
    """The certificates (DER) of a PKCS#7 signature (a v1 JAR signature's META-INF/*.RSA, *.EC, *.DSA)."""
    _, start, _ = _der(data, 0)                       # ContentInfo
    _, oid_start, oid_end = _der(data, start)         # contentType
    _, content, _ = _der(data, oid_end)               # [0] content
    _, signed, signed_end = _der(data, content)       # SignedData
    certs, i = [], signed
    while i < signed_end:
        tag, inner, end = _der(data, i)
        if tag == 0xA0:                               # [0] certificates
            j = inner
            while j < end:
                _, _, cert_end = _der(data, j)
                certs.append(data[j:cert_end])
                j = cert_end
            break
        i = end
    return certs


def _lp(buf, i):
    """A uint32-length-prefixed value at i: (value, next index)."""
    n = struct.unpack_from('<I', buf, i)[0]
    return buf[i + 4:i + 4 + n], i + 4 + n


def apk_signing_block_certificates(data):
    """The certificates (DER) of an APK's v2/v3 signature (the APK Signing Block before the central directory)."""
    eocd = data.rfind(b'PK\x05\x06', max(0, len(data) - 65557))
    if eocd < 0:
        return []
    cd_offset = struct.unpack_from('<I', data, eocd + 16)[0]
    if cd_offset < 24 or data[cd_offset - 16:cd_offset] != b'APK Sig Block 42':
        return []
    size = struct.unpack_from('<Q', data, cd_offset - 24)[0]
    i, end = cd_offset - size - 8 + 8, cd_offset - 24
    certs = []
    while i + 12 <= end:
        length, block_id = struct.unpack_from('<QI', data, i)
        value = data[i + 12:i + 8 + length]
        i += 8 + length
        if block_id not in (0x7109871a, 0xf05368c0, 0x1b93ad61):   # v2, v3, v3.1
            continue
        signers, _ = _lp(value, 0)
        j = 0
        while j < len(signers):
            signer, j = _lp(signers, j)
            signed_data, _ = _lp(signer, 0)
            _digests, k = _lp(signed_data, 0)
            cert_list, _ = _lp(signed_data, k)
            m = 0
            while m < len(cert_list):
                cert, m = _lp(cert_list, m)
                certs.append(cert)
        if certs:
            break   # (v3 repeats v2's signer)
    return certs


class Package:
    """An APK or an App Bundle opened for reading: its entries, assets, manifest, native libraries and certificates."""

    def __init__(self, path):
        self.path = Path(path)
        self.kind = 'aab' if self.path.suffix.lower() == '.aab' else 'apk'
        self.zip = zipfile.ZipFile(self.path)
        self.infos = {i.filename.replace('\\', '/'): i for i in self.zip.infolist() if not i.is_dir()}
        self.names = sorted(self.infos)
        # asset path (as the game asks for it: Assets/..., pakal_env.txt) -> entry: an APK's assets/, a bundle's
        # <module>/assets/ (the base module and the asset packs)
        self.assets = {}
        for name in self.names:
            parts = name.split('/')
            if self.kind == 'apk' and parts[0] == 'assets':
                self.assets['/'.join(parts[1:])] = name
            elif self.kind == 'aab' and len(parts) > 2 and parts[1] == 'assets':
                self.assets['/'.join(parts[2:])] = name
        self._manifest = None

    def read(self, name):
        return self.zip.read(self.infos[name])

    def read_asset(self, path):
        name = self.assets.get(path)
        return self.read(name) if name else None

    @property
    def manifest(self):
        if self._manifest is None:
            name = 'AndroidManifest.xml' if self.kind == 'apk' else 'base/manifest/AndroidManifest.xml'
            self._manifest = parse_manifest(self.read(name)) if name in self.infos else {}
        return self._manifest

    def application(self):
        return next((c for c in self.manifest.get('children', []) if c['tag'] == 'application'), {'attrs': {}})

    def native_libraries(self):
        prefix = 'lib/' if self.kind == 'apk' else 'base/lib/'
        return [n for n in self.names if n.startswith(prefix) and n.endswith('.so')]

    def certificates(self):
        """The signing certificates (DER): an APK's v2/v3 block, else (and for a bundle) the v1 JAR signature."""
        certs = apk_signing_block_certificates(self.path.read_bytes()) if self.kind == 'apk' else []
        if not certs:
            for name in self.names:
                if re.fullmatch(r'META-INF/[^/]+\.(RSA|EC|DSA)', name, re.I):
                    try:
                        certs += pkcs7_certificates(self.read(name))
                    except (IndexError, ValueError, struct.error):
                        pass
        return certs

    def modules(self):
        """A bundle's modules and their compressed size: {'base': bytes, 'gamedata': bytes...}."""
        sizes = {}
        for name, info in self.infos.items():
            top = name.split('/')[0]
            if '/' in name and top not in ('META-INF', 'BUNDLE-METADATA'):
                sizes[top] = sizes.get(top, 0) + info.compress_size
        return sizes

    def text_entries(self):
        """(entry, asset path or entry) of the files scanned as text."""
        for name in self.names:
            info = self.infos[name]
            if Path(name).suffix.lower() in TEXT_SUFFIXES and info.file_size <= TEXT_LIMIT and not name.startswith('META-INF/'):
                yield name


# ----------------------------------------------------------------------------------------------------------------------
# The context the checks share
# ----------------------------------------------------------------------------------------------------------------------

def find_game_dir(name):
    for folder in (ROOT / 'games' / name, ROOT / 'examples' / name):
        if folder.is_dir():
            return folder
    return None


def load_json(path):
    return json.loads(Path(path).read_text(encoding='utf-8')) if Path(path).is_file() else {}


def guess_game(package, path):
    """The game a package is of: the android.json whose package (or a flavor's) is the manifest's (a side copy's .dev
    dropped), else the file's name (pakal-<game>[-<flavor>])."""
    wanted = ''
    if package:
        wanted = str(package.manifest.get('attrs', {}).get('package', ''))
        wanted = wanted[:-4] if wanted.endswith('.dev') else wanted
    for recipe in sorted(list((ROOT / 'games').glob('*/android.json')) + list((ROOT / 'examples').glob('*/android.json'))):
        info = load_json(recipe)
        packages = [info.get('package')] + [f.get('package') for f in info.get('flavors', {}).values()]
        if wanted and wanted in packages:
            return recipe.parent.name
    if wanted.startswith('org.pakalengine.') and find_game_dir(wanted.rsplit('.', 1)[1]):
        return wanted.rsplit('.', 1)[1]
    match = re.match(r'pakal-([a-z0-9_]+)', Path(path).name)
    if match and find_game_dir(match.group(1)):
        return match.group(1)
    return None


def version_code(version):
    """The version code android/build_apk.py gives a version (its version_code), or None when it is not one."""
    match = re.fullmatch(r'(\d+)\.(\d+)\.(\d+)(?:-(alpha|beta|rc)\.(\d+))?', str(version))
    if not match:
        return None
    major, minor, patch, stage, number = match.groups()
    step = {'alpha': 0, 'beta': 20, 'rc': 60, None: 99}[stage] + (int(number) if number else 0)
    return int(major) * 1000000 + int(minor) * 10000 + int(patch) * 100 + step


class Context:
    """What every check sees: the package (None when it could not be opened), the game, its config and android.json, the
    channel, and report()."""

    def __init__(self, package_path, package, game, channel, args):
        self.root = ROOT
        self.package_path = Path(package_path)
        self.package = package
        self.game = game
        self.game_dir = find_game_dir(game) if game else None
        self.config = load_json(self.game_dir / 'release_check.json') if self.game_dir else {}
        self.android = load_json(self.game_dir / 'android.json') if self.game_dir else {}
        self.channel = channel
        self.args = args
        self.results = []
        self.flavor = None
        if package and self.android:
            name = str(package.manifest.get('attrs', {}).get('package', ''))
            self.flavor = next((k for k, f in self.android.get('flavors', {}).items() if f.get('package') == name), None)

    @property
    def store(self):
        return self.channel == 'store'

    def strict(self):
        """FAIL in a store package; WARN in a tester's (qa) one, where cheats, tools, a debug key and loose art are allowed."""
        return 'FAIL' if self.store else 'WARN'

    def report(self, requirement, check, status, message):
        """One result. A check the game's config exempts ("exempt": {"<check>": "<reason, who, when>"}) is WAIVED
        instead of FAIL or WARN."""
        if status not in STATUSES:
            raise ValueError(f'unknown status {status}')
        reason = self.config.get('exempt', {}).get(check)
        if reason and status in ('FAIL', 'WARN'):
            status, message = 'WAIVED', f'{message} (exempt: {reason})'
        self.results.append({'id': requirement, 'check': check, 'status': status, 'message': message})


# ----------------------------------------------------------------------------------------------------------------------
# The engine-wide checks of a package
# ----------------------------------------------------------------------------------------------------------------------

def check_debuggable(ctx):
    """C-BLD-2: the application is not debuggable."""
    value = ctx.package.application()['attrs'].get('debuggable', False)
    on = value is True or str(value).lower() == 'true'
    ctx.report('C-BLD-2', 'debuggable', 'FAIL' if on else 'PASS', 'android:debuggable is ' + ('true' if on else 'not set'))


def check_certificate(ctx):
    """C-BLD-4: signed by an expected certificate (release_check.json "certificates", the flavor's "signers")."""
    certs = ctx.package.certificates()
    if not certs:
        ctx.report('C-BLD-4', 'certificate', 'FAIL', 'the package is not signed (no v2/v3 block, no META-INF signature)')
        return
    found = [hashlib.sha256(c).hexdigest() for c in certs]
    expected = [d.lower().replace(':', '') for d in ctx.config.get('certificates', [])]
    if ctx.flavor:
        expected += [d.lower().replace(':', '') for d in ctx.android['flavors'][ctx.flavor].get('signers', [])]
    debug = any(name in c for c in certs for name in DEBUG_CERT_NAMES)
    shown = ', '.join(f[:16] + '...' for f in found)
    if not ctx.store:
        ctx.report('C-BLD-4', 'certificate', 'SKIP', f'qa package: any key may sign it (signed by {shown}{", a debug key" if debug else ""})')
    elif expected:
        unknown = [f for f in found if f not in expected]
        ctx.report('C-BLD-4', 'certificate', 'FAIL' if unknown else 'PASS',
                   f'signed by {shown}: ' + ('not an expected certificate' if unknown else 'an expected certificate'))
    elif debug:
        ctx.report('C-BLD-4', 'certificate', 'FAIL', f'signed by a debug key ({shown})')
    else:
        ctx.report('C-BLD-4', 'certificate', 'WARN', f'signed by {shown}, and release_check.json lists no "certificates" to compare with')


def check_version(ctx):
    """C-BLD-1 / C-BLD-4: the manifest's version code and name are the build's (Assets/build.json, <package>.build.json,
    the game's version file), the build is not dirty, and the code is above the last released one."""
    attrs = ctx.package.manifest.get('attrs', {})
    try:
        code = int(attrs.get('versionCode'))
    except (TypeError, ValueError):
        ctx.report('C-BLD-1', 'version', 'FAIL', f'the manifest has no version code ({attrs.get("versionCode")!r})')
        return
    name = str(attrs.get('versionName', ''))
    raw = ctx.package.read_asset('Assets/build.json')
    if raw is None:
        ctx.report('C-BLD-1', 'version', 'FAIL', 'Assets/build.json is not in the package')
        return
    try:
        build = json.loads(raw)
    except ValueError:
        ctx.report('C-BLD-1', 'version', 'FAIL', 'Assets/build.json is not JSON')
        return
    problems = []
    if build.get('code') != code:
        problems.append(f'build.json code {build.get("code")} != manifest {code}')
    if str(build.get('version')) != name:
        problems.append(f'build.json version {build.get("version")!r} != manifest {name!r}')
    version_file = ctx.android.get('version_file')
    if version_file and ctx.game_dir and (ctx.game_dir / version_file).is_file():
        declared = load_json(ctx.game_dir / version_file).get('version')
        if declared != name or version_code(declared) != code:
            problems.append(f'the game\'s {version_file} says {declared} (code {version_code(declared)})')
    ctx.report('C-BLD-1', 'version', 'FAIL' if problems else 'PASS',
               '; '.join(problems) if problems else f'version {name}, code {code}, build {build.get("build")}.{build.get("commit")}')

    beside = ctx.package_path.with_suffix('.build.json')
    if beside.is_file():
        same = load_json(beside) == build
        ctx.report('C-BLD-1', 'build-identity', 'PASS' if same else 'FAIL',
                   f'{beside.name} ' + ('is the same as Assets/build.json' if same else 'differs from Assets/build.json'))
    else:
        ctx.report('C-BLD-1', 'build-identity', 'WARN', f'{beside.name} not found beside the package')

    if build.get('dirty'):
        ctx.report('C-BLD-1', 'dirty', ctx.strict(), f'built from uncommitted changes ({build.get("commit")}.dirty)')
    else:
        ctx.report('C-BLD-1', 'dirty', 'PASS', f'built from commit {build.get("commit")}')

    channel = build.get('channel')
    if channel is not None:
        expected = 'store' if ctx.store else 'qa'
        ctx.report('C-ID-7', 'channel', 'PASS' if channel == expected else ctx.strict(), f'build.json channel "{channel}"'
                   + ('' if channel == expected else f', checked as {ctx.channel}'))

    last = ctx.args.last_code if ctx.args.last_code is not None else ctx.config.get('last_version_code')
    if not ctx.store:
        ctx.report('C-BLD-4', 'version-code', 'SKIP', 'qa package: the store\'s last version code is not compared')
    elif last is None:
        ctx.report('C-BLD-4', 'version-code', 'WARN', f'code {code}: the last released code is not known (--last-code, or "last_version_code")')
    else:
        ctx.report('C-BLD-4', 'version-code', 'PASS' if code > int(last) else 'FAIL', f'code {code}, last released {last}')


def package_env(ctx):
    raw = ctx.package.read_asset('pakal_env.txt')
    env = {}
    for line in (raw or b'').decode('utf-8', 'replace').splitlines():
        if '=' in line and not line.lstrip().startswith('#'):
            key, value = line.split('=', 1)
            env[key.strip()] = value.strip()
    return env


def check_release_env(ctx):
    """C-BLD-2: a store package carries PAKAL_RELEASE=1 (a players' copy) and is locked; a qa one never says it is a
    players' copy (no players' telemetry from testers)."""
    env = package_env(ctx)
    release = env.get('PAKAL_RELEASE') == '1'
    if ctx.store:
        ctx.report('C-BLD-2', 'release-env', 'PASS' if release else 'FAIL',
                   'pakal_env.txt has PAKAL_RELEASE=1' if release else 'pakal_env.txt has no PAKAL_RELEASE=1 (build it with --release)')
    else:
        ctx.report('C-BLD-2', 'release-env', 'FAIL' if release else 'PASS',
                   'qa package with PAKAL_RELEASE=1: its telemetry would count as a player\'s' if release else 'qa package: no PAKAL_RELEASE')
    libraries = ctx.package.native_libraries()
    if not ctx.store:
        ctx.report('C-BLD-2', 'locked', 'SKIP', 'qa package: it may read the device\'s pakal_env.txt')
    elif not libraries:
        ctx.report('C-BLD-2', 'locked', 'FAIL', 'no native library in the package')
    else:
        locked = any(LOCKED_MARKER in ctx.package.read(lib) for lib in libraries)
        state = 'PASS' if locked else ('FAIL' if ctx.config.get('require_locked') else 'WARN')
        ctx.report('C-BLD-2', 'locked', state, 'built with PAKAL_LOCKED (pakal_env.txt is not read)' if locked
                   else 'not built with PAKAL_LOCKED: the device\'s pakal_env.txt can turn on debug switches')


def check_pack(ctx):
    """C-BLD-11: the game's art is sealed in its keyed pack (android.json "pack"), kept uncompressed, and none of the
    pack's folders is loose in the package. The pack is not opened with its key: its entries must be sealed (PKP1)."""
    pack = ctx.android.get('pack')
    if pack is False:
        reason = ctx.android.get('pack_reason')
        ctx.report('C-BLD-11', 'pack', 'WAIVED' if reason else 'WARN',
                   f'android.json "pack": false ({reason})' if reason else 'android.json "pack": false with no "pack_reason"')
        return
    if not pack:
        ctx.report('C-BLD-11', 'pack', ctx.strict(), 'the game declares no sealed pack (android.json "pack"): its art is loose in the package')
        return
    file, folders = pack.get('file', ''), pack.get('folders', [])
    entry = ctx.package.assets.get(file)
    if not entry:
        ctx.report('C-BLD-11', 'pack', 'FAIL', f'{file} is not in the package')
        return
    problems = []
    if ctx.package.kind == 'apk' and ctx.package.infos[entry].compress_type != zipfile.ZIP_STORED:
        problems.append('compressed in the APK (the game maps it in place: it must be stored)')
    if ctx.package.kind == 'aab' and 'BundleConfig.pb' in ctx.package.infos and b'.pak' not in ctx.package.read('BundleConfig.pb'):
        problems.append('the bundle\'s config does not keep .pak files uncompressed')
    loose = sorted({a.split('/')[1] for a in ctx.package.assets for f in folders if a.lower().startswith(f'assets/{f.lower()}/')})
    if loose:
        problems.append('loose in the package: ' + ', '.join(f'Assets/{f}/' for f in loose))
    try:
        with ctx.package.zip.open(ctx.package.infos[entry]) as handle, zipfile.ZipFile(handle) as inner:   # (read in place)
            names = [n for n in inner.namelist() if not n.endswith('/')]
            unsealed = [n for n in names[:50] if inner.open(n).read(4) != b'PKP1']
        missing = [f for f in folders if not any(n.startswith(f'assets/{f.lower()}/') for n in names)]
        if not names:
            problems.append('the pack is empty')
        if unsealed:
            problems.append(f'{len(unsealed)} of its first {min(50, len(names))} files are not sealed (e.g. {unsealed[0]})')
        if missing:
            problems.append('folders not in the pack: ' + ', '.join(missing))
    except zipfile.BadZipFile:
        problems.append('the pack is not a zip')
    ctx.report('C-BLD-11', 'pack', 'FAIL' if problems else 'PASS',
               f'{file}: ' + ('; '.join(problems) if problems else f'sealed, {len(names)} files, folders {", ".join(folders)}'))


def check_secrets(ctx):
    """C-BLD-6: no secret file (forge.json, keystores, keys...) and nothing that looks like a key or a password in the
    package's text files or a private key in its native libraries. Only names are reported, never what matched."""
    names = SECRET_NAMES | {n.lower() for n in ctx.config.get('secret_names', [])}
    files = [n for n in ctx.package.names if Path(n).name.lower() in names or Path(n).suffix.lower() in SECRET_SUFFIXES]
    patterns = {**SECRET_PATTERNS, **ctx.config.get('secret_patterns', {})}
    allowed = ctx.config.get('secret_allow', [])
    compiled = {k: re.compile(v.encode('utf-8')) for k, v in patterns.items()}
    found = []
    for name in list(ctx.package.text_entries()) + ctx.package.native_libraries():
        if any(fnmatch.fnmatch(name, glob) for glob in allowed):
            continue
        data = ctx.package.read(name)
        for label, pattern in compiled.items():
            if name.endswith('.so') and label != 'private key':
                continue
            if pattern.search(data):
                found.append(f'{name} ({label})')
    problems = [f'secret file {f}' for f in files] + [f'looks like a secret: {f}' for f in found]
    ctx.report('C-BLD-6', 'secrets', 'FAIL' if problems else 'PASS', '; '.join(problems[:10]) if problems else
               f'no secret files, nothing secret-looking in {len(list(ctx.package.text_entries()))} text files')


def check_cheat_strings(ctx):
    """C-BLD-2: no cheat or tool in the build: none of the strings only those builds have in the native library (the
    engine's MCP server, release_check.json "native_forbidden"), and no forbidden file ("forbidden_files", cheats.rml)."""
    strings = ENGINE_NATIVE_FORBIDDEN + ctx.config.get('native_forbidden', [])
    hits = []
    for lib in ctx.package.native_libraries():
        data = ctx.package.read(lib)
        hits += [f'"{s}" in {Path(lib).name}' for s in strings if s.encode('utf-8') in data]
    globs = ['*/cheats.rml', 'cheats.rml'] + ctx.config.get('forbidden_files', [])
    files = [a for a in ctx.package.assets if any(fnmatch.fnmatch(a, g) for g in globs)]
    problems = hits + [f'file {f}' for f in files]
    ctx.report('C-BLD-2', 'cheat-strings', ctx.strict() if problems else 'PASS',
               ('; '.join(problems[:10]) + (' (qa: allowed)' if not ctx.store else '')) if problems
               else f'none of {len(strings)} strings in {len(ctx.package.native_libraries())} native libraries, no cheat files')


def _dev_flagged(value, path=''):
    if isinstance(value, dict):
        if value.get('dev') is True:
            yield path or '/'
        for k, v in value.items():
            yield from _dev_flagged(v, f'{path}/{k}')
    elif isinstance(value, list):
        for i, v in enumerate(value):
            yield from _dev_flagged(v, f'{path}/{i}')


def check_dev_content(ctx):
    """C-BLD-8: no data flagged "dev": true in the package's JSON files (test levels, dev missions, debug menus)."""
    flagged = []
    for name in ctx.package.text_entries():
        if name.endswith('.json'):
            try:
                document = json.loads(ctx.package.read(name))
            except ValueError:
                continue
            flagged += [f'{name}#{p}' for p in _dev_flagged(document)]
    ctx.report('C-BLD-8', 'dev-content', ctx.strict() if flagged else 'PASS',
               ('flagged "dev": true: ' + ', '.join(flagged[:10]) + (f' and {len(flagged) - 10} more' if len(flagged) > 10 else ''))
               if flagged else 'no data flagged "dev": true')


def _player_lines(name, text):
    """(line number, text) of a file's player-facing strings: a CSV's language columns (a header like key,notes,en,es:
    the columns named by a language code), or when its header names none every column but the first (the keys); a
    JSON's string values (not its keys); any other file whole."""
    if name.lower().endswith('.csv'):
        import csv
        rows = list(csv.reader(io.StringIO(text)))
        header = [h.strip() for h in rows[0]] if rows else []
        languages = [i for i, h in enumerate(header) if re.fullmatch(r'[a-z]{2,3}(?:[-_][A-Za-z]{2,4})?', h)
                     and h not in ('key', 'id')]
        for number, row in enumerate(rows[1:] if languages else rows, 2 if languages else 1):
            yield number, ' | '.join(row[i] for i in languages if i < len(row)) if languages else ' | '.join(row[1:])
    elif name.lower().endswith('.json'):
        try:
            document = json.loads(text)
        except ValueError:
            return
        stack = [document]
        while stack:
            value = stack.pop()
            if isinstance(value, dict):
                stack.extend(value.values())
            elif isinstance(value, list):
                stack.extend(value)
            elif isinstance(value, str):
                yield 0, value
    else:
        yield from enumerate(text.splitlines(), 1)


def check_dev_words(ctx):
    """C-BLD-8: no dev word (dev, debug, test, wip, cheat, TODO, F1..F9, dev panel) in player-facing text: the package's
    files and the game's sources that release_check.json "player_text" names ({"package": [globs], "source": [globs]})."""
    spec = ctx.config.get('player_text') or {}
    words = [re.compile(w) for w in ctx.config.get('dev_words', DEV_WORDS)]
    files = []
    for glob in spec.get('package', []):
        files += [(f'package:{a}', ctx.package.read_asset(a)) for a in ctx.package.assets if fnmatch.fnmatch(a, glob)]
    for glob in spec.get('source', []):
        if ctx.game_dir:
            files += [(f.relative_to(ctx.root).as_posix(), f.read_bytes()) for f in sorted(ctx.game_dir.glob(glob)) if f.is_file()]
    if not files:
        ctx.report('C-BLD-8', 'dev-words', 'SKIP', 'release_check.json names no player-facing text ("player_text")')
        return
    hits = []
    for name, data in files:
        for number, line in _player_lines(name, data.decode('utf-8-sig', 'replace')):
            found = list(dict.fromkeys(m.group(0) for word in words for m in [word.search(line)] if m))
            if found:
                hits.append((f'{name}:{number}' if number else name) + ' ' + ', '.join(f'"{w}"' for w in found))
    ctx.report('C-BLD-8', 'dev-words', ctx.strict() if hits else 'PASS',
               ('dev words in player-facing text: ' + ', '.join(hits[:10]) + (f' and {len(hits) - 10} more' if len(hits) > 10 else ''))
               if hits else f'no dev words in {len(files)} files of player-facing text')


def check_size(ctx):
    """C-PRF-5: the package within the game's limit (release_check.json "max_size_mb"); a bundle's base module under the
    store's 200 MB and each asset pack within "asset_pack_max_mb"."""
    size = ctx.package_path.stat().st_size / 1e6
    limit = ctx.config.get('max_size_mb')
    if limit:
        ctx.report('C-PRF-5', 'size', 'PASS' if size <= limit else 'FAIL', f'{size:.1f} MB (limit {limit} MB)')
    else:
        ctx.report('C-PRF-5', 'size', 'PASS', f'{size:.1f} MB (release_check.json sets no "max_size_mb")')
    if ctx.package.kind == 'aab':
        limits = ctx.config.get('asset_pack_max_mb', {})
        for module, bytes_ in sorted(ctx.package.modules().items()):
            mb = bytes_ / 1e6
            cap = ctx.config.get('base_max_mb', BASE_MODULE_LIMIT_MB) if module == 'base' else limits.get(module)
            if cap:
                ctx.report('C-PRF-5', f'size-{module}', 'PASS' if mb <= cap else 'FAIL', f'module {module}: {mb:.1f} MB compressed (limit {cap} MB)')
            else:
                ctx.report('C-PRF-5', f'size-{module}', 'PASS', f'module {module}: {mb:.1f} MB compressed (no limit set)')


PACKAGE_CHECKS = [check_debuggable, check_certificate, check_version, check_release_env, check_pack, check_secrets,
                  check_cheat_strings, check_dev_content, check_dev_words, check_size]

# Checks of the game's sources rather than its package, run after the package's: fn(ctx), reporting through ctx.report.
# The C-DAT scan (docs/RELEASE_CHECKLIST.md: element creation, inline styles, colors, player-facing literals in game
# code) plugs in here, with register_source_check, so that one command still checks a release.
SOURCE_CHECKS = []


def register_source_check(check):
    SOURCE_CHECKS.append(check)
    return check


# ----------------------------------------------------------------------------------------------------------------------
# Running
# ----------------------------------------------------------------------------------------------------------------------

def load_plugin(ctx):
    """The game's own checks: <game folder>/tools/release_checks.py (or release_check.json "plugin"), with run(ctx)."""
    if not ctx.game_dir:
        return None
    path = ctx.game_dir / ctx.config.get('plugin', 'tools/release_checks.py')
    if not path.is_file():
        return None
    spec = importlib.util.spec_from_file_location(f'{ctx.game}_release_checks', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def run_checks(package_path, game=None, channel=None, args=None, default_channel='store'):
    """Every check of a package; returns the Context with its results."""
    args = args or argparse.Namespace(last_code=None)
    package, error = None, None
    try:
        package = Package(package_path)
        package.manifest   # (read now: a broken manifest is a failure of its own)
    except FileNotFoundError:
        error = f'{package_path} not found'
    except (zipfile.BadZipFile, KeyError, ValueError, IndexError, struct.error) as e:
        error = f'{package_path} cannot be read ({e})'
        package = None
    game = game or guess_game(package, package_path)
    if not channel and package:
        build = json.loads(package.read_asset('Assets/build.json') or b'{}') if package.read_asset('Assets/build.json') else {}
        channel = {'store': 'store', 'qa': 'qa', 'dev': 'qa'}.get(build.get('channel'))
    ctx = Context(package_path, package, game, channel or default_channel, args)
    if error:
        ctx.report('C-BLD-4', 'package', 'FAIL', error)
    else:
        for check in PACKAGE_CHECKS:
            try:
                check(ctx)
            except Exception as e:   # one broken check must not hide the others
                ctx.report('C-BLD-4', check.__name__.replace('check_', '').replace('_', '-'), 'FAIL', f'the check failed: {e!r}')
    for check in SOURCE_CHECKS:
        check(ctx)
    plugin = load_plugin(ctx)
    if plugin and hasattr(plugin, 'run'):
        plugin.run(ctx)
    return ctx


def summary(ctx):
    counts = {s: sum(1 for r in ctx.results if r['status'] == s) for s in STATUSES}
    return {'package': str(ctx.package_path), 'game': ctx.game, 'flavor': ctx.flavor, 'channel': ctx.channel,
            'results': ctx.results, 'counts': counts, 'ok': counts['FAIL'] == 0}


def main(argv=None, default_channel='store'):
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('package', help='the .apk or .aab to check')
    parser.add_argument('--game', help='the game (games/<name> or examples/<name>); guessed from the package when left out')
    parser.add_argument('--channel', choices=CHANNELS,
                        help='store: the store package (the default; build.json\'s channel when it has one); qa: a tester\'s copy')
    parser.add_argument('--last-code', type=int, help='the last version code released to the store (the code must be above it)')
    parser.add_argument('--json', action='store_true', help='print the results as JSON')
    args = parser.parse_args(argv)
    for stream in (sys.stdout, sys.stderr):   # (a console that cannot show a character gets '?', not an exception)
        if hasattr(stream, 'reconfigure'):
            stream.reconfigure(errors='replace')
    if args.game and not find_game_dir(args.game):
        parser.error(f'no game "{args.game}" (games/{args.game} or examples/{args.game})')
    ctx = run_checks(args.package, args.game, args.channel, args, default_channel)
    result = summary(ctx)
    if args.json:
        print(json.dumps(result, indent=1))
    else:
        print(f'{ctx.package_path.name}: game {ctx.game or "unknown"}' + (f' ({ctx.flavor})' if ctx.flavor else '')
              + f', checked as a {ctx.channel} package')
        for r in ctx.results:
            print(f'{r["status"]:6} {r["id"]:9} {r["check"]:16} {r["message"]}')
        c = result['counts']
        print(f'\n{len(ctx.results)} checks: {c["FAIL"]} failed, {c["WARN"]} warnings, {c["WAIVED"]} waived, {c["SKIP"]} skipped')
    return 0 if result['ok'] else 1


if __name__ == '__main__':
    sys.exit(main())
