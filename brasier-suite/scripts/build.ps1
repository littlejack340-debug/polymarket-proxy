<#
    Brasier Suite - configuration + compilation (Visual Studio 2022 ou 2026, x64).

    Usage (depuis le dossier brasier-suite) :
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1               # Release
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests        # + tests hors-ligne
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Install      # + copie dans le dossier VST3
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Config Debug
        powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -VisualStudio 2026

    Par defaut, le script choisit tout seul la version de Visual Studio installee
    (2022 en priorite, sinon 2026 ; les "Build Tools" suffisent).
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',
    [ValidateSet('auto', '2022', '2026')]
    [string] $VisualStudio = 'auto',
    [switch] $Tests,
    [switch] $Install
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# ---------------------------------------------------------------------------
# Version de Visual Studio (avec les outils C++)
# ---------------------------------------------------------------------------
function Find-VisualStudioWithCpp ([string] $range) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    return & $vswhere -version $range -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
}

function Get-CMakeVersion ([string] $exe) {
    $line = (& $exe --version | Select-Object -First 1)
    if ($line -match '(\d+)\.(\d+)\.(\d+)') { return [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])" }
    return $null
}

# Nom du generateur Visual Studio (ex. "Visual Studio 18 2026") connu de ce CMake, ou $null
function Get-VsGenerator ([string] $exe, [string] $vsMajor) {
    return & $exe --help | ForEach-Object {
        if ($_ -match "^\*?\s*(Visual Studio $vsMajor \d{4})") { $Matches[1] }
    } | Select-Object -First 1
}

# CMake : celui du PATH, sinon C:\Program Files\CMake, sinon celui fourni avec Visual Studio.
# On garde le premier qui est assez recent pour JUCE (3.22) ET qui connait le generateur voulu.
function Find-CMake ([string] $vsPath, [string] $vsMajor) {
    $candidates = @()
    $inPath = Get-Command cmake -ErrorAction SilentlyContinue
    if ($inPath) { $candidates += $inPath.Source }
    $candidates += (Join-Path $env:ProgramFiles 'CMake\bin\cmake.exe')
    if ($vsPath) { $candidates += (Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe') }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            $version = Get-CMakeVersion $candidate
            if ($version -and $version -ge [version]'3.22.0') {
                $gen = Get-VsGenerator $candidate $vsMajor
                if ($gen) { return @{ Exe = $candidate; Generator = $gen; Version = $version } }
            }
        }
    }
    return $null
}

$vsPath = $null
if ($VisualStudio -eq 'auto') {
    $vsPath = Find-VisualStudioWithCpp '[17.0,18.0)'
    if ($vsPath) { $VisualStudio = '2022' }
    else {
        $vsPath = Find-VisualStudioWithCpp '[18.0,19.0)'
        if ($vsPath) { $VisualStudio = '2026' }
        else { throw "Aucun Visual Studio 2022 ou 2026 avec les outils C++ : lance scripts\check-prereqs.ps1" }
    }
}
else {
    $vsPath = Find-VisualStudioWithCpp $(if ($VisualStudio -eq '2026') { '[18.0,19.0)' } else { '[17.0,18.0)' })
}

$vsMajor = if ($VisualStudio -eq '2026') { '18' } else { '17' }
$found = Find-CMake $vsPath $vsMajor
if (-not $found) {
    throw "Aucun CMake (3.22+) connaissant Visual Studio $VisualStudio : winget install Kitware.CMake (puis rouvre le terminal)"
}
$cmake = $found.Exe
$generator = $found.Generator
Write-Host "CMake $($found.Version) : $cmake" -ForegroundColor Cyan

$buildDir = Join-Path $root "build\vs$VisualStudio"
Write-Host "Visual Studio $VisualStudio -> generateur '$generator', dossier $buildDir" -ForegroundColor Cyan

Push-Location $root
try {
    # 1. Configuration (le premier passage telecharge JUCE : quelques minutes)
    $configureArgs = @('-S', '.', '-B', $buildDir, '-G', $generator, '-A', 'x64')
    if ($Tests) { $configureArgs += '-DBRASIER_BUILD_TESTS=ON' }

    & $cmake @configureArgs
    if ($LASTEXITCODE -ne 0) { throw "Echec de la configuration CMake" }

    # 2. Compilation
    & $cmake --build $buildDir --config $Config --parallel
    if ($LASTEXITCODE -ne 0) { throw "Echec de la compilation" }

    # 3. Tests hors-ligne puis chargement des vrais .vst3 par un hote de test (optionnels)
    if ($Tests) {
        $testExe = Join-Path $buildDir "tests\BrasierTests_artefacts\$Config\BrasierTests.exe"
        & $testExe
        if ($LASTEXITCODE -ne 0) { throw "Des tests ont echoue" }

        $hostExe = Join-Path $buildDir "tests\BrasierHostTest_artefacts\$Config\BrasierHostTest.exe"
        & $hostExe (Join-Path $buildDir 'plugins')
        if ($LASTEXITCODE -ne 0) { throw "Le test de chargement des VST3 a echoue" }
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
        & (Join-Path $PSScriptRoot 'install-vst3.ps1') -Config $Config -BuildDir $buildDir
    }
}
finally {
    Pop-Location
}
