"""Reads a run's telemetry (PAKAL_PROFILE: one JSON object per line) and prints what it cost: the run's frame percentiles,
the sections of code by their time per frame, the memory and the counters, and how the frame rate went second by second.
Several files print side by side, to compare two runs (a change, two devices).

Usage: pakal_prof.py <run.ndjson> [<other.ndjson> ...] [--seconds]
"""
import argparse
import json
import sys
from pathlib import Path


def load(path):
    """The per-second records and the summary (made from the seconds when the run was cut before it wrote one)."""
    seconds, summary, info = [], None, {}
    for line in Path(path).read_text(encoding='utf-8').splitlines():
        if not line.strip():
            continue
        record = json.loads(line)
        info.update(record.get('info', {}))
        if record.get('summary'):
            summary = record
        else:
            seconds.append(record)
    if summary is None and seconds:
        # no summary (the run was stopped): the worst of each second's percentiles stands in for the run's
        frames = [r['frame_ms'] for r in seconds]
        summary = {'fps': sum(r['fps'] for r in seconds) / len(seconds), 'seconds': len(seconds),
                   'frame_ms': {k: max(f[k] for f in frames) if k != 'p50' else sorted(f['p50'] for f in frames)[len(frames) // 2]
                                for k in ('p50', 'p95', 'p99', 'max')},
                   'sections': {}, 'counters': seconds[-1].get('counters', {}), 'estimated': True}
        names = {name for r in seconds for name in r.get('sections', {})}
        for name in names:
            values = [r['sections'][name] for r in seconds if name in r.get('sections', {})]
            summary['sections'][name] = {'ms_per_frame': sum(v['ms_per_frame'] for v in values) / len(values),
                                         'worst_ms': max(v['worst_ms'] for v in values)}
    summary = summary or {}
    summary['info'] = {**info, **summary.get('info', {})}
    return seconds, summary


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('files', nargs='+')
    parser.add_argument('--seconds', action='store_true', help='also print every second')
    args = parser.parse_args()
    runs = [(Path(f).stem, *load(f)) for f in args.files]

    width = max(14, *(len(name) for name, _, _ in runs))
    print('run'.ljust(26) + ''.join(name[:width].rjust(width + 2) for name, _, _ in runs))

    def row(label, values):
        print(label.ljust(26) + ''.join(str(v).rjust(width + 2) for v in values))

    row('device', [s['info'].get('device', '?') for _, _, s in runs])
    row('quality', [s['info'].get('quality', '?') for _, _, s in runs])
    row('level', [Path(s['info'].get('level', '?')).parent.name or '?' for _, _, s in runs])
    row('seconds', [f"{s.get('seconds', 0):.0f}" + ('*' if s.get('estimated') else '') for _, _, s in runs])
    row('fps', [f"{s.get('fps', 0):.1f}" for _, _, s in runs])
    for k in ('p50', 'p95', 'p99', 'max'):
        row(f'frame {k} (ms)', [f"{s.get('frame_ms', {}).get(k, 0):.1f}" for _, _, s in runs])
    sections = sorted({n for _, _, s in runs for n in s.get('sections', {})},
                      key=lambda n: -max(s.get('sections', {}).get(n, {}).get('ms_per_frame', 0) for _, _, s in runs))
    print()
    for name in sections:
        row(f'{name} (ms/frame)', [f"{s.get('sections', {}).get(name, {}).get('ms_per_frame', 0):.2f}" for _, _, s in runs])
    counters = sorted({n for _, _, s in runs for n in s.get('counters', {})})
    print()
    for name in counters:
        row(name, [f"{s.get('counters', {}).get(name, 0):.1f}" if name in s.get('counters', {}) else '-' for _, _, s in runs])
    if any(s.get('estimated') for _, _, s in runs):
        print('\n* the run was stopped before its summary: the worst second stands for p95, p99 and max')
    if args.seconds:
        for name, seconds, _ in runs:
            print(f'\n{name}: t, fps, p50, p95, p99, MB, thermal')
            for r in seconds:
                c = r.get('counters', {})
                print(f"  {r['t']:6.1f} {r['fps']:6.1f} {r['frame_ms']['p50']:6.1f} {r['frame_ms']['p95']:6.1f} {r['frame_ms']['p99']:6.1f}"
                      f" {c.get('mem.process_mb', 0):7.0f} {c.get('thermal.status', '-')}")


if __name__ == '__main__':
    sys.exit(main())
