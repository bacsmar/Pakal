#!/usr/bin/env python3
"""pakal_atlas_editor: edits by hand a pakal-atlas (docs/ATLAS.md): its meshes and where its pieces are.

  pakal_atlas_editor.py [name.json]

Two views (Tab or the button switches):

  Frame   one frame over its picture, with its mesh
            left drag on a point         moves it            double click on an edge     adds a point there
            right click on a point       deletes it          drag inside a piece         moves the piece inside the frame
            Delete                       deletes the selected piece (the frame keeps at least one)
  Atlas   the whole atlas: every frame's rectangle and mesh
            click                        selects the frame and the piece under the pointer
            drag a piece                 moves it, pixels and mesh, into any frame it fits in (a frame's rectangle is fixed)
            Ctrl + drag                  moves the whole frame (its rectangle) to a free place of the atlas
  Both    wheel / middle drag zooms / pans       Ctrl+Z / Ctrl+Y / Ctrl+S   undo / redo / save       Up / Down   other frame
          Re-cut   cuts the frame again from its pixels with the sliders (alpha, tolerance, islands); Rectangle   no mesh

A point cannot be moved to where its loop would cross itself (the move is undone).  Saving writes name.json, name.png when pixels
were moved (the first time the old one is kept as name.orig.png) and name.edits.json: the meshes cut by hand, which a new `pack`
puts back.  Moved pixels exist only in the saved atlas: converting the sheets again starts them over (the frame is flagged
"pixelsEdited" and left out of name.edits.json).

Needs Pillow, numpy, scipy and Tk (all with a standard Python).
"""
import copy
import json
import math
import sys
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageTk

sys.path.insert(0, str(Path(__file__).parent))
import pakal_atlas as pa  # noqa: E402

HANDLE = 5          # radius of a point, pixels on the screen
PICK = 9            # how near the pointer must be to take a point or an edge
MARGIN = 24         # screen pixels around the frame
HISTORY = 200
GAP = 2             # pixels kept between rectangles when one is moved


