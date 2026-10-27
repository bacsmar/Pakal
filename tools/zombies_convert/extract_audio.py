"""Copies the sounds a converted Zombies level uses, decoded to 16-bit mono PCM WAV (SFML reads only PCM).

The game names a sound by an event label (evt_*: a random pick among several sounds) or by the label of
a single sound (sfx_*). Both are listed in the sound pack (ZOMBIES_SDD_VOX.xml). The names come from the
level's rules (every text value that is a known label) and from --extra.

Usage:
  extract_audio.py --vox <ZOMBIES_SDD_VOX.xml> --sounds <data/sounds> --level <level.json> [--extra name ...]

Writes <level dir>/sounds/<file>.wav and <level dir>/audio.json: {"name": ["sounds/file.wav", ...]}.
"""

import argparse
import json
import os
import struct
import sys
import xml.etree.ElementTree as ET

INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767]


def read_chunks(data):
    if data[0:4] != b'RIFF' or data[8:12] != b'WAVE':
        raise ValueError('not a WAV file')
    chunks = {}
    at = 12
    while at + 8 <= len(data):
        tag = data[at:at + 4]
        size = struct.unpack_from('<I', data, at + 4)[0]
        chunks.setdefault(tag, data[at + 8:at + 8 + size])
        at += 8 + size + (size & 1)
    return chunks


def decode_ima_block(block, channels, samples_per_block):
    """One block of Microsoft IMA ADPCM -> interleaved 16-bit samples."""
    predictor = []
    index = []
    out = [[] for _ in range(channels)]
    for c in range(channels):
        p, i = struct.unpack_from('<hB', block, 4 * c)
        predictor.append(p)
        index.append(min(max(i, 0), 88))
        out[c].append(p)
    at = 4 * channels
    while at + 4 * channels <= len(block) and len(out[0]) < samples_per_block:
        for c in range(channels):
            word = block[at:at + 4]
            at += 4
            for byte in word:
                for nibble in (byte & 0x0F, byte >> 4):
                    step = STEP_TABLE[index[c]]
                    diff = step >> 3
                    if nibble & 1: diff += step >> 2
                    if nibble & 2: diff += step >> 1
                    if nibble & 4: diff += step
                    if nibble & 8: diff = -diff
                    predictor[c] = min(max(predictor[c] + diff, -32768), 32767)
                    index[c] = min(max(index[c] + INDEX_TABLE[nibble], 0), 88)
                    out[c].append(predictor[c])
    count = min(len(o) for o in out)
    samples = []
    for s in range(min(count, samples_per_block)):
        for c in range(channels):
            samples.append(out[c][s])
    return samples


def to_pcm(path):
    """Reads a WAV (PCM or IMA ADPCM) and returns (channels, rate, 16-bit PCM bytes)."""
    with open(path, 'rb') as f:
        chunks = read_chunks(f.read())
    fmt = chunks[b'fmt ']
    tag, channels, rate, _, block_align, bits = struct.unpack_from('<HHIIHH', fmt, 0)
    data = chunks[b'data']
    if tag == 1 and bits == 16:
        return channels, rate, data
    if tag != 0x11:
        raise ValueError('%s: unsupported WAV format %d' % (path, tag))
    samples_per_block = struct.unpack_from('<H', fmt, 18)[0] if len(fmt) >= 20 else \
        (block_align - 4 * channels) * 8 // (4 * channels) + 1
    samples = []
    for at in range(0, len(data), block_align):
        samples.extend(decode_ima_block(data[at:at + block_align], channels, samples_per_block))
    return channels, rate, struct.pack('<%dh' % len(samples), *samples)


def to_mono(channels, pcm):
    """Averages the channels: SFML spatializes only mono sounds."""
    if channels == 1:
        return 1, pcm
    count = len(pcm) // 2
    samples = struct.unpack('<%dh' % count, pcm[:count * 2])
    mono = [sum(samples[i:i + channels]) // channels for i in range(0, count - channels + 1, channels)]
    return 1, struct.pack('<%dh' % len(mono), *mono)


def write_wav(path, channels, rate, pcm):
    with open(path, 'wb') as f:
        f.write(b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE')
        f.write(b'fmt ' + struct.pack('<IHHIIHH', 16, 1, channels, rate, rate * channels * 2, channels * 2, 16))
        f.write(b'data' + struct.pack('<I', len(pcm)) + pcm)


def text_values(node, out):
    if isinstance(node, dict):
        for value in node.values():
            text_values(value, out)
    elif isinstance(node, list):
        for value in node:
            text_values(value, out)
    elif isinstance(node, str):
        out.add(node)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--vox', required=True)
    parser.add_argument('--sounds', required=True)
    parser.add_argument('--level', required=True)
    parser.add_argument('--extra', nargs='*', default=[])
    args = parser.parse_args()

    pack = ET.parse(args.vox).getroot()
    by_uid = {}
    by_label = {}
    for sound in pack.iter('sound'):
        by_uid[sound.get('uid')] = sound.get('filename')
        by_label[sound.get('label')] = [sound.get('filename')]
    events = {}
    for event in pack.iter('event'):
        files = [by_uid[uid] for uid in (event.get('value') or '').split(',') if uid in by_uid]
        if files:
            events[event.get('label')] = files

    # Every file in the sound folder, by lower-case name (the pack names files without their folder)
    on_disk = {}
    for root, _, files in os.walk(args.sounds):
        for name in files:
            on_disk.setdefault(name.lower(), os.path.join(root, name))

    with open(args.level, encoding='utf-8') as f:
        level = json.load(f)
    wanted = set(args.extra)
    text_values(level.get('rules', {}), wanted)
    text_values(level.get('destructibles', []), wanted)

    level_dir = os.path.dirname(os.path.abspath(args.level))
    out_dir = os.path.join(level_dir, 'sounds')
    os.makedirs(out_dir, exist_ok=True)
    bank = {}
    missing = []
    for name in sorted(wanted):
        files = events.get(name) or by_label.get(name)
        if not files and (name + '.wav').lower() in on_disk:
            files = [name + '.wav']
        if not files:
            if name.startswith(('evt_', 'sfx_')):
                missing.append(name)
            continue
        written = []
        for file in files:
            source = on_disk.get(file.lower())
            if not source:
                missing.append(file)
                continue
            target = os.path.join(out_dir, file.lower())
            if not os.path.exists(target):
                channels, rate, pcm = to_pcm(source)
                channels, pcm = to_mono(channels, pcm)
                write_wav(target, channels, rate, pcm)
            written.append('sounds/' + file.lower())
        if written:
            bank[name] = written
    with open(os.path.join(level_dir, 'audio.json'), 'w', encoding='utf-8') as f:
        json.dump(bank, f, indent=1, sort_keys=True)
    print('audio: %d names, %d files' % (len(bank), len(os.listdir(out_dir))))
    for name in missing:
        print('  missing: %s' % name)
    return 0


if __name__ == '__main__':
    sys.exit(main())
