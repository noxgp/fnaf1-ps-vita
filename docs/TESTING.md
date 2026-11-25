# Release validation

## Update 4 (v1.0.4 / APP_VER 00.26)

- Python format tests and ARM core, renderer, asset reader, audio and UI regressions passed.
- The CAM 1B tests cover both Chica poses with Bonnie present, holding the selected frame past the old switch interval, and showing Bonnie after Chica leaves.
- A physical Vita result for this build has yet to be recorded.

## Update 3 (v1.0.3 / APP_VER 00.25)

- Python format tests and ARM core, renderer, asset reader, audio and UI regressions passed.
- The checks cover light toggling and the once-per-visit sighting cue, monitor input during animation, camera movement and wide glitch frames, Foxy frame requests, call completion and pause confirmation.
- The maintainer tested `00.25` on a physical PS Vita. Device model, firmware, storage type, individual results and measured FPS were not recorded.

## Update 2 (v0.1.0-beta.3 / APP_VER 00.23)

- Python format tests and ARM core, renderer, asset reader, audio and UI regressions passed.
- The ARM checks ran complete simulated nights with the AI enabled, checked portrait placement and camera perspective, tested menu audio and texture readiness, and verified the three control layouts, touch controls and save compatibility.
- The maintainer installed and tested the 00.23 build on a physical PS Vita and reported that it worked as expected. Device details, per-check results and measured FPS were not recorded.
- The release ships the same VPK bytes as the tested 00.23 build.

## Initial packaging validation (v0.1.0-beta.1 / APP_VER 00.17)

- Full preparation from the supplied APK: 529 sprites, 53 normalized sounds, 47 UI text textures, two shared sprite pages, 2,180 subtitle masks and an external caption index; 638 output files checksum-verified.
- Python format/compiler tests, ARM core/renderer/reader/audio/UI regression checks. Reader tests cover dynamic caption-index loading and malformed index rejection; audio tests deliberately use a wrong size hint to verify actual file-length handling.
- Public VPK checked by an exact archive allowlist: executable, SFO, original text-only launcher and dependency notices. No game media archive members.
- Existing gameplay performance optimizations are retained. Native caption dimensions follow the loaded external texture so local font differences do not invalidate packed offsets.

The maintainer also tested the public 00.17 build on a physical PS Vita and reported that it worked as expected. The device model, firmware, storage type, per-check results and FPS were not recorded.

## Checklist for future releases

Install the public VPK with the prepared external UI on a physical Vita before publication. Check startup, all menus, music, every camera, both lights/doors, plushies after closing cameras, subtitles in several languages including CJK, save/restart, outage/eyes/attack and one full normal night. Check that the old save survives. Also test missing-data startup: it must show preparation instructions instead of hanging.

Record the device model, firmware, storage type and any observed errors. Record FPS only if measured on the Vita.
