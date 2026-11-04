# SPDX-License-Identifier: GPL-3.0-only
"""Build optional stage replacements from user-supplied banks and an HDcube model."""

import argparse
from pathlib import Path
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
p.add_argument(
    "--base-pack", type=Path, required=True, help="Existing local texture ZIP, kept unchanged"
)
args = p.parse_args()
tools = Path(__file__).resolve().parent
work = Path("work")
for file in [work / "fmm-data.bin", work / "hdcube.onnx", args.base_pack]:
    if not file.is_file():
        p.error("Missing user-supplied file: " + str(file))
if not (work / "archive-textures/catalog.json").exists():
    subprocess.run(
        [
            sys.executable,
            str(tools / "export_banks.py"),
            str(work / "fmm-data.bin"),
            str(work / "archive-textures"),
        ],
        check=True,
    )
subprocess.run(
    [
        sys.executable,
        str(tools / "export_stage_maps.py"),
        "--workspace",
        str(work),
        "--output",
        str(work / "stage-batch"),
        "--convert",
    ],
    check=True,
)
subprocess.run([sys.executable, str(tools / "convert_stage_panels.py")], check=True)
subprocess.run(
    [sys.executable, str(tools / "package_stage_textures.py"), "--base-pack", str(args.base_pack)],
    check=True,
)
print("Completed: work/pack-all-stages-candidate.zip")
