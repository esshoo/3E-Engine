# 3E Player

Universal runtime host for 3E-Engine.

3E-Player is intentionally not hardcoded to Jackie Chan.

Launch forms:

```text
3E-Player.exe --game <game-id>
3E-Player.exe --project <path-to-project.3e.json>
3E-Player.exe --game <game-id> --project <path-to-project.3e.json>
```

Headless data verification:

```text
3E-Player.exe --game <game-id> --project <path> --verify-runtime
```

Games are resolved from:

```text
games/<game-id>/game.json
```

The project manifest can supply the game id and external read-only source paths.
Game-specific native runtime integration is attached behind this universal host.