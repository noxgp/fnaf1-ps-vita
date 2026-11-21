# FNaF 1 for PS Vita

An unofficial PS Vita port of **Five Nights at Freddy's** using the supported Android 2.0.7 data revision.

The port uses a game-specific native runtime written in C, with GXM rendering and a custom audio mixer. The Windows Data Tool prepares assets and compiles selected rules and layout data from your Android copy.

**This repository contains no game files.** You need your own legally obtained copy of the FNaF 1 Android APK.

**Release:** `v0.1.0-beta.1` · **Vita application version:** `00.17` · **Title ID:** `FNAFSCN01`

## Features

- Nights 1–5, Night 6 and Custom Night
- All four animatronics, the power outage and Freddy's outage attack, rare events and phone calls
- Cameras with room effects and the office perspective effect
- Subtitles in 11 languages, including Japanese, Chinese and Korean
- Touch controls and physical buttons
- Extras as local switches (no store purchases)
- Saves to `ux0:data/fnaf1/progress.sav`

## Controls

| Where | Button | Action |
|---|---|---|
| Office | L / R | Left / right door |
| Office | Hold □ / ○ | Left / right light |
| Office | D-pad ← / → or left stick | Look left / right |
| Office / Cameras | ✕ | Raise / lower the camera monitor |
| Cameras | D-pad | Switch cameras |
| Office / Cameras | △ | Mute the phone call |
| During a night | Start | Open the pause menu |
| Office / Cameras | Select | Open controls help |
| Title screen | Select | Toggle the diagnostic HUD |
| Menus | D-pad / ✕ / ○ | Move / confirm / back |

The touch screen works for the doors, lights, camera map and monitor bar, and for Freddy's poster nose.

In Extras, Unlimited Power, Fast Nights and Radar disable progress and stars for that run. Plushies don't.

## Installation

You need a Vita with homebrew support and VitaShell. You also need a **Windows** PC with Python 3.11+ and FFmpeg. Linux and macOS are not supported in this beta.

1. **Get the APK.** Use the FNaF 1 Android APK from your own purchase. Only one data revision is supported. The tool checks the SHA-256 of `res/raw/application.ccn` against this value:
   `70a1ec40284d4f5232d0ecedcbd94a68c51cd937983ad17d2cbf329aad35b6d4`
2. **Build the game data on your PC.** Download the **Data Tool** ZIP from [Releases](https://github.com/noxgp/fnaf1-ps-vita/releases), extract it and run:
   ```powershell
   py -3 -m pip install -r requirements-assets.txt
   py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ffmpeg "C:\Tools\ffmpeg.exe"
   ```
   If FFmpeg is on your PATH, you can also drag the APK onto `Prepare-Game-Data.cmd`. The tool needs about 2 GB of temporary space and the Windows fonts `consola.ttf`, `tahoma.ttf`, `YuGothM.ttc`, `simsun.ttc` and `malgun.ttf` (use `--font-dir` if they're elsewhere). It creates `prepared/fnaf1/`.
3. **Copy the data to the Vita.** Copy the `fnaf1` folder to `ux0:data/`, so you end up with `ux0:data/fnaf1/game.version`, `ux0:data/fnaf1/public-ui.version` and `ux0:data/fnaf1/ui/subtitles.idx`. Watch out for a nested `fnaf1/fnaf1` folder.
4. **Install the VPK.** Install the VPK from the [Releases](https://github.com/noxgp/fnaf1-ps-vita/releases) page with VitaShell. The first launch prepares sounds and textures, so give it a moment.

The prepared folder contains the game's files. Keep it for yourself. Don't upload it or attach it to anything.

### Upgrading from the private 00.16 build

Keep `ux0:data/fnaf1/progress.sav` and `progress.bak`, and don't uninstall the game. The public build loads its menu graphics from the data folder, so prepare a UI-only update into an empty folder:

```powershell
py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ui-only --output "prepared-ui/fnaf1" --ffmpeg "C:\Tools\ffmpeg.exe"
```

Merge it into `ux0:data/fnaf1/`, then install the new VPK.

## Known limitations

- **Beta.** The 00.17 build passed local validation and was tested on a physical PS Vita. See [testing](docs/TESTING.md) for the reported result.
- **AI and RNG.** Starting AI levels come from the supported game data. Edge cases and balance still need comparison with Android.
- **The perspective effect** is a native approximation.
- **Menu labels are in English.**
- **Loading.** The first time a camera is opened, its image depends on memory card speed. A jumpscare may briefly wait to load the first time.
- **No vibration on the handheld**, because the Vita has no rumble motor. Vibration targets supported paired controllers.

## Building from source

The build needs a native Windows [VitaSDK](https://vitasdk.org) with libvita2d, libogg, libvorbis and zlib. Run `prepare_assets.py` with your APK first, because the build uses headers generated from your game data (Git ignores them).

```powershell
py -3 -m pip install -r requirements.txt
./scripts/build.ps1 -SdkPath "C:\VitaSDK"
```

The script runs the Python tests and the ARM core, renderer, reader, audio and UI checks, then writes the VPK to `out/`. The VPK only contains files from an allowlist: the executable, Vita app metadata, original launcher artwork and license notices. The Python format tests work without game data.

### Source layout

| Path | Contents |
|---|---|
| `src/game.c` | Night simulation, animatronic behavior, doors, lights, power and save format |
| `src/game_events.c` | Rare events, hallucinations, poster encounter |
| `src/scene_engine.c` | Scene data access and partial Fusion event interpreter |
| `src/game_view.c`, `src/gpu_renderer.c` | Drawing and GXM GPU rendering |
| `src/asset_stream.c` | Background file-reader thread |
| `src/audio_cache.c`, `src/game_audio.c` | Sound caching and mixing |
| `src/android_menu.c` | Menus built from the original Android layout |
| `src/scene_probe.c` | Vita frontend and entry point, including input, timing and save I/O |
| `prepare_assets.py` | APK → Vita data, with hash and checksum verification |
| `scripts/decode_scenes.py`, `scripts/compile_scenes.py` | Decode Fusion build 296 data and compile it into C tables |
| `scripts/prepare_game.py` | Generate game rules (including starting AI levels) from your data |
| `tests/` | Python and ARM regression tests |

### Third-party code

| Library | Used for | License |
|---|---|---|
| libvita2d | Rendering support | MIT |
| libogg, libvorbis | OGG audio | BSD |
| zlib | Decompression | zlib |
| Cousine | Overlay font and launcher text | SIL OFL 1.1 |

Full notices are in `third_party/licenses/` and inside the VPK. See [CREDITS.md](CREDITS.md) for details and development references.

## Credits

- Port by **nox**.
- *Five Nights at Freddy's* © Scott Cawthon. Android edition by Clickteam. This is an unofficial, non-commercial fan port, not affiliated with or endorsed by them.
- Port code is under the [MIT license](LICENSE).

Report bugs in [Issues](https://github.com/noxgp/fnaf1-ps-vita/issues) with the app version, night/camera, Vita firmware and steps to reproduce. Never attach game files or save files.
