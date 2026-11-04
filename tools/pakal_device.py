"""Measures and tunes a running Pakal game, on this computer or on an Android device, through the engine's tools (its MCP
server: source/mcp/EngineTools, docs/PROFILING.md). Nothing but the standard library: an agent or a person runs it.

    pakal_device.py targets                               the local player (if it answers) and the devices adb sees
    pakal_device.py [--target T] forward                  sets up `adb forward` to a device (T: its serial)
    pakal_device.py [--target T] env --package P KEY=VALUE ...   adds lines to the app's pakal_env.txt (the app reads it at start)
    pakal_device.py [--target T] tools                    the tools the game has
    pakal_device.py [--target T] call <tool> ['<json>']   calls one (get_stats, get_settings, set_settings, set_camera...)
    pakal_device.py [--target T] stats                    the last second, in short
    pakal_device.py [--target T] set <setting> <value>    one setting (post.aoStrength, preset.shadowMapMax, renderScale, quality...)
    pakal_device.py [--target T] record <seconds> <out.ndjson>   the profiler's records, as PAKAL_PROFILE writes them
    pakal_device.py [--target T] ab <setting> <a> <b> [--seconds N] [--rounds R]   two values in turn, measured, compared
    pakal_device.py [--target T] writeback [--map <map.json>] [--settings <file.json>]   the tuned values into the game's data

T is "local" (the default: a game on this computer) or a device's serial (adb). --port is the game's PAKAL_MCP_PORT (8765 by
default; the same port is forwarded from the device), --token its PAKAL_MCP_TOKEN. adb is found from ADB, ANDROID_HOME or
ANDROID_SDK_ROOT (platform-tools), else on the PATH.

Write back: the game's own tool "write_back" when it has one (it knows its data files); else a map
{"fields": {"<setting>": {"file": "<json file, relative to the map>", "pointer": "/a/b"}}} says where each setting goes.
"""
import argparse
import json
import os
import shutil
import socket
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

DEFAULT_PORT = 8765


# --- reaching the game ---

def adb_path():
    if os.environ.get('ADB'):
        return os.environ['ADB']
    for variable in ('ANDROID_HOME', 'ANDROID_SDK_ROOT'):
        root = os.environ.get(variable)
        if root:
            candidate = Path(root) / 'platform-tools' / ('adb.exe' if os.name == 'nt' else 'adb')
            if candidate.is_file():
                return str(candidate)
    return shutil.which('adb') or 'adb'


def adb(*arguments, serial=None, check=True):
    command = [adb_path()] + (['-s', serial] if serial else []) + list(arguments)
    result = subprocess.run(command, capture_output=True, text=True)
    if check and result.returncode != 0:
        sys.exit(f'adb {" ".join(arguments)}: {result.stderr.strip() or result.stdout.strip()}')
    return result.stdout


def devices():
    try:
        out = adb('devices', check=False)
    except FileNotFoundError:
        return []
    found = []
    for line in out.splitlines()[1:]:
        parts = line.split()
        if len(parts) >= 2:
            found.append((parts[0], parts[1]))
    return found


