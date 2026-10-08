# Disney's Brother Bear (PC) support

This fork adds support for Disney's Brother Bear (PC, 2003). The game runs on KnowWonder's modified Unreal Engine 1, the same engine family as the Harry Potter 1 and 2 PC games.

You need your own copy of the game. This repository holds no game files and never will: maps, textures, sounds, music, dialog and the original script packages all load at runtime from your installed copy.

## Status

- Detects the US English retail release (`System/Game.exe`, SHA1 `e77df020e7c1c299bd88f229564cfac4476d4eda`).
- Loads every package (package version 80).
- Runs the game's UnrealScript, with Brother Bear's shifted opcodes remapped.
- Registers every engine native the game declares. Some are still stubs; the `nativeaudit` debugger command lists any gaps.
- Boots into the first level, runs the intro cutscene scripts and draws the HUD.
- Renders the levels with their lightmaps, including KnowWonder's sunlight.
- Draws skeletal meshes (characters, plants, props) and plays their animations.

Not working yet:

- Animations on part of the skeleton (eye blinks, head look, lip sync) and combined animations.
- Shadows from sunlight on meshes.
- Bink audio, so sounds are silent.
- The particle system.

## Running

Point SurrealEngine at the game's install folder, the one that contains `System`, `Maps`, `Textures` and the rest. On Linux the folder names must be capitalised the way the game's ini expects (`System`, `Maps`, `Textures`, `Sounds`, `Music`). The CD ships them in lowercase.

## Format notes

- Struct default values serialise every element of fixed-size array members. This fix is shared with every game.
- Script bytecode: one extra token was inserted before `GlobalFunction`, so tokens 0x39 to 0x46 map to the standard 0x38 to 0x45. Tokens from 0x47 up are standard.
- `Sound` objects carry 24 extra bytes after `Format`: flags, duration, sample count, bits per sample, channels and sample rate. Dialog sounds also carry a trailing block, probably lip sync data. Most sounds are Bink audio (`BIKi`) holding a 4x4 dummy video track.

- Lights with `LightEffect` 20 (`LE_Sunlight`) are directional. They shine along the light actor's rotation with no distance falloff; the level's shadow bits still decide which lightmap texels they reach.
- Skeletal meshes (`SkeletalMesh`) have no vertex animation frames. Their bone weights and bone-space points reproduce the reference pose exactly when each bone's quaternion is turned into a matrix the Unreal way (`FQuat` to `FMatrix`) and composed with its parent, with no conjugation for the root.
- `Animation` objects store each sequence's keys compressed. With each move, every track stores only its flags, its rotation, position and time key counts (each either 1 or the time key count), a position scale and a time scale. After the sequence list come all keys for all moves, in track order: rotations as three int16 modified Rodrigues parameters (`q = (2v, 1 - |v|²) / (1 + |v|²)` with `v = int16 / 32767`), positions as three int16 multiplied by the track's position scale and divided by 32767, and times as byte frame deltas multiplied by the time scale. `Moves[i]` belongs to the sequence at index `i`.

`Tools/BrotherBear` has small Python scripts used to research these formats. `upkg.py` dumps a package's name, import and export tables. `ubc.py` walks every function's bytecode and checks its size against the package. `kwanim.py` parses the compressed animations and checks that every one is read to its exact size.
