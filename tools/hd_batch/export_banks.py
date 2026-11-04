# SPDX-License-Identifier: GPL-3.0-only
"""Archive the original gzip members of Fighters Megamix's FMM_DATA.DAT."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=Path, help="Extracted FMM_DATA.DAT")
parser.add_argument("output", type=Path)
args = parser.parse_args()
data = args.source.read_bytes()
originals = args.output / "original"
originals.mkdir(parents=True, exist_ok=True)
manifest = []
end = 0
for match in re.finditer(rb"\x1f\x8b\x08\x08.{6}([A-Za-z0-9_.-]{1,80})\x00", data, re.S):
    start = match.start()
    if start < end:
        continue
    decoder = zlib.decompressobj(31)
    try:
        raw = decoder.decompress(memoryview(data)[start:], 32 * 1024 * 1024)
    except zlib.error:
        continue
    if not decoder.eof:
        continue
    end = len(data) - len(decoder.unused_data)
    name = match[1].decode("ascii")
    filename = f"{start:08x}-{name.lstrip('.')}"
    (originals / filename).write_bytes(raw)
    manifest.append(
        {
            "file": f"original/{filename}",
            "name": name,
            "category": "fighter-textures" if name.startswith("tex_") else "unclassified",
            "offset": start,
            "compressed_bytes": end - start,
            "bytes": len(raw),
            "sha256": hashlib.sha256(raw).hexdigest(),
        }
    )
(args.output / "catalog.json").write_text(
    json.dumps(
        {
            "source_sha256": hashlib.sha256(data).hexdigest(),
            "members": manifest,
        },
        indent=2,
    )
    + "\n",
    encoding="utf-8",
)
print(
    f"{len(manifest)} banks archived; {sum(m['category']=='fighter-textures' for m in manifest)} fighter texture banks"
)
