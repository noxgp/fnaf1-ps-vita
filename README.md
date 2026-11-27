# FNaF 1 for PS Vita

An unofficial PS Vita port of **Five Nights at Freddy's** using the supported Android 2.0.7 data revision.

The port uses a game-specific native runtime written in C, with GXM rendering and a custom audio mixer. The Data Tool prepares assets and compiles selected rules and layout data from your Android copy.

**This repository contains no game files.** You need your own legally obtained copy of the FNaF 1 Android APK.

**Release** · `v1.0.6 (Update 6)` · **Vita application version** · `00.28` · **Title ID** · `FNAFSCN01`

[Download Update 6](https://github.com/noxgp/fnaf1_ps_vita/releases/tag/v1.0.6) · [Patch notes](RELEASE-NOTES.md)

## Features

- Nights 1–5, Night 6 and Custom Night
- All four animatronics, the power outage and Freddy's outage attack, rare events and phone calls
- Cameras with room effects and optional perspective for the camera feed and office
- Subtitles in 11 languages, including Japanese, Chinese and Korean
- Touch controls plus Contextual, Classic and Custom button layouts
- Extras as local switches, with map portraits when Radar is enabled
- Saves to `ux0:data/fnaf1/progress.sav`

## Controls

Open **Options → Controls** to choose a layout:

- **Contextual** (default) uses one door and one light button for the side you face.
- **Classic** uses L/R for the left/right doors, Square/Circle for the left/right lights, and Cross for the monitor.
- **Custom** lets you assign L, R or the four face buttons to actions. Select an action with Cross and press a button to assign it. Left/Right cycles through the choices, including Unassigned. Assigning an occupied button swaps it with the previous binding. D-pad, Start, Select and touch keep their usual actions.

| Where | Contextual button | Action |
|---|---|---|
| Office | R | Toggle the door on the side you are facing |
| Office | □ | Toggle the light on the side you are facing |
| Office | D-pad ← / → or left stick | Look left / right |
| Office / Cameras | L or ✕ | Raise / lower the camera monitor |
| Cameras | D-pad | Switch cameras |
| Office / Cameras | △ | Mute the phone call |
| During a night | Start | Open the pause menu |
| Office / Cameras | Select | Open controls help |
| Title screen | Select | Toggle the diagnostic HUD |
| Menus | D-pad / ✕ / ○ | Move / confirm / back |

Hold either edge of the touchscreen to look around. Tap a visible door button, tap its light button to toggle the light, or tap the monitor bar and camera map. You can also tap Mute Call and Freddy's poster nose.

In Extras, Unlimited Power, Fast Nights and Radar disable progress and stars for that run. Plushies don't.

## Installation

1. Download the VPK. You need a homebrew-enabled PS Vita with VitaShell. [Latest release](https://github.com/noxgp/fnaf1_ps_vita/releases/latest)
2. Use the FNaF 1 Android APK from your own purchase. The Data Tool accepts one Android 2.0.7 data revision and checks that `res/raw/application.ccn` has SHA-256 `70a1ec40284d4f5232d0ecedcbd94a68c51cd937983ad17d2cbf329aad35b6d4`.
3. Prepare the data on your computer using the instructions below.

### Windows

Download and extract the **Data Tool** ZIP. Open a terminal in the extracted folder. You need Python 3.11 or newer and FFmpeg. [Latest release](https://github.com/noxgp/fnaf1_ps_vita/releases/latest)

```powershell
py -3 -m pip install -r requirements-assets.txt
py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ffmpeg "C:\Tools\ffmpeg.exe"
```

If FFmpeg is on your PATH, you can also drag the APK onto `Prepare-Game-Data.cmd`. The tool uses the Windows fonts `consola.ttf`, `tahoma.ttf`, `YuGothM.ttc`, `simsun.ttc` and `malgun.ttf`. Use `--font-dir` if your fonts are in a different folder.

### macOS and Linux

Download and extract the **Data Tool** ZIP. Open Terminal in the extracted folder. You need Python 3.11 or newer, FFmpeg, `curl` and about 2 GB of free working space. [Latest release](https://github.com/noxgp/fnaf1_ps_vita/releases/latest)

- **macOS with Homebrew** — `brew install python ffmpeg`
- **Ubuntu 24.04 / Debian 12 or newer** — `sudo apt install python3 python3-venv ffmpeg curl`
- **Other Linux distributions** — install the same dependencies with your package manager.

Download the three Noto Sans CJK fonts for Japanese, Simplified Chinese and Korean subtitles. The repository includes Cousine for menus and the other subtitle languages. [Noto Sans CJK downloads](https://github.com/notofonts/noto-cjk/tree/Sans2.004/Sans/OTF)

```sh
mkdir -p fonts
noto=https://raw.githubusercontent.com/notofonts/noto-cjk/Sans2.004/Sans/OTF
curl -fL "$noto/Japanese/NotoSansCJKjp-Regular.otf" -o fonts/NotoSansCJKjp-Regular.otf
curl -fL "$noto/SimplifiedChinese/NotoSansCJKsc-Regular.otf" -o fonts/NotoSansCJKsc-Regular.otf
curl -fL "$noto/Korean/NotoSansCJKkr-Regular.otf" -o fonts/NotoSansCJKkr-Regular.otf
```

Create a Python virtual environment and run the tool. Replace the example path with the path to your APK.

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-assets.txt
.venv/bin/python prepare_assets.py --apk "/path/to/your-copy.apk"
```

Use `--font-dir "/path/to/fonts"` if your Noto fonts are elsewhere, or `--ffmpeg "/path/to/ffmpeg"` if FFmpeg is outside your PATH.

### Copy the data and install the VPK

1. The tool creates `prepared/fnaf1/` and checks the generated files. Via VitaShell USB or FTP, copy the entire `fnaf1` folder into `ux0:data/`. Check for `ux0:data/fnaf1/game.version`, `ux0:data/fnaf1/public-ui.version` and `ux0:data/fnaf1/ui/subtitles.idx` at those exact paths. If a folder called `fnaf1` already exists, back up `progress.sav` and `progress.bak` before replacing data.
2. Copy the VPK to the Vita and install it with VitaShell. Give the first launch a moment to prepare sounds and textures.

The prepared folder contains the game's files. Keep it for yourself. Don't upload it or attach it to anything.

### Upgrading from a previous public release

Install the new VPK over your existing version with VitaShell. Keep `ux0:data/fnaf1/` and your saves in place. [Latest release](https://github.com/noxgp/fnaf1_ps_vita/releases/latest)

### Upgrading from older private builds

Keep a backup of `ux0:data/fnaf1/progress.sav` and `progress.bak` before copying data. If you have complete Game Data v2 from a private build such as 00.16 and the Vita asks for a UI update, prepare the missing public UI in an empty folder with `--ui-only`. This example uses Windows.

```powershell
py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ui-only --output "prepared-ui/fnaf1" --ffmpeg "C:\Tools\ffmpeg.exe"
```

On macOS and Linux, use `.venv/bin/python` and your local APK path with the same `--ui-only` and `--output` options. Merge the prepared UI into `ux0:data/fnaf1/`, then install the new VPK.

If the Vita reports **Old Game Data Format** or your sprites or audio are incomplete, follow the full installation steps above. Copy the newly prepared `fnaf1` folder and restore your backed-up saves.

## Known limitations

- **Testing.** Build 00.28 passed automated regression checks. Device timing for the startup, lighting and animation changes is pending. [Testing details](docs/TESTING.md)
- **AI and RNG.** Starting AI levels come from the supported game data. Edge cases and balance still need comparison with Android.
- **The perspective effect** is a native approximation.
- **Menu labels are in English.**
- **Loading.** The first time a camera is opened, its image depends on memory card speed. Jumpscares have preloaded opening frames, and later frames may briefly hold on slower cards.
- **No vibration on the handheld**, because the Vita has no rumble motor. Vibration targets supported paired controllers.

## Building from source

The build needs a native Windows [VitaSDK](https://vitasdk.org) with libvita2d, libogg, libvorbis and zlib. Run `prepare_assets.py` with your APK first, because the build uses headers generated from your game data (Git ignores them).

```powershell
py -3 -m pip install -r requirements.txt
./scripts/build.ps1 -SdkPath "C:\VitaSDK"
```

The script runs the Python tests and the ARM core, renderer, reader, audio and UI checks, then writes the VPK to `out/`. The package contains the executable, Vita metadata, launcher artwork and license notices. The published release uses the classic icon and LiveArea artwork. Pass `-LauncherVpk` with a local copy of the [beta.2 VPK](https://github.com/noxgp/fnaf1-ps-vita/releases/tag/v0.1.0-beta.2) to keep those visuals in your own build. Builds without that argument use the text-only launcher art in the repository. The Python format tests work without game data.

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

Report bugs with the app version, night or camera, Vita firmware and steps to reproduce. Keep game files and saves private. [GitHub Issues](https://github.com/noxgp/fnaf1_ps_vita/issues)
