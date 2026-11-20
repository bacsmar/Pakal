# Sprite atlases with polygon meshes

`tools/pakal_atlas` packs sprite frames into a texture atlas and cuts a **mesh** for each frame: the outline of what is opaque,
simplified and divided into triangles. The engine (`Pakal::SpriteAtlas`, `SpriteComponent2D::set_mesh`) draws those triangles
instead of a quad, so

- the transparent corners of a sprite cost nothing to fill (a fighter's frame is mostly air: about 30 % of the area of the
  untrimmed frames is drawn),
- the faint haze and the specks of other figures that a bad cut leaves in the margin of a picture are not drawn at all (the
  alpha threshold and `--min-island` throw them out),
- the frames are trimmed and equal frames are stored once, so the atlas is small.

## The tool

```
python tools/pakal_atlas/pakal_atlas.py pack <folder> -o <out/name> [--ppm 100] [--alpha 8] [--tolerance 1.2] [--min-island 0]
                                                          [--padding 2] [--max-size 4096] [--no-pot] [--no-polygon]
python tools/pakal_atlas/pakal_atlas.py info <name.json>
python tools/pakal_atlas/pakal_atlas.py preview <name.json> <preview.png>     # the triangles drawn over the atlas
```

Needs Pillow, numpy and scipy. The folder has one sub-folder for each animation (`idle/0001.png, idle/0002.png`, in name order);
a PNG in the folder itself is an animation of one frame. An optional `animations.json` in the folder gives each animation's timing
and loop: `{"idle": {"fps": 10, "loop": true}, "hit": {"ms": [60, 60, 120]}}`.

| Option | |
|---|---|
| `--alpha` | A pixel is drawn if its alpha is over this (default 8) |
| `--tolerance` | How far the simplified outline may be from the real one, in pixels (default 1.2). The mesh is made over the mask grown by a pixel, so it still covers every drawn pixel |
| `--min-island` | Opaque islands smaller than this many pixels are dropped from the picture |
| `--padding` | Transparent pixels around each frame in the atlas; the picture's edge is repeated in the first one so a scaled sprite shows no seam |
| `--no-polygon` | Trimmed rectangles instead of meshes |

A frame whose mesh would cover more than 92 % of its rectangle stays a rectangle (two triangles): a mesh is not worth it.
Holes in a figure are filled (they are drawn with the picture's own alpha). A frame with nothing opaque becomes one transparent
pixel, so an animation can blink.

As a library: `pack(frames, animations, Options(...))` returns the atlas image and its JSON; `import_texturepacker(xml, folder)`
reads the sheets of TexturePacker's "SpriteSheetAnimation" XML (the old Pakal's exporter) and `frames_from_strip(png, w, h, n, name)`
reads a strip of equal cells, to convert what a game already has. The tests: `python -m pytest tools/pakal_atlas`.

## Editing the meshes by hand

```
python tools/pakal_atlas/pakal_atlas_editor.py [name.json]
```

A Tk window (it needs nothing beyond what `pakal_atlas` already does) with two views, switched by the button or Tab. The list on the
left holds the frames (type to filter; `*` marks the ones cut by hand, `#` the ones whose pixels were moved).

- **Frame view**: one frame over its picture with its mesh. Drag a point to move it, double click an edge to add one, right click a point
  to delete it, drag inside a piece to move it inside the frame, Delete removes the selected piece (a frame keeps at least one).
- **Atlas view**: the whole atlas with the rectangle and mesh of every frame. Click selects the frame and the piece under the pointer;
  drag a piece to move it, **pixels and mesh**, into the rectangle of any frame it fits in (this is how pieces of one figure that a
  bad cut left in another frame are put back where they belong); Ctrl + drag moves a whole frame's rectangle to a free place of the
  atlas (frames that share a picture move together).

**Re-cut** cuts the frame again from its pixels with the alpha, tolerance and island size of the toolbar, **Rectangle** takes the mesh
away (the frame is drawn as a quad). A point cannot be moved to where its loop would cross itself. Wheel zooms, middle drag pans,
Ctrl+Z / Ctrl+Y undo and redo (pieces and pixels too), Ctrl+S saves, Up / Down change frame.

Saving writes `name.json`, `name.png` when pixels were moved (the first time the old one is kept as `name.orig.png`) and **`name.edits.json`**,
the sidecar: the loops of the frames cut by hand, in the pixels of the source frame. `write()` (so any converter that packs the atlas again)
puts the sidecar's meshes on the new atlas, so that work is not lost when the sheets are converted again; a frame that no longer exists is
reported as a warning. **Moved pixels are not kept by a new pack**: those frames (`pixelsEdited`) are left out of the sidecar, and converting
again starts them over, so fix the source sheets too if the conversion will run again.

## The format (`pakal-atlas`, version 1)

```json
{
  "format": "pakal-atlas", "version": 1,
  "image": "fighter.png", "size": [2048, 2048], "pixelsPerMeter": 142.35,
  "frames": {
    "kick/0": {
      "rect": [201, 475, 105, 235],          the trimmed picture in the atlas (pixels, from its top left)
      "offset": [11, 1],                     where it sits in the untrimmed frame (from its top left)
      "source": [211, 237],                  the untrimmed frame's size
      "pivot": [0.5, 0.5],                   in the untrimmed frame; the sprite is drawn centred on it (default 0.5, 0.5)
      "vertices": [[0, 0], [105, 0], ...],   the outline, pixels of the trimmed picture from its top left (none: a rectangle)
      "triangles": [0, 1, 2, ...],           indices into vertices, three for each triangle
      "outline": [[[0, 0], ...], ...],       optional, for the tools: the loops that were cut (the engine ignores it)
      "edited": true                         optional: the mesh was cut by hand (the editor)
    }
  },
  "animations": {
    "kick": { "frames": ["kick/0", "kick/1"], "ms": [40, 40], "loop": false }
  }
}
```

The sprite's size is the **untrimmed** frame, `source / pixelsPerMeter` meters, so a sprite's centre is the frame's pivot
whatever was trimmed off the frame: swapping frames of different trims does not make the figure jump.

## In the engine

```cpp
Pakal::SpriteAtlas atlas;
atlas.load_json(text, &error);                          // refuses another format or version, a frame outside the atlas,
                                                        // a triangle past its vertices, an animation of a frame that is not there
sprite->set_texture(folder + atlas.image());            // one sprite for a character: the atlas's picture

const Pakal::AtlasAnimation* walk = atlas.animation("walk");
const Pakal::AtlasFrame* frame = atlas.frame_at(*walk, elapsedMs);   // a loop goes round, one that plays once holds its last
atlas.show(*sprite, *frame);                            // the mesh, and the size of the untrimmed frame in meters
sprite->set_flip(left, false);                          // a flipped mesh is mirrored
```

`SpriteComponent2D::set_mesh(const SpriteMesh*)` is what `show` calls: positions in the sprite's unit square (y up, as the quad),
texture coordinates in pixels of the texture, indices of the triangles; null goes back to the quad. In the bgfx backend a frame
change rebuilds the sprite's vertex and index buffers (a few dozen vertices).

`SpriteStrip` (`source/gameplay/SpriteStrip.h`) is the other way a game can keep an animation's timing (frames, milliseconds, loop)
for its simulation, apart from the picture.

## Not done

- Holes are not cut out of a mesh (they are filled).
- No rotated frames, no trim-only-in-one-axis, no several atlases for one set of animations (a set that does not fit in
  `--max-size` is an error: split it into folders).
- The mesh is rebuilt on every frame change; a cache of the GPU buffers by frame would avoid it for sprites that show the same
  frames again and again.
