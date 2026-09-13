# Jackie runtime bridge

The Jackie integration deliberately reuses the proven runtime and viewer instead
of duplicating the game's parser/rendering pipeline inside 3E Studio.

## Play

`3E Studio -> 3E-Player.exe -> project GameRoot/3EChan.exe`

`3E-Player` is a generic supervisor. `games/jackie/game.json` supplies the
runtime executable and working directory using project variables. On Windows
the child process is assigned to a kill-on-close Job Object so Studio Stop can
terminate the supervised native game as one unit.

The original Jackie game files remain read-only and outside the Studio package.

## Map Viewer

The Jackie `Map Viewer` command launches the newest known proven browser viewer
found under the active project's GameRoot:

1. `ExportedAssets/3EChan-Level-Viewer-Pro-v3`
2. `ExportedAssets/3EChan-Level-Viewer-Pro-v2`
3. `ExportedAssets/3EChan-Level-Viewer-Pro-v1`

The launcher prefers `Start-Viewer.ps1`, then `Start-Viewer.cmd`, then
`index.html`. This preserves the working viewer rather than introducing a new
map parser before there is a reason to replace it.

## Boundaries

- `3E Studio`: project/tool UI and process control.
- `3E Player`: generic runtime supervisor.
- `3EChan.exe`: authoritative Jackie native runtime.
- Browser Level Viewer: authoritative current Jackie map viewer.
- Original game root: external, read-only source/runtime data.
- Project overlay: writable editor/project data.
