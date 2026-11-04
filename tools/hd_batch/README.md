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

Fighter originals can be exported independently:

    python tools/hd_batch/export_fighter_textures.py work/fmm-data.bin work/archive-textures

The PNG export accepts both ordinary and Gouraud-shaded RGB555 texture descriptors,
including Rent-A-Hero's two banks. Raw banks remain unchanged. The report distinguishes
complete banks, validated texture prefixes with an unclassified tail, and unresolved layouts.
An unclassified tail is kept in the raw bank and is not guessed as image data.

To refine the KABE concrete panels and preserve the original lettering as scalable paths,
install SciPy, vtracer and resvg_py in addition to NumPy and Pillow, then run:

    python tools/hd_batch/refine_stage_wall.py --base-pack path/to/local-pack.zip

The output is work/pack-wall-candidate.zip. All eight wall panels and their three palette
variants are processed. The paired Fighting Vipers artwork is traced as one image to avoid
a seam. The broad concrete lighting ramp is smoothed while joints and bolt heads are kept;
shared-channel dithering reduces banding when encoding the 15-bit replacement colors.
Original PNGs, generated SVGs, HD previews and a source checksum report stay under
work/wall-refined/. The base ZIP and original raw banks stay unchanged. Repeated runs replace
matching texture records rather than accumulating duplicates.
