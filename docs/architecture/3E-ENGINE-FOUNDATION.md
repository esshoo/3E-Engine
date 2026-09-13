# 3E-Engine Foundation

## Product Structure

3E-Engine is the complete platform.

3E-Studio is the generic editor and tooling host.

3E-Player is the universal game runtime host.

Games are integrations/modules beneath `games/`.

Jackie Chan Stuntmaster is the first game integration, not the engine itself.

## Foundation Rules

The existing `src/` tree remains intact during the foundation phase.

No mass source moves are allowed until build and dependency boundaries are understood.

3E-Studio must not hardcode Jackie-specific menus or tools.

ImGui menus, toolbars, panels, shortcuts and layouts are loaded from external files.

Every new game receives editable UI files copied from `games/_template`.

Games are automatically discovered from `games/*/game.json`.

The default Studio UI lives in `config/ui/default`.

Game UI definitions inherit from the default UI and may add or override entries.

Changes to JSON, schemas and scripts are intended to support live reload without rebuilding 3E-Studio.

Native C++ is reserved for the engine, native parsers, runtime bridges and performance-critical systems.

Original game data must remain read-only. Editing will use project overlays.

Studio and Player must eventually use the same game reader and engine renderer.

Play testing must use the real game runtime rather than an editor simulation.

## Command Model

UI entries reference command IDs.

Command implementations may later use:

- builtin
- process
- script
- file operation
- pipeline
- native module
- game integration

The ImGui layer must not directly contain game behavior.

## New Games

A new game starts as a copy of `games/_template`.

Eventually 3E-Studio will perform this internally through the Add Game UI.

Until that UI exists:

    .\scripts\3e-new-game.ps1 -Id mygame -Name "My Game"

## Build Philosophy

Data/UI/script changes:

    Save -> Live Reload

Native module changes:

    Incremental Compile -> Module Reload

3E-Studio host changes:

    Rare rebuild only

The goal is not to pretend C++ can execute without compilation.
The goal is to keep almost all daily editor and reverse-engineering iteration outside the host executable.
