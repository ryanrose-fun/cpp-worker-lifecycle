param([string]$BuildDir = 'build')
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path $BuildDir).Path
$exe = Join-Path $source 'Debug/lifetime_repro.exe'
$pdb = Join-Path $source 'Debug/lifetime_repro.pdb'
if (!(Test-Path $exe) -or !(Test-Path $pdb)) { throw 'Build Debug with MSVC before capturing.' }
$bundle = Join-Path $source ('capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory $bundle | Out-Null
Copy-Item -LiteralPath $exe,$pdb -Destination $bundle
Push-Location $bundle
try {
    & './lifetime_repro.exe' --crash
    $crashCode = $LASTEXITCODE
    if (!(Test-Path 'lifetime-crash.dmp')) { throw "Crash exit $crashCode did not produce a dump." }
    Get-FileHash -Algorithm SHA256 -LiteralPath 'lifetime_repro.exe','lifetime_repro.pdb','lifetime-crash.dmp' |
        Select-Object Path,Hash | ConvertTo-Json | Set-Content -Encoding utf8 'manifest.json'
    Write-Output "Captured synthetic crash (exit $crashCode) in $bundle"
} finally { Pop-Location }
