$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = if ($env:MCUXPRESSO_SDK) { $env:MCUXPRESSO_SDK } else { Join-Path (Split-Path -Parent $ProjectRoot) "mcuxpresso-sdk" }
$SdkExamples = Join-Path $SdkRoot "mcuxsdk/examples/multicore_examples"
$Template = Join-Path $SdkExamples "hello_world"
$Overlay = Join-Path $SdkExamples "aied_dual_core_led_toggle"
$BuildDir = if ($env:RT1180_BUILD_DIR) { $env:RT1180_BUILD_DIR } else { Join-Path $ProjectRoot "build/rt1180-dual-core-led-toggle" }
$Marker = ".aied-generated-overlay"

foreach ($Tool in @("west", "cmake", "ninja")) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "$Tool is required and must be on PATH."
    }
}

if ($env:ARMGCC_DIR) {
    $env:Path = (Join-Path $env:ARMGCC_DIR "bin") + ";" + $env:Path
}
if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    throw "Arm GNU Toolchain 14.2.x is required. Install it and set ARMGCC_DIR to its installation root."
}
if (-not (Test-Path (Join-Path $SdkRoot ".west")) -or -not (Test-Path (Join-Path $Template "primary/CMakeLists.txt"))) {
    throw "The pinned MCUXpresso SDK workspace is missing or incomplete. Run scripts/setup_sdk.ps1 first."
}

if (Test-Path $Overlay) {
    if (-not (Test-Path (Join-Path $Overlay $Marker))) {
        throw "Refusing to overwrite unmarked directory: $Overlay"
    }
    Remove-Item -Recurse -Force $Overlay
}

Copy-Item -Recurse $Template $Overlay
New-Item -ItemType File -Path (Join-Path $Overlay $Marker) | Out-Null
Copy-Item (Join-Path $ProjectRoot "examples/dual_core_led_toggle/cm33/main.c") (Join-Path $Overlay "primary/main.c") -Force
Copy-Item (Join-Path $ProjectRoot "examples/dual_core_led_toggle/cm7/main.c") (Join-Path $Overlay "secondary/main.c") -Force
Copy-Item (Join-Path $ProjectRoot "examples/dual_core_led_toggle/common/app_events.h") (Join-Path $Overlay "primary/app_events.h") -Force
Copy-Item (Join-Path $ProjectRoot "examples/dual_core_led_toggle/common/app_events.h") (Join-Path $Overlay "secondary/app_events.h") -Force

if (-not (Test-Path (Join-Path $Overlay "primary/sysbuild.cmake")) -or -not (Test-Path (Join-Path $Overlay "secondary/CMakeLists.txt"))) {
    throw "The copied NXP SysBuild template is incomplete."
}

Push-Location $SdkRoot
try {
    west build -b evkmimxrt1180 --sysbuild (Join-Path $Overlay "primary") --config flexspi_nor_debug --toolchain armgcc -Dcore_id=cm33 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -p always -d $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "RT1180 SysBuild failed." }
}
finally {
    Pop-Location
}

Write-Host "Build completed: $BuildDir"
