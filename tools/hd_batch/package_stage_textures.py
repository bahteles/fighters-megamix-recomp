# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
from PIL import Image
from zipfile import ZipFile, ZIP_DEFLATED
import json, struct, zlib, sys

sys.path.insert(0, "work/dmlpy")
import numpy as np
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--base-pack", type=Path, required=True)
args = parser.parse_args()
root = Path("work/archive-textures")
catalog = json.loads((root / "catalog.json").read_text())["members"]
banks = {}
for entry in catalog:
    for layer in ("r", "f", "b"):
        suffix = "_" + layer + ".cel.bin"
        if entry["name"].endswith(suffix):
            banks.setdefault(entry["name"][: -len(suffix)], {})[layer] = entry
entries = {}
missing = []
source = Path("work/fmm-data.bin").read_bytes()
surfaceInfo = {}
for stage in json.loads(Path("work/stage-batch/manifest.json").read_text())["stages"]:
    for surface in stage["surfaces"]:
        surfaceInfo[(stage["stage"], surface["layer"], surface["page"])] = surface
for stage, files in banks.items():
    for layer, cel in files.items():
        me = next(e for e in catalog if e["name"] == stage + "_" + layer + ".map.bin")
        mapping = (root / me["file"]).read_bytes()
        cells = (root / cel["file"]).read_bytes()
        if not cells or not mapping:
            continue
        pages = []
        if layer == "r":
            pages = [mapping[i : i + 8192] for i in range(0, len(mapping), 8192)]
        else:
            cursor = 0
            while cursor < len(mapping):
                w, h = struct.unpack_from(">HH", mapping, cursor)
                cursor += 4
                grid = np.frombuffer(mapping[cursor : cursor + w * h * 2], ">u2").reshape(h, w)
                cursor += w * h * 2
                for y in range(0, h, 64):
                    for x in range(0, w, 64):
                        pages.append(grid[y : y + 64, x : x + 64].astype(">u2").tobytes())
        bank = {"r": 0x60000, "f": 0x10000, "b": 0x8000}[layer]
        paletteAt = me["offset"] + me["compressed_bytes"]
        for page, original in enumerate(pages):
            paletteCount = surfaceInfo[(stage, layer, page)]["palette_count"]
            palette = np.frombuffer(source[paletteAt : paletteAt + paletteCount * 2], ">u2").astype(
                "uint32"
            )
            rgba = (
                0xFF000000
                | ((palette & 31) << 3)
                | (((palette >> 5) & 31) << 11)
                | (((palette >> 10) & 31) << 19)
            )
            paletteCrc = zlib.crc32(rgba.astype("<u4").tobytes())
            hd = Path(f"work/stage-batch/{stage}/{layer}-page-{page:02}-hd-2x.png")
            if not hd.exists():
                missing.append(str(hd))
                continue
            name = f"stages/{bank:x}-{len(cells):x}-{zlib.crc32(cells):x}-{zlib.crc32(original):x}-{paletteCount:x}-{paletteCrc:x}.rgba"
            entries[name] = hd
            if layer != "r":
                words = np.frombuffer(original, ">u2").astype("uint32")
                adjusted = (words & 0xF000) | ((words + (bank >> 5)) & 0xFFF)
                liveCrc = zlib.crc32(adjusted.astype(">u2").tobytes())
                alias = f"stages/{bank:x}-{len(cells):x}-{zlib.crc32(cells):x}-{liveCrc:x}-{paletteCount:x}-{paletteCrc:x}.rgba"
                entries[alias] = hd
out = Path("work/pack-all-stages-candidate.zip")
with ZipFile(args.base_pack) as inp, ZipFile(out, "w", ZIP_DEFLATED, compresslevel=3) as z:
    for n in inp.namelist():
        if not n.startswith(("stages/", "stage-sprites/")):
            z.writestr(n, inp.read(n))
    spriteManifest = Path("work/stage-sprites/manifest.json")
    sprites = json.loads(spriteManifest.read_text()) if spriteManifest.exists() else []
    spriteNames = set()
    for e in sprites:
        name = f"stage-sprites/{e['bank']:x}-{e['bytes']:x}-{e['cells_crc']:x}-{e['width']:x}-{e['phase']:x}-{e['palette_crc']:x}.rgb555"
        if name in spriteNames:
            continue
        spriteNames.add(name)
        z.write(Path("work/stage-sprites") / e["file"], name)
    for name, file in entries.items():
        im = Image.open(file).convert("RGBA")
        assert im.size == (1024, 1024)
        z.writestr(name, im.tobytes())
print("Stage pages packaged", len(entries), "missing", len(missing))
Path("work/stage-package-report.json").write_text(
    json.dumps(
        {"pages": len(entries), "sprite_variants": len(spriteNames), "missing": missing}, indent=2
    )
)
