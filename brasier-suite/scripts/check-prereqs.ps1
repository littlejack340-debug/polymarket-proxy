<#
    Brasier Suite - verification des prerequis de build sous Windows.

    Usage (depuis le dossier brasier-suite) :
        powershell -ExecutionPolicy Bypass -File scripts\check-prereqs.ps1

    Verifie : Visual Studio 2022 ou 2026 + outils C++ (MSVC), Windows SDK,
              CMake >= 3.22 (>= 4.2 avec VS 2026), Git, chemins longs, dossier VST3.
#>

$ErrorActionPreference = 'SilentlyContinue'
$script:missing = 0

function Write-Ok   ($msg)       { Write-Host "[OK]      $msg" -ForegroundColor Green }
function Write-Info ($msg)       { Write-Host "[INFO]    $msg" -ForegroundColor Cyan }
function Write-Miss ($msg, $fix) {
    Write-Host "[MANQUE]  $msg" -ForegroundColor Red
    if ($fix) { Write-Host "          -> $fix" -ForegroundColor Yellow }
    $script:missing++
}

Write-Host ""
Write-Host "=== Brasier Suite : verification des prerequis ===" -ForegroundColor White
Write-Host ""

# ---------------------------------------------------------------------------
# 1. Visual Studio 2022 ou 2026 avec les outils C++ (les Build Tools suffisent)
# ---------------------------------------------------------------------------
$vsInstallCmd = 'winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = $null
$vsVersion = $null   # '2022' ou '2026'
$cpp = 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'

if (Test-Path $vswhere) {
    foreach ($candidate in @(@{ Name = '2022'; Range = '[17.0,18.0)' }, @{ Name = '2026'; Range = '[18.0,19.0)' })) {
        $path = & $vswhere -version $candidate.Range -products * -requires $cpp -latest -property installationPath
        if ($path) {
            $vsPath = $path
            $vsVersion = $candidate.Name
            $vsName = & $vswhere -version $candidate.Range -products * -requires $cpp -latest -property displayName
            Write-Ok "$vsName avec les outils C++ : $vsPath"
            break
        }
    }

    if (-not $vsPath) {
        $vsAny = & $vswhere -version '[17.0,19.0)' -products * -latest -property installationPath
        if ($vsAny) {
            Write-Miss "Visual Studio est installe mais SANS les outils C++ ($vsAny)" `
                       "Visual Studio Installer > Modifier > cocher 'Developpement Desktop en C++'"
        }
        else {
            Write-Miss "Aucun Visual Studio 2022 / 2026 avec les outils C++" $vsInstallCmd
        }
    }
}
else {
    Write-Miss "Aucun Visual Studio installe (vswhere.exe introuvable)" $vsInstallCmd
}

# ---------------------------------------------------------------------------
# 2. Windows SDK (installe avec la charge de travail C++)
# ---------------------------------------------------------------------------
$kits = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Include'
$sdks = Get-ChildItem $kits -Directory | Where-Object { Test-Path (Join-Path $_.FullName 'um\windows.h') } | Sort-Object Name -Descending

if ($sdks) {
    Write-Ok "Windows SDK : $($sdks[0].Name)"
}
else {
    Write-Miss "Windows 10/11 SDK introuvable" `
               "Visual Studio Installer > Modifier > Composants individuels > 'Windows 11 SDK'"
}

# ---------------------------------------------------------------------------
# 3. CMake >= 3.22 (minimum de JUCE 8)
# ---------------------------------------------------------------------------
$cmake = Get-Command cmake
if ($cmake) {
    $line = (& cmake --version | Select-Object -First 1)
    if ($line -match '(\d+)\.(\d+)\.(\d+)') {
        $ver = [version]"$($Matches[1]).$($Matches[2]).$($Matches[3])"
        # JUCE 8 demande CMake 3.22 ; Visual Studio 2026 demande un CMake qui connait son generateur (4.2+)
        $minimum = if ($vsVersion -eq '2026') { [version]'4.2.0' } else { [version]'3.22.0' }
        if ($ver -ge $minimum) {
            Write-Ok "CMake $ver : $($cmake.Source)"
        }
        else {
            Write-Miss "CMake $ver est trop ancien ($minimum minimum ici)" "winget upgrade Kitware.CMake  (puis rouvrir le terminal)"
        }
    }
}
else {
    $vsCmake = $null
    if ($vsPath) { $vsCmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }

    if ($vsCmake -and (Test-Path $vsCmake)) {
        Write-Miss "CMake n'est pas dans le PATH (celui fourni avec Visual Studio existe : $vsCmake)" `
                   "winget install Kitware.CMake  (puis rouvrir le terminal)"
    }
    else {
        Write-Miss "CMake introuvable" "winget install Kitware.CMake  (puis rouvrir le terminal)"
    }
}

# ---------------------------------------------------------------------------
# 4. Git (FetchContent telecharge JUCE avec git)
# ---------------------------------------------------------------------------
$git = Get-Command git
if ($git) {
    Write-Ok "$(& git --version)"
}
else {
    Write-Miss "Git introuvable (necessaire pour telecharger JUCE)" "winget install Git.Git  (puis rouvrir le terminal)"
}

# ---------------------------------------------------------------------------
# 5. Informations utiles
# ---------------------------------------------------------------------------
$longPaths = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' -Name LongPathsEnabled).LongPathsEnabled
if ($longPaths -eq 1) {
    Write-Ok "Chemins longs Windows actives"
}
else {
    Write-Info "Chemins longs desactives : place le projet dans un chemin court (ex. C:\dev\)"
}

$here = (Get-Location).Path
if ($here.Length -gt 60) {
    Write-Info "Dossier actuel assez long ($($here.Length) caracteres) : un chemin plus court evite les soucis de MAX_PATH"
}

$vst3Dir = Join-Path $env:CommonProgramFiles 'VST3'
if (Test-Path $vst3Dir) {
    Write-Ok "Dossier VST3 : $vst3Dir"
}
else {
    Write-Info "Le dossier $vst3Dir n'existe pas encore (il sera cree a l'installation)"
}

Write-Host ""
if ($script:missing -eq 0) {
    Write-Host "Tout est pret. Lance : powershell -ExecutionPolicy Bypass -File scripts\build.ps1" -ForegroundColor Green
}
else {
    Write-Host "$($script:missing) element(s) manquant(s) : installe-les puis relance ce script." -ForegroundColor Red
}
Write-Host ""

exit $script:missing
