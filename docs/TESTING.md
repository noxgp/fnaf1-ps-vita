# Release validation

Local validation for v0.1.0-beta.1 / APP_VER 00.17:

- Full preparation from the supplied APK: 529 sprites, 53 normalized sounds, 47 UI text textures, two shared sprite pages, 2,180 subtitle masks and an external caption index; 638 output files checksum-verified.
- Python format/compiler tests, ARM core/renderer/reader/audio/UI regression checks. Reader tests cover dynamic caption-index loading and malformed index rejection; audio tests deliberately use a wrong size hint to verify actual file-length handling.
- Public VPK checked by an exact archive allowlist: executable, SFO, original text-only launcher and dependency notices. No game media archive members.
- Existing gameplay performance optimizations are retained. Native caption dimensions follow the loaded external texture so local font differences do not invalidate packed offsets.

Before publishing the draft release or submitting to VitaDB, install the public VPK with the prepared external UI on a physical Vita. Check startup, all menus, music, every camera, both lights/doors, plushies after closing cameras, subtitles in several languages including CJK, save/restart, outage/eyes/attack and one full normal night. Check that the old save survives. Also test missing-data startup: it must show preparation instructions instead of hanging.

The public resource-layout build has no hardware result recorded yet. Record the device/firmware, storage type and observed errors/FPS here after the check. Do not claim 60 FPS or 1:1 Android parity from CPU emulation.
