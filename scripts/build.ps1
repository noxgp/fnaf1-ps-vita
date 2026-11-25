param(
    [string]$SdkPath = '',
    [string]$LauncherVpk = ''
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SdkPath) { $SdkPath = Join-Path $projectRoot '.tools/vitasdk' }
if ($LauncherVpk) { $LauncherVpk = (Resolve-Path -LiteralPath $LauncherVpk).Path }
$sdkBin = Join-Path ([IO.Path]::GetFullPath($SdkPath)) 'bin'
$env:PATH = $sdkBin + ';' + $env:PATH
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'build/scene') | Out-Null
Push-Location $projectRoot
try {
    python -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) { throw 'Format/compiler tests failed' }
    if (-not (Test-Path 'build/generated_scene_data.h')) { throw 'Run prepare_assets.py with your APK first to create local build headers.' }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/game.c src/game_events.c src/camera_map.c src/scare_animation.c src/touch_input.c src/perspective_mesh.c src/scene_engine.c src/raster.c src/data_files.c src/audio_mix.c src/menu_ui.c tests/game_checks.c tests/event_checks.c tests/native_checks.c tests/native_memory.c tests/native_io.c `
        -Ibuild -Isrc -O2 -std=c11 -Wall -Wextra -Werror -nostdlib '-Wl,-Ttext=0x10000,-e,run_checks' -lgcc -o build/native_checks.elf
    if ($LASTEXITCODE -ne 0) { throw 'Native test compilation failed' }
    python scripts/test_native.py
    if ($LASTEXITCODE -ne 0) { throw 'Actual ARM runtime checks failed' }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/gpu_renderer.c src/perspective_mesh.c src/scare_animation.c tests/renderer_checks.c tests/native_memory.c `
        -Ibuild -Isrc -O2 -ffunction-sections -fdata-sections -std=c11 -Wall -Wextra -Werror -nostdlib '-Wl,--gc-sections,-Ttext=0x10000,-e,run_checks' -lgcc -o build/renderer_checks.elf
    if ($LASTEXITCODE -ne 0) { throw 'Renderer check compilation failed' }
    $priorCheckElf = $env:FNAF_CHECK_ELF
    try {
        $env:FNAF_CHECK_ELF = 'build/renderer_checks.elf'
        python scripts/test_native.py
        if ($LASTEXITCODE -ne 0) { throw 'Renderer lifetime/cache checks failed' }
    } finally { $env:FNAF_CHECK_ELF = $priorCheckElf }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/asset_stream.c src/caption_index.c src/scare_animation.c tests/asset_stream_checks.c tests/native_memory.c `
        -DASSET_STREAM_TEST -Ibuild -Isrc -O2 -ffunction-sections -fdata-sections -std=c11 -Wall -Wextra -Werror -nostdlib '-Wl,--gc-sections,-Ttext=0x10000,-e,run_checks' -lgcc -lz -o build/asset_checks.elf
    if ($LASTEXITCODE -ne 0) { throw 'Asset reader check compilation failed' }
    try {
        $env:FNAF_CHECK_ELF = 'build/asset_checks.elf'
        python scripts/test_native.py
        if ($LASTEXITCODE -ne 0) { throw 'Asset reader priority/ownership checks failed' }
    } finally { $env:FNAF_CHECK_ELF = $priorCheckElf }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/game_audio.c src/audio_cache.c tests/game_audio_checks.c tests/native_memory.c `
        -DGAME_AUDIO_TEST -Ibuild -Isrc -O2 -ffunction-sections -fdata-sections -std=c11 -Wall -Wextra -Werror -nostdlib '-Wl,--gc-sections,-Ttext=0x10000,-e,run_checks' -lgcc -o build/game_audio_checks.elf
    if ($LASTEXITCODE -ne 0) { throw 'Game audio check compilation failed' }
    try {
        $env:FNAF_CHECK_ELF = 'build/game_audio_checks.elf'
        python scripts/test_native.py
        if ($LASTEXITCODE -ne 0) { throw 'Game audio queue/mixer checks failed' }
    } finally { $env:FNAF_CHECK_ELF = $priorCheckElf }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/game.c src/game_events.c src/camera_map.c src/scare_animation.c src/touch_input.c src/scene_engine.c src/game_view.c src/android_menu.c tests/game_capture.c tests/android_menu_checks.c tests/native_memory.c `
        -Ibuild -Isrc -O2 -ffunction-sections -fdata-sections -std=c11 -Wall -Wextra -Werror -nostdlib '-Wl,--gc-sections,-Ttext=0x10000,-e,capture_case,--undefined=run_checks,--undefined=capture_commands' -lgcc -o build/game_capture.elf
    if ($LASTEXITCODE -ne 0) { throw 'Android UI check compilation failed' }
    $env:FNAF_CHECK_ELF = 'build/game_capture.elf'
    python scripts/test_native.py
    $env:FNAF_CHECK_ELF = $priorCheckElf
    if ($LASTEXITCODE -ne 0) { throw 'Android menu checks/capture failed' }
    & (Join-Path $sdkBin 'arm-vita-eabi-gcc.exe') src/scene_probe.c src/scene_engine.c src/gpu_renderer.c src/asset_stream.c src/caption_index.c src/perspective_mesh.c src/data_files.c src/game.c src/game_events.c src/camera_map.c src/scare_animation.c src/touch_input.c src/game_audio.c src/audio_cache.c src/game_view.c src/android_menu.c `
        -Ibuild -Isrc -O2 -ffunction-sections -fdata-sections -std=c11 -Wall -Wextra -Werror '-Wl,-q,--gc-sections' -o build/scene/app.elf `
        -lvita2d -lvorbisfile -lvorbis -logg -lz -lm `
        -lSceGxm_stub -lSceDisplay_stub -lSceSysmodule_stub -lScePgf_stub -lScePvf_stub -lSceCommonDialog_stub `
        -lSceRtc_stub -lSceAppMgr_stub -lSceCtrl_stub -lSceTouch_stub -lSceAudio_stub `
        -lSceSysmem_stub -lSceProcessmgr_stub -lSceKernelThreadMgr_stub
    if ($LASTEXITCODE -ne 0) { throw 'Scene runtime compilation failed' }
    & (Join-Path $sdkBin 'vita-elf-create.exe') build/scene/app.elf build/scene/app.velf
    if ($LASTEXITCODE -ne 0) { throw 'Vita ELF conversion failed' }
    & (Join-Path $sdkBin 'vita-make-fself.exe') -s build/scene/app.velf build/scene/eboot.bin
    if ($LASTEXITCODE -ne 0) { throw 'SELF creation failed' }
    & (Join-Path $sdkBin 'vita-mksfoex.exe') -s TITLE_ID=FNAFSCN01 -s APP_VER=00.25 "Five Nights At Freddy's" build/scene/param.sfo
    if ($LASTEXITCODE -ne 0) { throw 'SFO creation failed' }
    $packageArgs = @()
    if ($LauncherVpk) { $packageArgs += '--launcher-vpk', $LauncherVpk }
    python scripts/package_public.py @packageArgs
    if ($LASTEXITCODE -ne 0) { throw 'Scene VPK packaging failed' }
} finally { Pop-Location }
