**What this changes and why**

**Checklist**
- [ ] Nothing from the game (files, generated code, block lists, dumps, save states, screenshots), no BIOS, nothing from an SDK and no emulator's code is added. `make audit` passes.
- [ ] `make check` and `make check-rewrites` pass, if the translator, the machine or the game's code changed.
- [ ] Gameplay or control changes are an optional feature, off in Pure; with every feature off the game runs as before.
- [ ] No added input lag or slowdown.
- [ ] Docs and `CHANGELOG.md` updated if behavior changed.
