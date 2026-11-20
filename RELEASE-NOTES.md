# v0.1.0-beta.1 — public packaging beta

First public packaging of the nox native adaptation. Vita APP_VER is 00.17; TITLE_ID remains FNAFSCN01.

- Small public VPK without original APK media, plus a local data-preparation tool.
- Formerly bundled menu/atlas/subtitle assets now load from ux0:data/fnaf1/ui/.
- External subtitle index and texture dimensions tolerate local font-pack differences; audio bank reads actual file lengths instead of assuming identical FFmpeg byte sizes.
- Original text-only launcher artwork and an openly licensed native overlay font.
- Prior 00.16 gameplay changes retained: foreground camera reads, resident static, buzz-only light sound and interpolated door movement.

Requires the supported application-data revision listed in README. Existing private v2 installations need a one-time UI data update. Saves remain compatible.

Validation: local APK preparation/checksums and Python/ARM tests passed. Public 00.17 installation on real Vita is still pending; keep this release as a draft until that check. No APK or prepared game data is attached.
