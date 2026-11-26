# Update 4 Hotfix (v1.0.5)

Vita app version `00.28` · Title ID `FNAFSCN01`

Thanks to the players who reported the door light delay, jumpscare stalls, and main menu reset.

- **Door lights.** Preloads all four lit views, including the ones with Bonnie or Chica at the door. Toggling a light while a door is closing no longer has to load the lighting image from storage. The short, irregular light flicker remains.
- **Jumpscares.** Preloads the opening frames during the night. The scene and its audio can start as soon as the opening frame is ready, while the later frames stream during playback. If a frame is late, the renderer holds the closest loaded frame while the animation timer advances.
- **Main menu.** Pressing Circle on the title screen leaves the selection, animation, and music in place. Circle still returns from submenus.
- **Older installations.** Startup messages distinguish a missing public UI pack from a game-data format that needs a fresh preparation. Retrying after copying the data reloads any existing save.

If you already have a public release, install the new VPK over it in VitaShell and keep `ux0:data/fnaf1/` and your saves. Fresh installs require your own supported Android 2.0.7 copy. [Installation and upgrade guide](https://github.com/noxgp/fnaf1-ps-vita#installation)

Automated checks passed for the lighting frames, jumpscare startup and frame fallback, Circle, and the older data layouts. Hardware timing for this build is pending. [Testing details](https://github.com/noxgp/fnaf1-ps-vita/blob/main/docs/TESTING.md)

---

# Update 4 (v1.0.4)

Vita app version `00.26` · Title ID `FNAFSCN01`

- Fixed CAM 1B (Dining Area) to follow the Android camera rules. When Bonnie and Chica share the room, the feed shows Chica without cycling between the two. Bonnie appears if he is still there when Chica leaves.
- The title screen now shows **Update 4**.
- The release Data Tool ZIP now includes the Windows, macOS, and Linux preparation paths.

If you already have the port, install the new VPK over the previous version in VitaShell and keep your `ux0:data/fnaf1/` folder and saves. For a fresh setup, use your own supported Android 2.0.7 copy. [Installation guide](https://github.com/noxgp/fnaf1-ps-vita#installation)

Automated checks passed, including the frame selection for both occupants and the change to Bonnie after Chica leaves. [Testing details](https://github.com/noxgp/fnaf1-ps-vita/blob/main/docs/TESTING.md)

---

# Update 3 (v1.0.3)

Vita app version `00.25` · Title ID `FNAFSCN01`

Thanks to everyone who tested Update 2 and reported these issues.

### Office and controls

- Tap a light button with the touchscreen or a mapped control to switch it on immediately. Tap again to switch it off.
- The light briefly flickers at irregular intervals while the panel button stays lit. Pulling up the monitor or looking away from the panel switches the light off.
- Seeing Bonnie or Chica at an open door plays the sighting sound once per visit, even if you toggle the light again.
- Increased the office panorama and sped up the desk fan.

### Cameras and animatronics

- Increased perspective on the camera feed. The camera map, its buttons and the HUD remain flat.
- Restored the original wide white glitch bands when opening the monitor, switching cameras, or when an animatronic moves into or out of the current feed. Camera buttons stay visible over the effect.
- Camera static animates faster. When an animatronic moves on the monitored feed, the stronger static lasts longer.
- The monitor completes its raise or lower animation before accepting another flip.
- Foxy's West Hall run preloads its frames ahead of time so the animation can advance through the sequence.
- Bonnie and Chica prefer rooms they have not visited on the current approach when an alternative is available. They can still retreat when blocked by a closed door.

### Audio and menus

- Lowered the circus music volume, reduced how often it starts, and increased its cooldown. Other random ambience is more audible and plays more often.
- **Mute Call** disappears when the call finishes.
- **New Game** always asks for confirmation.
- Pause menu touch targets are limited to the displayed options. Returning to the title or exiting the app asks for a second confirmation.
- Updated the title screen label to **Update 3**.

### Installation and testing

Already on a public beta? Install the new VPK over the existing one with VitaShell and keep `ux0:data/fnaf1/` and your saves. The prepared data format is the same.

For a fresh installation, download the VPK and Windows Data Tool and prepare the data from your own supported Android 2.0.7 copy. [Installation guide](https://github.com/noxgp/fnaf1-ps-vita#installation)

The maintainer tested Vita build `00.25` on a physical PS Vita. The Python and ARM core, renderer, asset reader, audio and UI checks passed. If you spot an issue, include the night or camera, Vita firmware and steps to reproduce. [Testing details](https://github.com/noxgp/fnaf1-ps-vita/blob/main/docs/TESTING.md) · [Report an issue](https://github.com/noxgp/fnaf1-ps-vita/issues)

---

# Update 2 (v0.1.0-beta.3)

Vita app version `00.23` · Title ID `FNAFSCN01`

### Controls

- Added Contextual, Classic and Custom layouts under **Options → Controls**. Contextual uses R for the door and Square (hold) for the light on the side you face. L or Cross raises and lowers the monitor. Classic uses L/R for the left/right doors, Square/Circle (hold) for the left/right lights, and Cross for the monitor.
- Custom supports assigning L, R and the four face buttons to individual gameplay actions. The selected layout and bindings are saved. Touch controls work with every layout.
- Hold either edge of the touchscreen to look around the office. Tap visible panels, the monitor bar or the camera map to interact. Removed the on-screen pause label. START pauses the game.

### Office, cameras and Extras

- Strengthened the office panorama and applied the **Perspective** setting to the camera feed. The camera map, labels and HUD stay flat.
- Doors open and close in about half a second and wait for the animation to finish before accepting another press. Door and light inputs require facing the side panel with the monitor fully lowered. Held lights start flickering after about 120 ms.
- When a camera has multiple occupants and only separate images are available, the feed cycles through those images. Cameras with group artwork keep their group view.
- Radar uses portraits for Freddy, Bonnie, Chica and Foxy and shows separate markers when they share a camera.
- Enabled Extras and plushies show a checkmark in a white circle.

### Timing, audio and startup

- Menu elements appear together and the menu track loads first. Gameplay audio loads in the background.
- Shortened the startup and night transitions. New Game and Continue show the night number with a small preparation indicator when needed.
- Bonnie and Chica can jam a door, and their office attack requires raising and lowering the monitor.
- Circus ambience plays less often, has a 60-second cooldown, and stops when Freddy's kitchen music starts. The outage music now has a one-in-five chance to start every five seconds, with a 20-second fallback.
- Added the **Update 2** label to the title screen.

### Installation and testing

Existing public beta players can install the new VPK over their current version. Keep the prepared `ux0:data/fnaf1/` folder and saves. For a fresh setup, download the VPK and Data Tool and follow the [installation guide](https://github.com/noxgp/fnaf1-ps-vita#installation). The tool needs a supported Android 2.0.7 copy and Windows.

The maintainer tested this 00.23 build on a physical PS Vita. Local Python and ARM regression checks passed. See [testing details](https://github.com/noxgp/fnaf1-ps-vita/blob/main/docs/TESTING.md).

---

# v0.1.0-beta.2

Restored the **Five Nights At Freddy's** app name, classic bubble icon, LiveArea background and START artwork. Vita app version and prepared data format stayed the same.

---

# v0.1.0-beta.1 — public packaging beta

First public packaging of the nox native adaptation. Vita APP_VER is 00.17; TITLE_ID remains FNAFSCN01.

- Small public VPK without original APK media, plus a local data-preparation tool.
- Formerly bundled menu/atlas/subtitle assets now load from ux0:data/fnaf1/ui/.
- External subtitle index and texture dimensions tolerate local font-pack differences; audio bank reads actual file lengths instead of assuming identical FFmpeg byte sizes.
- Original text-only launcher artwork and an openly licensed native overlay font.
- Prior 00.16 gameplay changes retained: foreground camera reads, resident static, buzz-only light sound and interpolated door movement.

Requires the supported application-data revision listed in README. Existing private v2 installations need a one-time UI data update. Saves remain compatible.

Local APK preparation and Python/ARM checks passed. The maintainer later tested 00.17 on a physical Vita.
