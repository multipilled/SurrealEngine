# Disney's Brother Bear (PC) support

This fork adds support for Disney's Brother Bear (PC, 2003). The game runs on KnowWonder's modified Unreal Engine 1, the same engine family as the Harry Potter 1 and 2 PC games.

You need your own copy of the game. This repository holds no game files and never will: maps, textures, sounds, music, dialog and the original script packages all load at runtime from your installed copy.

## Status

- Detects the US English retail release (`System/Game.exe`, SHA1 `e77df020e7c1c299bd88f229564cfac4476d4eda`).
- Loads every package (package version 80).
- Runs the game's UnrealScript, with Brother Bear's shifted opcodes remapped.
- Registers every engine native the game declares. Some are still stubs; the `nativeaudit` debugger command lists any gaps.
- Boots into the first level and plays the Aspen Forest intro cutscene through, with the camera flying along its splines.
- Renders the levels with their lightmaps, including KnowWonder's sunlight.
- Draws skeletal meshes (characters, plants, props) and plays their animations, including anim channels such as eye blinks and head look that animate part of the skeleton.
- Plays the game's music, sound effects and dialog (stored as Bink audio), with subtitles.

Not working yet:

- Combined animations (`AT_Combine`), transient anim channels and tweening between skeletal animations.
- Shadows from sunlight on meshes.
- The 9 sounds stored as `XA` audio, which stay silent.
- The particle system.

## Running

Point SurrealEngine at the game's install folder, the one that contains `System`, `Maps`, `Textures` and the rest. On Linux the folder names must be capitalised the way the game's ini expects (`System`, `Maps`, `Textures`, `Sounds`, `Music`). The CD ships them in lowercase.

## Format notes

- Struct default values serialise every element of fixed-size array members. This fix is shared with every game.
- Script bytecode: one extra token was inserted before `GlobalFunction`, so tokens 0x39 to 0x46 map to the standard 0x38 to 0x45. Tokens from 0x47 up are standard.
- `Sound` objects carry 24 extra bytes after `Format`: flags, duration, sample count, bits per sample, channels and sample rate. Dialog sounds also carry a trailing block, probably lip sync data. Most sounds are Bink audio (`BIKi`) holding a 4x4 dummy video track.
- Bink sounds all use the DCT flavour of Bink 1 audio (track flag 0x1000). Each frame starts with the audio packet: the decoded size, then a least-significant-bit-first stream of blocks padded to 32 bits. A block holds, per channel, two float coefficients, a quantizer per critical band, then runs of coefficients of a given bit width. An inverse DCT turns each block into 512, 1024 or 2048 samples (by sample rate), and the first sixteenth is cross-faded with the end of the previous block.

- Lights with `LightEffect` 20 (`LE_Sunlight`) are directional. They shine along the light actor's rotation with no distance falloff; the level's shadow bits still decide which lightmap texels they reach.
- Skeletal meshes (`SkeletalMesh`) have no vertex animation frames. Their bone weights and bone-space points reproduce the reference pose exactly when each bone's quaternion is turned into a matrix the Unreal way (`FQuat` to `FMatrix`) and composed with its parent, with no conjugation for the root.
- `Animation` objects store each sequence's keys compressed. With each move, every track stores only its flags, its rotation, position and time key counts (each either 1 or the time key count), a position scale and a time scale. After the sequence list come all keys for all moves, in track order: rotations as three int16 modified Rodrigues parameters (`q = (2v, 1 - |v|²) / (1 + |v|²)` with `v = int16 / 32767`), positions as three int16 multiplied by the track's position scale and divided by 32767, and times as byte frame deltas multiplied by the time scale. `Moves[i]` belongs to the sequence at index `i`.

- Interpolation (`PHYS_Interpolating`) keeps its path state in an `InterpolationManager` actor that the moving actor owns and has as its `TickParent`: the `Last` and `Dest` points, `PhysAlpha` and `PhysRate`. Each section is a cubic bezier from `Last` to `Dest` through `Last.StartControlPoint` and `Dest.EndControlPoint` (offsets from each point), travelled at the actor's `IPSpeed`. Reaching `Dest` calls the point's `InterpolateEnd(Manager, bForward)`, which sets the next `Dest`, starts a pause, or finishes.
- The audio device's `MusicVolume` and `SoundVolume` are floats from 0 to 1 (the ini's `ALAudio.ALAudioSubsystem` section), not bytes. The options page sets them at startup with `set ini:Engine.Engine.AudioDevice`.
- Menu text (`HPMenu.int`) and dialog subtitles (`hpdialog.int`) are UTF-16 files.
- Cutscene scripts are localization files in `System/CutScenes`, which the game names like `Cutscenes\AspFTut1_KodaIntro`. The cutscene parser switches on its command words, so it relies on string `switch` cases matching regardless of case, as they do in UE1.

`Tools/BrotherBear` has small Python scripts used to research these formats. `upkg.py` dumps a package's name, import and export tables. `ubc.py` walks every function's bytecode and checks its size against the package. `kwanim.py` parses the compressed animations and checks that every one is read to its exact size.
