"""Seals a game's assets into one pack that the engine reads as a zip (ZipSourceMiniz with a key): each file is deflated and mixed
with a stream made from the key and its name, so that it cannot be opened by hand. Not strong encryption: the key lives in the game.

    python pakal_pack.py <folder> <out.pak> <key> [--prefix Assets/]

Every file under <folder> goes in under <prefix><its path in the folder>, lower case, with '/' (the names the engine asks for).
The game mounts it with ResourceMgr.add_source<ZipSource>(path, true, key)."""
import os
import struct
import sys
import zipfile
import zlib

MASK = (1 << 64) - 1


MULTIPLIER = 2685821657736338717
LARGE = 1 << 20   # bytes from which numpy makes the stream (when it is installed)

try:
    import numpy
except ImportError:
    numpy = None


def first_state(key, name):
    h = 1469598103934665603
    for c in (key + "|" + name).encode("utf-8"):
        h ^= c
        h = (h * 1099511628211) & MASK
    return h or 0x9E3779B97F4A7C15


def step(s):
    s ^= s >> 12
    s ^= (s << 25) & MASK
    s ^= s >> 27
    return s


def stream_words(s, count):
    """The stream's 64-bit words: before each one the state takes a step."""
    words = []
    for _ in range(count):
        s = step(s)
        words.append((s * MULTIPLIER) & MASK)
    return words


# A step is linear over the bits (shifts and xors): a 64x64 bit matrix, kept as the image of each bit. Its powers jump a state
# any number of steps ahead at once, so that many stretches of the stream are made side by side.
def apply(matrix, s):
    out = 0
    b = 0
    while s:
        if s & 1:
            out ^= matrix[b]
        s >>= 1
        b += 1
    return out


def power(n):
    result = [1 << b for b in range(64)]
    base = [step(1 << b) for b in range(64)]
    while n:
        if n & 1:
            result = [apply(base, v) for v in result]
        base = [apply(base, v) for v in base]
        n >>= 1
    return result


def stream_numpy(s, count):
    """The same words with numpy: 'lanes' stretches of the stream, each started where the one before ends, stepped together."""
    lanes = 4096
    length = -(-count // lanes)
    jump = power(length)
    starts = []
    for _ in range(lanes):
        starts.append(s)
        s = apply(jump, s)
    state = numpy.array(starts, dtype=numpy.uint64)
    words = numpy.empty((length, lanes), dtype=numpy.uint64)
    multiplier = numpy.uint64(MULTIPLIER)
    a, b, c = numpy.uint64(12), numpy.uint64(25), numpy.uint64(27)
    for i in range(length):
        state ^= state >> a
        state ^= state << b
        state ^= state >> c
        words[i] = state * multiplier
    return words.T.reshape(-1)[:count]


def stream_xor(data, key, name):
    """The data mixed with the stream of its key and name (the same call unmixes it). numpy makes a large file's stream; the
    stream is the same either way."""
    s = first_state(key, name)
    count = (len(data) + 7) // 8
    if numpy is not None and len(data) >= LARGE:
        stream = stream_numpy(s, count).astype("<u8").view(numpy.uint8)[: len(data)]
        return (numpy.frombuffer(data, dtype=numpy.uint8) ^ stream).tobytes()
    stream = b"".join(w.to_bytes(8, "little") for w in stream_words(s, count))[: len(data)]
    return (int.from_bytes(data, "little") ^ int.from_bytes(stream, "little")).to_bytes(len(data), "little")


def seal(data, key, name):
    return b"PKP1" + struct.pack("<I", len(data)) + stream_xor(zlib.compress(data, 9), key, name)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    prefix = sys.argv[sys.argv.index("--prefix") + 1] if "--prefix" in sys.argv else "Assets/"
    if "--prefix" in sys.argv:
        args.remove(prefix)
    folder, out, key = args
    count = 0
    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as z:
        for d, _, files in os.walk(folder):
            for f in sorted(files):
                full = os.path.join(d, f)
                name = (prefix + os.path.relpath(full, folder)).replace("\\", "/").lower()
                with open(full, "rb") as src:
                    z.writestr(name, seal(src.read(), key, name))
                count += 1
    print(f"{out}: {count} files, {os.path.getsize(out) / 1e6:.0f} MB")


if __name__ == "__main__":
    main()
