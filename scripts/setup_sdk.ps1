$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = if ($env:MCUXPRESSO_SDK) { $env:MCUXPRESSO_SDK } else { Join-Path (Split-Path -Parent $ProjectRoot) "mcuxpresso-sdk" }
$SdkTag = "v26.09.00"
$Board = "evkmimxrt1180"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git is required."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -or -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
    throw "Install CMake and Ninja before continuing."
}

$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) {
    $Python = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $Python) {
    throw "Python 3.8 or newer is required."
}
$PythonCommand = $Python.Source
$PythonArgs = @()
if ($Python.Name -eq "py.exe" -or $Python.Name -eq "py") {
    $PythonArgs = @("-3")
}
& $PythonCommand @PythonArgs -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)"
if ($LASTEXITCODE -ne 0) {
    throw "Python 3.8 or newer is required."
}

if (-not (Get-Command west -ErrorAction SilentlyContinue)) {
    & $PythonCommand @PythonArgs -m pip install --user west
    if ($LASTEXITCODE -ne 0) { throw "Unable to install west." }
    $UserBase = (& $PythonCommand @PythonArgs -c "import site; print(site.USER_BASE)").Trim()
    $env:Path = (Join-Path $UserBase "Scripts") + ";" + $env:Path
}
if (-not (Get-Command west -ErrorAction SilentlyContinue)) {
    throw "west is not on PATH. Add the Python user Scripts directory to PATH and retry."
}

$SdkParent = Split-Path -Parent $SdkRoot
New-Item -ItemType Directory -Force -Path $SdkParent | Out-Null
if (-not (Test-Path (Join-Path $SdkRoot ".west"))) {
    if ((Test-Path $SdkRoot) -and (Get-ChildItem -Force $SdkRoot | Select-Object -First 1)) {
        throw "$SdkRoot exists and is not an initialized West workspace. Select an empty directory with MCUXPRESSO_SDK."
    }
    if (Test-Path $SdkRoot) { Remove-Item -Force $SdkRoot }
    west init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git $SdkRoot --mr $SdkTag
    if ($LASTEXITCODE -ne 0) { throw "west init failed." }
}

Push-Location $SdkRoot
try {
    west manifest --resolve | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Unable to resolve the pinned SDK manifest." }
    west update_board --set board $Board
    if ($LASTEXITCODE -ne 0) { throw "west update_board failed." }
    west list
    if ($LASTEXITCODE -ne 0) { throw "west list failed." }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "MCUXpresso SDK workspace ready: $SdkRoot"
Write-Host "Set MCUXPRESSO_SDK if you use a different location."
Write-Host "Install Arm GNU Toolchain 14.2.x and set ARMGCC_DIR to its installation root."
Write-Host "Then run the VS Code task: Build RT1180 dual-core firmware."
