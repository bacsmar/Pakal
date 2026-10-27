"""Runs the Zombies bot on the heaviest level with the telemetry on (PAKAL_PROFILE), on this computer or on Android devices
by adb, and says whether each device holds its budget (tools/bench_budgets.json, or a bench_budgets.local.json next to it
with one's own devices: the quality it runs at, its frame rate,
how long its slow frames may be). The runs' files stay in --out for tools/pakal_prof.py.

Usage: pakal_bench.py [--device windows|<adb serial>|android|all] [--seconds 60] [--skip 10] [--level <level.json>]
                      [--quality low|medium|high|ultra] [--out <folder>]
Exits 1 when a device misses its budget.
"""
import argparse
import fnmatch
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
def find_adb():
    """$ADB, else the SDK's (ANDROID_SDK, or android/local_paths.json), else adb on the PATH."""
    if os.environ.get('ADB'):
        return os.environ['ADB']
    local = ROOT / 'android' / 'local_paths.json'
    sdk = os.environ.get('ANDROID_SDK') or (json.loads(local.read_text(encoding='utf-8')).get('ANDROID_SDK') if local.is_file() else None)
    if sdk:
        adb = Path(sdk) / 'platform-tools' / ('adb.exe' if os.name == 'nt' else 'adb')
        if adb.is_file():
            return str(adb)
    return 'adb'


ADB = find_adb()
PACKAGE = 'org.pakalengine.zombies'
EXTERNAL = f'/sdcard/Android/data/{PACKAGE}/files'


def adb(serial, *args, check=True):
    return subprocess.run([ADB, '-s', serial, *args], capture_output=True, text=True, check=check).stdout.strip()


def android_devices():
    out = subprocess.run([ADB, 'devices'], capture_output=True, text=True).stdout.splitlines()[1:]
    return [line.split()[0] for line in out if line.strip().endswith('device')]


def budget_for(model, budgets):
    for entry in budgets['devices']:
        if model.startswith(entry['match']) or fnmatch.fnmatch(model, entry['match']):
            return entry
    return None


def environment(level, seconds, skip, quality, profile_file, game_profile):
    return {'ZOMBIES_LEVEL': level, 'ZOMBIES_AUTOPLAY': '1', 'ZOMBIES_SEED': '2026', 'ZOMBIES_PROFILE': game_profile,
            'PAKAL_PROFILE': profile_file, 'PAKAL_PROFILE_SECONDS': str(seconds + skip), 'PAKAL_PROFILE_SKIP': str(skip),
            'PAKAL_QUALITY': quality}


def run_windows(args, quality, out):
    bin_dir = ROOT / 'build-win64' / 'bin'
    profile = out / 'windows.ndjson'
    env = dict(os.environ, **environment(args.level, args.seconds, args.skip, quality, str(profile), str(out / 'windows_profile.json')))
    process = subprocess.Popen([str(bin_dir / 'PakalPlayer.exe'), 'ZombiesModule.dll'], cwd=bin_dir, env=env,
                               stdout=open(out / 'windows.log', 'w'), stderr=subprocess.STDOUT)
    try:
        process.wait(timeout=args.seconds + args.skip + 90)
    except subprocess.TimeoutExpired:
        process.kill()
    return profile


