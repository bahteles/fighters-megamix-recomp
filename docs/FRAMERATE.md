# Frame rate

In the launcher, open **Features > Display > Frame rate**:

| Setting | Behavior |
| --- | --- |
| Original | Original Saturn cadence (approximately 59.8261 fields per second), without interpolation. |
| 30 FPS | Present at 30 FPS while retaining the original simulation and audio clock. |
| 60 FPS | Present at 60 FPS with optional intermediate polygon positions. |
| 120 FPS | Present at 120 FPS with optional intermediate polygon positions. |

Original is the default. The approximately 59.8 Hz hardware/simulation clock and the number of
window presentations are separate from animation poses or genuinely new source pictures. An
external overlay reading 60 FPS in Original mode does not establish the animation's frame rate. The setting is also available in the in-game Display menu.
The command-line equivalent is `--features display_framerate=3` (0 Original, 1 30, 2 60, 3 120).

## Interpolation

The 60 and 120 settings interpolate rendered polygon positions between source pictures, without
running gameplay or sound faster. A monitor capable of the requested refresh rate is needed to
see every displayed picture. Achieving the selected rate depends on available CPU/GPU performance.

This experimental interpolation requires the high-resolution GPU renderer. It does not generate
new animation poses or interpolate bitmap backgrounds. New polygons, large jumps, ambiguous matches
and incompatible commands use their original positions. Menus and Gallery remain on their own
presentation paths. CRT presets receive the interpolated game picture.

Interpolation retains two source pictures, adding approximately one source-picture interval of
visual delay. Run-ahead is temporarily bypassed at 60/120 FPS to avoid mixing speculative picture
histories; its saved setting is retained. Use Original for the original presentation and run-ahead.
Pause, single-frame stepping and unthrottled operation bypass interpolation.

## Validation

Local replay checks compared all 900 simulation and audio hashes across Original/30/60/120;
they matched. Separate checks cover matching reordered commands, appearing/disappearing polygons,
interpolation endpoints, large jumps, ambiguous matches, erase commands and clipping changes.
The standalone checks can run without game data:

```sh
c++ -std=c++20 -I satrecomp/video tools/test_interpolation.cpp -o test-interpolation
./test-interpolation
```

## Presentation cost and recovery

Intermediate pictures use dedicated sprite/VRAM targets. They reuse source materials, background
textures and composition settings; they do not reconstruct the machine's displayed and pending
framebuffers. GPU framebuffer comparisons validate that those originals are preserved.

The CRT path at 60/120 uses a small queue of GPU readbacks, consuming completed images rather
than waiting on a fresh fence for every presentation. This can add a buffered presentation interval
of visual latency. Original mode and screenshots retain synchronous readback. Output-size and
save-state changes invalidate the queue.

The selected rate is a target, not a performance guarantee. In a local 1440p replay with CRT, HD
textures and geometry correction, the previous build fell to about 25 presentations per second.
The optimized build measured approximately 73–116 FPS across test runs while retaining the original
simulation cadence. The latest sample delivered about 116 completed CRT pictures per second, not just 116 window
presentations. These short samples were taken with another 3D application running; they are
regression comparisons, not an isolated maximum-performance benchmark or proof of locked 120 FPS.
