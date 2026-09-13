# 3E Player

`3E-Player` is the universal runtime supervisor used by 3E Studio.

For Jackie Chan Stuntmaster the current integration mode is `legacy-bridge`:

1. Studio launches `3E-Player.exe` with the active game and project.
2. 3E Player reads `games/jackie/game.json` and `project.3e.json`.
3. The runtime executable is resolved from the project GameRoot.
4. 3E Player launches the proven native Jackie runtime (`3EChan.exe`) with the
   GameRoot as its working directory and waits for it.
5. On Windows the child runtime is placed in a kill-on-close Job Object, so
   Studio Stop (`Shift+F5`) terminates the supervised Jackie runtime too.

Verification does not launch the game:

```text
3E-Player.exe --game jackie --project <project.3e.json> --verify-runtime
```

The game manifest stays data-driven. A future game can provide another runtime
executable without hardcoding Jackie paths into the generic Player host.