class Game:
    """A JSON-RPC client of the game's MCP server (http://127.0.0.1:<port>/mcp)."""

    def __init__(self, port, token=None, timeout=10.0):
        self.port = port
        self.token = token
        self.timeout = timeout
        self.next_id = 1

    def rpc(self, method, params=None):
        body = {'jsonrpc': '2.0', 'id': self.next_id, 'method': method}
        if params is not None:
            body['params'] = params
        self.next_id += 1
        answer = json.loads(self.post(json.dumps(body).encode()))
        if 'error' in answer:
            raise RuntimeError(answer['error'].get('message', str(answer['error'])))
        return answer['result']

    def post(self, payload):
        """One HTTP POST over a plain socket, read to its end. Its body is taken whether it comes whole or chunked: a local
        proxy (some antivirus web shields) may rewrite the headers of loopback HTTP without rewriting the body."""
        headers = f'POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: {len(payload)}\r\n'
        if self.token:
            headers += f'Authorization: Bearer {self.token}\r\n'
        with socket.create_connection(('127.0.0.1', self.port), timeout=self.timeout) as connection:
            connection.sendall((headers + 'Connection: close\r\n\r\n').encode() + payload)
            data = b''
            while True:
                chunk = connection.recv(65536)
                if not chunk:
                    break
                data += chunk
        head, _, rest = data.partition(b'\r\n\r\n')
        status = head.split(b'\r\n', 1)[0].decode(errors='replace')
        if ' 200 ' not in status + ' ':
            raise ConnectionError(status or 'no answer')
        if rest.lstrip().startswith(b'{'):
            return rest.decode()
        body, at = b'', 0
        while at < len(rest):
            line_end = rest.find(b'\r\n', at)
            if line_end < 0:
                break
            size = int(rest[at:line_end].strip() or b'0', 16)
            if size == 0:
                break
            body += rest[line_end + 2:line_end + 2 + size]
            at = line_end + 2 + size + 2
        return body.decode()

    def tools(self):
        return self.rpc('tools/list')['tools']

    def call(self, name, arguments=None):
        result = self.rpc('tools/call', {'name': name, 'arguments': arguments or {}})
        payload = json.loads(result['content'][0]['text'])
        if result.get('isError'):
            raise RuntimeError(payload.get('error', str(payload)))
        return payload

    def answers(self):
        try:
            self.rpc('initialize', {})
            return True
        except (OSError, RuntimeError, ValueError):
            return False


def connect(args):
    if args.target != 'local':
        adb('forward', f'tcp:{args.port}', f'tcp:{args.port}', serial=args.target)
    return Game(args.port, args.token)


# --- settings by a dotted name ---

def setting_arguments(name, value):
    """'post.aoStrength', 0.5 -> {"post": {"aoStrength": 0.5}}; 'preset.shadowMapMax' likewise; 'renderScale' -> {...}."""
    parts = name.split('.')
    out = value
    for part in reversed(parts):
        out = {part: out}
    return out


def setting_value(settings, name):
    """The value of a dotted setting in get_settings' answer (preset.* is the current quality's)."""
    parts = name.split('.')
    if parts[0] == 'preset':
        node = settings['presets'][settings['quality']]
        parts = parts[1:]
    else:
        node = settings
    for part in parts:
        node = node[part]
    return node


def parse_value(text):
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return text


# --- measuring ---

def take_records(game, seconds, after=None, on_record=None):
    """The profiler's records of the next 'seconds' (each a dict), as the game makes them."""
    if after is None:
        last = game.call('get_stats')
        after = last.get('clock', -1.0)
    records, start = [], time.time()
    while time.time() - start < seconds:
        time.sleep(1.0)
        for record in game.call('get_profile', {'after': after})['records']:
            after = record.get('clock', after)
            records.append(record)
            if on_record:
                on_record(record)
    return records, after


def summarize(records):
    if not records:
        return {}
    def counter(name):
        values = [r['counters'][name] for r in records if name in r.get('counters', {})]
        return statistics.mean(values) if values else None
    out = {
        'seconds': len(records),
        'fps': statistics.mean(r['fps'] for r in records),
        'frame_p50': statistics.median(r['frame_ms']['p50'] for r in records),
        'frame_p95': max(r['frame_ms']['p95'] for r in records),
        'gpu_ms': counter('gpu.frame_ms'),
        'draws': counter('gpu.draws'),
        'primitives': counter('gpu.primitives'),
    }
    passes = sorted({name for r in records for name in r.get('counters', {}) if name.startswith('gpu.view.')})
    for name in passes:
        out[name] = counter(name)
    return out


# --- commands ---

def cmd_targets(args):
    local = Game(args.port, args.token, timeout=1.0)
    print(f'local       port {args.port}: ' + ('answers' if local.answers() else 'nothing (start it with PAKAL_MCP_PORT)'))
    for serial, state in devices():
        print(f'{serial:<12}{state}')


def cmd_forward(args):
    if args.target == 'local':
        sys.exit('forward needs --target <serial>')
    adb('forward', f'tcp:{args.port}', f'tcp:{args.port}', serial=args.target)
    print(f'127.0.0.1:{args.port} -> {args.target}:{args.port}')


