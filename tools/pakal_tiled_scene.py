#!/usr/bin/env python3
"""Convert simple Pakal scene JSON files to and from Tiled JSON maps.

The converter is intentionally focused on the current ContraGame scene shape:
solid-color rectangle entities with SpritePhysicsComponent/SpriteComponent2D and
a CameraComponent2D entity. Tiled maps use one tile as one Pakal world unit.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any, Dict, Iterable, List, MutableMapping, Optional, Tuple

JsonDict = Dict[str, Any]

DEFAULT_COLORS = {
    "player": {"r": 0.18, "g": 0.55, "b": 1.0, "a": 1.0},
    "enemy": {"r": 0.95, "g": 0.24, "b": 0.24, "a": 1.0},
    "goal": {"r": 0.16, "g": 0.85, "b": 0.36, "a": 1.0},
    "platform": {"r": 0.58, "g": 0.45, "b": 0.30, "a": 1.0},
    "terrain": {"r": 0.27, "g": 0.34, "b": 0.40, "a": 1.0},
}

DEFAULT_LAYER = {
    "player": 10,
    "enemy": 10,
    "goal": 4,
    "platform": 2,
    "terrain": 1,
}

TILED_LAYER_COLORS = {
    "terrain": "#455766",
    "platforms": "#94734d",
    "actors": "#338cff",
    "camera": "#f0d24a",
}


def read_json(path: Path) -> JsonDict:
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def write_json(path: Path, data: JsonDict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as file:
        json.dump(data, file, indent=2)
        file.write("\n")


def component(entity: JsonDict, component_type: str) -> Optional[JsonDict]:
    for item in entity.get("components", []):
        if item.get("type") == component_type:
            return item
    return None


def vector_value(data: JsonDict, name: str, default_x: float = 0.0, default_y: float = 0.0, default_z: float = 0.0) -> Tuple[float, float, float]:
    value = data.get(name, {})
    return (
        float(value.get("x", default_x)),
        float(value.get("y", default_y)),
        float(value.get("z", default_z)),
    )


def classify_entity(name: str, entity: JsonDict) -> str:
    lowered = name.lower()
    if lowered == "player":
        return "player"
    if lowered == "goal":
        return "goal"
    if lowered == "camera":
        return "camera"
    if "enemy" in lowered:
        return "enemy"
    if "platform" in lowered:
        return "platform"
    return "terrain"


def object_group_for_kind(kind: str) -> str:
    if kind == "platform":
        return "platforms"
    if kind in {"player", "enemy", "goal"}:
        return "actors"
    if kind == "camera":
        return "camera"
    return "terrain"


def property_value(properties: Iterable[JsonDict], name: str, default: Any = None) -> Any:
    for prop in properties or []:
        if prop.get("name") == name:
            return prop.get("value", default)
    return default


def property_map(properties: Iterable[JsonDict]) -> Dict[str, Any]:
    return {prop.get("name"): prop.get("value") for prop in properties or [] if "name" in prop}


def tiled_property(name: str, value: Any) -> JsonDict:
    if isinstance(value, bool):
        prop_type = "bool"
    elif isinstance(value, int) and not isinstance(value, bool):
        prop_type = "int"
    elif isinstance(value, float):
        prop_type = "float"
    else:
        prop_type = "string"
    return {"name": name, "type": prop_type, "value": value}


def tiled_properties(values: MutableMapping[str, Any]) -> List[JsonDict]:
    return [tiled_property(name, value) for name, value in values.items()]


def color_to_string(color: JsonDict) -> str:
    return ",".join(str(float(color.get(channel, 1.0 if channel == "a" else 0.0))) for channel in ("r", "g", "b", "a"))


def parse_color(value: Any, fallback: JsonDict) -> JsonDict:
    if isinstance(value, dict):
        return {
            "r": float(value.get("r", fallback["r"])),
            "g": float(value.get("g", fallback["g"])),
            "b": float(value.get("b", fallback["b"])),
            "a": float(value.get("a", fallback["a"])),
        }
    if isinstance(value, str):
        pieces = [piece.strip() for piece in value.split(",")]
        if len(pieces) == 4:
            try:
                return {"r": float(pieces[0]), "g": float(pieces[1]), "b": float(pieces[2]), "a": float(pieces[3])}
            except ValueError:
                pass
    return dict(fallback)


def scene_bounds(scene: JsonDict) -> Tuple[float, float, float, float]:
    entities = scene.get("entities", {})
    camera = component(entities.get("camera", {}), "CameraComponent2D")
    if camera and isinstance(camera.get("bounds"), dict):
        bounds = camera["bounds"]
        return (
            float(bounds.get("minX", 0.0)),
            float(bounds.get("minY", 0.0)),
            float(bounds.get("maxX", 32.0)),
            float(bounds.get("maxY", 18.0)),
        )

    min_x = min_y = math.inf
    max_x = max_y = -math.inf
    for name, entity in entities.items():
        if classify_entity(name, entity) == "camera":
            continue
        physics = component(entity, "SpritePhysicsComponent")
        sprite = component(entity, "SpriteComponent2D")
        source = physics or sprite
        if not source:
            continue
        x, y, _ = vector_value(source, "position")
        width = float(source.get("width", 1.0))
        height = float(source.get("height", 1.0))
        min_x = min(min_x, x - width * 0.5)
        max_x = max(max_x, x + width * 0.5)
        min_y = min(min_y, y - height * 0.5)
        max_y = max(max_y, y + height * 0.5)

    if not math.isfinite(min_x):
        return (0.0, 0.0, 32.0, 18.0)
    return (math.floor(min_x), math.floor(min_y), math.ceil(max_x), math.ceil(max_y + 4.0))


def world_to_tiled_rect(x: float, y: float, width: float, height: float, bounds: Tuple[float, float, float, float], tile_size: int) -> Tuple[float, float, float, float]:
    min_x, _, _, max_y = bounds
    return (
        (x - width * 0.5 - min_x) * tile_size,
        (max_y - (y + height * 0.5)) * tile_size,
        width * tile_size,
        height * tile_size,
    )


def world_to_tiled_point(x: float, y: float, bounds: Tuple[float, float, float, float], tile_size: int) -> Tuple[float, float]:
    min_x, _, _, max_y = bounds
    return ((x - min_x) * tile_size, (max_y - y) * tile_size)


def tiled_rect_to_world(obj: JsonDict, map_data: JsonDict) -> Tuple[float, float, float, float]:
    tile_size = float(map_data.get("tilewidth", 32))
    props = property_map(map_data.get("properties", []))
    min_x = float(props.get("pakal_min_x", 0.0))
    max_y = float(props.get("pakal_max_y", float(map_data.get("height", 18))))
    width = float(obj.get("width", 0.0)) / tile_size
    height = float(obj.get("height", 0.0)) / tile_size
    x = min_x + float(obj.get("x", 0.0)) / tile_size + width * 0.5
    y = max_y - float(obj.get("y", 0.0)) / tile_size - height * 0.5
    return x, y, width, height


def tiled_point_to_world(obj: JsonDict, map_data: JsonDict) -> Tuple[float, float]:
    tile_size = float(map_data.get("tilewidth", 32))
    props = property_map(map_data.get("properties", []))
    min_x = float(props.get("pakal_min_x", 0.0))
    max_y = float(props.get("pakal_max_y", float(map_data.get("height", 18))))
    return min_x + float(obj.get("x", 0.0)) / tile_size, max_y - float(obj.get("y", 0.0)) / tile_size


def pakal_to_tiled(input_path: Path, output_path: Path, tile_size: int) -> None:
    root = read_json(input_path)
    scene = root.get("scene", root)
    entities = scene.get("entities", {})
    bounds = scene_bounds(scene)
    min_x, min_y, max_x, max_y = bounds
    map_width = max(1, math.ceil(max_x - min_x))
    map_height = max(1, math.ceil(max_y - min_y))

    grouped_objects: Dict[str, List[JsonDict]] = {"terrain": [], "platforms": [], "actors": [], "camera": []}
    next_object_id = 1

    for entity_name, entity in entities.items():
        kind = classify_entity(entity_name, entity)
        group_name = object_group_for_kind(kind)

        if kind == "camera":
            camera = component(entity, "CameraComponent2D")
            if not camera:
                continue
            x, y, _ = vector_value(camera, "position")
            tiled_x, tiled_y = world_to_tiled_point(x, y, bounds, tile_size)
            orthographic = camera.get("orthographic", {})
            camera_bounds = camera.get("bounds", {})
            grouped_objects[group_name].append({
                "id": next_object_id,
                "name": entity_name,
                "type": "camera",
                "point": True,
                "rotation": 0,
                "visible": True,
                "x": tiled_x,
                "y": tiled_y,
                "width": 0,
                "height": 0,
                "properties": tiled_properties({
                    "pakal_kind": "camera",
                    "orthographic_width": float(orthographic.get("width", 24.0)),
                    "orthographic_height": float(orthographic.get("height", 13.5)),
                    "near": float(orthographic.get("near", -100.0)),
                    "far": float(orthographic.get("far", 100.0)),
                    "zoom": float(camera.get("zoom", 1.0)),
                    "bounds_minX": float(camera_bounds.get("minX", min_x)),
                    "bounds_minY": float(camera_bounds.get("minY", min_y)),
                    "bounds_maxX": float(camera_bounds.get("maxX", max_x)),
                    "bounds_maxY": float(camera_bounds.get("maxY", max_y)),
                }),
            })
            next_object_id += 1
            continue

        physics = component(entity, "SpritePhysicsComponent")
        sprite = component(entity, "SpriteComponent2D")
        source = physics or sprite
        if not source:
            continue

        x, y, _ = vector_value(source, "position")
        width = float(source.get("width", 1.0))
        height = float(source.get("height", 1.0))
        tiled_x, tiled_y, tiled_width, tiled_height = world_to_tiled_rect(x, y, width, height, bounds, tile_size)
        color = (sprite or {}).get("solid_color", DEFAULT_COLORS.get(kind, DEFAULT_COLORS["terrain"]))
        layer = int((sprite or {}).get("layer", DEFAULT_LAYER.get(kind, 1)))
        body_type = str((physics or {}).get("body_type", "dynamic" if kind in {"player", "enemy"} else "static"))
        gravity_scale = float((physics or {}).get("gravity_scale", 1.0 if body_type == "dynamic" else 0.0))
        fixed_rotation = bool((physics or {}).get("fixed_rotation", True))
        is_sensor = bool((physics or {}).get("is_sensor", kind == "goal"))

        grouped_objects[group_name].append({
            "id": next_object_id,
            "name": entity_name,
            "type": kind,
            "rotation": 0,
            "visible": True,
            "x": tiled_x,
            "y": tiled_y,
            "width": tiled_width,
            "height": tiled_height,
            "properties": tiled_properties({
                "pakal_kind": kind,
                "entity_type": entity.get("type", "Pakal::GenericEntity"),
                "solid_color": color_to_string(color),
                "sprite_layer": layer,
                "body_type": body_type,
                "gravity_scale": gravity_scale,
                "fixed_rotation": fixed_rotation,
                "is_sensor": is_sensor,
            }),
        })
        next_object_id += 1

    layers = []
    next_layer_id = 1
    for name in ("terrain", "platforms", "actors", "camera"):
        layers.append({
            "id": next_layer_id,
            "name": name,
            "type": "objectgroup",
            "draworder": "topdown",
            "opacity": 1,
            "visible": True,
            "x": 0,
            "y": 0,
            "color": TILED_LAYER_COLORS[name],
            "objects": grouped_objects[name],
        })
        next_layer_id += 1

    tiled = {
        "type": "map",
        "version": "1.10",
        "tiledversion": "1.10.2",
        "orientation": "orthogonal",
        "renderorder": "right-down",
        "compressionlevel": -1,
        "infinite": False,
        "width": map_width,
        "height": map_height,
        "tilewidth": tile_size,
        "tileheight": tile_size,
        "backgroundcolor": "#2b2b2b",
        "nextlayerid": next_layer_id,
        "nextobjectid": next_object_id,
        "tilesets": [],
        "properties": tiled_properties({
            "scene_name": scene.get("name", input_path.stem),
            "difficulty": int(scene.get("difficulty", 1)),
            "pakal_min_x": float(min_x),
            "pakal_min_y": float(min_y),
            "pakal_max_x": float(max_x),
            "pakal_max_y": float(max_y),
            "pakal_world_units_per_tile": 1.0,
        }),
        "layers": layers,
    }
    write_json(output_path, tiled)


def object_kind(obj: JsonDict, layer_name: str) -> str:
    props = property_map(obj.get("properties", []))
    explicit = str(props.get("pakal_kind", obj.get("type", ""))).lower()
    if explicit:
        return explicit
    lowered_name = str(obj.get("name", "")).lower()
    if lowered_name == "player":
        return "player"
    if lowered_name == "goal":
        return "goal"
    if "enemy" in lowered_name:
        return "enemy"
    if layer_name == "platforms":
        return "platform"
    if layer_name == "camera":
        return "camera"
    return "terrain"


def default_entity_name(kind: str, counters: Dict[str, int]) -> str:
    if kind in {"player", "goal", "camera"}:
        return kind
    counters[kind] = counters.get(kind, 0) + 1
    prefix = "ground" if kind == "terrain" else kind
    return f"{prefix}_{counters[kind]:02d}"


def make_sprite_entity(name: str, kind: str, obj: JsonDict, map_data: JsonDict) -> JsonDict:
    props = property_map(obj.get("properties", []))
    x, y, width, height = tiled_rect_to_world(obj, map_data)
    fallback_color = DEFAULT_COLORS.get(kind, DEFAULT_COLORS["terrain"])
    color = parse_color(props.get("solid_color"), fallback_color)
    body_type = str(props.get("body_type", "dynamic" if kind in {"player", "enemy"} else "static"))
    gravity_scale = float(props.get("gravity_scale", 1.0 if body_type == "dynamic" else 0.0))
    fixed_rotation = bool(props.get("fixed_rotation", True))
    is_sensor = bool(props.get("is_sensor", kind == "goal"))
    layer = int(props.get("sprite_layer", DEFAULT_LAYER.get(kind, 1)))

    physics = {
        "type": "SpritePhysicsComponent",
        "position": {"x": round(x, 4), "y": round(y, 4), "z": 0.0},
        "scale": 1.0,
        "body_type": body_type,
        "fixed_rotation": fixed_rotation,
        "gravity_scale": gravity_scale,
        "width": round(width, 4),
        "height": round(height, 4),
    }
    if is_sensor:
        physics["is_sensor"] = True

    return {
        "type": str(props.get("entity_type", "Pakal::GenericEntity")),
        "components": [
            physics,
            {
                "type": "SpriteComponent2D",
                "solid_color": color,
                "position": {"x": round(x, 4), "y": round(y, 4)},
                "width": round(width, 4),
                "height": round(height, 4),
                "layer": layer,
            },
        ],
    }


def make_camera_entity(obj: JsonDict, map_data: JsonDict) -> JsonDict:
    props = property_map(obj.get("properties", []))
    map_props = property_map(map_data.get("properties", []))
    x, y = tiled_point_to_world(obj, map_data)
    return {
        "type": "Pakal::GenericEntity",
        "components": [
            {
                "type": "CameraComponent2D",
                "position": {"x": round(x, 4), "y": round(y, 4), "z": 0.0},
                "orthographic": {
                    "width": float(props.get("orthographic_width", 24.0)),
                    "height": float(props.get("orthographic_height", 13.5)),
                    "near": float(props.get("near", -100.0)),
                    "far": float(props.get("far", 100.0)),
                },
                "zoom": float(props.get("zoom", 1.0)),
                "bounds": {
                    "minX": float(props.get("bounds_minX", map_props.get("pakal_min_x", 0.0))),
                    "minY": float(props.get("bounds_minY", map_props.get("pakal_min_y", 0.0))),
                    "maxX": float(props.get("bounds_maxX", map_props.get("pakal_max_x", map_data.get("width", 32)))),
                    "maxY": float(props.get("bounds_maxY", map_props.get("pakal_max_y", map_data.get("height", 18)))),
                },
            }
        ],
    }


def tiled_to_pakal(input_path: Path, output_path: Path) -> None:
    map_data = read_json(input_path)
    map_props = property_map(map_data.get("properties", []))
    entities: Dict[str, JsonDict] = {}
    counters: Dict[str, int] = {}
    spawn_points: Dict[str, JsonDict] = {}

    for layer in map_data.get("layers", []):
        if layer.get("type") != "objectgroup":
            continue
        layer_name = str(layer.get("name", ""))
        for obj in layer.get("objects", []):
            kind = object_kind(obj, layer_name)
            name = str(obj.get("name", "")).strip() or default_entity_name(kind, counters)
            if kind == "camera":
                name = "camera"
                entities[name] = make_camera_entity(obj, map_data)
                continue

            if kind in {"player", "goal"}:
                name = kind
            elif kind == "enemy" and not name.startswith("enemy"):
                counters["enemy"] = counters.get("enemy", 0) + 1
                name = f"enemy_{counters['enemy']:02d}"

            entities[name] = make_sprite_entity(name, kind, obj, map_data)
            physics = component(entities[name], "SpritePhysicsComponent")
            if kind == "player" and physics:
                spawn_points["player_start"] = dict(physics["position"])
            if kind == "goal" and physics:
                spawn_points["goal"] = dict(physics["position"])

    if "camera" not in entities:
        camera_obj = {
            "x": 12.0 * float(map_data.get("tilewidth", 32)),
            "y": 6.75 * float(map_data.get("tileheight", 32)),
            "properties": [],
        }
        entities["camera"] = make_camera_entity(camera_obj, map_data)

    enemy_count = sum(1 for name in entities if "enemy" in name)
    scene = {
        "scene": {
            "name": str(map_props.get("scene_name", input_path.stem)),
            "difficulty": int(map_props.get("difficulty", 1)),
            "expected_enemy_count": enemy_count,
            "spawn_points": spawn_points,
            "entities": entities,
        }
    }
    write_json(output_path, scene)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert Pakal scene JSON files to and from Tiled .tmj maps.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    to_tiled = subparsers.add_parser("pakal-to-tiled", help="Convert a Pakal scene JSON file into a Tiled .tmj map.")
    to_tiled.add_argument("input", type=Path)
    to_tiled.add_argument("output", type=Path)
    to_tiled.add_argument("--tile-size", type=int, default=32, help="Pixels per Pakal world unit. Default: 32")

    to_pakal = subparsers.add_parser("tiled-to-pakal", help="Convert a Tiled .tmj map into a Pakal scene JSON file.")
    to_pakal.add_argument("input", type=Path)
    to_pakal.add_argument("output", type=Path)

    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "pakal-to-tiled":
        pakal_to_tiled(args.input, args.output, args.tile_size)
    elif args.command == "tiled-to-pakal":
        tiled_to_pakal(args.input, args.output)
    else:
        raise SystemExit(f"Unknown command: {args.command}")


if __name__ == "__main__":
    main()
