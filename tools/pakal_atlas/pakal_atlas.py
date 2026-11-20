#!/usr/bin/env python3
"""pakal_atlas: packs sprite frames into texture atlases with polygon meshes, for Pakal's SpriteAtlas.

A frame is trimmed to what is drawn in it, and (with --polygon, the default) the trimmed picture gets a mesh: the outline of what
is opaque, simplified and cut into triangles.  The engine draws those triangles instead of a quad, so the transparent
corners of a sprite cost nothing to fill and a bit of another figure left in the margin of a cut is not drawn at all.

  pakal_atlas.py pack  <folder> -o <out/name> [options]    a folder of frames (one sub-folder per animation) -> name.png + name.json
  pakal_atlas.py info  <name.json>                          what an atlas holds and what the polygons save
  pakal_atlas.py preview <name.json> <preview.png>          the atlas with the polygons drawn over it

The folder: `idle/0001.png, idle/0002.png, ...` is the animation `idle` of those frames in name order; a PNG in the folder itself
is an animation of one frame.  An optional `animations.json` in the folder gives each animation's timing and loop:
`{"idle": {"fps": 10, "loop": true}, "hit": {"ms": [60, 60, 120]}}`.

As a library (what a converter of an existing game's sheets uses): `pack(frames, animations, Options(...))` -> `(PIL image, dict)`; see
`Frame` and `Animation`.  The format of the JSON is in docs/ATLAS.md.

Needs Pillow, numpy and scipy.
"""
import argparse
import hashlib
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage

FORMAT = "pakal-atlas"
VERSION = 1


# ---------------------------------------------------------------------------------------------------------------------
# the input

@dataclass
class Frame:
    """One picture.  'image' is RGBA; 'offset' is where its top-left corner sits in the untrimmed frame of size 'source'
    (what a sheet that was trimmed before gives); a whole picture has offset (0, 0) and source its own size."""
    name: str
    image: Image.Image
    offset: tuple = (0, 0)
    source: tuple = None
    pivot: tuple = (0.5, 0.5)     # in the source frame, 0..1 (the sprite is drawn centred on it)

    def __post_init__(self):
        if self.source is None:
            self.source = self.image.size


@dataclass
class Animation:
    name: str
    frames: list                  # frame names
    ms: list = None               # of each frame
    loop: bool = False


@dataclass
class Options:
    polygon: bool = True
    alpha: int = 8                # a pixel is drawn when its alpha is over this
    tolerance: float = 1.2        # of the simplified outline (pixels)
    min_island: int = 0           # opaque islands smaller than this (pixels) are dropped: the specks and bits of other figures left by a cut
    padding: int = 2              # transparent pixels around each frame in the atlas (the picture's edge is repeated in the first)
    max_size: int = 4096
    power_of_two: bool = True
    pixels_per_meter: float = 100.0
    keep_rect_over: float = 0.92  # a mesh that covers more than this share of its rectangle is not worth it: a rectangle
    default_ms: int = 100


# ---------------------------------------------------------------------------------------------------------------------
# trimming and the mesh

def opaque_mask(image, alpha, min_island=0):
    mask = np.asarray(image.getchannel("A")) > alpha
    if min_island > 0 and mask.any():
        labels, count = ndimage.label(mask, structure=np.ones((3, 3)))
        if count > 1:
            sizes = ndimage.sum(mask, labels, index=range(1, count + 1))
            keep = np.zeros(count + 1, dtype=bool)
            keep[1:] = np.asarray(sizes) >= min_island
            if keep.any():                     # (a picture of only specks keeps them)
                mask = keep[labels]
    return mask