def cmd_env(args):
    if args.target == 'local':
        sys.exit('env needs --target <serial>')
    remote = f'/sdcard/Android/data/{args.package}/files/pakal_env.txt'
    current = subprocess.run([adb_path(), '-s', args.target, 'shell', 'cat', remote], capture_output=True, text=True).stdout
    lines = [line for line in current.splitlines() if line.strip()]
    for pair in args.pairs:
        key = pair.split('=', 1)[0]
        lines = [line for line in lines if not line.startswith(key + '=')] + [pair]
    with tempfile.NamedTemporaryFile('w', delete=False, suffix='.txt', newline='\n') as file:
        file.write('\n'.join(lines) + '\n')
    adb('push', file.name, remote, serial=args.target)
    os.unlink(file.name)
    print('\n'.join(lines))
    print('(the app reads it when it starts)')


def cmd_tools(args):
    for tool in connect(args).tools():
        print(f'{tool["name"]}: {tool["description"]}')


def cmd_call(args):
    # '<json>', or @file with it (shells that eat quotes)
    text = args.arguments or ''
    if text.startswith('@'):
        text = Path(text[1:]).read_text(encoding='utf-8')
    arguments = json.loads(text) if text.strip() else {}
    print(json.dumps(connect(args).call(args.tool, arguments), indent=1))


def cmd_stats(args):
    record = connect(args).call('get_stats')
    if 'frame_ms' not in record:
        print(record)
        return
    counters = record.get('counters', {})
    print(f'{record["fps"]:.1f} fps, frame p50 {record["frame_ms"]["p50"]:.1f} p95 {record["frame_ms"]["p95"]:.1f} ms, '
          f'GPU {counters.get("gpu.frame_ms", 0):.1f} ms, {counters.get("gpu.draws", 0):.0f} draws, '
          f'{counters.get("gpu.primitives", 0):.0f} triangles, {counters.get("mem.process_mb", 0):.0f} MB')
    for name in sorted(n for n in counters if n.startswith('gpu.view.')):
        print(f'  {name[9:]:<14}{counters[name]:.2f} ms')
    for name, section in sorted(record.get('sections', {}).items(), key=lambda kv: -kv[1]['ms_per_frame'])[:8]:
        print(f'  {name:<22}{section["ms_per_frame"]:.2f} ms/frame')


def cmd_set(args):
    settings = connect(args).call('set_settings', setting_arguments(args.setting, parse_value(args.value)))
    print(f'{args.setting} = {json.dumps(setting_value(settings, args.setting))}')


def cmd_record(args):
    game = connect(args)
    settings = game.call('get_settings')
    out = Path(args.out)
    first = True
    with out.open('w', encoding='utf-8', newline='\n') as file:
        def write(record):
            nonlocal first
            if first:
                # what the run was: written once, as the profiler does
                record.setdefault('info', {}).update({'target': args.target, 'quality': settings.get('quality'),
                                                      'render_scale': settings.get('renderScale'), 'fsr': settings.get('fsr')})
                first = False
            file.write(json.dumps(record) + '\n')
            file.flush()
            print(f'{record["t"]:7.1f} s {record["fps"]:6.1f} fps  p95 {record["frame_ms"]["p95"]:5.1f} ms', flush=True)
        take_records(game, args.seconds, on_record=write)
    print(f'written {out} (tools/pakal_prof.py {out})')


