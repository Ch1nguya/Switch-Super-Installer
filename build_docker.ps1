param(
    [string]$Tag = "20240224",
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
    & docker run --rm -v $mount -w /source $Image bash -lc $MakeCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Docker build command failed with exit code $LASTEXITCODE"
    }
}

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

Write-Host "== Switch-Super-Installer Docker build ==" -ForegroundColor Green
Write-Host "Repo : $repoRoot"
Write-Host "Tag  : $Tag"
Write-Host "Jobs : $Jobs"
Write-Host ""

try {
    docker --version | Out-Null
}
catch {
    throw "Docker is not available. Start Docker Desktop and retry."
}

$image = "devkitpro/devkita64:$Tag"

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
