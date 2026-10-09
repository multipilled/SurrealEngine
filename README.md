> **This is a fork: the `brother-bear` branch adds support for Disney's Brother Bear (PC, 2003).**
> It is an AI-assisted fork. Upstream SurrealEngine does not accept AI-written code (see `NO-AI Code Rule.md`), so
> none of this work is offered upstream; please don't open pull requests or issues about it on the upstream project.
> You need your own copy of the game: no game files are in this repository. Status (about 65%): every package loads,
> the game's script runs, all 16 maps load and run, rendering, animation, audio, particles, cutscenes, climbing and
> save/load work; a verified play-through of the levels and arenas is next. Details: [Docs/BrotherBear.md](Docs/BrotherBear.md).
> Build: [Docs/Building.md](Docs/Building.md). The rest of this README is upstream's.
>
> **Brother Bear roadmap** (goal: front end to end credits, all 10 levels, 5 arenas, the Secret Totem Cave, save/load; weighted, overall **65%**)
>
> | Milestone | Weight | Done |
> |---|---|---|
> | Packages load, script VM (opcode remap), every native registered | 10 | 100% |
> | Rendering: BSP, lightmaps, sunlight, meshes, skeletal animation and blending | 10 | 100% |
> | Audio: Bink SFX and dialog, Ogg music, subtitles, XA sounds | 7 | 100% |
> | Boot, intros, cutscenes, spline cameras | 8 | 100% |
> | All 16 maps load and run without crashing | 8 | 100% |
> | Spawns and zones | 5 | 100% |
> | Climbing root motion, bone position/rotation, climb triggers | 7 | 100% |
> | Particles (mesh spawning, emitter chains, auto reset) | 4 | 100% |
> | Front end, saving and loading | 6 | 100% |
> | Play-through of the 10 main levels | 25 | 0% (not yet verified end to end) |
> | Combat arenas (5) and the Secret Totem Cave | 6 | 0% (untested) |
> | Sunlight shadows on meshes, polish | 2 | 0% |
> | Docs and improvement ideas | 2 | 0% |
>
> Want to help with the fork? See [CONTRIBUTING.md](CONTRIBUTING.md).

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
