# bb-surreal

Disney's **Brother Bear** (PC, 2003) running on [SurrealEngine](https://github.com/dpjudas/SurrealEngine), an open
source reimplementation of Unreal Engine 1. Brother Bear was built by KnowWonder on a modified Unreal Engine 1 (the same
family as the Harry Potter 1 and 2 PC games); this fork teaches SurrealEngine that dialect so the game runs from your
own install.

**Progress: about 65% of the goal** (front end to end credits: all 10 levels, the 5 combat arenas, the Secret Totem
Cave, saving and loading). Every package loads, the game's script runs, all 16 maps load and run, and rendering,
animation, audio, particles, cutscenes, climbing and save/load work. Next: a verified play-through of every level.

| Milestone | Weight | Done |
|---|---|---|
| Packages load, script VM (opcode remap), every native registered | 10 | 100% |
| Rendering: BSP, lightmaps, sunlight, meshes, skeletal animation and blending | 10 | 100% |
| Audio: Bink SFX and dialog, Ogg music, subtitles, XA sounds | 7 | 100% |
| Boot, intros, cutscenes, spline cameras | 8 | 100% |
| All 16 maps load and run without crashing | 8 | 100% |
| Spawns and zones | 5 | 100% |
| Climbing root motion, bone position/rotation, climb triggers | 7 | 100% |
| Particles (mesh spawning, emitter chains, auto reset) | 4 | 100% |
| Front end, saving and loading | 6 | 100% |
| Play-through of the 10 main levels | 25 | 0% (not yet verified end to end) |
| Combat arenas (5) and the Secret Totem Cave | 6 | 0% (untested) |
| Sunlight shadows on meshes, polish | 2 | 0% |
| Docs and improvement ideas | 2 | 0% |

Details and known gaps: [Docs/BrotherBear.md](Docs/BrotherBear.md). Build: [Docs/Building.md](Docs/Building.md).
Help wanted, especially play-testing: [CONTRIBUTING.md](CONTRIBUTING.md) and the open issues.

## Fan project and AI notice

- Non-commercial fan project, not affiliated with or endorsed by Disney or KnowWonder. You need your own copy of the
  game: this repository contains no game files, extracted assets or decompiled code; everything loads at run time.
  Rights holders: open an issue and any requested change will be made promptly.
- **AI-assisted fork.** Upstream SurrealEngine does not accept AI-written code (see `NO-AI Code Rule.md`), so none of
  this work is offered upstream. Please don't open issues or pull requests about Brother Bear on the upstream project.
- The `brother-bear` branch holds the Brother Bear work; `master` mirrors upstream.

## About SurrealEngine (upstream README)

![SEBANNER](Resources/surreal-engine-banner.png)

# Welcome to Surreal Engine!

Surreal Engine is a project that aims to reimplement Unreal Engine 1; currently focused on making Unreal (Gold) and Unreal Tournament (UT99) playable. The scope of this project might expand to cover more UE1 games in the future.

## Current Status

Please refer to [Status.md](Docs/Status.md) for the current status of Surreal Engine!

## System Requirements

* Original copies of the UE1 games you want to run
* Windows 10+, macOS 15+ or a modern Linux distro
* A Direct3D 11, OpenGL 3.2 or Vulkan capable graphics card (macOS requires a Metal 2+ GPU)

## Building Surreal Engine

Please refer to [Building.md](Docs/Building.md) for details!

## Downloads

[Nightly builds are available on the Releases section](https://github.com/dpjudas/SurrealEngine/releases/tag/nightly).

Additionally, Surreal Engine is available on following Linux distributions:

* Arch: [AUR](https://aur.archlinux.org/packages/surrealengine-git)
* Nix: [Package Search](https://search.nixos.org/packages?channel=unstable&show=surreal-engine) | [Quickstart](https://github.com/NixOS/nixpkgs/pull/337069)

## How to Play

* Run the `SurrealEngine` executable.
* Add the UE1 games you want in the Folders tab.
* Select the game you want to play in Games tab.
* Click "Play"!

## Discord Server

Visit us on Discord at https://discord.gg/5AEry4s

## Command Line Parameters

`SurrealEngine [--url=<mapname>] [--engineversion=X] [Path to game folder]`

If no game folder is specified, and the executable isn't in a System folder, the engine will search the registry (Windows only) for the registry keys Epic originally set.

If no URL is specified it will use the default URL in the ini file (per default the intro map).

The `--engineversion` argument overrides the internal version detected by the engine and should only be used for debugging purposes.
