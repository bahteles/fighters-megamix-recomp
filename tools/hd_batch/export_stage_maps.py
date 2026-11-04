# SPDX-License-Identifier: GPL-3.0-only
"""Export and batch-convert stage maps from the user's preserved archive banks."""

from pathlib import Path
from zipfile import ZipFile
import argparse, json, hashlib, struct, sys

sys.path.insert(0, str(Path.cwd() / "work/dmlpy"))
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("--workspace", type=Path, required=True)
ap.add_argument("--output", type=Path, required=True)
ap.add_argument("--convert", action="store_true")
ap.add_argument("--stage", action="append")
args = ap.parse_args()
workspace = args.workspace.resolve()
destination = args.output.resolve()
original = workspace / "archive-textures"
if destination == original or original in destination.parents:
    ap.error("Output must be outside the preserved originals.")
catalog = json.loads((original / "catalog.json").read_text())["members"]
source = (workspace / "fmm-data.bin").read_bytes()
groups = {}
ordered = sorted(catalog, key=lambda e: e["offset"])
for e in ordered:
    for part in ("r.cel", "r.map", "f.cel", "f.map", "b.cel", "b.map", "s.tex", "c.tex"):
        suffix = "_" + part + ".bin"
        if e["name"].endswith(suffix):
            groups.setdefault(e["name"][: -len(suffix)], {})[part] = e
destination.mkdir(parents=True, exist_ok=True)
cache = {}
report = {
    "stages": [],
    "rules": {
        "floor_scale": 2,
        "background_scale": 2,
        "transparent_mask": "source coverage",
        "model": "HDcube 4x reduced to 2x",
    },
}
session = None
if args.convert:
    import onnxruntime as ort

    opts = ort.SessionOptions()
    opts.intra_op_num_threads = 4
    opts.enable_mem_pattern = False
    opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
    session = ort.InferenceSession(
        str(workspace / "hdcube.onnx"),
        opts,
        providers=(
            [("DmlExecutionProvider", {"device_id": 0}), "CPUExecutionProvider"]
            if "DmlExecutionProvider" in ort.get_available_providers()
            else ["CPUExecutionProvider"]
        ),
    )


def convert(image, target):
    a = np.array(image)
    h, w = a.shape[:2]
    out = Image.new("RGBA", (w * 2, h * 2))
    padded = np.pad(a[:, :, :3], ((16, 16), (16, 16), (0, 0)), mode="edge")
    for y in range(0, h, 128):
        for x in range(0, w, 128):
            pixels = padded[y : y + 160, x : x + 160].copy()
            tensor = np.ascontiguousarray(pixels.transpose(2, 0, 1)[None], np.float32) / 255
            pred = session.run(None, {session.get_inputs()[0].name: tensor})[0][0].transpose(
                1, 2, 0
            )
            pred = pred[64:576, 64:576]
            patch = Image.fromarray(np.rint(pred.clip(0, 1) * 255).astype("uint8")).resize(
                (256, 256), Image.Resampling.LANCZOS
            )
            patch.putalpha(
                Image.fromarray(a[y : y + 128, x : x + 128, 3]).resize(
                    (256, 256), Image.Resampling.NEAREST
                )
            )
            out.paste(patch, (x * 2, y * 2))
    out.save(target)


