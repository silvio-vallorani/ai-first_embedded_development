<#
.SYNOPSIS
Build the CM33 + CM7 LED-toggle application through the NXP SysBuild template.
.PARAMETER Board
Board under boards/: mimxrt1180_evk (default) or am254-4.
.PARAMETER McuBoot
Link for the MCUboot primary slot (ADR-0014) instead of direct ROM boot.
.PARAMETER Version
Version X.Y.Z printed by the application (use the signed image version).
.PARAMETER TogglePeriodUs
LED toggle period in microseconds.
.PARAMETER NoConfirm
MCUboot variant only: never confirm a test image (rollback test).
#>
param(
    [string]$Board = "mimxrt1180_evk",
    [switch]$McuBoot,
    [ValidatePattern('^\d+\.\d+\.\d+$')][string]$Version,
    [ValidateRange(1, [uint32]::MaxValue)][uint32]$TogglePeriodUs,
    [switch]$NoConfirm
)
$ErrorActionPreference = "Stop"
if ($NoConfirm -and -not $McuBoot) { throw "-NoConfirm requires -McuBoot." }

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = if ($env:MCUXPRESSO_SDK) { $env:MCUXPRESSO_SDK } else { Join-Path (Split-Path -Parent $ProjectRoot) "mcuxpresso-sdk" }
$SdkExamples = Join-Path $SdkRoot "mcuxsdk/examples/multicore_examples"
$Template = Join-Path $SdkExamples "hello_world"
$Overlay = Join-Path $SdkExamples "aied_dual_core_led_toggle"
$ExampleDir = Join-Path $ProjectRoot "examples/dual_core_led_toggle"
$BoardDir = Join-Path $ProjectRoot "boards/$Board"
if (-not (Test-Path (Join-Path $BoardDir "include/aied_board.h"))) {
    throw "Unknown board $Board (expected boards/<name>/include/aied_board.h)."
}
$Variant = if ($McuBoot) { "mcuboot" } else { "standalone" }
# Short build-directory prefix per board (Windows path-length limit, see the rt1180-build skill).
$BuildPrefix = if ($Board -eq "mimxrt1180_evk") { "rt1180" } else { $Board }
$DefaultBuildDir = if ($McuBoot) { "build/$BuildPrefix-led-mcuboot" } else { "build/$BuildPrefix-led-toggle" }
$BuildDir = if ($env:RT1180_BUILD_DIR) { $env:RT1180_BUILD_DIR } else { Join-Path $ProjectRoot $DefaultBuildDir }
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
Copy-Item (Join-Path $ExampleDir "cm33/main.c") (Join-Path $Overlay "primary/main.c") -Force
Copy-Item (Join-Path $ExampleDir "cm7/main.c") (Join-Path $Overlay "secondary/main.c") -Force
Copy-Item (Join-Path $ExampleDir "common/app_events.h") (Join-Path $Overlay "primary/app_events.h") -Force
Copy-Item (Join-Path $ExampleDir "common/app_events.h") (Join-Path $Overlay "secondary/app_events.h") -Force
Copy-Item (Join-Path $BoardDir "include/aied_board.h") (Join-Path $Overlay "primary/aied_board.h") -Force
Copy-Item (Join-Path $BoardDir "include/aied_board.h") (Join-Path $Overlay "secondary/aied_board.h") -Force
# A board without an SDK board port of its own replaces EVK board-port files of an image:
# pin_mux.[ch] and/or hardware_init.c + app.h. CUSTOM_PRJ_CONF_PATHS is merged after the SDK
# board-category prj.conf, which selects the EVK files.
foreach ($Image in @(@("primary", "cm33"), @("secondary", "cm7"))) {
    $Src = Join-Path $BoardDir "sdk/$($Image[1])"
    $ImageDir = Join-Path $Overlay $Image[0]
    $Conf = ""
    $Files = @()
    if (Test-Path (Join-Path $Src "pin_mux.c")) {
        $Conf += "CONFIG_MCUX_PRJSEG_module.board.pinmux_customize_folder=y`n"
        $Files += "pin_mux.c", "pin_mux.h"
    }
    if (Test-Path (Join-Path $Src "hardware_init.c")) {
        $Conf += "CONFIG_MCUX_PRJSEG_project.hw_app_customize_folder=y`n"
        $Files += "hardware_init.c", "app.h"
    }
    if ($Files.Count -eq 0) { continue }
    foreach ($File in $Files) { Copy-Item (Join-Path $Src $File) $ImageDir -Force }
    [System.IO.File]::WriteAllText((Join-Path $ImageDir "board_port.conf"), $Conf)
    $ImageCMake = Join-Path $ImageDir "CMakeLists.txt"
    $Text = [System.IO.File]::ReadAllText($ImageCMake)
    $Patched = [regex]::new('(?m)^(project\()').Replace($Text, "set(CUSTOM_PRJ_CONF_PATHS `${CMAKE_CURRENT_LIST_DIR}/board_port.conf)`n`$1", 1)
    if ($Patched -eq $Text) { throw "Cannot select the board-port files in $ImageCMake." }
    [System.IO.File]::WriteAllText($ImageCMake, $Patched + "`nmcux_add_source(`n    SOURCES $($Files -join ' ')`n)`n")
}

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

