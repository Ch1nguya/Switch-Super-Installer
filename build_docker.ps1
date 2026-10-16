param(
    [string]$Image = "devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282",
    [int]$Jobs = 4,
    [switch]$NoClean
)

$ErrorActionPreference = "Stop"

function Run-ContainerMake {
    param(
        [string]$Image,
        [string]$MakeCommand
    )

    $mount = "${PWD}:/source"
    Write-Host ">> docker run --rm -v `"$mount`" -w /source $Image bash -lc `"$MakeCommand`"" -ForegroundColor Cyan
    # Compiler warnings go to stderr: do not let PowerShell treat them as errors, rely on the exit code.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & docker run --rm -v $mount -w /source $Image bash -lc $MakeCommand
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $previous
    if ($exitCode -ne 0) {
        throw "Docker build command failed with exit code $exitCode"
    }
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

Write-Host "== Switch-Super-Installer Docker build ==" -ForegroundColor Green
Write-Host "Repo : $repoRoot"
Write-Host "Image: $Image"
Write-Host "Jobs : $Jobs"
Write-Host ""

try {
    docker --version | Out-Null
}
catch {
    throw "Docker is not available. Start Docker Desktop and retry."
}

$image = $Image

if (-not $NoClean) {
    Run-ContainerMake -Image $image -MakeCommand "make clean"
}

Run-ContainerMake -Image $image -MakeCommand "make -j$Jobs"

$builtNro = Join-Path $repoRoot "source.nro"
$finalNro = Join-Path $repoRoot "Switch-Super-Installer.nro"
if (Test-Path $builtNro) {
    Move-Item -Path $builtNro -Destination $finalNro -Force
}

Write-Host ""
Write-Host "Build completed." -ForegroundColor Green
Write-Host "Output: $finalNro"