for stage, files in groups.items():
    if args.stage and stage not in args.stage:
        continue
    item = {"stage": stage, "surfaces": [], "warnings": []}
    for layer, depth, base in [("r", 8, 0), ("f", 8, 0x10000), ("b", 4, 0x8000)]:
        ce, me = files[layer + ".cel"], files[layer + ".map"]
        cells = (original / ce["file"]).read_bytes()
        mapping = (original / me["file"]).read_bytes()
        if not cells or not mapping:
            continue
        paletteAt = me["offset"] + me["compressed_bytes"]
        nextAt = min((e["offset"] for e in ordered if e["offset"] > paletteAt), default=paletteAt)
        paletteBytes = source[paletteAt : min(nextAt, paletteAt + 512)]
        # Stage map palettes are stored immediately after the compressed map.
        if len(paletteBytes) < 2 or len(paletteBytes) > 2048 or len(paletteBytes) % 2:
            item["warnings"].append(layer + ": palette layout requires inspection")
            continue
        colors = np.frombuffer(paletteBytes, ">u2").astype("uint32")
        rgb = np.stack([((colors >> k) & 31) << 3 for k in (0, 5, 10)], axis=1).astype("uint8")
        pages = []
        if layer == "r":
            if len(mapping) % 8192:
                raise ValueError(stage + ": unexpected floor map")
            pages = [mapping[o : o + 8192] for o in range(0, len(mapping), 8192)]
        else:
            cursor = 0
            while cursor < len(mapping):
                width, height = struct.unpack_from(">HH", mapping, cursor)
                cursor += 4
                if not width or not height or width % 64 or height % 64:
                    raise ValueError(stage + ": unexpected map dimensions")
                length = width * height * 2
                grid = np.frombuffer(mapping[cursor : cursor + length], ">u2").reshape(
                    height, width
                )
                cursor += length
                for py in range(0, height, 64):
                    for px in range(0, width, 64):
                        pages.append(grid[py : py + 64, px : px + 64].astype(">u2").tobytes())
        if layer != "b" and (
            max(cells) >= len(colors)
            or (
                layer == "r"
                and any((v >> 12) & 7 for blob in pages for v, in struct.iter_unpack(">H", blob))
            )
        ):
            depth = 4
        for page, mapBytes in enumerate(pages):
            words = np.frombuffer(mapBytes, ">u2").astype("int64")
            number = words & 4095
            flips = False
            if depth == 4:
                a = ((number * 32) < len(cells)).mean()
                masked = words & 1023
                if ((masked * 32) < len(cells)).mean() > a:
                    number = masked
                    flips = True
            addresses = number * 32
            offsets = addresses[:, None] + np.arange(64 if depth == 8 else 32)
            valid = (offsets >= 0) & (offsets < len(cells))
            packed = np.frombuffer(cells, np.uint8)[np.clip(offsets, 0, max(0, len(cells) - 1))]
            if depth == 4:
                dots = np.stack([packed >> 4, packed & 15], axis=-1).reshape(-1, 64)
                index = ((words >> 12) * 16)[:, None] + dots
                valid = np.repeat(valid, 2, axis=1)
            else:
                dots = packed
                index = (((words >> 12) & 7) * 256)[:, None] + dots
            valid &= index < len(rgb)
            if flips:
                # Pattern-name bits 10 and 11 flip the 8x8 character in non-extended mode.
                flipH = (words & 1024) != 0
                flipV = (words & 2048) != 0
                order = np.arange(64)[None, :].repeat(4096, axis=0)
                order[flipH] ^= 7
                order[flipV] ^= 56
                index = np.take_along_axis(index, order, axis=1)
                valid = np.take_along_axis(valid, order, axis=1)
                dots = np.take_along_axis(dots, order, axis=1)
            used = index[valid]
            paletteCount = (
                min(len(colors), max(16, ((int(used.max()) + 16) // 16) * 16))
                if len(used)
                else min(16, len(colors))
            )
            pixels = rgb[np.clip(index, 0, len(rgb) - 1)]
            alpha = (valid & (dots != 0)).astype("uint8") * 255
            rgba = (
                np.dstack((pixels, alpha))
                .reshape(64, 64, 8, 8, 4)
                .transpose(0, 2, 1, 3, 4)
                .reshape(512, 512, 4)
            )
            name = f"{layer}-page-{page:02}"
            folder = destination / stage
            folder.mkdir(exist_ok=True)
            originalFile = folder / (name + "-original.png")
            hdFile = folder / (name + "-hd-2x.png")
            im = Image.fromarray(rgba)
            if (
                originalFile.exists()
                and hdFile.exists()
                and Image.open(originalFile).convert("RGBA").tobytes() != im.tobytes()
            ):
                hdFile.unlink()
            im.save(originalFile)
            if session and not hdFile.exists():
                signature = hashlib.sha256(im.tobytes()).hexdigest()
                if signature in cache:
                    import shutil

                    shutil.copyfile(cache[signature], hdFile)
                elif not (rgba[:, :, 3] > 0).any():
                    im.resize((1024, 1024), Image.Resampling.NEAREST).save(hdFile)
                else:
                    convert(im, hdFile)
                cache[signature] = hdFile
            item["surfaces"].append(
                {
                    "layer": layer,
                    "page": page,
                    "original": str(originalFile.relative_to(destination)),
                    "sha256": hashlib.sha256(im.tobytes()).hexdigest(),
                    "hd": str(hdFile.relative_to(destination)) if hdFile.exists() else None,
                    "palette_count": paletteCount,
                    "depth": depth,
                    "invalid_samples": int((~valid).sum()),
                    "requires_review": bool((~valid).any()),
                    "cel_sha256": ce["sha256"],
                    "map_sha256": me["sha256"],
                    "palette_offset": paletteAt,
                }
            )
    # Retain raw stage panels and palettes for a separate indexed-sprite conversion.
    for key in ("s.tex", "c.tex"):
        if key in files:
            e = files[key]
            folder = destination / stage
            folder.mkdir(exist_ok=True)
            (folder / (key + ".original.bin")).write_bytes((original / e["file"]).read_bytes())
    report["stages"].append(item)
    (destination / "manifest.json").write_text(json.dumps(report, indent=2))
    print(stage, len(item["surfaces"]), "surfaces", len(item["warnings"]), "warnings", flush=True)
print("Completed", len(report["stages"]), "stages", flush=True)