# Logging service (both variants) and variant/build-time parameters (CM33 primary image only).
$PrimaryCMake = Join-Path $Overlay "primary/CMakeLists.txt"
$PrimaryConf = Join-Path $Overlay "primary/prj.conf"
[System.IO.File]::AppendAllText($PrimaryCMake, "`nset(AIED_ROOT `"$($ProjectRoot -replace '\\', '/')`")`n")
[System.IO.File]::AppendAllText($PrimaryCMake, [System.IO.File]::ReadAllText((Join-Path $ExampleDir "log/primary.cmake")))
[System.IO.File]::AppendAllText($PrimaryConf, [System.IO.File]::ReadAllText((Join-Path $ExampleDir "log/primary.conf")))
if ($McuBoot) {
    [System.IO.File]::AppendAllText($PrimaryCMake, [System.IO.File]::ReadAllText((Join-Path $ExampleDir "mcuboot/primary.cmake")))
    [System.IO.File]::AppendAllText((Join-Path $Overlay "primary/prj.conf"), [System.IO.File]::ReadAllText((Join-Path $ExampleDir "mcuboot/primary.conf")))
} else {
    [System.IO.File]::AppendAllText($PrimaryCMake, [System.IO.File]::ReadAllText((Join-Path $ExampleDir "standalone/primary.cmake")))
}
$AppMacros = @()
if ($Version) {
    $Parts = $Version.Split('.')
    $AppMacros += "-DAPP_VERSION_MAJOR=$($Parts[0]) -DAPP_VERSION_MINOR=$($Parts[1]) -DAPP_VERSION_PATCH=$($Parts[2])"
}
if ($TogglePeriodUs) { $AppMacros += "-DAPP_TOGGLE_PERIOD_US=${TogglePeriodUs}U" }
if ($NoConfirm) { $AppMacros += "-DAIED_SKIP_IMAGE_CONFIRM=1" }
if ($AppMacros.Count -gt 0) {
    [System.IO.File]::AppendAllText($PrimaryCMake, "`nmcux_add_macro(`n    CC `"$($AppMacros -join ' ')`"`n)`n")
}

Push-Location $SdkRoot
try {
    & $West build -b evkmimxrt1180 --sysbuild (Join-Path $Overlay "primary") --config flexspi_nor_debug --toolchain armgcc -Dcore_id=cm33 "-DPython3_EXECUTABLE=$VenvPython" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -p always -d $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "RT1180 SysBuild failed." }
}
finally {
    Pop-Location
}

Write-Host "Build completed ($Board, $Variant): $BuildDir"
