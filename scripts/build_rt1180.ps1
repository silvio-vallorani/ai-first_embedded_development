$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = if ($env:MCUXPRESSO_SDK) { $env:MCUXPRESSO_SDK } else { Join-Path (Split-Path -Parent $ProjectRoot) "mcuxpresso-sdk" }
$SdkExamples = Join-Path $SdkRoot "mcuxsdk/examples/multicore_examples"
$Template = Join-Path $SdkExamples "hello_world"
$Overlay = Join-Path $SdkExamples "aied_dual_core_led_toggle"
$BuildDir = if ($env:RT1180_BUILD_DIR) { $env:RT1180_BUILD_DIR } else { Join-Path $ProjectRoot "build/rt1180-led-toggle" }
$VenvDir = Join-Path $ProjectRoot ".venv"
$West = Join-Path $VenvDir "Scripts/west.exe"
$VenvPython = Join-Path $VenvDir "Scripts/python.exe"
$Marker = ".aied-generated-overlay"
$TemplateImagePrefix = "hello_world"
$AppImagePrefix = "led_toggle"

if (-not (Test-Path $West)) {
    throw "The project Python environment is missing. Run scripts/setup_sdk.ps1 first."
}
$env:Path = (Join-Path $VenvDir "Scripts") + ";" + $env:Path

foreach ($Tool in @("cmake", "ninja")) {
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

# Rename the template's image and IDE workspace names so build outputs carry this
# application's name. Board-port paths (multicore_examples/hello_world) are NXP-owned
# and stay unchanged.
$ImageNamePattern = "\b$TemplateImagePrefix" + '_(primary|secondary)_core'
$WorkspacePattern = "(?m)(shared-workspace: )$TemplateImagePrefix(\r?)$"
$OverlayFiles = (Get-ChildItem -Path $Overlay -Recurse -File).FullName
foreach ($File in $OverlayFiles) {
    $Text = [System.IO.File]::ReadAllText($File)
    $Renamed = ($Text -replace $ImageNamePattern, ($AppImagePrefix + '_$1_core')) -replace $WorkspacePattern, ('${1}' + $AppImagePrefix + '$2')
    if ($Renamed -ne $Text) {
        [System.IO.File]::WriteAllText($File, $Renamed)
    }
}
$Sysbuild = [System.IO.File]::ReadAllText((Join-Path $Overlay "primary/sysbuild.cmake"))
if ((Select-String -Path $OverlayFiles -Pattern $ImageNamePattern, $WorkspacePattern -CaseSensitive -Quiet) -or
    -not $Sysbuild.Contains("APPLICATION ${AppImagePrefix}_secondary_core")) {
    throw "The NXP template image names could not be renamed."
}

Push-Location $SdkRoot
try {
    & $West build -b evkmimxrt1180 --sysbuild (Join-Path $Overlay "primary") --config flexspi_nor_debug --toolchain armgcc -Dcore_id=cm33 "-DPython3_EXECUTABLE=$VenvPython" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -p always -d $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "RT1180 SysBuild failed." }
}
finally {
    Pop-Location
}

Write-Host "Build completed: $BuildDir"
