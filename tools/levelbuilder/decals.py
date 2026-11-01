"""Calcomanías de desgaste (decals): baches, grietas, parches de bacheo, manchas, lodo, escurrimientos y pintura caída.

Un atlas propio de 4 x 8 (1024 x 2048 px, RGBA) hecho aquí con ruido: no viene de ningún paquete. Cada calcomanía es un cuadro
del atlas puesto como un quad con transparencia, plano sobre el piso o parado contra una fachada (roadmesh.Mesh.decal y
facade_decal). Lo que se pone y dónde lo deciden los spawners (spawners.py); aquí solo se dibuja.

    python tools/levelbuilder/decals.py <out.png>
"""
import math
import random
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

CELL = 256
GRID = 4      # columnas
ROWS = 8      # filas (1024 x 2048: lugar para más clases)
# nombre -> celdas del atlas (fila mayor: celda = fila * 4 + columna)
KINDS = {
    'pothole': [0, 1, 2, 3],
    'crack': [4, 5, 6],
    'patch': [7],
    'stain': [8, 9],
    'mud': [10, 11],
    'drips': [12, 13],
    'peeling': [14],
    'broken': [15],          # banqueta rota: grietas en estrella
    'bump': [16, 17, 18, 19],  # asfalto levantado (raíces, calor): un bordo con su luz y su sombra
}