def distance_to_segment(p, a, b):
    """(distance, nearest point) from p to the segment ab."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length2 = dx * dx + dy * dy
    t = 0.0 if length2 == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / length2))
    q = (a[0] + t * dx, a[1] + t * dy)
    return math.hypot(p[0] - q[0], p[1] - q[1]), q


def point_in_loop(p, loop):
    inside = False
    n = len(loop)
    for i in range(n):
        (x0, y0), (x1, y1) = loop[i], loop[(i + 1) % n]
        if (y0 > p[1]) != (y1 > p[1]) and p[0] < (x1 - x0) * (p[1] - y0) / (y1 - y0) + x0:
            inside = not inside
    return inside


def checker(size, cell=8):
    w, h = size
    y, x = np.mgrid[0:h, 0:w]
    dark = ((x // cell + y // cell) % 2).astype(bool)
    board = np.empty((h, w, 4), dtype=np.uint8)
    board[...] = (70, 70, 78, 255)
    board[dark] = (90, 90, 98, 255)
    return Image.fromarray(board)


def loop_box(loop):
    xs, ys = [p[0] for p in loop], [p[1] for p in loop]
    return min(xs), min(ys), max(xs), max(ys)


class Editor:
    def __init__(self, root, path=None):
        self.root = root
        self.path = None
        self.data = None
        self.atlas = None
        self.name = None
        self.loops = []          # of the frame shown, in the pixels of its rectangle
        self.sel = None          # the selected piece (index in loops)
        self.mode = "frame"
        self.views = {"frame": [4.0, [MARGIN, MARGIN]], "atlas": [0.5, [MARGIN, MARGIN]]}
        self.undo, self.redo = [], []
        self.dirty = False
        self.dirty_pixels = False
        self.version = 0         # of the atlas's pixels (a picture on the canvas is made again when it changes)
        self.drag = None         # what the left button is doing
        self.photo = None
        self.cache = None
        self.valid = True
        self.shown = []
        self._build()
        if path:
            self.open(path)

    # -- the view of the mode ----------------------------------------------------------------------------------------

    @property
    def zoom(self):
        return self.views[self.mode][0]

    @zoom.setter
    def zoom(self, value):
        self.views[self.mode][0] = value

    @property
    def pan(self):
        return self.views[self.mode][1]

    @pan.setter
    def pan(self, value):
        self.views[self.mode][1] = value

    # -- window ------------------------------------------------------------------------------------------------------

    def _build(self):
        root = self.root
        root.title("pakal-atlas editor")
        root.geometry("1200x800")
        bar = ttk.Frame(root)
        bar.pack(side="top", fill="x", padx=6, pady=4)
        ttk.Button(bar, text="Open", command=self.ask_open).pack(side="left")
        ttk.Button(bar, text="Save", command=self.save).pack(side="left", padx=(4, 12))
        ttk.Button(bar, text="Undo", command=self.do_undo).pack(side="left")
        ttk.Button(bar, text="Redo", command=self.do_redo).pack(side="left", padx=(4, 12))
        self.mode_button = ttk.Button(bar, text="Atlas view", command=self.toggle_mode)
        self.mode_button.pack(side="left", padx=(0, 12))
        self.alpha = tk.IntVar(value=pa.Options.alpha)
        self.tolerance = tk.DoubleVar(value=pa.Options.tolerance)
        self.island = tk.IntVar(value=pa.Options.min_island)
        for label, var, lo, hi, step in (("alpha", self.alpha, 0, 255, 1), ("tolerance", self.tolerance, 0, 6, 0.1), ("min island", self.island, 0, 400, 1)):
            ttk.Label(bar, text=label).pack(side="left")
            ttk.Spinbox(bar, from_=lo, to=hi, increment=step, textvariable=var, width=5).pack(side="left", padx=(2, 8))
        ttk.Button(bar, text="Re-cut", command=self.recut).pack(side="left")
        ttk.Button(bar, text="Rectangle", command=self.rectangle).pack(side="left", padx=(4, 12))
        self.show_triangles = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="triangles", variable=self.show_triangles, command=self.redraw).pack(side="left")
        self.show_picture = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="picture", variable=self.show_picture, command=self.redraw).pack(side="left")
        self.show_meshes = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="all meshes", variable=self.show_meshes, command=self.redraw).pack(side="left")

        body = ttk.Panedwindow(root, orient="horizontal")
        body.pack(fill="both", expand=True)
        left = ttk.Frame(body, width=240)
        body.add(left, weight=0)
        self.filter = tk.StringVar()
        self.filter.trace_add("write", lambda *_: self.fill_list())
        ttk.Entry(left, textvariable=self.filter).pack(fill="x", padx=4, pady=4)
        holder = ttk.Frame(left)
        holder.pack(fill="both", expand=True)
        self.list = tk.Listbox(holder, exportselection=False, activestyle="none")
        scroll = ttk.Scrollbar(holder, command=self.list.yview)
        self.list.config(yscrollcommand=scroll.set)
        self.list.pack(side="left", fill="both", expand=True)
        scroll.pack(side="right", fill="y")
        self.list.bind("<<ListboxSelect>>", lambda _: self.pick_from_list())
        self.canvas = tk.Canvas(body, background="#303038", highlightthickness=0)
        body.add(self.canvas, weight=1)
        self.status = tk.StringVar(value="Open an atlas (name.json)")
        ttk.Label(root, textvariable=self.status, anchor="w").pack(side="bottom", fill="x", padx=6, pady=3)

        c = self.canvas
        c.bind("<ButtonPress-1>", self.on_press)
        c.bind("<B1-Motion>", self.on_drag)
        c.bind("<ButtonRelease-1>", self.on_release)
        c.bind("<Double-Button-1>", self.on_double)
        c.bind("<ButtonPress-3>", self.on_delete_point)
        c.bind("<ButtonPress-2>", self.on_pan_start)
        c.bind("<B2-Motion>", self.on_pan)
        c.bind("<MouseWheel>", self.on_wheel)
        c.bind("<Configure>", lambda _: self.redraw())
        root.bind("<Control-z>", lambda _: self.do_undo())
        root.bind("<Control-y>", lambda _: self.do_redo())
        root.bind("<Control-s>", lambda _: self.save())
        root.bind("<Up>", lambda _: self.step(-1))
        root.bind("<Down>", lambda _: self.step(1))
        root.bind("<Delete>", lambda _: self.delete_piece())
        root.bind("<Tab>", lambda _: None if self.typing() else (self.toggle_mode(), "break")[1])
        root.protocol("WM_DELETE_WINDOW", self.close)

    # -- files -------------------------------------------------------------------------------------------------------

    def ask_open(self):
        if not self.confirm_discard():
            return
        path = filedialog.askopenfilename(title="Open an atlas", filetypes=[("pakal-atlas", "*.json")])
        if path:
            self.open(path)

    def open(self, path):
        path = Path(path)
        data = json.loads(path.read_text(encoding="utf-8"))
        if data.get("format") != pa.FORMAT:
            messagebox.showerror("pakal-atlas editor", f"{path.name} is not a {pa.FORMAT} file")
            return
        self.path, self.data = path, data
        self.png_path = path.with_name(data["image"])
        self.atlas = Image.open(self.png_path).convert("RGBA")
        self.undo, self.redo, self.dirty, self.dirty_pixels = [], [], False, False
        self.version += 1
        self.name = None
        self.root.title(f"pakal-atlas editor - {path.name}")
        self.fill_list()
        if self.list.size():
            self.select(0)

    def save(self):
        if not self.data:
            return
        if self.dirty_pixels:
            backup = self.png_path.with_name(self.png_path.stem + ".orig.png")
            if not backup.exists():
                backup.write_bytes(self.png_path.read_bytes())
            self.atlas.save(self.png_path, optimize=True)
            self.dirty_pixels = False
        self.path.write_text(json.dumps(self.data, separators=(",", ":")), encoding="utf-8")
        edits = pa.edits_of(self.data)
        sidecar = pa.edits_path(self.path)
        if edits["frames"]:
            sidecar.write_text(json.dumps(edits, indent=1), encoding="utf-8")
        elif sidecar.exists():
            sidecar.unlink()
        self.dirty = False
        self.set_status(f"saved {self.path.name} ({len(edits['frames'])} frames cut by hand)")

    def confirm_discard(self):
        return not self.dirty or messagebox.askyesno("pakal-atlas editor", "There are unsaved changes. Leave without saving?")

    def close(self):
        if self.confirm_discard():
            self.root.destroy()

    # -- the list of frames ------------------------------------------------------------------------------------------

    def names(self):
        text = self.filter.get().strip().lower()
        return [n for n in self.data["frames"] if text in n.lower()] if self.data else []

    def fill_list(self):
        self.shown = self.names()
        self.list.delete(0, "end")
        for n in self.shown:
            f = self.data["frames"][n]
            mark = "# " if f.get("pixelsEdited") else ("* " if f.get("edited") else "  ")
            self.list.insert("end", mark + n)
        if self.name in self.shown:
            self.list.selection_set(self.shown.index(self.name))

    def pick_from_list(self):
        selection = self.list.curselection()
        if selection:
            self.show(self.shown[selection[0]])

    def select(self, index):
        index = max(0, min(self.list.size() - 1, index))
        self.list.selection_clear(0, "end")
        self.list.selection_set(index)
        self.list.see(index)
        self.show(self.shown[index])

    def step(self, delta):
        if self.data and self.name in self.shown:
            self.select(self.shown.index(self.name) + delta)

    # -- the frame shown ---------------------------------------------------------------------------------------------

    @property
    def frame(self):
        return self.data["frames"][self.name]

    def rect_of(self, name):
        return self.data["frames"][name]["rect"]

    def show(self, name, fit=True):
        self.name = name
        self.sel = None
        self.loops = [list(map(tuple, loop)) for loop in pa.frame_outline(self.frame)]
        self.valid = True
        self.cache = None
        if fit and self.mode == "frame":
            w, h = self.frame["rect"][2], self.frame["rect"][3]
            view_w, view_h = max(200, self.canvas.winfo_width()), max(200, self.canvas.winfo_height())
            zoom = max(1.0, min((view_w - 2 * MARGIN) / max(1, w), (view_h - 2 * MARGIN) / max(1, h)))
            self.zoom = float(int(zoom)) if zoom >= 2 else zoom
            self.pan = [(view_w - w * self.zoom) / 2, (view_h - h * self.zoom) / 2]
        self.redraw()

    def toggle_mode(self):
        if not self.data:
            return
        self.mode = "atlas" if self.mode == "frame" else "frame"
        self.mode_button.config(text="Frame view" if self.mode == "atlas" else "Atlas view")
        self.drag = None
        if self.mode == "atlas" and self.views["atlas"][0] == 0.5 and self.views["atlas"][1] == [MARGIN, MARGIN]:
            w, h = self.atlas.size
            view_w, view_h = max(200, self.canvas.winfo_width()), max(200, self.canvas.winfo_height())
            self.zoom = min((view_w - 2 * MARGIN) / w, (view_h - 2 * MARGIN) / h)
            self.pan = [(view_w - w * self.zoom) / 2, (view_h - h * self.zoom) / 2]
        self.cache = None
        self.redraw()

    # -- coordinates: what is drawn is in the pixels of the frame (frame view) or of the atlas (atlas view) ----------

    def origin(self, name=None):
        """The atlas pixel of the frame's rectangle corner, in the coordinates of the mode (the atlas view's); 0, 0 in the frame view."""
        if self.mode == "frame":
            return 0.0, 0.0
        r = self.rect_of(name or self.name)
        return float(r[0]), float(r[1])

    def to_screen(self, p):
        return self.pan[0] + p[0] * self.zoom, self.pan[1] + p[1] * self.zoom

    def to_view(self, x, y):
        return (x - self.pan[0]) / self.zoom, (y - self.pan[1]) / self.zoom

    def loop_screen(self, loop, name=None):
        ox, oy = self.origin(name)
        return [self.to_screen((x + ox, y + oy)) for x, y in loop]

    # -- drawing -----------------------------------------------------------------------------------------------------

    def redraw(self):
        c = self.canvas
        c.delete("all")
        if not self.data or not self.name:
            return
        if self.mode == "frame":
            self.draw_frame_view()
        else:
            self.draw_atlas_view()
        self.draw_ghost()
        self.update_status()

    def draw_frame_view(self):
        c = self.canvas
        x, y, w, h = self.frame["rect"]
        if self.show_picture.get():
            key = ("frame", self.name, round(self.zoom, 3), self.version)
            if self.cache != key:
                crop = self.atlas.crop((x, y, x + w, y + h))
                size = (max(1, int(round(w * self.zoom))), max(1, int(round(h * self.zoom))))
                board = checker(size)
                board.alpha_composite(crop.resize(size, Image.NEAREST))
                self.photo = ImageTk.PhotoImage(board)
                self.cache = key
            c.create_image(self.pan[0], self.pan[1], image=self.photo, anchor="nw")
        ox, oy = self.to_screen((0, 0))
        ex, ey = self.to_screen((w, h))
        c.create_rectangle(ox, oy, ex, ey, outline="#5a8a5a", dash=(3, 3))
        if self.show_triangles.get() and self.loops and self.valid:
            vertices, indices = pa.mesh_from_loops(self.loops)
            if vertices:
                for i in range(0, len(indices), 3):
                    pts = [self.to_screen(vertices[indices[i + k]]) for k in range(3)]
                    c.create_polygon(*pts[0], *pts[1], *pts[2], fill="", outline="#7a4a4a")
        for li, loop in enumerate(self.loops):
            colour = "#ffd000" if li == self.sel else ("#ff5050" if self.valid else "#ffa040")
            pts = self.loop_screen(loop)
            for i in range(len(pts)):
                a, b = pts[i], pts[(i + 1) % len(pts)]
                c.create_line(*a, *b, fill=colour, width=3 if li == self.sel else 2)
            for px, py in pts:
                c.create_rectangle(px - HANDLE, py - HANDLE, px + HANDLE, py + HANDLE, fill="#ffffff", outline="#000000")

    def view_box(self):
        """The atlas pixels the canvas shows: (x0, y0, x1, y1)."""
        w, h = max(1, self.canvas.winfo_width()), max(1, self.canvas.winfo_height())
        x0, y0 = self.to_view(0, 0)
        x1, y1 = self.to_view(w, h)
        return x0, y0, x1, y1

    def draw_atlas_view(self):
        c = self.canvas
        x0, y0, x1, y1 = self.view_box()
        aw, ah = self.atlas.size
        if self.show_picture.get():
            bx0, by0, bx1, by1 = max(0, math.floor(x0)), max(0, math.floor(y0)), min(aw, math.ceil(x1)), min(ah, math.ceil(y1))
            if bx1 > bx0 and by1 > by0:
                key = ("atlas", bx0, by0, bx1, by1, round(self.zoom, 4), self.version)
                if self.cache != key:
                    size = (max(1, int(round((bx1 - bx0) * self.zoom))), max(1, int(round((by1 - by0) * self.zoom))))
                    crop = self.atlas.crop((bx0, by0, bx1, by1))
                    board = checker(size)
                    board.alpha_composite(crop.resize(size, Image.NEAREST if self.zoom >= 1 else Image.BOX))
                    self.photo = ImageTk.PhotoImage(board)
                    self.cache = key
                c.create_image(*self.to_screen((bx0, by0)), image=self.photo, anchor="nw")
        sx0, sy0 = self.to_screen((0, 0))
        sx1, sy1 = self.to_screen((aw, ah))
        c.create_rectangle(sx0, sy0, sx1, sy1, outline="#707080")
        for name, f in self.data["frames"].items():
            rx, ry, rw, rh = f["rect"]
            if rx > x1 or ry > y1 or rx + rw < x0 or ry + rh < y0:
                continue
            chosen = name == self.name
            a, b = self.to_screen((rx, ry)), self.to_screen((rx + rw, ry + rh))
            c.create_rectangle(*a, *b, outline="#5aa05a" if chosen else "#44484c", width=2 if chosen else 1)
            if self.zoom >= 1.5 or chosen:
                c.create_text(a[0] + 2, a[1] + 1, text=name, anchor="nw", fill="#a0e0a0" if chosen else "#808890", font=("TkDefaultFont", 8))
            if not chosen and self.show_meshes.get():
                for loop in pa.frame_outline(f):
                    pts = self.loop_screen(loop, name)
                    c.create_line(*[v for p in pts + pts[:1] for v in p], fill="#8a4a4a")
        if self.loops:
            if self.show_triangles.get() and self.valid:
                vertices, indices = pa.mesh_from_loops(self.loops)
                if vertices:
                    ox, oy = self.origin()
                    for i in range(0, len(indices), 3):
                        pts = [self.to_screen((vertices[indices[i + k]][0] + ox, vertices[indices[i + k]][1] + oy)) for k in range(3)]
                        c.create_polygon(*pts[0], *pts[1], *pts[2], fill="", outline="#7a4a4a")
            for li, loop in enumerate(self.loops):
                pts = self.loop_screen(loop)
                c.create_line(*[v for p in pts + pts[:1] for v in p], fill="#ffd000" if li == self.sel else "#ff5050", width=3 if li == self.sel else 2)

    def draw_ghost(self):
        """What a drag in progress would do: the piece (or the frame) where it would land."""
        g = self.drag
        if not g or g["kind"] not in ("island", "frame"):
            return
        dx, dy = g["dx"], g["dy"]
        if g["kind"] == "island" and self.sel is not None:
            ox, oy = self.origin()
            if self.mode == "frame":
                loop = [(x + dx, y + dy) for x, y in self.loops[self.sel]]
                pts = [self.to_screen((x, y)) for x, y in loop]
            else:
                pts = [self.to_screen((x + ox + dx, y + oy + dy)) for x, y in self.loops[self.sel]]
            self.canvas.create_line(*[v for p in pts + pts[:1] for v in p], fill="#40ff80", width=3, dash=(4, 3))
        elif g["kind"] == "frame":
            r = self.frame["rect"]
            a, b = self.to_screen((r[0] + dx, r[1] + dy)), self.to_screen((r[0] + dx + r[2], r[1] + dy + r[3]))
            self.canvas.create_rectangle(*a, *b, outline="#40ff80", width=3, dash=(4, 3))

    def update_status(self):
        if not self.name:
            return
        w, h = self.frame["rect"][2], self.frame["rect"][3]
        vertices, indices = pa.mesh_from_loops(self.loops) if self.loops else (None, None)
        if vertices:
            area = sum(abs(pa.signed_area(loop)) for loop in self.loops)
            text = f"{len(self.loops)} pieces, {len(vertices)} points, {len(indices) // 3} triangles, {100 * area / (w * h):.0f}% of the rectangle"
        else:
            text = "rectangle (no mesh)" if not self.loops else "this outline cannot be cut: a loop crosses itself"
        flags = ("  [pixels moved]" if self.frame.get("pixelsEdited") else "") or ("  [cut by hand]" if self.frame.get("edited") else "")
        if self.message:
            text = self.message
        self.set_status(f"{self.name}   {w}x{h}   {text}{flags}{'   * unsaved' if self.dirty else ''}")

    message = ""

    def set_status(self, text):
        self.status.set(text)

    def say(self, text):
        self.message = text
        self.redraw()
        self.message = ""

    # -- history: a change is the frames and the pixels as they were and as they are ---------------------------------

    def capture(self, names):
        return {n: copy.deepcopy(self.data["frames"][n]) for n in names}

    def crop_patches(self, boxes):
        return [(tuple(b), self.atlas.crop(b)) for b in boxes]

    def push(self, before, boxes, old_patches):
        names = list(before)
        after = self.capture(names)
        new_patches = [(box, self.atlas.crop(box)) for box, _ in old_patches]
        change = {"before": before, "after": after, "old": old_patches, "new": new_patches, "name": self.name}
        self.undo.append(change)
        del self.undo[:-HISTORY]
        self.redo.clear()
        self.dirty = True
        if old_patches:
            self.dirty_pixels = True
            self.version += 1

    def apply(self, change, forward):
        frames = change["after" if forward else "before"]
        for n, f in frames.items():
            target = self.data["frames"][n]
            target.clear()
            target.update(copy.deepcopy(f))
        for (box, image) in (change["new"] if forward else change["old"]):
            self.atlas.paste(image, box[:2])
        if change["old"]:
            self.version += 1
            self.dirty_pixels = True
        self.dirty = True
        if self.name in self.data["frames"]:
            self.loops = [list(map(tuple, loop)) for loop in pa.frame_outline(self.frame)]
        self.sel = None
        self.cache = None
        self.fill_list()
        self.redraw()

    def do_undo(self):
        if self.undo:
            change = self.undo.pop()
            self.redo.append(change)
            self.apply(change, False)

    def do_redo(self):
        if self.redo:
            change = self.redo.pop()
            self.undo.append(change)
            self.apply(change, True)

    # -- editing the mesh of the frame -------------------------------------------------------------------------------

    def commit(self, before_frame, loops_before):
        """The loops of the frame changed: keeps them in the data (an outline that cannot be cut is refused) and in the history."""
        if not pa.set_frame_outline(self.frame, self.loops):
            self.loops = loops_before
            self.say("that change would make a loop cross itself: undone")
            return False
        self.push({self.name: before_frame}, [], [])
        self.fill_list()
        self.redraw()
        return True

    def recut(self):
        if not self.name:
            return
        x, y, w, h = self.frame["rect"]
        crop = self.atlas.crop((x, y, x + w, y + h))
        options = pa.Options(alpha=int(self.alpha.get()), tolerance=float(self.tolerance.get()), min_island=int(self.island.get()))
        mask = pa.opaque_mask(crop, options.alpha, options.min_island)
        before, loops_before = copy.deepcopy(self.frame), [list(loop) for loop in self.loops]
        loops = pa.mesh_loops(mask, options) if mask.any() else []
        self.loops = [[(min(max(px, 0.0), w), min(max(py, 0.0), h)) for px, py in loop] for loop in loops]
        self.sel = None
        self.commit(before, loops_before)

    def rectangle(self):
        if self.name and self.loops:
            before, loops_before = copy.deepcopy(self.frame), [list(loop) for loop in self.loops]
            self.loops = []
            self.sel = None
            self.commit(before, loops_before)

    def typing(self):
        return isinstance(self.root.focus_get(), (ttk.Entry, ttk.Spinbox, tk.Entry))

    def delete_piece(self):
        if self.typing() or not self.name or self.sel is None or self.sel >= len(self.loops):
            return
        if len(self.loops) <= 1:
            self.say("a frame keeps at least one piece (Rectangle takes the mesh away)")
            return
        before, loops_before = copy.deepcopy(self.frame), [list(loop) for loop in self.loops]
        del self.loops[self.sel]
        self.sel = None
        self.commit(before, loops_before)

    # -- moving pieces and frames (pixels and meshes) ----------------------------------------------------------------

    def group(self, name):
        """The frames that share this one's rectangle (equal pictures are stored once)."""
        rect = self.data["frames"][name]["rect"]
        return [n for n, f in self.data["frames"].items() if f["rect"] == rect]

    def frames_containing(self, box):
        """The frames whose rectangle holds the atlas box (x0, y0, x1, y1)."""
        out = []
        for n, f in self.data["frames"].items():
            rx, ry, rw, rh = f["rect"]
            if box[0] >= rx and box[1] >= ry and box[2] <= rx + rw and box[3] <= ry + rh:
                out.append(n)
        return out

    def move_piece(self, dx, dy):
        """The selected piece of the frame moved by (dx, dy) atlas pixels: its pixels to where it lands, its mesh to the frame
        whose rectangle holds it there."""
        name, index = self.name, self.sel
        rx, ry = self.frame["rect"][0], self.frame["rect"][1]
        loop = self.loops[index]
        bx0, by0, bx1, by1 = loop_box(loop)
        ix0, iy0, ix1, iy1 = int(math.floor(bx0)), int(math.floor(by0)), int(math.ceil(bx1)), int(math.ceil(by1))
        src = (rx + ix0, ry + iy0, rx + ix1, ry + iy1)
        dst = (src[0] + dx, src[1] + dy, src[2] + dx, src[3] + dy)
        aw, ah = self.atlas.size
        targets = self.frames_containing(dst) if 0 <= dst[0] and 0 <= dst[1] and dst[2] <= aw and dst[3] <= ah else []
        if not targets:
            self.say("it does not fit inside the rectangle of a frame there (a frame's rectangle is fixed)")
            return False
        # the frame that gets it: the one under its centre, else the first
        cx, cy = (dst[0] + dst[2]) / 2, (dst[1] + dst[3]) / 2
        target = next((n for n in targets if self.rect_of(n)[0] <= cx < self.rect_of(n)[0] + self.rect_of(n)[2]
                       and self.rect_of(n)[1] <= cy < self.rect_of(n)[1] + self.rect_of(n)[3]), targets[0])
        same = self.rect_of(target) == self.rect_of(name)
        source_group, target_group = self.group(name), self.group(target)
        # what the frames that share the pictures hold: a frame left without a piece is not allowed
        if not same:
            for n in source_group:
                if len(pa.frame_outline(self.data["frames"][n])) <= 1:
                    self.say(f"{n} would be left without a piece")
                    return False
        before = self.capture(source_group + [n for n in target_group if n not in source_group])
        boxes = [src, dst] if src != dst else [src]
        old_patches = self.crop_patches(boxes)
        # the pixels: lift the piece (inside its polygon), clear where it was, lay it over what is there
        mask = Image.new("L", (ix1 - ix0, iy1 - iy0), 0)
        ImageDraw.Draw(mask).polygon([(x - ix0, y - iy0) for x, y in loop], fill=255)
        piece = self.atlas.crop(src)
        piece.putalpha(ImageChops.multiply(piece.getchannel("A"), mask))
        self.atlas.paste(Image.new("RGBA", mask.size, (0, 0, 0, 0)), src[:2], mask)
        region = self.atlas.crop(dst)
        region.alpha_composite(piece)
        self.atlas.paste(region, dst[:2])
        # the meshes
        for n in source_group:
            f = self.data["frames"][n]
            loops = pa.frame_outline(f)
            if not any(len(l) == len(loop) and all(abs(a[0] - b[0]) < 1e-6 and abs(a[1] - b[1]) < 1e-6 for a, b in zip(l, loop)) for l in loops):
                continue          # (a frame that shares the picture and was cut differently is left as it is)
            keep = [l for l in loops if not (len(l) == len(loop) and all(abs(a[0] - b[0]) < 1e-6 and abs(a[1] - b[1]) < 1e-6 for a, b in zip(l, loop)))]
            if same:
                keep.append([(x + dx, y + dy) for x, y in loop])
            pa.set_frame_outline(f, keep)
            f["pixelsEdited"] = True
        if not same:
            trx, try_ = self.rect_of(target)[0], self.rect_of(target)[1]
            for n in target_group:
                f = self.data["frames"][n]
                moved = [(x + rx + dx - trx, y + ry + dy - try_) for x, y in loop]
                pa.set_frame_outline(f, pa.frame_outline(f) + [moved])
                f["pixelsEdited"] = True
        self.push(before, boxes, old_patches)
        # (the box patches were captured before the move: push takes the new pixels now)
        self.loops = [list(map(tuple, l)) for l in pa.frame_outline(self.frame)]
        self.sel = None
        if not same:
            self.name = target
            self.loops = [list(map(tuple, l)) for l in pa.frame_outline(self.frame)]
            self.sel = len(self.loops) - 1
        self.cache = None
        self.fill_list()
        self.redraw()
        return True

    def move_frame(self, dx, dy):
        """The whole rectangle of the frame (and of the frames that share it) moved by (dx, dy) to a free place of the atlas."""
        r = self.frame["rect"]
        new = (r[0] + dx, r[1] + dy, r[2], r[3])
        aw, ah = self.atlas.size
        if new[0] < 0 or new[1] < 0 or new[0] + new[2] > aw or new[1] + new[3] > ah:
            self.say("it would leave the atlas")
            return False
        for n, f in self.data["frames"].items():
            if f["rect"] == r:
                continue
            o = f["rect"]
            if new[0] < o[0] + o[2] + GAP and o[0] < new[0] + new[2] + GAP and new[1] < o[1] + o[3] + GAP and o[1] < new[1] + new[3] + GAP:
                self.say(f"it would touch {n}: the rectangles need a free place")
                return False
        names = self.group(self.name)
        before = self.capture(names)
        src = (r[0], r[1], r[0] + r[2], r[1] + r[3])
        dst = (new[0], new[1], new[0] + new[2], new[1] + new[3])
        old_patches = self.crop_patches([src, dst])
        picture = self.atlas.crop(src)
        self.atlas.paste(Image.new("RGBA", picture.size, (0, 0, 0, 0)), src[:2])
        self.atlas.paste(picture, dst[:2])
        for n in names:
            f = self.data["frames"][n]
            f["rect"] = [new[0], new[1], new[2], new[3]]
            f["pixelsEdited"] = True
        self.push(before, [src, dst], old_patches)
        self.cache = None
        self.fill_list()
        self.redraw()
        return True

    # -- the pointer -------------------------------------------------------------------------------------------------

    def hit_point(self, x, y):
        best = None
        for li, loop in enumerate(self.loops):
            for pi, sp in enumerate(self.loop_screen(loop)):
                d = math.hypot(sp[0] - x, sp[1] - y)
                if d <= PICK and (best is None or d < best[0]):
                    best = (d, li, pi)
        return best

    def hit_edge(self, x, y):
        best = None
        for li, loop in enumerate(self.loops):
            pts = self.loop_screen(loop)
            for pi in range(len(pts)):
                d, q = distance_to_segment((x, y), pts[pi], pts[(pi + 1) % len(pts)])
                if d <= PICK and (best is None or d < best[0]):
                    best = (d, li, pi, q)
        return best

    def hit_piece(self, x, y):
        """The index of the piece of the frame under the pointer (the last drawn wins)."""
        ox, oy = self.origin()
        vx, vy = self.to_view(x, y)
        local = (vx - ox, vy - oy)
        for li in range(len(self.loops) - 1, -1, -1):
            if point_in_loop(local, self.loops[li]):
                return li
        return None

    def hit_frame(self, x, y):
        vx, vy = self.to_view(x, y)
        found = None
        for n, f in self.data["frames"].items():
            rx, ry, rw, rh = f["rect"]
            if rx <= vx < rx + rw and ry <= vy < ry + rh:
                if n == self.name:
                    return n
                found = found or n
        return found

    def clamp(self, p):
        w, h = self.frame["rect"][2], self.frame["rect"][3]
        return min(max(p[0], 0.0), float(w)), min(max(p[1], 0.0), float(h))

    def on_press(self, e):
        if not self.name:
            return
        ctrl = bool(e.state & 0x4)
        self.drag = None
        if self.mode == "atlas":
            hit = self.hit_frame(e.x, e.y)
            if hit and hit != self.name:
                self.show(hit, fit=False)
                if hit in self.shown:
                    self.list.selection_clear(0, "end")
                    self.list.selection_set(self.shown.index(hit))
                    self.list.see(self.shown.index(hit))
            if not hit:
                return
            if ctrl:
                self.drag = {"kind": "frame", "x": e.x, "y": e.y, "dx": 0, "dy": 0}
                return
            piece = self.hit_piece(e.x, e.y)
            self.sel = piece
            if piece is not None:
                self.drag = {"kind": "island", "x": e.x, "y": e.y, "dx": 0, "dy": 0}
            self.redraw()
            return
        hit = self.hit_point(e.x, e.y)
        if hit:
            self.sel = hit[1]
            self.drag = {"kind": "vertex", "loop": hit[1], "index": hit[2], "frame": copy.deepcopy(self.frame), "loops": [list(l) for l in self.loops]}
            return
        piece = self.hit_piece(e.x, e.y)
        self.sel = piece
        if piece is not None:
            self.drag = {"kind": "island", "x": e.x, "y": e.y, "dx": 0, "dy": 0}
        self.redraw()

    def on_drag(self, e):
        g = self.drag
        if not g:
            return
        if g["kind"] == "vertex":
            self.loops[g["loop"]][g["index"]] = self.clamp(self.to_view(e.x, e.y))
            self.valid = pa.loop_is_simple(self.loops[g["loop"]])
        else:
            g["dx"] = round((e.x - g["x"]) / self.zoom)
            g["dy"] = round((e.y - g["y"]) / self.zoom)
        self.redraw()

    def on_release(self, e):
        g, self.drag = self.drag, None
        if not g:
            return
        if g["kind"] == "vertex":
            self.valid = True
            if self.loops != g["loops"]:
                self.commit(g["frame"], g["loops"])
            else:
                self.redraw()
        elif g["kind"] == "island":
            if (g["dx"] or g["dy"]) and self.sel is not None:
                self.move_piece(g["dx"], g["dy"])
            else:
                self.redraw()
        elif g["kind"] == "frame":
            if g["dx"] or g["dy"]:
                self.move_frame(g["dx"], g["dy"])
            else:
                self.redraw()

    def on_double(self, e):
        if not self.name or self.mode != "frame" or self.hit_point(e.x, e.y):
            return
        hit = self.hit_edge(e.x, e.y)
        if hit:
            _, li, pi, q = hit
            before, loops_before = copy.deepcopy(self.frame), [list(l) for l in self.loops]
            self.loops[li].insert(pi + 1, self.clamp(self.to_view(*q)))
            self.drag = None
            self.commit(before, loops_before)

    def on_delete_point(self, e):
        hit = self.hit_point(e.x, e.y) if self.name and self.mode == "frame" else None
        if hit:
            _, li, pi = hit
            before, loops_before = copy.deepcopy(self.frame), [list(l) for l in self.loops]
            if len(self.loops[li]) <= 3:
                if len(self.loops) <= 1:
                    self.say("a frame keeps at least one piece")
                    return
                del self.loops[li]               # a triangle with a point taken is no loop
            else:
                del self.loops[li][pi]
            self.sel = None
            self.commit(before, loops_before)

    # -- view --------------------------------------------------------------------------------------------------------

    def on_pan_start(self, e):
        self.pan_from = (e.x, e.y, self.pan[0], self.pan[1])

    def on_pan(self, e):
        x, y, px, py = self.pan_from
        self.pan = [px + e.x - x, py + e.y - y]
        self.redraw()

    def on_wheel(self, e):
        if not self.name:
            return
        before = self.to_view(e.x, e.y)
        low = 0.05 if self.mode == "atlas" else 0.5
        self.zoom = max(low, min(40.0, self.zoom * (1.15 if e.delta > 0 else 1 / 1.15)))
        self.pan = [e.x - before[0] * self.zoom, e.y - before[1] * self.zoom]
        self.redraw()


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    root = tk.Tk()
    Editor(root, argv[0] if argv else None)
    root.mainloop()


if __name__ == "__main__":
    main()
