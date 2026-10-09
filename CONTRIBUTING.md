# Contributing to the Brother Bear fork

This file is about the `brother-bear` branch of this fork, which adds Disney's Brother Bear (PC, 2003) to
SurrealEngine. For SurrealEngine itself, go to the upstream project.

## Important: this fork is AI-assisted

Most of the Brother Bear work was written with AI assistance. Upstream SurrealEngine does not accept AI-written code
(see `NO-AI Code Rule.md`), so **none of this work is or will be offered upstream**. Please don't open issues or pull
requests about Brother Bear on the upstream project, and don't copy code from this branch into upstream pull requests.
Contributions to this fork, AI-assisted or not, are welcome here.

## Rules

- **No game files.** Maps, textures, sounds, music, dialog, script packages, decompiled code, DLL disassembly,
  screenshots and recordings never go into the repository. The engine loads everything from your own install.
- Keep the engine's own style (see the surrounding code) and keep Brother Bear specifics behind the game detection.
- Test against the real game and say how (which map, what you did, what the log showed).

## Getting started

1. Build: [Docs/Building.md](Docs/Building.md). Game notes, status and known gaps: [Docs/BrotherBear.md](Docs/BrotherBear.md).
2. Run a map with the debugger build and check `SE-Log-LastRun.txt` in `%LOCALAPPDATA%\SurrealEngine`.
3. Pick an issue labelled `good first issue` or a 0% row of the roadmap in the README, and say in the issue that you
   are on it.

The most useful help right now is play-testing: play a level from start to end and report where it breaks.
