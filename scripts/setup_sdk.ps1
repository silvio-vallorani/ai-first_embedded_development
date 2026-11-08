$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SdkRoot = if ($env:MCUXPRESSO_SDK) { $env:MCUXPRESSO_SDK } else { Join-Path (Split-Path -Parent $ProjectRoot) "mcuxpresso-sdk" }
$VenvDir = Join-Path $ProjectRoot ".venv"
$VenvPython = Join-Path $VenvDir "Scripts/python.exe"
$West = Join-Path $VenvDir "Scripts/west.exe"
$SdkTag = "v26.09.00"
$Board = "evkmimxrt1180"

foreach ($Tool in @("git", "cmake", "ninja")) {
    if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        throw "$Tool is required and must be on PATH."
    }
}

$Python = Get-Command python -ErrorAction SilentlyContinue
if (-not $Python) { $Python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $Python) { throw "Python 3.8 or newer is required." }

$PythonCommand = $Python.Source
$PythonArgs = @()
if ($Python.Name -eq "py.exe" -or $Python.Name -eq "py") { $PythonArgs = @("-3") }
& $PythonCommand @PythonArgs -c "import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)"
if ($LASTEXITCODE -ne 0) { throw "Python 3.8 or newer is required." }

if (-not (Test-Path $VenvPython)) {
    if (Test-Path $VenvDir) {
        throw "$VenvDir exists but is not a valid virtual environment. Remove it and rerun setup."
    }
    & $PythonCommand @PythonArgs -m venv $VenvDir
    if ($LASTEXITCODE -ne 0) { throw "Unable to create the project virtual environment." }
}

& $VenvPython -m pip install --disable-pip-version-check -r (Join-Path $ProjectRoot "requirements.txt")
if ($LASTEXITCODE -ne 0) { throw "Unable to install the pinned Python dependencies." }
if (-not (Test-Path $West)) { throw "West was not installed in the project virtual environment." }
$env:Path = (Join-Path $VenvDir "Scripts") + ";" + $env:Path
& $West --version
if ($LASTEXITCODE -ne 0) { throw "Unable to execute West from the project virtual environment." }

$SdkParent = Split-Path -Parent $SdkRoot
New-Item -ItemType Directory -Force -Path $SdkParent | Out-Null
if (-not (Test-Path (Join-Path $SdkRoot ".west"))) {
    if ((Test-Path $SdkRoot) -and (Get-ChildItem -Force $SdkRoot | Select-Object -First 1)) {
        throw "$SdkRoot exists and is not an initialized West workspace. Select an empty directory with MCUXPRESSO_SDK."
    }
    if (Test-Path $SdkRoot) { Remove-Item -Force $SdkRoot }
    & $West init -m https://github.com/nxp-mcuxpresso/mcuxsdk-manifests.git $SdkRoot --mr $SdkTag
    if ($LASTEXITCODE -ne 0) { throw "west init failed." }
}

Push-Location $SdkRoot
try {
    & $West manifest --resolve | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Unable to resolve the pinned SDK manifest." }
    & $West update_board --set board $Board
    if ($LASTEXITCODE -ne 0) { throw "west update_board failed." }
    & $West list
    if ($LASTEXITCODE -ne 0) { throw "west list failed." }
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Python environment ready: $VenvDir"
Write-Host "West version: $(& $West --version)"
Write-Host "MCUXpresso SDK workspace ready: $SdkRoot"
Write-Host "Set MCUXPRESSO_SDK if you use a different location."
Write-Host "Install Arm GNU Toolchain 14.2.x and set ARMGCC_DIR to its installation root."
Write-Host "Then run the VS Code task: Build RT1180 dual-core firmware."
