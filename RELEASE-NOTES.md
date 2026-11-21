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
