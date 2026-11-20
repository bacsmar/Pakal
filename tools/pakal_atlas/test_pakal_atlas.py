"""Tests of pakal_atlas (python -m pytest tools/pakal_atlas)."""
import json
import sys
from pathlib import Path

import numpy as np
import pytest
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).parent))
import pakal_atlas as pa  # noqa: E402


def blob(size=(60, 50), margin=0):
    """A figure: a head and a body and a thin arm out to the side, over a transparent picture."""
    image = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(image)
    d.ellipse([20, 4 + margin, 36, 20 + margin], fill=(200, 50, 50, 255))
    d.rectangle([22, 20 + margin, 34, 44], fill=(50, 50, 200, 255))
    d.line([34, 26, 56, 18], fill=(250, 250, 50, 255), width=2)
    return image


def covered(vertices, triangles, shape):
    """The pixels the triangles cover."""
    canvas = Image.new("L", (shape[1], shape[0]), 0)
    d = ImageDraw.Draw(canvas)
    for i in range(0, len(triangles), 3):
        d.polygon([tuple(vertices[triangles[i + k]]) for k in range(3)], fill=255)
    return np.asarray(canvas) > 0


def test_mesh_covers_every_opaque_pixel():
    options = pa.Options()
    image, mask, _ = pa.trim(pa.Frame("a", blob()), options)
    vertices, triangles = pa.build_mesh(mask, options)
    assert vertices is not None, "a figure with a thin arm is worth a mesh"
    inside = covered(vertices, triangles, mask.shape)
    assert not (mask & ~inside).any(), "a pixel that is drawn is outside the mesh"
    assert inside.sum() < 0.9 * mask.size, "the mesh is not tighter than the rectangle"


def test_triangles_add_up_to_the_polygon():
    loop = [(0, 0), (10, 0), (10, 4), (4, 4), (4, 10), (0, 10)]      # an L
    triangles = pa.triangulate(loop)
    assert triangles is not None and len(triangles) == len(loop) - 2
    area = sum(abs(pa.signed_area([loop[a], loop[b], loop[c]])) for a, b, c in triangles)
    assert area == pytest.approx(abs(pa.signed_area(loop)))


def test_simplified_outline_stays_near():
    mask = np.asarray(blob().getchannel("A")) > 8
    loop = pa.outline_loops(mask)[0]
    simple = pa.simplify_closed(loop, 1.2)
    assert len(simple) < len(loop) / 3
    # every point of the original loop is near the simplified one
    for p in loop[::5]:
        d = min(_distance(p, simple[i], simple[(i + 1) % len(simple)]) for i in range(len(simple)))
        assert d <= 1.3


def _distance(p, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length2 = dx * dx + dy * dy
    t = 0.0 if length2 == 0 else max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / length2))
    return ((p[0] - (ax + t * dx)) ** 2 + (p[1] - (ay + t * dy)) ** 2) ** 0.5


def test_islands_are_dropped():
    image = blob()
    ImageDraw.Draw(image).rectangle([2, 40, 5, 43], fill=(0, 255, 0, 255))     # a speck of another figure
    kept = pa.trim(pa.Frame("a", image), pa.Options(min_island=0))
    dropped = pa.trim(pa.Frame("a", image), pa.Options(min_island=30))
    assert kept[1].shape[1] > dropped[1].shape[1], "the speck is no longer in the trimmed picture"
    assert kept[2][0] < 5 and dropped[2][0] > 10, "the trimmed picture starts at the figure, not at the speck"


def test_a_full_rectangle_stays_a_rectangle():
    image = Image.new("RGBA", (20, 20), (255, 0, 0, 255))
    _, mask, _ = pa.trim(pa.Frame("a", image), pa.Options())
    assert pa.build_mesh(mask, pa.Options()) == (None, None)


def test_pack_places_without_overlap_and_keeps_the_pixels():
    frames = [pa.Frame(f"f{i}", blob((60 + 7 * i, 50)), (3, 2), (80, 70)) for i in range(6)]
    animations = [pa.Animation("walk", [f.name for f in frames], [50] * 6, True)]
    atlas, data = pa.pack(frames, animations, pa.Options())
    rects = {f["rect"][0:2] + f["rect"][2:4] and tuple(f["rect"]) for f in data["frames"].values()}
    rects = list(rects)
    for i, a in enumerate(rects):
        assert a[0] + a[2] <= atlas.width and a[1] + a[3] <= atlas.height
        for b in rects[i + 1:]:
            assert a[0] + a[2] <= b[0] or b[0] + b[2] <= a[0] or a[1] + a[3] <= b[1] or b[1] + b[3] <= a[1], "two frames overlap"
    assert data["animations"]["walk"]["loop"] is True and len(data["animations"]["walk"]["frames"]) == 6
    # the offset is where the trimmed picture sits in the source frame: the pixels in the atlas are the ones of the picture
    name = "f2"
    f = data["frames"][name]
    x, y, w, h = f["rect"]
    original = frames[2].image
    cut = np.asarray(atlas.crop((x, y, x + w, y + h)))
    ox, oy = f["offset"][0] - 3, f["offset"][1] - 2
    expected = np.asarray(original.crop((ox, oy, ox + w, oy + h)))
    assert (cut[..., 3] > 8).sum() == (expected[..., 3] > 8).sum()


def test_equal_pictures_are_stored_once():
    image = blob()
    frames = [pa.Frame("a", image), pa.Frame("b", image.copy()), pa.Frame("c", blob(margin=2))]
    _, data = pa.pack(frames, [], pa.Options())
    assert data["frames"]["a"]["rect"] == data["frames"]["b"]["rect"]
    assert data["frames"]["a"]["rect"] != data["frames"]["c"]["rect"]


