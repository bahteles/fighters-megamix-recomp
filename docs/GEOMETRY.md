# Geometry correction

Enable **Features > Display > Geometry correction** in the launcher, or the Geometry correction
switch in the in-game menu. The command-line option is `--features geometry_correction`.
It is optional and off by default. Use the high-resolution GPU renderer.

The feature observes Megamix's original vertex transforms before their results are rounded to
integer screen positions. It retains fractional matrix products and translations on the host,
smooths the original projection lookup between depth buckets, and passes subpixel coordinates
to matched 3D drawing commands. Game memory, original command tables, gameplay and audio are
unchanged. This reduces coordinate snapping rather than hiding it with a temporal blur.

Matches require the whole polygon's original coordinates and texture signature. Ambiguous
matches, excessive corrections, clipped/unobserved geometry and expired entries use the original
coordinates. The cache expires after two simulation fields and clears when loading a state.
This game-specific feature is inspired by the goal of PGXP; it does not include emulator code.

## Scope

The correction applies to matched textured polygons and flat polygons handled by the observed
transform routines. It does not replace bitmap backgrounds, change the combat renderer's texture
projection, or remove all sources of animation jitter. The Gallery already projects its retained
3D models independently. Geometry correction and the 60/120 FPS interpolation can be combined:
one preserves spatial precision, the other supplies intermediate positions in time.

## Validation

A 1,000-frame replay with correction off/on produced identical machine/audio hashes. A second
comparison covers widescreen with 120 FPS interpolation enabled. Visual captures confirm changed
3D rendering while interface artwork retains its original placement. The projection arithmetic
also has standalone tests for fractional movement, negative rounding and fixed-point scaling:

```sh
c++ -std=c++20 -I games/fighters_megamix/src tools/test_geometry_math.cpp -o test-geometry-math
./test-geometry-math
```

No disc data, generated game code, extracted models or texture packs are included.
