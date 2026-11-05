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

At 120 FPS with CRT, a persistent producer prepares two exact-time interpolated pictures while
OpenGL presents the preceding pair on the main thread. Window events and modal interfaces stay
on the main thread, and the producer finishes before input or settings are changed. This adds
approximately 16.7 ms of buffering, in addition to interpolation history. Other presentation modes
retain their existing paths. The 60 FPS CRT path uses asynchronous GPU readbacks.

The 120 FPS CRT presenter spaces swaps by at least 8.33 ms. A missed deadline starts a new interval;
it does not trigger a short-interval burst to recover the average FPS count. Preparation uses the
planned picture time rather than the time at which a slow simulation update finishes. Each prepared
picture is read back synchronously on the producer, avoiding stale pictures from a non-ready queue.

Set `SATRECOMP_PRESENT_TRACE` to a local CSV path to record presentation timestamps, simulation
frame numbers, input-picture CRCs and CPU render/submission duration. Tracing adds CRC overhead and
measures swap-call timing, not physical monitor scanout. It is disabled by default.

In a local 1440p replay with CRT, HD textures and geometry correction, the first sample measured
8.334 ms median, 8.423 ms p95 and 11.800 ms p99 between swaps (115.2 FPS average), with no adjacent
repeated input pictures in the sampled moving scene. A second run under competing system load
measured 8.335 ms median but 28.146 ms p95 (83.5 FPS average). Peaks remain possible: this is a
pacing correction, not proof of locked 120 FPS or an isolated hardware benchmark. Replay simulation
and audio hashes matched the previous implementation for all 1,000 compared simulation frames.