def test_an_empty_frame_is_a_pixel():
    frames = [pa.Frame("blank", Image.new("RGBA", (10, 10), (0, 0, 0, 0))), pa.Frame("a", blob())]
    _, data = pa.pack(frames, [], pa.Options())
    assert data["frames"]["blank"]["rect"][2:] == [1, 1]
    assert data["frames"]["blank"]["source"] == [10, 10]


def test_a_folder_is_packed_and_written(tmp_path):
    for anim, n in (("idle", 3), ("hit", 2)):
        (tmp_path / "in" / anim).mkdir(parents=True)
        for i in range(n):
            blob().save(tmp_path / "in" / anim / f"{i:04d}.png")
    (tmp_path / "in" / "animations.json").write_text(json.dumps({"idle": {"fps": 10, "loop": True}, "hit": {"ms": [30, 90]}}))
    pa.main(["pack", str(tmp_path / "in"), "-o", str(tmp_path / "out" / "set")])
    data = json.loads((tmp_path / "out" / "set.json").read_text())
    assert data["format"] == "pakal-atlas" and data["image"] == "set.png" and (tmp_path / "out" / "set.png").exists()
    assert data["animations"]["idle"]["ms"] == [100, 100, 100] and data["animations"]["idle"]["loop"] is True
    assert data["animations"]["hit"]["ms"] == [30, 90]
    pa.main(["preview", str(tmp_path / "out" / "set.json"), str(tmp_path / "prev.png")])
    assert (tmp_path / "prev.png").exists()


def test_a_texturepacker_sheet_is_imported(tmp_path):
    sheet = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    ImageDraw.Draw(sheet).rectangle([2, 2, 21, 29], fill=(255, 0, 0, 255))
    sheet.save(tmp_path / "s.png")
    (tmp_path / "s.xml").write_text(
        '<SpriteSheetAnimation texture="s.png" width="64" height="32" scale="100:2"><animation name="stand" duration="200" looped="true">'
        '<frame name="a" oX="4" oY="1" x="2" y="2" w="20" h="28" pX="15" pY="16"/></animation></SpriteSheetAnimation>')
    frames, animations, ppm = pa.import_texturepacker(tmp_path / "s.xml", tmp_path)
    assert ppm == 50 and animations[0].loop and animations[0].ms == [200]
    assert frames[0].source == (30, 32) and frames[0].offset == (4, 1)


# ---- meshes cut by hand

def packed_blob():
    options = pa.Options()
    frames = [pa.Frame("walk/0", blob()), pa.Frame("walk/1", blob(margin=2))]
    return pa.pack(frames, [pa.Animation("walk", ["walk/0", "walk/1"])], options)


def test_pack_keeps_the_outline_of_each_mesh():
    _, data = packed_blob()
    for f in data["frames"].values():
        assert "outline" in f
        assert sum(len(loop) for loop in f["outline"]) == len(f["vertices"])
        assert pa.outline_of(f["vertices"], f["triangles"]) == [[tuple(p) for p in loop] for loop in f["outline"]]


def test_loop_is_simple_refuses_a_bow_tie_and_a_fold():
    assert pa.loop_is_simple([(0, 0), (10, 0), (10, 10), (0, 10)])
    assert not pa.loop_is_simple([(0, 0), (10, 10), (10, 0), (0, 10)])       # the edges cross
    assert not pa.loop_is_simple([(0, 0), (10, 0), (5, 0), (5, 10)])         # an edge folded over its neighbour


def test_set_frame_outline_cuts_the_new_loop_and_flags_it():
    _, data = packed_blob()
    f = data["frames"]["walk/0"]
    assert pa.set_frame_outline(f, [[(0, 0), (20, 0), (20, 10), (0, 10)]])
    assert f["edited"] and len(f["vertices"]) == 4 and len(f["triangles"]) == 6
    before = json.dumps(f)
    assert not pa.set_frame_outline(f, [[(0, 0), (10, 10), (10, 0), (0, 10)]])   # a bow tie: nothing changes
    assert json.dumps(f) == before
    assert pa.set_frame_outline(f, [])                                             # a rectangle
    assert "vertices" not in f and f["edited"]


def test_edits_survive_packing_again(tmp_path):
    atlas, data = packed_blob()
    f = data["frames"]["walk/1"]
    ox, oy = f["offset"]
    pa.set_frame_outline(f, [[(1, 1), (15, 1), (15, 12), (1, 12)]])
    sidecar = pa.edits_of(data)
    assert list(sidecar["frames"]) == ["walk/1"]
    assert sidecar["frames"]["walk/1"][0][0] == [1 + ox, 1 + oy]            # in the pixels of the source frame
    pa.edits_path(tmp_path / "a.json").write_text(json.dumps(sidecar), encoding="utf-8")
    atlas2, data2 = packed_blob()                                            # converting again...
    pa.write(atlas2, data2, tmp_path / "a")                                  # ...picks the sidecar up
    written = json.loads((tmp_path / "a.json").read_text(encoding="utf-8"))
    assert written["frames"]["walk/1"]["edited"] and len(written["frames"]["walk/1"]["vertices"]) == 4
    assert "edited" not in written["frames"]["walk/0"]


def test_a_frame_with_moved_pixels_is_not_in_the_sidecar():
    _, data = packed_blob()
    pa.set_frame_outline(data["frames"]["walk/0"], [[(1, 1), (15, 1), (15, 12), (1, 12)]])
    pa.set_frame_outline(data["frames"]["walk/1"], [[(1, 1), (15, 1), (15, 12), (1, 12)]])
    data["frames"]["walk/1"]["pixelsEdited"] = True      # (its pixels were moved: a new pack would not have them)
    assert list(pa.edits_of(data)["frames"]) == ["walk/0"]
