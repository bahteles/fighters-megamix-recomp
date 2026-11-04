# SPDX-License-Identifier: GPL-3.0-only
"""Precompute phase-aligned indexed stage-panel replacements, preserving source masks."""

import sys, json, math, struct, zlib
from pathlib import Path

sys.path.insert(0, "work/dmlpy")
import numpy as np, onnxruntime as ort
from PIL import Image

root = Path("work/archive-textures")
catalog = json.loads((root / "catalog.json").read_text())["members"]
source = Path("work/fmm-data.bin").read_bytes()
dest = Path("work/stage-sprites")
dest.mkdir(exist_ok=True)
opts = ort.SessionOptions()
opts.enable_mem_pattern = False
opts.execution_mode = ort.ExecutionMode.ORT_SEQUENTIAL
opts.intra_op_num_threads = 4
model = ort.InferenceSession(
    "work/hdcube.onnx",
    opts,
    providers=(
        [("DmlExecutionProvider", {"device_id": 0}), "CPUExecutionProvider"]
        if "DmlExecutionProvider" in ort.get_available_providers()
        else ["CPUExecutionProvider"]
    ),
)
manifest = []


def upscale(a):
    h, w = a.shape[:2]
    ph = (h + 127) // 128 * 128
    padded = np.pad(a, ((16, ph - h + 16), (16, 16), (0, 0)), mode="edge")
    result = np.empty((ph * 4, w * 4, 3), np.uint8)
    for y in range(0, ph, 128):
        t = np.ascontiguousarray(padded[y : y + 160].transpose(2, 0, 1)[None], np.float32) / 255
        b = model.run(None, {"input": t})[0][0].transpose(1, 2, 0)[64:576, 64 : 64 + w * 4]
        result[y * 4 : y * 4 + 512] = np.rint(b.clip(0, 1) * 255).astype("uint8")
    return result[: h * 4]


for e in catalog:
    if not e["name"].endswith(("_s.tex.bin", "_c.tex.bin")):
        continue
    stage = e["name"][:-10]
    raw = (root / e["file"]).read_bytes()
    if not raw:
        continue
    paletteAt = e["offset"] + e["compressed_bytes"]
    palettes = []
    for bank in range(16):
        words = np.frombuffer(
            source[paletteAt + bank * 32 : paletteAt + (bank + 1) * 32], ">u2"
        ).astype("uint32")
        if len(words) != 16 or ((words >= 0x8000) & (words != 0xFFFF)).any():
            break
        rgb = np.stack([((words >> k) & 31) << 3 for k in (0, 5, 10)], axis=1).astype("uint8")
        colors = (
            0xFF000000
            | ((words & 31) << 3)
            | (((words >> 5) & 31) << 11)
            | (((words >> 10) & 31) << 19)
        )
        palettes.append((rgb, zlib.crc32(colors.astype("<u4").tobytes())))
    dots = np.frombuffer(raw, np.uint8)
    dots = np.stack([dots >> 4, dots & 15], axis=1).reshape(-1)
    for width in (8, 16, 24, 32, 40, 48, 64, 80, 96, 128, 256):
        for phase in range(0, width, math.gcd(16, width)):
            prefix = (-phase) % width
            height = (len(dots) + prefix + width - 1) // width
            padded = np.zeros(height * width, np.uint8)
            padded[prefix : prefix + len(dots)] = dots
            indices = padded.reshape(height, width)
            mask = (indices > 0) & (indices != 15)
            hdMask = (
                np.array(
                    Image.fromarray(mask.astype("uint8") * 255).resize(
                        (width * 4, height * 4), Image.Resampling.LANCZOS
                    )
                )
                >= 100
            )
            hdMask &= np.repeat(np.repeat(mask, 4, axis=0), 4, axis=1)
            for palette, (rgb, paletteCrc) in enumerate(palettes):
                name = f"{zlib.crc32(raw):08x}-{width}-{phase}-{paletteCrc:08x}.rgb555"
                target = dest / name
                if not target.exists() or target.stat().st_size != height * width * 32:
                    image = rgb[indices].copy()
                    # Extend RGB into transparency to avoid bright matte colors at thin panel edges.
                    opaque = image[mask]
                    matte = (
                        np.median(opaque, axis=0).astype("uint8")
                        if len(opaque)
                        else np.zeros(3, np.uint8)
                    )
                    image[~mask] = matte
                    hd = upscale(image)
                    nearest = np.repeat(np.repeat(image, 4, axis=0), 4, axis=1)
                    hd = np.minimum(
                        hd, np.minimum(nearest.astype("uint16") + 8, 255).astype("uint8")
                    )
                    c = hd.astype("uint16") >> 3
                    encoded = 0x8000 | c[:, :, 0] | (c[:, :, 1] << 5) | (c[:, :, 2] << 10)
                    encoded[~hdMask] = 0
                    target.write_bytes(encoded.astype(">u2").tobytes())
                manifest.append(
                    {
                        "stage": stage,
                        "bank": 0x5F000 if e["name"].endswith("_c.tex.bin") else 0x58000,
                        "bytes": len(raw),
                        "cells_crc": zlib.crc32(raw),
                        "palette_crc": paletteCrc,
                        "width": width,
                        "phase": phase,
                        "file": name,
                    }
                )
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2))
    print(stage, len(palettes), "palettes", len(manifest), "bank variants", flush=True)
print("Complete", len(manifest), "indexed variants")