def trim(frame, options):
    """The frame cut to its mask (a pixel of margin kept for the mesh): (frame image, mask, offset in the source) or None if empty."""
    mask = opaque_mask(frame.image, options.alpha, options.min_island)
    if not mask.any():
        return None
    ys, xs = np.where(mask)
    margin = 1 if options.polygon else 0
    x0, y0 = max(0, xs.min() - margin), max(0, ys.min() - margin)
    x1, y1 = min(mask.shape[1], xs.max() + 1 + margin), min(mask.shape[0], ys.max() + 1 + margin)
    image = frame.image.crop((x0, y0, x1, y1))
    mask = mask[y0:y1, x0:x1]
    # what the mask took out (the specks) is no longer in the picture
    rgba = np.array(image)
    rgba[~mask, 3] = 0
    return Image.fromarray(rgba), mask, (frame.offset[0] + x0, frame.offset[1] + y0)


def outline_loops(mask):
    """The outer outlines of the opaque areas (holes filled) as loops of pixel corners (x, y), the picture's top-left (0, 0)."""
    filled = ndimage.binary_fill_holes(mask)
    h, w = filled.shape
    padded = np.zeros((h + 2, w + 2), dtype=bool)
    padded[1:-1, 1:-1] = filled
    edges = {}   # start corner -> [end corners]; the inside is on the right of an edge (clockwise on the screen)
    ys, xs = np.where(filled)
    for y, x in zip(ys.tolist(), xs.tolist()):
        py, px = y + 1, x + 1
        if not padded[py - 1, px]:
            edges.setdefault((x, y), []).append((x + 1, y))
        if not padded[py, px + 1]:
            edges.setdefault((x + 1, y), []).append((x + 1, y + 1))
        if not padded[py + 1, px]:
            edges.setdefault((x + 1, y + 1), []).append((x, y + 1))
        if not padded[py, px - 1]:
            edges.setdefault((x, y + 1), []).append((x, y))
    loops = []
    while edges:
        start = next(iter(edges))
        loop, here, heading = [start], start, None
        while True:
            options = edges.get(here)
            if not options:
                break
            if len(options) == 1 or heading is None:
                nxt = options[0]
            else:
                # a corner where two parts touch: turn right (keep them apart)
                def turn(candidate):
                    d = (candidate[0] - here[0], candidate[1] - here[1])
                    return heading[0] * d[1] - heading[1] * d[0]
                nxt = max(options, key=turn)
            options.remove(nxt)
            if not options:
                del edges[here]
            heading = (nxt[0] - here[0], nxt[1] - here[1])
            here = nxt
            if here == start:
                break
            loop.append(here)
        if len(loop) >= 3:
            loops.append(loop)
    # holes that a touching corner makes have the other way round
    areas = [signed_area(loop) for loop in loops]
    if not areas:
        return []
    sign = 1 if max(areas, key=abs) > 0 else -1
    return [loop for loop, area in zip(loops, areas) if area * sign > 0]


def signed_area(points):
    total = 0.0
    for i in range(len(points)):
        x0, y0 = points[i]
        x1, y1 = points[(i + 1) % len(points)]
        total += x0 * y1 - x1 * y0
    return total / 2.0


def simplify_closed(points, tolerance):
    """Douglas-Peucker on a closed loop."""
    if len(points) <= 4:
        return list(points)
    # split at the point farthest from the first, so both halves have two ends that stay
    far = max(range(len(points)), key=lambda i: (points[i][0] - points[0][0]) ** 2 + (points[i][1] - points[0][1]) ** 2)
    first = simplify_open(points[0:far + 1], tolerance)
    second = simplify_open(points[far:] + [points[0]], tolerance)
    return first[:-1] + second[:-1]


def simplify_open(points, tolerance):
    if len(points) < 3:
        return list(points)
    keep = [False] * len(points)
    keep[0] = keep[-1] = True
    stack = [(0, len(points) - 1)]
    while stack:
        a, b = stack.pop()
        ax, ay = points[a]
        bx, by = points[b]
        dx, dy = bx - ax, by - ay
        length = math.hypot(dx, dy) or 1.0
        worst, index = 0.0, -1
        for i in range(a + 1, b):
            distance = abs(dx * (ay - points[i][1]) - (ax - points[i][0]) * dy) / length
            if distance > worst:
                worst, index = distance, i
        if worst > tolerance and index > 0:
            keep[index] = True
            stack += [(a, index), (index, b)]
    return [p for p, k in zip(points, keep) if k]