def cmd_ab(args):
    game = connect(args)
    before = setting_value(game.call('get_settings'), args.setting)
    values = {'A': parse_value(args.a), 'B': parse_value(args.b)}
    taken = {'A': [], 'B': []}
    try:
        for round_ in range(args.rounds):
            for label in ('A', 'B'):
                game.call('set_settings', setting_arguments(args.setting, values[label]))
                _, after = take_records(game, args.settle)   # the change settles (shaders, targets, the governor)
                records, _ = take_records(game, args.seconds, after)
                taken[label] += records
                print(f'round {round_ + 1} {label} ({args.setting} = {json.dumps(values[label])}): '
                      f'{summarize(records).get("frame_p50", 0):.2f} ms p50', flush=True)
    finally:
        game.call('set_settings', setting_arguments(args.setting, before))
    a, b = summarize(taken['A']), summarize(taken['B'])
    print(f'\n{"":<22}{"A":>10}{"B":>10}{"B - A":>10}')
    for key in a:
        if key == 'seconds' or a.get(key) is None or b.get(key) is None:
            continue
        print(f'{key:<22}{a[key]:>10.2f}{b[key]:>10.2f}{b[key] - a[key]:>+10.2f}')
    print(f'(each over {a.get("seconds", 0)} s; {args.setting} put back to {json.dumps(before)})')


def set_pointer(document, pointer, value):
    parts = [part.replace('~1', '/').replace('~0', '~') for part in pointer.strip('/').split('/')]
    node = document
    for part in parts[:-1]:
        node = node.setdefault(part, {}) if isinstance(node, dict) else node[int(part)]
    if isinstance(node, list):
        node[int(parts[-1])] = value
    else:
        node[parts[-1]] = value


def cmd_writeback(args):
    game = connect(args)
    settings = json.loads(Path(args.settings).read_text(encoding='utf-8')) if args.settings else game.call('get_settings')
    if not args.map:
        if 'write_back' not in {tool['name'] for tool in game.tools()}:
            sys.exit('the game has no write_back tool: give --map <map.json> (docs/PROFILING.md)')
        print(json.dumps(game.call('write_back', {'settings': settings}), indent=1))
        return
    mapping = json.loads(Path(args.map).read_text(encoding='utf-8'))
    base = Path(args.map).parent
    files = {}
    for name, place in mapping.get('fields', {}).items():
        try:
            value = setting_value(settings, name)
        except (KeyError, TypeError):
            print(f'{name}: not in the settings, left')
            continue
        path = (base / place['file']).resolve()
        if path not in files:
            files[path] = json.loads(path.read_text(encoding='utf-8'))
        set_pointer(files[path], place['pointer'], value)
        print(f'{name} = {json.dumps(value)} -> {path}#{place["pointer"]}')
    for path, document in files.items():
        path.write_text(json.dumps(document, indent=1, ensure_ascii=False) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--target', default='local', help='"local" or a device serial')
    parser.add_argument('--port', type=int, default=int(os.environ.get('PAKAL_MCP_PORT', DEFAULT_PORT)))
    parser.add_argument('--token', default=os.environ.get('PAKAL_MCP_TOKEN'))
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('targets')
    sub.add_parser('forward')
    env = sub.add_parser('env')
    env.add_argument('--package', required=True)
    env.add_argument('pairs', nargs='+')
    sub.add_parser('tools')
    call = sub.add_parser('call')
    call.add_argument('tool')
    call.add_argument('arguments', nargs='?')
    sub.add_parser('stats')
    set_ = sub.add_parser('set')
    set_.add_argument('setting')
    set_.add_argument('value')
    record = sub.add_parser('record')
    record.add_argument('seconds', type=float)
    record.add_argument('out')
    ab = sub.add_parser('ab')
    ab.add_argument('setting')
    ab.add_argument('a')
    ab.add_argument('b')
    ab.add_argument('--seconds', type=float, default=5.0)
    ab.add_argument('--rounds', type=int, default=2)
    ab.add_argument('--settle', type=float, default=2.0)
    writeback = sub.add_parser('writeback')
    writeback.add_argument('--map')
    writeback.add_argument('--settings')
    args = parser.parse_args()
    commands = {'targets': cmd_targets, 'forward': cmd_forward, 'env': cmd_env, 'tools': cmd_tools, 'call': cmd_call,
                'stats': cmd_stats, 'set': cmd_set, 'record': cmd_record, 'ab': cmd_ab, 'writeback': cmd_writeback}
    try:
        commands[args.command](args)
    except (OSError, ConnectionError) as error:
        sys.exit(f'no game answers on 127.0.0.1:{args.port} ({error}); start it with PAKAL_MCP_PORT={args.port}')


if __name__ == '__main__':
    main()
