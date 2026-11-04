# Optional stage texture batch

Run from the repository root. The tools read the user's extracted FMM_DATA.DAT, preserve source
banks, and generate optional replacements. No game data or model is included.

Supply work/fmm-data.bin, a compatible 4x HDcube ONNX model as work/hdcube.onnx, and an existing
local texture ZIP. Install Python with NumPy, Pillow and ONNX Runtime. On Windows,
onnxruntime-directml enables GPU inference. CPU inference is supported as a fallback.

    python tools/hd_batch/run.py --base-pack path/to/local-pack.zip

The output is work/pack-all-stages-candidate.zip. The baseline ZIP is preserved. Source banks
are under work/archive-textures/, decoded originals under work/stage-batch/, and panel caches
under work/stage-sprites/. Keep those directories to resume interrupted work. Use a fresh work
directory when testing a different model to avoid mixing cached model results.

The decoder handles multi-image map headers, 16-color character flips and 256-color data.
Floors and backgrounds are inferred at 4x then reduced to 2x. Indexed panels are inferred at 4x
for supported widths and alignments. Source transparency and end codes are preserved, with
edge color limits to avoid light halos.

The runtime verifies source and palette checksums before applying replacements and loads the
pages and panel variants needed by the current stage. Missing or unsupported resources retain
the original rendering. Flat-color geometry does not have replacement textures.
