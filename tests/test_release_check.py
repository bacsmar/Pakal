"""Tests of tools/release_check.py on tiny fake packages (python -m unittest tests/test_release_check.py)."""
import contextlib
import hashlib
import importlib.util
import io
import json
import shutil
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'tools' / 'release_check.py'
spec = importlib.util.spec_from_file_location('release_check', path)
rc = importlib.util.module_from_spec(spec)
spec.loader.exec_module(rc)

CERT = b'\x30\x11fake release cert'   # (DER SEQUENCEs: the tool does not read a certificate's fields)
DEBUG_CERT = b'\x30\x12CN=Pakal Debug cert'
PACKAGE = 'com.example.demo'


# ----------------------------------------------------------------------------------------------------------------------
# Fake packages: a binary manifest (APK) or a protocol buffer one (bundle), a v2 signing block or a v1 PKCS#7 signature
# ----------------------------------------------------------------------------------------------------------------------

def axml(debuggable=False, code=10021, name='0.1.0-beta.1'):
    """A binary AndroidManifest.xml: <manifest package versionCode versionName><application [debuggable]/></manifest>."""
    strings = []

    def index(text):
        if text not in strings:
            strings.append(text)
        return strings.index(text)

    def start(tag, attrs):
        body = b''
        for key, value in attrs:
            if isinstance(value, bool):
                raw, kind, data = 0xFFFFFFFF, 0x12, 0xFFFFFFFF if value else 0
            elif isinstance(value, int):
                raw, kind, data = 0xFFFFFFFF, 0x10, value
            else:
                raw = index(value)
                kind, data = 0x03, raw
            body += struct.pack('<IIIHBBI', 0xFFFFFFFF, index(key), raw, 8, 0, kind, data)
        ext = struct.pack('<IIHHHHHH', 0xFFFFFFFF, index(tag), 20, 20, len(attrs), 0, 0, 0) + body
        return struct.pack('<HHIII', 0x0102, 16, 16 + len(ext), 1, 0xFFFFFFFF) + ext

    def end(tag):
        return struct.pack('<HHIIIII', 0x0103, 16, 24, 1, 0xFFFFFFFF, 0xFFFFFFFF, index(tag))

    application = [('debuggable', True)] if debuggable else [('label', 'Demo')]
    nodes = (start('manifest', [('package', PACKAGE), ('versionCode', code), ('versionName', name)])
             + start('application', application) + end('application') + end('manifest'))
    data = b''.join(struct.pack('<H', len(s)) + s.encode('utf-16-le') + b'\0\0' for s in strings)
    data += b'\0' * (-len(data) % 4)
    header = 28 + 4 * len(strings)
    offsets, at = [], 0
    for s in strings:
        offsets.append(at)
        at += 2 + 2 * len(s) + 2
    pool = struct.pack('<HHIIIIII', 0x0001, 28, header + len(data), len(strings), 0, 0, header, 0)
    pool += struct.pack(f'<{len(strings)}I', *offsets) + data
    return struct.pack('<HHI', 0x0003, 8, 8 + len(pool) + len(nodes)) + pool + nodes


def _varint(n):
    out = b''
    while True:
        b = n & 0x7f
        n >>= 7
        out += bytes([b | (0x80 if n else 0)])
        if not n:
            return out


def _field(number, data):
    return _varint(number << 3 | 2) + _varint(len(data)) + data


def proto_manifest(debuggable=False, code=10021, name='0.1.0-beta.1'):
    """A bundle's AndroidManifest.xml as aapt2's protocol buffer XmlNode."""
    def attr(key, value):
        return _field(4, _field(2, key.encode()) + _field(3, str(value).encode()))
    application = _field(3, b'application') + (attr('debuggable', 'true') if debuggable else b'')
    manifest = (_field(3, b'manifest') + attr('package', PACKAGE) + attr('versionCode', code) + attr('versionName', name)
                + _field(5, _field(1, application)))
    return _field(1, manifest)


def _lp(data):
    return struct.pack('<I', len(data)) + data


def with_signing_block(data, cert):
    """A zip with an APK Signing Block (v2) holding 'cert', put before its central directory."""
    eocd = data.rfind(b'PK\x05\x06')
    cd = struct.unpack_from('<I', data, eocd + 16)[0]
    signed_data = _lp(b'') + _lp(_lp(cert)) + _lp(b'')
    value = _lp(_lp(_lp(signed_data) + _lp(b'') + _lp(b'')))
    pairs = struct.pack('<QI', 4 + len(value), 0x7109871a) + value
    size = len(pairs) + 8 + 16
    block = struct.pack('<Q', size) + pairs + struct.pack('<Q', size) + b'APK Sig Block 42'
    out = bytearray(data[:cd] + block + data[cd:])
    struct.pack_into('<I', out, eocd + len(block) + 16, cd + len(block))
    return bytes(out)


