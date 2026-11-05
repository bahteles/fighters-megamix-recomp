# Model Gallery

The community build adds a Gallery row to the original MODE SELECT screen. It opens in the game
window, applies the selected display shader (including CRT presets), and restores the game's
aspect and display scaling when it closes. The added menu row stays aligned to the 4:3 menu area
when the window or widescreen output is wider.

## Local resources

The repository contains the Gallery implementation only. Its catalog, extracted models, interface
artwork and optional HD texture ZIP must remain on the player's computer. They are not included
in source checkouts or produced automatically by compiling the program.

An existing local installation uses a `gallery/` folder beside the game executable containing:

- `catalog.txt`, listing model variants.
- `.fmg` model files referenced by the catalog.
- `gallery-theme.fmui` for Gallery interface artwork.
- `gallery-entry.fmui` for the additional MODE SELECT row.

The menu entry requires its local artwork. Browsing the catalog requires valid local model files;
a fresh source checkout alone does not provide a populated Gallery. Keep existing local resources
when replacing a locally built executable. Optional HD previews use the texture ZIP selected by
the player; the pack is not embedded in the program.

Catalog rows use `Name|full.fmg|broken.fmg|alternate-full.fmg|alternate-broken.fmg`.
Each pair describes a color, and an empty broken-model field means that color has no broken armor
variant. Model references must be filenames within the catalog folder.

## Controls

Rotate and zoom the model using the on-screen controls. Use **V/B**, the **Color** buttons, or
left/right on a controller to switch colors. **A** switches armor states where available.
The Gallery also supports switching the optional HD textures without changing the model catalog.

## Keeping resources private

Never commit the local `gallery/` folder, `.fmg`, `.fmui`, `.rgba`, texture archives, disc images,
saves, memory dumps, or generated game code. Ignore rules cover local Gallery resources and
archives; `tools/audit.sh` also rejects their formats among tracked files.

## Updating the local typography

The character name appears once above the model. Settings use compact option buttons, with
the active choice highlighted in gold. Colors use lettered buttons (A, B, C, and so on).
The interface does not display an SD/HD badge or texture-pack shortcut hint.
After updating the executable, update an existing local theme using Python and Pillow:

```sh
python tools/gallery/update_typography.py /path/to/gallery/gallery-theme.fmui --serif /path/to/serif.ttf --sans /path/to/sans.ttf
```

Use a bold italic serif face for titles and a regular sans-serif face for settings. Back up the
local theme first. The script retains existing artwork and adds the labels needed by the current
layout; it does not bundle or upload artwork, models, fonts, or texture packs.

## Model visibility

Gallery resolves triangle visibility with per-pixel depth testing and perspective-correct texture
coordinates. This fixes rear faces covering the face or torso when whole polygons were sorted by
average depth. Transparent texels leave the surfaces behind them visible. The correction applies
to the Gallery renderer and does not change model files, texture packs or combat rendering.

Regression checks can run without game resources:

```sh
c++ -std=c++20 -I games/fighters_megamix/src tools/test_gallery_raster.cpp -o test-gallery-raster
./test-gallery-raster
```