def _noise(rng, size, scale):
    small = rng.random((max(size // scale, 2), max(size // scale, 2)))
    image = Image.fromarray((small * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
    return np.asarray(image, dtype=np.float32) / 255.0


def _blob(rng, size, radius, wobble=0.28):
    """Una mancha de borde irregular: distancia normalizada al centro (<1 adentro)."""
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = size / 2
    angle = np.arctan2(yy - c, xx - c)
    harmonics = sum(rng.uniform(-1, 1) * np.cos(k * angle + rng.uniform(0, 6.28)) / k for k in range(2, 7))
    r = radius * (1 + wobble * harmonics) * (0.9 + 0.2 * _noise(rng, size, 24))
    return np.hypot(xx - c, yy - c) / np.maximum(r, 1.0)


def pothole(rng):
    """Un bache: hundido (sombra adentro del lado de donde viene la luz, el borde de enfrente iluminado), con grava
    clara en el fondo, el borde de asfalto roto en pedazos (no una orilla lisa) y grietas que salen de él. Cada celda de
    otro tamaño y alargada a su modo: no se ven iguales."""
    s = CELL
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)
    c = s / 2
    # alargado y girado: un óvalo irregular
    stretch, turn = rng.uniform(1.0, 1.6), rng.uniform(0, math.pi)
    u = (xx - c) * math.cos(turn) + (yy - c) * math.sin(turn)
    v = -(xx - c) * math.sin(turn) + (yy - c) * math.cos(turn)
    angle = np.arctan2(v, u)
    harmonics = sum(rng.uniform(-1, 1) * np.cos(k * angle + rng.uniform(0, 6.28)) / k for k in range(2, 8))
    radius = s * rng.uniform(0.2, 0.32) * (1 + 0.32 * harmonics)
    d = np.hypot(u / stretch, v) / np.maximum(radius, 1.0)
    # el borde roto: ruido fino en la distancia, así la orilla es dentada
    d = d * (1 + 0.22 * (_noise(rng, s, 14) - 0.5) + 0.07 * (_noise(rng, s, 6) - 0.5))
    grit = np.asarray(Image.fromarray((_noise(rng, s, 4) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.2)), np.float32) / 255.0
    stones = 0.6 * np.asarray(Image.fromarray(((_noise(rng, s, 4) > 0.8) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(0.7)), np.float32) / 255.0
    depth = np.clip(1 - d, 0, 1) ** 0.6
    # el tono del asfalto, más oscuro y terroso al fondo, con piedritas claras
    base = np.array([58, 55, 52], np.float32) * (0.55 + 0.3 * grit)[..., None]
    base = base * (1 - 0.35 * depth[..., None]) + np.array([62, 50, 38], np.float32) * 0.25 * depth[..., None]
    base = base * (1 - stones[..., None] * depth[..., None]) + stones[..., None] * depth[..., None] * np.array([118, 110, 98], np.float32) * (0.8 + 0.4 * grit[..., None])
    # hundido: la luz de arriba a la izquierda deja en sombra la pared de ese lado y alumbra la de enfrente
    wall = np.clip(1 - d, 0, 1)
    wall = np.asarray(Image.fromarray((wall * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(6)), np.float32) / 255.0
    gy, gx = np.gradient(wall)
    shade = np.clip((gx + gy) * 0.7 * 40, -1, 1)
    base *= (1 + 0.55 * shade)[..., None]
    # el borde de asfalto quebrado alrededor, en pedazos
    chunks = np.asarray(Image.fromarray(((_noise(rng, s, 10) > 0.45) * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.0)), np.float32) / 255.0
    rim = np.clip(1 - np.abs(d - 1.06) / 0.07, 0, 1) * chunks
    base = base * (1 - rim[..., None]) + rim[..., None] * np.array([84, 80, 76], np.float32) * (0.8 + 0.3 * grit[..., None])
    alpha = np.clip((1.02 - d) / 0.06, 0, 1) * 238
    alpha = np.maximum(alpha, rim * 210)
    # agua estancada en algunos: oscura y lisa al fondo
    if rng.random() < 0.35:
        puddle = np.clip((0.5 * rng.uniform(0.7, 1.0) - d) / 0.05, 0, 1)
        base = base * (1 - puddle[..., None] * 0.7) + puddle[..., None] * np.array([30, 36, 42], np.float32) * 0.7
    # grietas que salen del borde
    cracks = Image.new('L', (s, s), 0)
    draw = ImageDraw.Draw(cracks)
    for _ in range(rng.randint(2, 5)):
        a = rng.uniform(0, 2 * math.pi)
        r0 = float(np.mean(radius)) * 0.95
        x, y = c + math.cos(a) * r0 * stretch * 0.8, c + math.sin(a) * r0
        points = [(x, y)]
        for _ in range(rng.randint(4, 9)):
            a += rng.uniform(-0.6, 0.6)
            x, y = x + math.cos(a) * 7, y + math.sin(a) * 7
            points.append((x, y))
        draw.line(points, fill=200, width=2)
    lines = np.asarray(cracks.filter(ImageFilter.GaussianBlur(0.6)), np.float32) / 255.0
    base = base * (1 - 0.6 * lines[..., None])
    alpha = np.maximum(alpha, lines * 170)
    return base, alpha


def bump(rng):
    """Asfalto levantado: un bordo suave (alargado y torcido a su modo) que solo sombrea lo de abajo, claro del lado de
    la luz (arriba a la izquierda) y oscuro del otro, con el piel de cocodrilo de grietas encima."""
    s = CELL
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)
    c = s / 2
    stretch, turn = rng.uniform(1.2, 2.4), rng.uniform(0, math.pi)
    u = (xx - c) * math.cos(turn) + (yy - c) * math.sin(turn)
    v = -(xx - c) * math.sin(turn) + (yy - c) * math.cos(turn)
    radius = s * rng.uniform(0.18, 0.28)
    d = np.hypot(u / stretch, v) / radius
    d = d * (1 + 0.14 * (_noise(rng, s, 48) - 0.5))
    height = np.clip(1 - d * d, 0, 1) ** 1.5
    height = np.asarray(Image.fromarray((height * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(5)), np.float32) / 255.0
    gy, gx = np.gradient(height)
    # the slope that faces the light (from the top left: -x, -y) is lit, the other in shade
    shade = np.clip((gx + gy) * 0.7 * 45, -1, 1)
    light = np.array([150, 144, 136], np.float32)
    dark = np.array([12, 11, 10], np.float32)
    rgb = np.where(shade[..., None] > 0, light, dark) * np.ones((s, s, 3), np.float32)
    alpha = np.abs(shade) * 150
    # piel de cocodrilo: celdas de grietas sobre lo alto
    cells = Image.new('L', (s, s), 0)
    draw = ImageDraw.Draw(cells)
    for _ in range(rng.randint(14, 24)):
        a = rng.uniform(0, 2 * math.pi)
        r = rng.uniform(0, radius * 0.8)
        x, y = c + math.cos(a) * r * stretch * 0.7, c + math.sin(a) * r
        points = [(x, y)]
        b = rng.uniform(0, 2 * math.pi)
        for _ in range(rng.randint(2, 4)):
            b += rng.uniform(-1.2, 1.2)
            x, y = x + math.cos(b) * 9, y + math.sin(b) * 9
            points.append((x, y))
        draw.line(points, fill=210, width=1)
    lines = np.asarray(cells.filter(ImageFilter.GaussianBlur(0.5)), np.float32) / 255.0 * np.clip(height * 2.0, 0, 1)
    rgb = rgb * (1 - lines[..., None]) + lines[..., None] * dark
    alpha = np.maximum(alpha, lines * 190)
    return rgb, alpha


def crack(rng, star=False):
    s = CELL
    image = Image.new('RGBA', (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    def walk(x, y, angle, length, width):
        points = [(x, y)]
        for _ in range(int(length / 6)):
            angle += rng.uniform(-0.5, 0.5)
            x += math.cos(angle) * 6
            y += math.sin(angle) * 6
            points.append((x, y))
            if rng.random() < 0.08 and width > 1:
                walk(x, y, angle + rng.choice((-1, 1)) * rng.uniform(0.6, 1.2), length * 0.4, width - 1)
        draw.line(points, fill=(18, 16, 14, 230), width=width)

    if star:
        cx, cy = s / 2, s / 2
        for k in range(rng.randint(5, 8)):
            walk(cx, cy, k * 6.28 / 7 + rng.uniform(-0.3, 0.3), s * rng.uniform(0.3, 0.45), 3)
    else:
        walk(s * 0.1, s * rng.uniform(0.3, 0.7), rng.uniform(-0.4, 0.4), s * 1.1, 3)
    image = image.filter(ImageFilter.GaussianBlur(0.8))
    a = np.asarray(image, dtype=np.float32)
    return a[..., :3], a[..., 3]


def patch(rng):
    s = CELL
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)
    n = _noise(rng, s, 20) * 18
    w, h = s * rng.uniform(0.3, 0.42), s * rng.uniform(0.22, 0.4)
    inside = (np.abs(xx - s / 2) < w + n) & (np.abs(yy - s / 2) < h + n)
    inside = Image.fromarray((inside * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.5))
    alpha = np.asarray(inside, np.float32) * 0.9
    grit = _noise(rng, s, 3)
    rgb = np.stack([30 + 18 * grit] * 3, -1)
    # la junta: un borde más oscuro
    edge = np.asarray(Image.fromarray(alpha.astype(np.uint8)).filter(ImageFilter.FIND_EDGES), np.float32) / 255
    rgb *= (1 - 0.5 * edge)[..., None]
    return rgb, alpha


def stain(rng, color=(14, 14, 16), strength=150):
    s = CELL
    d = _blob(rng, s, s * rng.uniform(0.25, 0.42), 0.4)
    alpha = np.clip(1 - d, 0, 1) ** 0.7 * strength * (0.7 + 0.3 * _noise(rng, s, 6))
    rgb = np.ones((s, s, 3), np.float32) * np.array(color, np.float32)
    return rgb, alpha


def drips(rng):
    s = CELL
    alpha = np.zeros((s, s), np.float32)
    for _ in range(rng.randint(10, 22)):
        x = rng.uniform(0, s)
        width = rng.uniform(3, 14)
        length = rng.uniform(0.3, 1.0) * s
        xs = np.arange(s, dtype=np.float32)
        across = np.exp(-((xs - x) / width) ** 2)
        down = np.clip(1 - np.arange(s, dtype=np.float32) / length, 0, 1) ** 1.5
        alpha += np.outer(down, across) * rng.uniform(60, 130)
    alpha *= 0.7 + 0.3 * _noise(rng, s, 8)
    rgb = np.ones((s, s, 3), np.float32) * np.array([38, 34, 28], np.float32)
    return rgb, np.clip(alpha, 0, 200)


def peeling(rng):
    s = CELL
    d = _blob(rng, s, s * 0.36, 0.45)
    grit = _noise(rng, s, 4)
    # el aplanado caído deja ver ladrillo
    brick = np.array([150, 82, 60], np.float32) * (0.75 + 0.35 * grit[..., None])
    yy, xx = np.mgrid[0:s, 0:s]
    mortar = ((yy % 22) < 3) | (((xx + (yy // 22) * 20) % 44) < 3)
    brick[mortar] = [170, 160, 145]
    alpha = np.clip((1 - d) / 0.15, 0, 1) * 235
    rim = np.exp(-((d - 1.0) / 0.08) ** 2)
    brick += (rim * 50)[..., None]
    return brick, alpha


def atlas(seed=5):
    rng_py = random.Random(seed)
    rng = np.random.default_rng(seed)
    out = np.zeros((CELL * ROWS, CELL * GRID, 4), np.float32)

    class Both:   # random.Random para las formas, numpy para el ruido
        def __getattr__(self, name):
            return getattr(rng_py, name)

        def random(self, shape=None):
            return rng.random(shape) if shape is not None else rng_py.random()

    r = Both()
    makers = {}
    for kind, cells in KINDS.items():
        for cell in cells:
            makers[cell] = kind
    for cell in range(GRID * ROWS):
        kind = makers.get(cell)
        if kind == 'pothole':
            rgb, a = pothole(r)
        elif kind == 'bump':
            rgb, a = bump(r)
        elif kind == 'crack':
            rgb, a = crack(r)
        elif kind == 'broken':
            rgb, a = crack(r, star=True)
        elif kind == 'patch':
            rgb, a = patch(r)
        elif kind == 'stain':
            rgb, a = stain(r)
        elif kind == 'mud':
            rgb, a = stain(r, (70, 52, 34), 190)
        elif kind == 'drips':
            rgb, a = drips(r)
        elif kind == 'peeling':
            rgb, a = peeling(r)
        else:
            continue
        # nada pega en la orilla de la celda (el filtrado de las mips no trae la vecina)
        margin = np.ones((CELL, CELL), np.float32)
        ramp = np.clip(np.minimum(np.arange(CELL), np.arange(CELL)[::-1]) / 10.0, 0, 1)
        margin *= np.minimum.outer(ramp, ramp)
        y, x = divmod(cell, GRID)
        out[y * CELL:(y + 1) * CELL, x * CELL:(x + 1) * CELL, :3] = np.clip(rgb, 0, 255)
        out[y * CELL:(y + 1) * CELL, x * CELL:(x + 1) * CELL, 3] = np.clip(a, 0, 255) * margin
    return Image.fromarray(out.astype(np.uint8), 'RGBA')


def sand_texture(seed=3, size=512):
    """Arena de playa clara que se repite: ruido en varias escalas, granos oscuros y restos de conchas."""
    rng = np.random.default_rng(seed)

    def tile_noise(scale):
        small = rng.random((size // scale, size // scale))
        small = np.pad(small, 2, mode='wrap')
        big = Image.fromarray((small * 255).astype(np.uint8)).resize(((size // scale + 4) * scale,) * 2, Image.BICUBIC)
        a = np.asarray(big, np.float32)[2 * scale:2 * scale + size, 2 * scale:2 * scale + size] / 255.0
        return a

    base = np.array([214, 192, 146], np.float32)
    n = tile_noise(64) * 0.5 + tile_noise(16) * 0.3 + tile_noise(4) * 0.2
    rgb = base[None, None, :] * (0.86 + 0.24 * n[..., None])
    grains = rng.random((size, size))
    rgb[grains > 0.985] *= 0.7
    rgb[grains < 0.006] = [236, 230, 214]
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8)).filter(ImageFilter.SMOOTH)


def cell_uv(cell):
    """(u0, v0, u1, v1) de una celda del atlas."""
    y, x = divmod(cell, GRID)
    return x / GRID, y / ROWS, (x + 1) / GRID, (y + 1) / ROWS


if __name__ == '__main__':
    path = Path(sys.argv[1] if len(sys.argv) > 1 else 'decals.png')
    atlas().save(path)
    print(path)
