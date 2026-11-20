# FNAF 1 for PS Vita — nox

Unofficial native adaptation of the Android game's data for PS Vita. This is a game-specific runtime, not a general Android or Clickteam emulator, and is not affiliated with the original game's authors.

**Release:** `v0.1.0-beta.1` · **Vita application version:** `00.17` · **Title ID:** `FNAFSCN01`

The public VPK contains the executable, original text-only launcher artwork and dependency notices. **It contains no APK, original game images, audio or subtitle pack.** You must prepare data locally from your own supported copy.

## Compatibility

The tested application-data revision reports Android version **2.0.7**. App version alone does not guarantee compatibility. The preparation tool verifies the `res/raw/application.ccn` SHA-256:

`70a1ec40284d4f5232d0ecedcbd94a68c51cd937983ad17d2cbf329aad35b6d4`

Other application-data revisions, including different store packages, have not been independently verified and are refused by the tool. APK signing/packaging may differ if that data hash matches. No APK download links are provided.

## Install

1. Get the public VPK and **Data Tool** ZIP from [Releases](https://github.com/noxgp/fnaf1-ps-vita/releases).
2. On Windows, extract the Data Tool, install Python 3.11 or newer and FFmpeg, and install its dependency:

```powershell
py -3 -m pip install -r requirements-assets.txt
py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ffmpeg "C:\Tools\ffmpeg.exe"
```

The tool currently requires local Windows fonts `consola.ttf`, `tahoma.ttf`, `YuGothM.ttc`, `simsun.ttc` and `malgun.ttf` for the original menu and all 11 subtitle languages. Install the relevant Windows language-font features if missing, or pass `--font-dir` to a folder containing your local font files. These fonts are not bundled. Allow roughly 2 GB of temporary disk space.

3. Wait for all six preparation stages and checksum verification. The output is `prepared/fnaf1/`.
4. Copy that **fnaf1 folder** to `ux0:data/`, resulting in `ux0:data/fnaf1/game.version`, `ux0:data/fnaf1/public-ui.version` and `ux0:data/fnaf1/ui/subtitles.idx`. Avoid a nested `fnaf1/fnaf1` directory.
5. Install the VPK through VitaShell and launch it. Allow the initial sound/texture preparation to finish.

FFmpeg can instead be placed on PATH. With dependencies installed, you can drag the APK onto `Prepare-Game-Data.cmd`.

Prepared game files are private user data: **do not upload the APK, prepared folder or conversion workspace to GitHub or attach them to releases.** The tool does not execute the Android application and does not download game content.

## Upgrade from the private 00.16 build

Save files remain compatible: keep `ux0:data/fnaf1/progress.sav` and `progress.bak`. Do not uninstall the game or delete its data.

The public build moves formerly bundled UI into external data, so installing only its VPK on old private data is insufficient. Prepare a UI-only update into a separate empty output folder:

```powershell
py -3 prepare_assets.py --apk "C:\Games\your-copy.apk" --ui-only --output "prepared-ui/fnaf1" --ffmpeg "C:\Tools\ffmpeg.exe"
```

Merge this folder into the existing `ux0:data/fnaf1/`, then install the public VPK. The UI-only output is also private and must not be distributed.

## Controls

| Control | Action |
| --- | --- |
| L / R | Left / right door |
| Hold Square / Circle | Left / right light |
| Cross | Raise / lower monitor |
| D-pad | Spatial camera navigation; office left/right |
| Left stick | Office look |
| Triangle | Mute phone call |
| Start | Pause menu |
| Select | Help; diagnostic HUD on title screen |
| Front touch | Door/light controls, camera map and monitor bar; Freddy poster nose |

Menus use D-pad, Cross to confirm and Circle to return. The Extras page exposes local switches instead of platform purchases. Unlimited Power, Fast Nights and Radar disable progression/stars for that run; plushies do not. The handheld has no built-in rumble; vibration targets a supported paired controller.

## Scope and testing

Implemented: nights 1–5, night 6, Custom Night, saving/options, cameras and room effects, four main enemies, power outage and its separate Freddy attack, rare events, phone audio, 11 subtitle languages, touch controls and extras.

Known limits: AI/RNG edge fidelity and balance need Android comparison; the perspective effect is a native approximation; menu labels are English. First uncached camera images still depend on storage speed. A cold jumpscare may wait for its sequence to prepare.

The private 00.16 gameplay build was tested on a user's Vita. **The public 00.17 resource-layout build has passed local Python/ARM checks and full APK data preparation, but still needs an on-device installation test.** CPU tests do not emulate Vita OS, GPU or audio output. See [testing](docs/TESTING.md).

## Build from source

Generate local build headers first by running `prepare_assets.py` with your supported APK. Generated headers and extracted data stay ignored by Git.

```powershell
py -3 -m pip install -r requirements.txt
./scripts/build.ps1 -SdkPath "C:\VitaSDK"
```

Requires a working native Windows VitaSDK containing libvita2d, libogg, libvorbis and zlib. The script runs Python format/compiler tests and ARM core, renderer, asset-reader, audio and UI checks, then creates a safe SELF and an allowlisted VPK under `out/`. The SDK and proprietary platform modules are not included. The Python format tests can run without game data. The complete build requires game data prepared locally.

## Credits and licensing

Port/adaptation: **nox**. Original game, characters, art and sound belong to their respective owners; see [credits](CREDITS.md). The [MIT license](LICENSE) covers the port code. Original game content belongs to its respective owners. Dependency licenses are preserved in `third_party/licenses/` and inside the VPK. Report bugs through [Issues](https://github.com/noxgp/fnaf1-ps-vita/issues), including application version, night/camera, Vita firmware and steps to reproduce; never attach game files or saves containing personal information.