def run_android(args, serial, quality, out):
    profile_remote = f'{EXTERNAL}/bench.ndjson'
    lines = environment(args.level, args.seconds, args.skip, quality, profile_remote, f'{EXTERNAL}/bench_profile.json')
    env_file = out / f'{serial}_env.txt'
    env_file.write_text(''.join(f'{k}={v}\n' for k, v in lines.items()), encoding='ascii')
    adb(serial, 'shell', 'am', 'force-stop', PACKAGE)
    adb(serial, 'shell', 'rm', '-f', profile_remote, f'{EXTERNAL}/bench_profile.json', check=False)
    adb(serial, 'push', str(env_file), f'{EXTERNAL}/pakal_env.txt')
    # awake for the whole run: a device that locks stops drawing (the screen stays on while it is plugged in by USB)
    adb(serial, 'shell', 'svc', 'power', 'stayon', 'usb', check=False)
    adb(serial, 'shell', 'input', 'keyevent', 'KEYCODE_WAKEUP', check=False)
    adb(serial, 'shell', 'wm', 'dismiss-keyguard', check=False)
    adb(serial, 'shell', 'am', 'start', '-n', f'{PACKAGE}/.PakalActivity')
    deadline = time.time() + args.seconds + args.skip + 120
    profile = out / f'{serial}.ndjson'
    try:
        while time.time() < deadline:
            time.sleep(5)
            adb(serial, 'shell', 'input', 'keyevent', 'KEYCODE_WAKEUP', check=False)
            if '"summary"' in adb(serial, 'shell', 'tail', '-n', '1', profile_remote, check=False):
                break
        adb(serial, 'pull', profile_remote, str(profile), check=False)
    finally:
        adb(serial, 'shell', 'rm', '-f', f'{EXTERNAL}/pakal_env.txt', check=False)   # the game opens normally again
        adb(serial, 'shell', 'am', 'force-stop', PACKAGE, check=False)
    return profile


def verdict(profile, budget):
    """(passed, text) of a run against its budget."""
    records = [json.loads(line) for line in profile.read_text(encoding='utf-8').splitlines() if line.strip()] if profile.is_file() else []
    summary = next((r for r in records if r.get('summary')), None)
    if not summary:
        return False, 'no summary (the run did not finish)'
    fps, p95, p99 = summary['fps'], summary['frame_ms']['p95'], summary['frame_ms']['p99']
    misses = []
    if fps < budget['fps'] * 0.95:
        misses.append(f"fps {fps:.1f} < {budget['fps']}")
    if p95 > budget['p95_ms']:
        misses.append(f"p95 {p95:.1f} ms > {budget['p95_ms']}")
    if p99 > budget['p99_ms']:
        misses.append(f"p99 {p99:.1f} ms > {budget['p99_ms']}")
    text = f"{summary['info'].get('quality', '?')}: {fps:.1f} fps, p95 {p95:.1f} ms, p99 {p99:.1f} ms, {summary['counters'].get('mem.process_mb', 0):.0f} MB"
    return not misses, text + ('' if not misses else '  MISSES: ' + ', '.join(misses))


def main():
    # tools/bench_budgets.local.json (not in Git) lists this machine's own devices; without it, the generic budgets
    local = ROOT / 'tools' / 'bench_budgets.local.json'
    budgets = json.loads((local if local.is_file() else ROOT / 'tools' / 'bench_budgets.json').read_text(encoding='utf-8'))
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--device', default='all')
    parser.add_argument('--seconds', type=int, default=60)
    parser.add_argument('--skip', type=int, default=10, help='seconds left out at the start (the loading)')
    parser.add_argument('--level', default=budgets['level'])
    parser.add_argument('--quality', help="instead of the device's budget")
    parser.add_argument('--out', default=str(ROOT / 'build-win64' / 'bench'))
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    targets = []
    if args.device in ('windows', 'all') and os.name == 'nt':
        targets.append(('windows', 'windows'))
    if args.device in ('android', 'all'):
        targets += [(serial, adb(serial, 'shell', 'getprop', 'ro.product.model')) for serial in android_devices()]
    elif args.device not in ('windows', 'all'):
        targets.append((args.device, adb(args.device, 'shell', 'getprop', 'ro.product.model')))

    failed = False
    for serial, model in targets:
        budget = budget_for(model, budgets) or {'name': model, 'quality': 'ultra', 'fps': 30, 'p95_ms': 50, 'p99_ms': 80}
        quality = args.quality or budget['quality']
        print(f"{budget['name']} ({model}): {args.seconds} s of the bot at {quality}...", flush=True)
        profile = run_windows(args, quality, out) if serial == 'windows' else run_android(args, serial, quality, out)
        passed, text = verdict(profile, budget)
        failed = failed or not passed
        print(f"  {'PASS' if passed else 'FAIL'}  {text}  ({profile})", flush=True)
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
