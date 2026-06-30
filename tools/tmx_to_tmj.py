#!/usr/bin/env python3
"""
Converts Tiled .tmx (XML) maps to .tmj (Tiled JSON) maps.

- Embeds tileset info directly in the .tmj (firstgid, image path, columns, etc.)
  so the C loader doesn't need to chase external .tsx files.
- Rewrites tileset image paths to be relative to the .tmj file itself
  (we keep tilesets in assets/maps/tilesets/, maps in assets/maps/).
- Appends an empty "spawns" object layer (type=objectgroup) at the end of
  the layer list. Open the .tmj in Tiled and drop objects into it:
      name="player_spawn"      -> player start position
      name="enemy_spawn"       -> one per enemy (can have many)
      name="boss_spawn"        -> boss start position
  The C loader classifies spawns by substring match on the object's name,
  so any naming like "enemy_spawn_1", "EnemySpawn2" etc. also works.

Usage: python3 tmx_to_tmj.py <input.tmx> <output.tmj>
"""
import json
import os
import sys
import xml.etree.ElementTree as ET


def parse_csv_layer_data(data_text):
    text = data_text.strip()
    return [int(v) for v in text.replace("\n", "").split(",") if v.strip() != ""]


def convert(tmx_path, tmj_path):
    tree = ET.parse(tmx_path)
    root = tree.getroot()

    map_width = int(root.get("width"))
    map_height = int(root.get("height"))
    tile_width = int(root.get("tilewidth"))
    tile_height = int(root.get("tileheight"))

    tmx_dir = os.path.dirname(os.path.abspath(tmx_path))
    tmj_dir = os.path.dirname(os.path.abspath(tmj_path))

    tilesets = []
    for ts in root.findall("tileset"):
        image = ts.find("image")
        image_source = image.get("source")
        # NOTE: the tmx files' stored relative paths don't match this repo's
        # actual asset layout (they were authored against a different folder
        # structure). Tileset PNGs all live alongside maps in
        # assets/maps/tilesets/, so we just take the basename and point there
        # instead of trusting the tmx's original relative path.
        rel_to_tmj = "tilesets/" + os.path.basename(image_source)

        tilesets.append({
            "columns": int(ts.get("columns")),
            "firstgid": int(ts.get("firstgid")),
            "image": rel_to_tmj,
            "imageheight": int(image.get("height")),
            "imagewidth": int(image.get("width")),
            "margin": 0,
            "name": ts.get("name"),
            "spacing": 0,
            "tilecount": int(ts.get("tilecount")),
            "tileheight": int(ts.get("tileheight")),
            "tilewidth": int(ts.get("tilewidth")),
        })

    layers = []
    next_layer_id = 1
    for layer in root.findall("layer"):
        data_el = layer.find("data")
        encoding = data_el.get("encoding", "")
        if encoding != "csv":
            raise ValueError(f"Unsupported layer encoding '{encoding}' in {tmx_path}; only csv is supported")
        gids = parse_csv_layer_data(data_el.text)

        layer_id = int(layer.get("id", next_layer_id))
        next_layer_id = max(next_layer_id, layer_id + 1)

        layers.append({
            "data": gids,
            "height": int(layer.get("height", map_height)),
            "id": layer_id,
            "name": layer.get("name", f"layer{layer_id}"),
            "opacity": 1,
            "type": "tilelayer",
            "visible": True,
            "width": int(layer.get("width", map_width)),
            "x": 0,
            "y": 0,
        })

    # Carry over any object layers that already exist in the tmx
    for og in root.findall("objectgroup"):
        layer_id = int(og.get("id", next_layer_id))
        next_layer_id = max(next_layer_id, layer_id + 1)
        objects = []
        next_obj_id = 1
        for obj in og.findall("object"):
            obj_id = int(obj.get("id", next_obj_id))
            next_obj_id = max(next_obj_id, obj_id + 1)
            objects.append({
                "id": obj_id,
                "name": obj.get("name", ""),
                "type": obj.get("type", obj.get("class", "")),
                "x": float(obj.get("x", 0)),
                "y": float(obj.get("y", 0)),
                "width": float(obj.get("width", 0)),
                "height": float(obj.get("height", 0)),
                "visible": True,
            })
        layers.append({
            "draworder": "topdown",
            "id": layer_id,
            "name": og.get("name", "objects"),
            "objects": objects,
            "opacity": 1,
            "type": "objectgroup",
            "visible": True,
            "x": 0,
            "y": 0,
        })

    # Always make sure there's a "spawns" object layer for player/enemy/boss
    # placement. Left empty here -- place objects for it in Tiled.
    if not any(l["type"] == "objectgroup" and l["name"] == "spawns" for l in layers):
        layers.append({
            "draworder": "topdown",
            "id": next_layer_id,
            "name": "spawns",
            "objects": [],
            "opacity": 1,
            "type": "objectgroup",
            "visible": True,
            "x": 0,
            "y": 0,
        })
        next_layer_id += 1

    tmj = {
        "compressionlevel": -1,
        "height": map_height,
        "infinite": False,
        "layers": layers,
        "nextlayerid": next_layer_id,
        "nextobjectid": 1,
        "orientation": root.get("orientation", "orthogonal"),
        "renderorder": root.get("renderorder", "right-down"),
        "tiledversion": root.get("tiledversion", ""),
        "tileheight": tile_height,
        "tilesets": tilesets,
        "tilewidth": tile_width,
        "type": "map",
        "version": str(root.get("version", "1.10")),
        "width": map_width,
    }

    with open(tmj_path, "w") as f:
        json.dump(tmj, f, indent=2)

    print(f"Converted {tmx_path} -> {tmj_path} "
          f"({map_width}x{map_height} tiles, {len(tilesets)} tileset(s), {len(layers)} layer(s))")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    convert(sys.argv[1], sys.argv[2])