def triangulate(points):
    """Ear clipping of a simple polygon: a list of (i, j, k) of its points, or None if it is not one we can cut."""
    n = len(points)
    if n < 3:
        return None
    orientation = 1.0 if signed_area(points) > 0 else -1.0
    index = list(range(n))
    triangles = []

    def cross_p(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    def cross(a, b, c):
        return cross_p(points[a], points[b], points[c])

    def inside(p, a, b, c):
        d1, d2, d3 = cross_p(points[a], points[b], p), cross_p(points[b], points[c], p), cross_p(points[c], points[a], p)
        return (d1 * orientation >= 0) and (d2 * orientation >= 0) and (d3 * orientation >= 0)

    guard = 0
    while len(index) > 3 and guard < 10 * n * n:
        guard += 1
        clipped = False
        for k in range(len(index)):
            a, b, c = index[k - 1], index[k], index[(k + 1) % len(index)]
            if cross(a, b, c) * orientation <= 1e-9:
                continue                                 # a reflex or flat corner is not an ear
            if any(inside(points[o], a, b, c) and points[o] not in (points[a], points[b], points[c]) for o in index if o not in (a, b, c)):
                continue
            triangles.append((a, b, c))
            del index[k]
            clipped = True
            break
        if not clipped:
            # flat points (collinear) can hold the cutting back: drop one and go on
            for k in range(len(index)):
                a, b, c = index[k - 1], index[k], index[(k + 1) % len(index)]
                if abs(cross(a, b, c)) <= 1e-9:
                    del index[k]
                    clipped = True
                    break
        if not clipped:
            return None
    if len(index) == 3 and abs(cross(*index)) > 1e-9:
        triangles.append(tuple(index))
    return triangles


def mesh_loops(mask, options):
    """The outlines of the mask, simplified: a list of loops of (x, y) pixel corners."""
    safe = ndimage.binary_dilation(mask, iterations=1) if options.tolerance > 0 else mask
    return [[(float(x), float(y)) for x, y in simplify_closed(loop, options.tolerance)] for loop in outline_loops(safe)]


def mesh_from_loops(loops):
    """(vertices [[x, y]...], triangle indices) of loops, each cut on its own; (None, None) if one of them cannot be cut
    (a loop that crosses itself) or the mesh is empty."""
    vertices, indices = [], []
    for loop in loops:
        if len(loop) < 3 or not loop_is_simple(loop):
            return None, None
        triangles = triangulate(loop)
        if triangles is None:
            return None, None
        base = len(vertices)
        vertices += [[float(x), float(y)] for x, y in loop]
        for a, b, c in triangles:
            indices += [base + a, base + b, base + c]
    if not vertices or len(indices) % 3 or max(indices) > 65535:
        return None, None
    return vertices, indices


def loop_is_simple(loop):
    """True when no two edges of the loop cross or touch (neighbours share their corner and nothing else)."""
    n = len(loop)

    def side(p, q, r):
        v = (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
        return (v > 1e-9) - (v < -1e-9)

    def on_segment(p, q, r):
        return min(p[0], q[0]) - 1e-9 <= r[0] <= max(p[0], q[0]) + 1e-9 and min(p[1], q[1]) - 1e-9 <= r[1] <= max(p[1], q[1]) + 1e-9

    def meet(a, b, c, d):
        d1, d2, d3, d4 = side(a, b, c), side(a, b, d), side(c, d, a), side(c, d, b)
        if d1 != d2 and d3 != d4:
            return True
        return (d1 == 0 and on_segment(a, b, c)) or (d2 == 0 and on_segment(a, b, d)) or (d3 == 0 and on_segment(c, d, a)) or (d4 == 0 and on_segment(c, d, b))

    for i in range(n):
        a, b = loop[i], loop[(i + 1) % n]
        if a == b:
            return False
        for j in range(i + 1, n):
            c, d = loop[j], loop[(j + 1) % n]
            if j == i + 1 or (i == 0 and j == n - 1):
                # neighbours: only the shared corner may be common (an edge folded back over its neighbour is a touch)
                shared, p, q = (b, a, d) if j == i + 1 else (a, b, c)
                v1, v2 = (p[0] - shared[0], p[1] - shared[1]), (q[0] - shared[0], q[1] - shared[1])
                if v1[0] * v2[1] - v1[1] * v2[0] == 0 and v1[0] * v2[0] + v1[1] * v2[1] > 0:
                    return False
                continue
            if meet(a, b, c, d):
                return False
    return True


def build_mesh(mask, options):
    """(vertices [[x, y]...], triangle indices) of the mask, or (None, None) if a rectangle does it as well."""
    h, w = mask.shape
    if not options.polygon:
        return None, None
    loops = mesh_loops(mask, options)
    vertices, indices = mesh_from_loops(loops)
    if vertices is None:
        return None, None
    area = sum(abs(signed_area(loop)) for loop in loops)
    if area >= options.keep_rect_over * w * h:
        return None, None
    return vertices, indices


def outline_of(vertices, triangles):
    """The loops of a mesh that has no 'outline' (an atlas of an older run).  The vertices are listed loop after loop and each
    loop was cut on its own, so a loop is a run of vertices that the triangles do not leave."""
    loops, start, reach = [], 0, 0
    for v in range(len(vertices)):
        for i in range(0, len(triangles), 3):
            tri = triangles[i:i + 3]
            if min(tri) <= v:
                reach = max(reach, max(tri))
        if reach <= v:
            loops.append([tuple(p) for p in vertices[start:v + 1]])
            start = v + 1
    return loops


# ---------------------------------------------------------------------------------------------------------------------
# editing: the meshes cut by hand (pakal_atlas_editor.py)

EDITS_FORMAT = "pakal-atlas-edits"


def edits_path(json_path):
    """name.json -> name.edits.json"""
    json_path = Path(json_path)
    return json_path.with_name(json_path.stem + ".edits.json")


def set_frame_outline(frame, loops):
    """Gives an atlas frame (the dict of data['frames']) the mesh of these loops, in the pixels of its rectangle.  No loops: a
    plain rectangle.  Returns False (and changes nothing) when the loops cannot be cut."""
    if not loops:
        for key in ("vertices", "triangles", "outline"):
            frame.pop(key, None)
        frame["edited"] = True
        return True
    vertices, indices = mesh_from_loops(loops)
    if vertices is None:
        return False
    frame["vertices"], frame["triangles"] = vertices, indices
    frame["outline"] = [[[float(x), float(y)] for x, y in loop] for loop in loops]
    frame["edited"] = True
    return True


def frame_outline(frame):
    """The loops of a frame's mesh ([] for a rectangle), in the pixels of its rectangle."""
    if "outline" in frame:
        return [[(float(x), float(y)) for x, y in loop] for loop in frame["outline"]]
    if "vertices" in frame:
        return outline_of(frame["vertices"], frame["triangles"])
    return []


def edits_of(data):
    """The sidecar's content: the frames edited by hand, their loops in the pixels of the source frame (so a new cut of the
    rectangle, with other options, still finds them)."""
    out = {}
    for name, frame in data["frames"].items():
        if frame.get("edited") and not frame.get("pixelsEdited"):   # (moved pixels are not in a new pack: their mesh would not fit)
            ox, oy = frame["offset"]
            out[name] = [[[x + ox, y + oy] for x, y in loop] for loop in frame_outline(frame)]
    return {"format": EDITS_FORMAT, "version": 1, "frames": out}


def apply_edits(data, edits):
    """Puts the sidecar's meshes on a freshly packed atlas.  Returns the names it could not apply."""
    missed = []
    for name, loops in edits.get("frames", {}).items():
        frame = data["frames"].get(name)
        if frame is None:
            missed.append(name)
            continue
        ox, oy = frame["offset"]
        w, h = frame["rect"][2], frame["rect"][3]
        local = [[(min(max(x - ox, 0.0), w), min(max(y - oy, 0.0), h)) for x, y in loop] for loop in loops]
        if not set_frame_outline(frame, local):
            missed.append(name)
    return missed


# ---------------------------------------------------------------------------------------------------------------------
# packing (MaxRects, best short side fit)

class MaxRects:
    def __init__(self, width, height):
        self.free = [(0, 0, width, height)]

    def insert(self, w, h):
        best, score = None, None
        for (fx, fy, fw, fh) in self.free:
            if w <= fw and h <= fh:
                s = (min(fw - w, fh - h), max(fw - w, fh - h))
                if score is None or s < score:
                    best, score = (fx, fy), s
        if best is None:
            return None
        placed = (best[0], best[1], w, h)
        new_free = []
        for rect in self.free:
            new_free += self._split(rect, placed)
        # drop what another free rectangle contains
        self.free = [r for i, r in enumerate(new_free) if not any(i != j and self._contains(o, r) for j, o in enumerate(new_free))]
        return best

    @staticmethod
    def _contains(a, b):
        return a[0] <= b[0] and a[1] <= b[1] and a[0] + a[2] >= b[0] + b[2] and a[1] + a[3] >= b[1] + b[3]

    @staticmethod
    def _split(free, used):
        fx, fy, fw, fh = free
        ux, uy, uw, uh = used
        if ux >= fx + fw or ux + uw <= fx or uy >= fy + fh or uy + uh <= fy:
            return [free]
        out = []
        if ux > fx:
            out.append((fx, fy, ux - fx, fh))
        if ux + uw < fx + fw:
            out.append((ux + uw, fy, fx + fw - (ux + uw), fh))
        if uy > fy:
            out.append((fx, fy, fw, uy - fy))
        if uy + uh < fy + fh:
            out.append((fx, uy + uh, fw, fy + fh - (uy + uh)))
        return out


def pack_rects(sizes, options):
    """Places rectangles (w, h): (atlas width, height, [(x, y)]) in the order of 'sizes'."""
    total = sum(w * h for w, h in sizes) or 1
    widest = max((w for w, _ in sizes), default=1)
    tallest = max((h for _, h in sizes), default=1)

    def round_up(n):
        return 1 << (max(1, n) - 1).bit_length() if options.power_of_two else n

    side = max(round_up(int(math.sqrt(total * 1.12))), round_up(widest), round_up(tallest))
    candidates = []
    for w in (side, side * 2, side // 2):
        for h in (side, side * 2, side // 2):
            if w >= widest and h >= tallest and w * h >= total and w <= options.max_size and h <= options.max_size:
                candidates.append((w * h, w, h))
    if not options.power_of_two:
        # (sizes of any width and height: the doubles and halves of a power of two's side miss most of them; widths from the widest
        # frame to the largest, each with the least height its area needs and taller, in steps of 64)
        for w in sorted({max(widest, min(options.max_size, v)) for v in (side, int(side * 1.25), int(side * 1.5), side * 2, 1024, 2048, options.max_size)}):
            for h in range(max(tallest, int(total / w) // 64 * 64), options.max_size + 1, 64):
                if w * h >= total:
                    candidates.append((w * h, w, h))
    candidates.sort()
    order = sorted(range(len(sizes)), key=lambda i: -max(sizes[i]))
    for _, w, h in candidates:
        bin_ = MaxRects(w, h)
        positions = [None] * len(sizes)
        for i in order:
            spot = bin_.insert(*sizes[i])
            if spot is None:
                break
            positions[i] = spot
        else:
            return w, h, positions
    raise SystemExit(f"the frames do not fit in {options.max_size}x{options.max_size}: split them in several atlases (more folders) or lower --padding")


# ---------------------------------------------------------------------------------------------------------------------
# the atlas

def pack(frames, animations, options=None):
    """(atlas image, JSON dict).  Equal pictures are stored once."""
    options = options or Options()
    pad = options.padding
    entries = []      # one per distinct picture: dict(image, mask, vertices, indices, key)
    by_key = {}
    placed_frames = {}
    for frame in frames:
        cut = trim(frame, options)
        if cut is None:
            # nothing is drawn: a frame of one transparent pixel (a blink of an animation)
            image, mask, offset = Image.new("RGBA", (1, 1), (0, 0, 0, 0)), np.zeros((1, 1), dtype=bool), frame.offset
        else:
            image, mask, offset = cut
        key = hashlib.sha1(image.tobytes() + repr(image.size).encode()).hexdigest()
        if key not in by_key:
            vertices, indices = build_mesh(mask, options) if cut is not None else (None, None)
            by_key[key] = len(entries)
            entries.append({"image": image, "vertices": vertices, "indices": indices,
                            "outline": outline_of(vertices, indices) if vertices else None})
        placed_frames[frame.name] = (by_key[key], offset, frame)
    sizes = [(e["image"].width + 2 * pad, e["image"].height + 2 * pad) for e in entries]
    width, height, positions = pack_rects(sizes, options) if entries else (1, 1, [])
    atlas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    rects = []
    for entry, (x, y) in zip(entries, positions):
        image = entry["image"]
        if pad > 0:
            # the picture's edge repeated one pixel out, so a scaled sprite does not show a seam
            edge = np.pad(np.array(image), ((1, 1), (1, 1), (0, 0)), mode="edge")
            edge[0, :, 3] = edge[-1, :, 3] = 0
            edge[:, 0, 3] = edge[:, -1, 3] = 0
            atlas.paste(Image.fromarray(edge), (x + pad - 1, y + pad - 1))
        atlas.paste(image, (x + pad, y + pad))
        rects.append((x + pad, y + pad, image.width, image.height))
    data = {
        "format": FORMAT, "version": VERSION,
        "image": "", "size": [width, height],
        "pixelsPerMeter": options.pixels_per_meter,
        "frames": {}, "animations": {},
    }
    for name, (index, offset, frame) in placed_frames.items():
        x, y, w, h = rects[index]
        entry = entries[index]
        out = {"rect": [x, y, w, h], "offset": [int(offset[0]), int(offset[1])], "source": [int(frame.source[0]), int(frame.source[1])]}
        if tuple(frame.pivot) != (0.5, 0.5):
            out["pivot"] = [frame.pivot[0], frame.pivot[1]]
        if entry["vertices"]:
            out["vertices"] = entry["vertices"]
            out["triangles"] = entry["indices"]
            out["outline"] = [[list(p) for p in loop] for loop in entry["outline"]]
        data["frames"][name] = out
    for animation in animations:
        ms = animation.ms or [options.default_ms] * len(animation.frames)
        data["animations"][animation.name] = {"frames": list(animation.frames), "ms": [int(m) for m in ms], "loop": bool(animation.loop)}
    return atlas, data


def write(atlas, data, out_base):
    """name.png and name.json.  The meshes cut by hand, kept in name.edits.json (the editor's sidecar), are put on the atlas."""
    out_base = Path(out_base)
    out_base.parent.mkdir(parents=True, exist_ok=True)
    sidecar = edits_path(out_base.with_suffix(".json"))
    if sidecar.exists():
        missed = apply_edits(data, json.loads(sidecar.read_text(encoding="utf-8")))
        if missed:
            print(f"warning: {len(missed)} edited meshes of {sidecar.name} no longer apply: {', '.join(missed[:5])}", file=sys.stderr)
    data["image"] = out_base.with_suffix(".png").name
    atlas.save(out_base.with_suffix(".png"), optimize=True)
    out_base.with_suffix(".json").write_text(json.dumps(data, separators=(",", ":")), encoding="utf-8")


def stats(data):
    """What the polygons save: the area drawn against the area of the source frames and of the trimmed rectangles."""
    source = trimmed = drawn = 0.0
    meshed = 0
    for f in data["frames"].values():
        sw, sh = f["source"]
        w, h = f["rect"][2], f["rect"][3]
        source += sw * sh
        trimmed += w * h
        if "vertices" in f:
            meshed += 1
            v, t = f["vertices"], f["triangles"]
            for i in range(0, len(t), 3):
                a, b, c = v[t[i]], v[t[i + 1]], v[t[i + 2]]
                drawn += abs((b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1])) / 2
        else:
            drawn += w * h
    return {"frames": len(data["frames"]), "meshed": meshed, "source_px": source, "trimmed_px": trimmed, "drawn_px": drawn,
            "atlas": data["size"]}


# ---------------------------------------------------------------------------------------------------------------------
# frames from files

def frames_from_folder(folder, options):
    folder = Path(folder)
    timing = {}
    if (folder / "animations.json").exists():
        timing = json.loads((folder / "animations.json").read_text(encoding="utf-8"))
    frames, animations = [], []

    def add(animation, files):
        names = []
        for i, f in enumerate(files):
            name = f"{animation}/{i}" if len(files) > 1 else animation
            frames.append(Frame(name, Image.open(f).convert("RGBA")))
            names.append(name)
        t = timing.get(animation, {})
        if "ms" in t:
            ms = t["ms"] if isinstance(t["ms"], list) else [t["ms"]] * len(names)
        elif "fps" in t:
            ms = [round(1000 / t["fps"])] * len(names)
        else:
            ms = None
        animations.append(Animation(animation, names, ms, t.get("loop", False)))

    for sub in sorted(p for p in folder.iterdir() if p.is_dir()):
        files = sorted(sub.glob("*.png"))
        if files:
            add(sub.name, files)
    for f in sorted(folder.glob("*.png")):
        add(f.stem, [f])
    return frames, animations


def import_texturepacker(xml_path, texture_root, animation_ms_default=60):
    """The old Pakal's TexturePacker sheets (SpriteSheetAnimation XML) as frames and animations, each frame in its untrimmed
    frame of twice its pivot, as the sheet meant it.  Also the pixels per meter its 'scale' says ("242:1.70")."""
    import xml.etree.ElementTree as ET
    root = ET.fromstring(Path(xml_path).read_text(encoding="utf-8", errors="replace").strip())
    texture = Image.open(Path(texture_root) / root.get("texture")).convert("RGBA")
    scale = root.get("scale", "")
    ppm = None
    if ":" in scale:
        pixels, meters = scale.split(":")
        ppm = float(pixels) / float(meters)
    frames, animations = [], []
    for animation in root.findall("animation"):
        names = []
        for i, f in enumerate(animation.findall("frame")):
            a = f.attrib
            x, y, w, h = int(a["x"]), int(a["y"]), int(a["w"]), int(a["h"])
            image = texture.crop((x, y, x + (h if a.get("r") == "y" else w), y + (w if a.get("r") == "y" else h)))
            if a.get("r") == "y":
                image = image.rotate(90, expand=True)
            px, py = float(a["pX"]), float(a["pY"])
            name = f"{animation.get('name')}/{i}"
            frames.append(Frame(name, image, (int(a.get("oX", 0)), int(a.get("oY", 0))), (int(round(px * 2)), int(round(py * 2)))))
            names.append(name)
        duration = animation.get("duration")
        total = int(duration) if duration else animation_ms_default * len(names)
        ms = [max(1, round(total / max(1, len(names))))] * len(names)
        animations.append(Animation(animation.get("name"), names, ms, animation.get("looped") == "true"))
    return frames, animations, ppm


def frames_from_strip(png_path, frame_width, frame_height, count, name):
    """A strip of equal cells (frames side by side) as the frames of one animation."""
    image = Image.open(png_path).convert("RGBA")
    frames, names = [], []
    for i in range(count):
        frames.append(Frame(f"{name}/{i}", image.crop((i * frame_width, 0, (i + 1) * frame_width, frame_height))))
        names.append(f"{name}/{i}")
    return frames, names


# ---------------------------------------------------------------------------------------------------------------------
# preview and command line

def preview(json_path, out_png):
    json_path = Path(json_path)
    data = json.loads(json_path.read_text(encoding="utf-8"))
    image = Image.open(json_path.with_name(data["image"])).convert("RGBA")
    background = Image.new("RGBA", image.size, (60, 60, 70, 255))
    background.alpha_composite(image)
    draw = ImageDraw.Draw(background)
    for f in data["frames"].values():
        x, y, w, h = f["rect"]
        if "vertices" in f:
            v, t = f["vertices"], f["triangles"]
            for i in range(0, len(t), 3):
                tri = [(x + v[t[i + k]][0], y + v[t[i + k]][1]) for k in range(3)]
                draw.polygon(tri, outline=(255, 80, 80, 255))
        else:
            draw.rectangle([x, y, x + w - 1, y + h - 1], outline=(80, 255, 120, 255))
    background.convert("RGB").save(out_png)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("pack", help="pack a folder of frames")
    p.add_argument("folder")
    p.add_argument("-o", "--output", required=True, help="the atlas without extension (name -> name.png, name.json)")
    p.add_argument("--no-polygon", action="store_true", help="rectangles (trimmed) instead of meshes")
    p.add_argument("--alpha", type=int, default=Options.alpha)
    p.add_argument("--tolerance", type=float, default=Options.tolerance, help="of the outline, in pixels")
    p.add_argument("--min-island", type=int, default=Options.min_island, help="drop opaque islands smaller than this many pixels")
    p.add_argument("--padding", type=int, default=Options.padding)
    p.add_argument("--max-size", type=int, default=Options.max_size)
    p.add_argument("--no-pot", action="store_true", help="the atlas need not be a power of two")
    p.add_argument("--ppm", type=float, default=Options.pixels_per_meter, help="pixels to the meter")
    i = sub.add_parser("info", help="what an atlas holds")
    i.add_argument("json")
    v = sub.add_parser("preview", help="draw the meshes over the atlas")
    v.add_argument("json")
    v.add_argument("png")
    args = parser.parse_args(argv)

    if args.command == "pack":
        options = Options(polygon=not args.no_polygon, alpha=args.alpha, tolerance=args.tolerance, min_island=args.min_island,
                          padding=args.padding, max_size=args.max_size, power_of_two=not args.no_pot, pixels_per_meter=args.ppm)
        frames, animations = frames_from_folder(args.folder, options)
        atlas, data = pack(frames, animations, options)
        write(atlas, data, args.output)
        report(data)
    elif args.command == "info":
        report(json.loads(Path(args.json).read_text(encoding="utf-8")))
    else:
        preview(args.json, args.png)


def report(data):
    s = stats(data)
    print(f"{s['frames']} frames, {len(data['animations'])} animations, atlas {s['atlas'][0]}x{s['atlas'][1]}, {s['meshed']} with a mesh")
    if s["source_px"]:
        print(f"drawn: {100 * s['drawn_px'] / s['source_px']:.0f}% of the source frames' area ({100 * s['trimmed_px'] / s['source_px']:.0f}% if only trimmed)")


if __name__ == "__main__":
    main()
