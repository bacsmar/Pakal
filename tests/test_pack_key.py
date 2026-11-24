"""A game pack's key never goes on a command line, where any process listing (Task Manager, `ps`) shows it while the pack is
sealed: build_apk.py seals in its own process, and pakal_pack.py takes the key from PAKAL_PACK_KEY or its standard input. The
key here is a dummy; no real key is read."""
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
DUMMY_KEY = 'dummy-key-for-the-test-7f3a'


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pakal_pack = load('pakal_pack', ROOT / 'tools' / 'pakal_pack.py')


def make_folder(base):
    folder = Path(base) / 'assets'
    (folder / 'textures').mkdir(parents=True)
    (folder / 'textures' / 'Wall.png').write_bytes(b'not really a png' * 50)
    (folder / 'sounds.json').write_text('{"step": "step.ogg"}', encoding='utf-8')
    return folder


def entries(path):
    with zipfile.ZipFile(path) as z:
        return {name: z.read(name) for name in z.namelist()}


class PackKey(unittest.TestCase):
    def test_seal_folder_matches_the_command_line_tool(self):
        with tempfile.TemporaryDirectory() as base:
            folder = make_folder(base)
            inproc, viaenv, viastdin = (Path(base) / n for n in ('a.pak', 'b.pak', 'c.pak'))
            self.assertEqual(pakal_pack.seal_folder(str(folder), str(inproc), DUMMY_KEY), 2)
            env = dict(os.environ, PAKAL_PACK_KEY=DUMMY_KEY)
            subprocess.check_call([sys.executable, str(ROOT / 'tools' / 'pakal_pack.py'), str(folder), str(viaenv)], env=env,
                                  stdout=subprocess.DEVNULL)
            subprocess.run([sys.executable, str(ROOT / 'tools' / 'pakal_pack.py'), str(folder), str(viastdin), '-'],
                           input=DUMMY_KEY + '\n', text=True, check=True, stdout=subprocess.DEVNULL)
            self.assertEqual(entries(inproc), entries(viaenv), 'the key from the environment seals the same pack')
            self.assertEqual(entries(inproc), entries(viastdin), 'the key from the standard input seals the same pack')
            self.assertEqual(sorted(entries(inproc)), ['assets/sounds.json', 'assets/textures/wall.png'])
            self.assertTrue(all(data.startswith(b'PKP1') for data in entries(inproc).values()))

    def test_the_tool_refuses_a_key_on_its_command_line(self):
        with tempfile.TemporaryDirectory() as base:
            folder = make_folder(base)
            env = {k: v for k, v in os.environ.items() if k != 'PAKAL_PACK_KEY'}
            result = subprocess.run([sys.executable, str(ROOT / 'tools' / 'pakal_pack.py'), str(folder), str(Path(base) / 'x.pak'),
                                     DUMMY_KEY], env=env, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((Path(base) / 'x.pak').exists(), 'nothing is sealed with a key from the command line')
            self.assertIn('PAKAL_PACK_KEY', result.stderr + result.stdout)

    def test_the_tool_refuses_to_seal_without_a_key(self):
        with tempfile.TemporaryDirectory() as base:
            folder = make_folder(base)
            env = {k: v for k, v in os.environ.items() if k != 'PAKAL_PACK_KEY'}
            result = subprocess.run([sys.executable, str(ROOT / 'tools' / 'pakal_pack.py'), str(folder), str(Path(base) / 'x.pak')],
                                    env=env, capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse((Path(base) / 'x.pak').exists())

    def test_build_apk_puts_the_key_on_no_child_command_line(self):
        try:
            sys.path.insert(0, str(ROOT / 'android'))
            build_apk = load('build_apk', ROOT / 'android' / 'build_apk.py')
        except (Exception, SystemExit) as error:   # no Android SDK on this machine: build_apk reads its tools at import
            self.skipTest(f'build_apk.py does not load here: {error}')
        finally:
            sys.path.pop(0)
        with tempfile.TemporaryDirectory() as base:
            stage = Path(base) / 'stage'
            make_folder(stage).rename(stage / 'Assets')
            header = Path(base) / 'key.h'
            header.write_text(f'inline const char* pack_key() {{ return "{DUMMY_KEY}"; }}\n', encoding='utf-8')
            game = {'pack': {'file': 'game.pak', 'folders': ['textures'], 'key': str(header)}}
            spawned = []

            def record(command, *args, **kwargs):
                spawned.append([str(part) for part in command])
                return 0
            with mock.patch.dict(build_apk.GAMES, {'testgame': game}), \
                    mock.patch.object(build_apk, 'BUILD', Path(base) / 'build'), \
                    mock.patch.object(build_apk, 'write_asset_list', lambda stage: None), \
                    mock.patch.object(subprocess, 'check_call', side_effect=record), \
                    mock.patch.object(subprocess, 'call', side_effect=record), \
                    mock.patch.object(subprocess, 'run', side_effect=record), \
                    mock.patch.object(subprocess, 'Popen', side_effect=record):
                (Path(base) / 'build').mkdir()
                build_apk.seal_pack(stage, 'testgame')
            self.assertFalse(any(DUMMY_KEY in part for command in spawned for part in command),
                             f'the key is on a child command line: {spawned}')
            self.assertTrue((stage / 'game.pak').is_file(), 'the pack is sealed')
            self.assertEqual(sorted(entries(stage / 'game.pak')), ['assets/textures/wall.png'])
            self.assertFalse((stage / 'Assets' / 'textures').exists(), 'the sealed folder is out of the loose assets')


if __name__ == '__main__':
    unittest.main()
