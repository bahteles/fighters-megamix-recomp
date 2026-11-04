# SPDX-License-Identifier: GPL-3.0-only
"""Export direct RGB555 fighter textures, preserving their original pixels."""

import argparse
from pathlib import Path
import json, struct, re
from PIL import Image

parser = argparse.ArgumentParser(
    description="Export RGB555 fighter textures from the archived game banks."
)
parser.add_argument("source", type=Path, help="Extracted FMM_DATA.DAT")
parser.add_argument(
    "archive", type=Path, help="Folder with catalog.json and original banks"
)
args = parser.parse_args()
root = args.archive
d = args.source.read_bytes()
manifest = json.loads((root / "catalog.json").read_text())["members"]
out = root / "png"
out.mkdir(exist_ok=True)
report = []
for i, m in enumerate(manifest):
    if m["category"] != "fighter-textures":
        continue
    raw = (root / m["file"]).read_bytes()
    lo = max(0, m["offset"] - 0x24000)
    if i:
        lo = max(lo, manifest[i - 1]["offset"] + manifest[i - 1]["compressed_bytes"])
    desc = set()
    # Both ordinary and Gouraud-shaded RGB555 commands reference bank textures.
    # Rent-A-Hero uses both: omitting 0x1028 leaves a gap and rejects his bank.
    for hit in re.finditer(rb"\x10[\x28\x2c]\xff\x00", d[lo : m["offset"]]):
        addr, shape = struct.unpack_from(">HH", d, lo + hit.start() + 4)
        at, w, h = addr * 8 - 0x8000, (shape >> 8) * 8, shape & 255
        if at >= 0 and w and h and at + w * h * 2 <= len(raw):
            desc.add((at, w, h))
    cursor = 0
    for at, w, h in sorted(desc):
        if at < cursor or at - cursor >= 32 or any(raw[cursor:at]):
            break
        cursor = at + w * h * 2
    else:
        if desc:
            folder = out / Path(m["name"]).stem
            folder.mkdir(exist_ok=True)
            for at, w, h in sorted(desc):
                im = Image.new("RGBA", (w, h))
                im.putdata(
                    [
                        (
                            ((v & 31) << 3),
                            (((v >> 5) & 31) << 3),
                            (((v >> 10) & 31) << 3),
                            255 if v & 32768 else 0,
                        )
                        for (v,) in struct.iter_unpack(">H", raw[at : at + w * h * 2])
                    ]
                )
                im.save(folder / f"{at:05x}-{w}x{h}.png")
            report.append(
                dict(
                    bank=m["name"],
                    textures=len(desc),
                    status="validated" if cursor == len(raw) else "partial",
                    unclassified_bytes=len(raw) - cursor,
                )
            )
            continue
    report.append(dict(bank=m["name"], textures=0, status="unresolved"))
(out / "export-report.json").write_text(json.dumps(report, indent=2))
print(
    len(report),
    sum(x["status"] == "validated" for x in report),
    sum(x["textures"] for x in report),
    sum(x["status"] == "partial" for x in report),
)
