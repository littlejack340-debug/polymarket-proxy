<#
    Brasier Suite - configuration + compilation (Visual Studio 2022, x64).

    Usage (depuis le dossier brasier-suite) :
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1               # Release
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests        # + tests hors-ligne
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Install      # + copie dans le dossier VST3
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Config Debug
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',
    [switch] $Tests,
    [switch] $Install
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root 'build\vs2022'

Push-Location $root
try {
    # 1. Configuration (le premier passage telecharge JUCE : quelques minutes)
    $configureArgs = @('--preset', 'vs2022')
    if ($Tests) { $configureArgs += '-DBRASIER_BUILD_TESTS=ON' }

    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "Echec de la configuration CMake" }

    # 2. Compilation
    & cmake --build $buildDir --config $Config --parallel
    if ($LASTEXITCODE -ne 0) { throw "Echec de la compilation" }

    # 3. Tests hors-ligne (optionnels)
    if ($Tests) {
        $testExe = Join-Path $buildDir "tests\BrasierTests_artefacts\$Config\BrasierTests.exe"
        & $testExe
        if ($LASTEXITCODE -ne 0) { throw "Des tests ont echoue" }
    }

    # 4. Recapitulatif
    Write-Host ""
    Write-Host "Plugins VST3 construits :" -ForegroundColor Green
    Get-ChildItem (Join-Path $buildDir 'plugins') -Recurse -Directory -Filter '*.vst3' |
        Where-Object { $_.FullName -match "\\$Config\\VST3\\[^\\]+\.vst3$" } |
        ForEach-Object { Write-Host "  $($_.FullName)" }

    Write-Host ""
    Write-Host "Applications Standalone :" -ForegroundColor Green
    Get-ChildItem (Join-Path $buildDir 'plugins') -Recurse -File -Filter '*.exe' |
        Where-Object { $_.FullName -match "\\$Config\\Standalone\\" } |
        ForEach-Object { Write-Host "  $($_.FullName)" }

    # 5. Installation (optionnelle)
    if ($Install) {
        & (Join-Path $PSScriptRoot 'install-vst3.ps1') -Config $Config
    }
}
finally {
    Pop-Location
}
