# SPDX-License-Identifier: GPL-3.0-only
"""Refine KABE wall lettering and concrete from preserved indexed source banks."""

from pathlib import Path
import argparse, io, json, struct, hashlib
import numpy as np, vtracer, resvg_py
from scipy.ndimage import gaussian_filter1d
from PIL import Image
from zipfile import ZipFile, ZIP_DEFLATED

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--workspace", type=Path, default=Path("work"))
parser.add_argument("--base-pack", type=Path, required=True)
parser.add_argument("--output", type=Path)
args = parser.parse_args()
work = args.workspace
root = work / "archive-textures"
catalog = json.loads((root / "catalog.json").read_text())["members"]
data = (work / "fmm-data.bin").read_bytes()
entry = next(e for e in catalog if e["name"] == "KABE_s.tex.bin")
raw = (root / entry["file"]).read_bytes()
a = np.frombuffer(raw, np.uint8)
indices = np.stack([a >> 4, a & 15], axis=1).reshape(8, 96, 48)
out = work / "wall-refined"
out.mkdir(exist_ok=True)
palette_at = entry["offset"] + entry["compressed_bytes"]
records = bytearray()
report = []
rng = np.random.default_rng(417)
noise = rng.random((384, 192)) - 0.5
for bank in range(16):
    words = np.frombuffer(
        data[palette_at + bank * 32 : palette_at + (bank + 1) * 32], ">u2"
    ).astype(np.uint32)
    if ((words >= 0x8000) & (words != 0xFFFF)).any():
        break
    rgb = np.stack([((words >> s) & 31) * 8 for s in (0, 5, 10)], axis=1).astype(
        np.uint8
    )
    original = rgb[indices[0]].astype(float)
    base = np.median(original[:, 4:-4, 0], axis=1)
    smoothed = base.copy()
    for lo, hi in ((1, 47), (49, 95)):
        smoothed[lo:hi] = gaussian_filter1d(base[lo:hi], 3, mode="nearest")
    detail = np.clip(original[:, :, 0] - base[:, None], -5, 5) * 0.3
    material = np.repeat(smoothed[:, None], 48, axis=1) + detail
    # Keep the mortar seams and bolt heads, while smoothing only the broad lighting ramp.
    yy, xx = np.indices((96, 48))
    features = (
        (yy % 48 < 2)
        | (yy % 48 > 45)
        | (xx < 2)
        | (xx > 45)
        | (((xx < 6) | (xx > 41)) & ((yy % 48 < 8) | (yy % 48 > 40)))
    )
    material[features] = original[:, :, 0][features]
    rgbmat = (
        np.stack(
            [material + (original[:, :, c] - original[:, :, 0]) for c in range(3)],
            axis=-1,
        )
        .clip(0, 248)
        .astype(np.uint8)
    )
    hdmat = (
        Image.fromarray(rgbmat)
        .resize((192, 384), Image.Resampling.LANCZOS)
        .convert("RGBA")
    )
    hd = [hdmat.copy() for _ in range(8)]
    for group in ((1, 2), (3,), (4,), (5,), (6,), (7,)):
        grid = np.concatenate([indices[n] for n in group], axis=1)
        art = rgb[grid]
        mask = grid >= 10
        rgba = np.concatenate([art, mask[:, :, None].astype(np.uint8) * 255], axis=2)
        rgba[~mask, :3] = 0
        image = out / f"palette-{bank}-art-{group[0]}.png"
        svg = image.with_suffix(".svg")
        Image.fromarray(rgba).save(image)
        vtracer.convert_image_to_svg_py(
            str(image),
            str(svg),
            colormode="color",
            hierarchical="stacked",
            mode="spline",
            filter_speckle=0,
            color_precision=8,
            layer_difference=1,
            corner_threshold=40,
            length_threshold=0.25,
            max_iterations=10,
            splice_threshold=15,
            path_precision=3,
        )
        graphic = (
            Image.open(
                io.BytesIO(
                    resvg_py.svg_to_bytes(
                        svg_path=str(svg.resolve()), width=len(group) * 384, height=768
                    )
                )
            )
            .convert("RGBA")
            .resize((len(group) * 192, 384), Image.Resampling.LANCZOS)
        )
        for n, panel in enumerate(group):
            hd[panel].alpha_composite(graphic.crop((n * 192, 0, (n + 1) * 192, 384)))
    for panel in range(8):
        # Source mode disables end codes on graphic panels: index 15 is a real paint color.
        native_rgb = rgb[indices[panel]].astype(np.uint16) >> 3
        native = (
            0x8000
            | native_rgb[:, :, 0]
            | (native_rgb[:, :, 1] << 5)
            | (native_rgb[:, :, 2] << 10)
        )
        native[indices[panel] == 0] = 0
        pixels = np.array(hd[panel], dtype=float)[:, :, :3]
        quantized = np.clip(
            np.floor(pixels / 8 + 0.5 + noise[:, :, None]), 0, 31
        ).astype(np.uint16)
        encoded = (
            0x8000
            | quantized[:, :, 0]
            | (quantized[:, :, 1] << 5)
            | (quantized[:, :, 2] << 10)
        )
        records += (
            struct.pack("<HH", 48, 96)
            + native.astype(">u2").tobytes()
            + encoded.astype(">u2").tobytes()
        )
        hd[panel].save(out / f"palette-{bank}-panel-{panel}-hd.png")
        Image.fromarray(rgb[indices[panel]]).save(
            out / f"palette-{bank}-panel-{panel}-original.png"
        )
    report.append({"palette": bank, "panels": 8})
    print("palette", bank, "complete", flush=True)
base = args.base_pack
dest = args.output or work / "pack-wall-candidate.zip"
if base.resolve() == dest.resolve():
    parser.error("The output must differ from the preserved base pack.")
with ZipFile(base) as z, ZipFile(dest, "w", ZIP_DEFLATED, compresslevel=3) as w:
    for e in z.infolist():
        content = z.read(e.filename)
        if e.filename == "fighters.fmmtex":
            assert content[:8] == b"FMMHD001"
            # Replace matching originals on repeat runs instead of appending duplicates.
            keys = set()
            offset = 0
            while offset < len(records):
                width, height = struct.unpack_from("<HH", records, offset)
                size = width * height
                keys.add(
                    (width, height, bytes(records[offset + 4 : offset + 4 + 2 * size]))
                )
                offset += 4 + 34 * size
            filtered = bytearray(content[:8])
            offset = 8
            while offset < len(content):
                width, height = struct.unpack_from("<HH", content, offset)
                size = width * height
                end = offset + 4 + 34 * size
                if (
                    width,
                    height,
                    content[offset + 4 : offset + 4 + 2 * size],
                ) not in keys:
                    filtered += content[offset:end]
                offset = end
            assert offset == len(content)
            content = filtered + records
        w.writestr(e.filename, content)
with ZipFile(dest) as z:
    assert z.testzip() is None
(out / "report.json").write_text(
    json.dumps(
        {"source_sha256": hashlib.sha256(raw).hexdigest(), "palettes": report}, indent=2
    )
)
print("Pack", dest.stat().st_size)