def _tlv(tag, content):
    n = len(content)
    length = bytes([n]) if n < 128 else bytes([0x80 | ((n.bit_length() + 7) // 8)]) + n.to_bytes((n.bit_length() + 7) // 8, 'big')
    return bytes([tag]) + length + content


def pkcs7(cert):
    signed = _tlv(0x30, _tlv(0x02, b'\x01') + _tlv(0x31, b'') + _tlv(0x30, _tlv(0x06, b'\x2a\x86')) + _tlv(0xA0, cert))
    return _tlv(0x30, _tlv(0x06, b'\x2a\x86\x48') + _tlv(0xA0, signed))


def sealed_pack(folders=('art',), sealed=True):
    """A pack as tools/pakal_pack.py writes it: entries 'assets/<folder>/...' that start with PKP1 when sealed."""
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_STORED) as z:
        for folder in folders:
            z.writestr(f'assets/{folder}/hero.png', (b'PKP1' if sealed else b'\x89PNG') + b'\0' * 12)
    return buffer.getvalue()


BUILD = {'build': 7, 'commit': 'abc1234', 'branch': 'main', 'dirty': False, 'version': '0.1.0-beta.1', 'code': 10021}


def make_package(folder, kind='apk', debuggable=False, cert=CERT, build=None, env='PAKAL_RELEASE=1\n', locked=True,
                 pack='ok', extra=None, native=b'', beside=True, name=None):
    """Writes a fake package (and its <package>.build.json) into 'folder'; returns its path."""
    build = dict(BUILD, **(build or {}))
    a = 'assets/' if kind == 'apk' else 'base/assets/'
    files = {a + 'Assets/build.json': json.dumps(build).encode(), a + 'Assets/ui/menu.rml': b'<rml>Play</rml>'}
    if env is not None:
        files[a + 'pakal_env.txt'] = env.encode()
    library = b'\x7fELF' + native + (rc.LOCKED_MARKER if locked else b'')
    files[('lib/' if kind == 'apk' else 'base/lib/') + 'arm64-v8a/libdemo.so'] = library
    if kind == 'apk':
        files['AndroidManifest.xml'] = axml(debuggable, build['code'], build['version'])
    else:
        files['base/manifest/AndroidManifest.xml'] = proto_manifest(debuggable, build['code'], build['version'])
        files['BundleConfig.pb'] = b'\x0a\x10assets/**/*.pak'
        files['META-INF/UPLOAD.RSA'] = pkcs7(cert)
    files.update(extra or {})
    path = Path(folder) / (name or f'pakal-demo.{kind}')
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w', zipfile.ZIP_DEFLATED) as z:
        for entry, data in files.items():
            z.writestr(entry, data)
        if pack:
            z.writestr(a + 'demo.pak', sealed_pack(sealed=pack != 'unsealed'),
                       compress_type=zipfile.ZIP_DEFLATED if pack == 'compressed' else zipfile.ZIP_STORED)
    data = buffer.getvalue()
    path.write_bytes(with_signing_block(data, cert) if kind == 'apk' else data)
    if beside:
        path.with_suffix('.build.json').write_text(json.dumps(build))
    return path


class ReleaseCheckTests(unittest.TestCase):
    def setUp(self):
        self.root = Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.root, True)
        self.saved_root = rc.ROOT
        rc.ROOT = self.root
        self.addCleanup(setattr, rc, 'ROOT', self.saved_root)
        self.game = self.root / 'games' / 'demo'
        (self.game / 'data').mkdir(parents=True)
        (self.game / 'data' / 'version.json').write_text('{"version": "0.1.0-beta.1"}')
        self.write('android.json', {'package': PACKAGE, 'label': 'Demo', 'library': 'demo', 'version_file': 'data/version.json',
                                    'pack': {'file': 'demo.pak', 'folders': ['art'], 'key': 'PackKey.h'}})
        self.config = {'certificates': [hashlib.sha256(CERT).hexdigest()], 'last_version_code': 10001, 'max_size_mb': 5}
        self.write('release_check.json', self.config)
        self.out = self.root / 'out'
        self.out.mkdir()

    def write(self, name, value):
        (self.game / name).write_text(json.dumps(value))

    def configure(self, **changes):
        self.config.update(changes)
        self.write('release_check.json', self.config)

    def check(self, package, channel=None, game=None):
        ctx = rc.run_checks(package, game, channel, None)
        return {r['check']: r for r in ctx.results}, ctx

    def status(self, results, check):
        return results[check]['status']

    # ------------------------------------------------------------------------------------------------------------------

    def test_a_good_store_apk_passes_every_check(self):
        results, ctx = self.check(make_package(self.out))
        self.assertEqual(ctx.game, 'demo')   # guessed from the manifest's package
        failed = {k: r['message'] for k, r in results.items() if r['status'] not in ('PASS', 'SKIP')}
        self.assertEqual(failed, {})
        for check in ('debuggable', 'certificate', 'version', 'build-identity', 'dirty', 'version-code', 'release-env',
                      'locked', 'pack', 'secrets', 'cheat-strings', 'dev-content', 'size'):
            self.assertEqual(self.status(results, check), 'PASS', check)
        self.assertEqual(results['debuggable']['id'], 'C-BLD-2')

    def test_a_good_store_bundle_passes(self):
        results, _ = self.check(make_package(self.out, kind='aab'))
        failed = {k: r['message'] for k, r in results.items() if r['status'] not in ('PASS', 'SKIP')}
        self.assertEqual(failed, {})
        self.assertIn('size-base', results)

    def test_debuggable_fails(self):
        for kind in ('apk', 'aab'):
            results, _ = self.check(make_package(self.out, kind=kind, debuggable=True))
            self.assertEqual(self.status(results, 'debuggable'), 'FAIL', kind)

    def test_certificate(self):
        results, _ = self.check(make_package(self.out, cert=b'\x30\x05other'))
        self.assertEqual(self.status(results, 'certificate'), 'FAIL')
        self.configure(certificates=[])
        results, _ = self.check(make_package(self.out, cert=DEBUG_CERT))
        self.assertEqual(self.status(results, 'certificate'), 'FAIL')   # a debug key, nothing to compare with
        results, _ = self.check(make_package(self.out, cert=DEBUG_CERT), channel='qa')
        self.assertEqual(self.status(results, 'certificate'), 'SKIP')

    def test_flavor_signers_are_expected_certificates(self):
        flavored = b'\x30\x06flavor'
        self.write('android.json', {'package': 'com.example.other', 'version_file': 'data/version.json', 'pack': False,
                                    'pack_reason': 'test', 'flavors': {'trial': {'package': PACKAGE,
                                                                                 'signers': [hashlib.sha256(flavored).hexdigest()]}}})
        results, ctx = self.check(make_package(self.out, cert=flavored), game='demo')
        self.assertEqual(ctx.flavor, 'trial')
        self.assertEqual(self.status(results, 'certificate'), 'PASS')
        self.assertEqual(self.status(results, 'pack'), 'WAIVED')

    def test_version(self):
        package = make_package(self.out, build={'code': 10022})   # the manifest follows build.json; version.json does not
        results, _ = self.check(package)
        self.assertEqual(self.status(results, 'version'), 'FAIL')
        package = make_package(self.out, build={'dirty': True})
        results, _ = self.check(package)
        self.assertEqual(self.status(results, 'dirty'), 'FAIL')
        self.configure(last_version_code=10021)
        results, _ = self.check(make_package(self.out))
        self.assertEqual(self.status(results, 'version-code'), 'FAIL')
        package = make_package(self.out)
        package.with_suffix('.build.json').write_text(json.dumps(dict(BUILD, build=8)))
        results, _ = self.check(package)
        self.assertEqual(self.status(results, 'build-identity'), 'FAIL')

    def test_release_env(self):
        results, _ = self.check(make_package(self.out, env='PAKAL_NO_3D=1\n'))
        self.assertEqual(self.status(results, 'release-env'), 'FAIL')
        results, _ = self.check(make_package(self.out, env=None), channel='qa')
        self.assertEqual(self.status(results, 'release-env'), 'PASS')
        results, _ = self.check(make_package(self.out), channel='qa')
        self.assertEqual(self.status(results, 'release-env'), 'FAIL')   # a tester's copy that says it is a player's
        results, _ = self.check(make_package(self.out, locked=False))
        self.assertEqual(self.status(results, 'locked'), 'WARN')
        self.configure(require_locked=True)
        results, _ = self.check(make_package(self.out, locked=False))
        self.assertEqual(self.status(results, 'locked'), 'FAIL')

    def test_channel_from_build_json(self):
        _, ctx = self.check(make_package(self.out, build={'channel': 'qa'}, env=None))
        self.assertEqual(ctx.channel, 'qa')

    def test_pack(self):
        for pack in ('compressed', 'unsealed', None):
            results, _ = self.check(make_package(self.out, pack=pack))
            self.assertEqual(self.status(results, 'pack'), 'FAIL', pack)
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/art/hero.png': b'\x89PNG'}))
        self.assertEqual(self.status(results, 'pack'), 'FAIL')
        self.assertIn('Assets/art/', results['pack']['message'])

    def test_secrets(self):
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/forge.json': b'{}'}))
        self.assertEqual(self.status(results, 'secrets'), 'FAIL')
        key = b'-----BEGIN PRIVATE KEY-----\nMIIEvQIBADANBgkq\n-----END PRIVATE KEY-----'
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/ui/config.txt': key}))
        self.assertEqual(self.status(results, 'secrets'), 'FAIL')
        self.assertNotIn('MIIE', results['secrets']['message'])   # what matched is never reported
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/ui/x.json': b'{"password": "hunter2hunter2"}'}))
        self.assertEqual(self.status(results, 'secrets'), 'FAIL')
        self.assertNotIn('hunter2', results['secrets']['message'])

    def test_cheat_strings(self):
        results, _ = self.check(make_package(self.out, native=b'[McpServer] listening on'))
        self.assertEqual(self.status(results, 'cheat-strings'), 'FAIL')
        self.configure(native_forbidden=['GodMode'])
        results, _ = self.check(make_package(self.out, native=b'GodMode'))
        self.assertEqual(self.status(results, 'cheat-strings'), 'FAIL')
        results, _ = self.check(make_package(self.out, native=b'GodMode', env=None), channel='qa')
        self.assertEqual(self.status(results, 'cheat-strings'), 'WARN')
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/ui/cheats.rml': b'<rml/>'}))
        self.assertEqual(self.status(results, 'cheat-strings'), 'FAIL')

    def test_dev_content_and_words(self):
        levels = json.dumps({'levels': [{'id': 'one'}, {'id': 'lab', 'dev': True}]}).encode()
        results, _ = self.check(make_package(self.out, extra={'assets/Assets/data/levels.json': levels}))
        self.assertEqual(self.status(results, 'dev-content'), 'FAIL')
        self.assertIn('/levels/1', results['dev-content']['message'])
        self.assertEqual(self.status(results, 'dev-words'), 'SKIP')
        (self.game / 'strings.csv').write_text('key,en\nDEBUG_HINT,Press Start\nHELP,Dev panel: F1\n')
        self.configure(player_text={'source': ['strings.csv'], 'package': ['Assets/ui/*.rml']})
        results, _ = self.check(make_package(self.out))
        self.assertEqual(self.status(results, 'dev-words'), 'FAIL')
        message = results['dev-words']['message']
        self.assertIn('strings.csv:3', message)
        self.assertNotIn('strings.csv:2', message)   # the key column is not player-facing

    def test_size(self):
        self.configure(max_size_mb=0.000001)
        results, _ = self.check(make_package(self.out))
        self.assertEqual(self.status(results, 'size'), 'FAIL')
        self.configure(max_size_mb=None, base_max_mb=0.000001)
        results, _ = self.check(make_package(self.out, kind='aab'))
        self.assertEqual(self.status(results, 'size-base'), 'FAIL')

    def test_plugin_and_exemptions(self):
        (self.game / 'tools').mkdir()
        (self.game / 'tools' / 'release_checks.py').write_text(
            'def run(ctx):\n    ctx.report("C-BLD-4", "demo-data", "FAIL" if ctx.package else "WARN", "the game\'s own check")\n')
        self.configure(exempt={'release-env': 'a test, the release lead, 2026-11-19'})
        results, _ = self.check(make_package(self.out, env=''))
        self.assertEqual(self.status(results, 'demo-data'), 'FAIL')
        self.assertEqual(self.status(results, 'release-env'), 'WAIVED')
        self.assertIn('the release lead', results['release-env']['message'])

    def test_missing_package_still_runs_the_game_checks(self):
        (self.game / 'tools').mkdir()
        (self.game / 'tools' / 'release_checks.py').write_text(
            'def run(ctx):\n    ctx.report("C-BLD-4", "demo-data", "PASS", "data")\n')
        results, _ = self.check(self.out / 'missing.apk', game='demo')
        self.assertEqual(self.status(results, 'package'), 'FAIL')
        self.assertEqual(self.status(results, 'demo-data'), 'PASS')

    def test_main_exit_code_and_json(self):
        good, bad = make_package(self.out), make_package(self.out, debuggable=True, name='pakal-demo-bad.apk')
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(rc.main([str(good), '--game', 'demo']), 0)
        self.assertIn('PASS   C-BLD-2   debuggable', text.getvalue())
        with contextlib.redirect_stdout(io.StringIO()) as text:
            self.assertEqual(rc.main([str(bad), '--game', 'demo', '--json']), 1)
        result = json.loads(text.getvalue())
        self.assertFalse(result['ok'])
        self.assertIn({'id': 'C-BLD-2', 'check': 'debuggable', 'status': 'FAIL', 'message': 'android:debuggable is true'},
                      result['results'])


if __name__ == '__main__':
    unittest.main()
