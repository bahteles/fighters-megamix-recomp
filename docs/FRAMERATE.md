# Frame rate

In the launcher, open **Features > Display > Frame rate**:

| Setting | Behavior |
| --- | --- |
| Original | Original Saturn cadence (approximately 59.8261 fields per second), without interpolation. |
| 30 FPS | Present at 30 FPS while retaining the original simulation and audio clock. |
| 60 FPS | Present at 60 FPS with optional intermediate polygon positions. |
| 120 FPS | Present at 120 FPS with optional intermediate polygon positions. |

Original is the default. The setting is also available in the in-game Display menu.
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
