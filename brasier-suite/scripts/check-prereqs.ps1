<#
    Brasier Suite - verification des prerequis de build sous Windows.

    Usage (depuis le dossier brasier-suite) :
        powershell -ExecutionPolicy Bypass -File scripts\check-prereqs.ps1

    Verifie : Visual Studio 2022 + outils C++ (MSVC), Windows SDK,
              CMake >= 3.22, Git, chemins longs, dossier VST3.
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
# 1. Visual Studio 2022 avec les outils C++ (MSVC v143)
# ---------------------------------------------------------------------------
$vsInstallCmd = 'winget install Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = $null

if (Test-Path $vswhere) {
    $vsCpp = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
    $vsAny = & $vswhere -version '[17.0,18.0)' -products * -latest -property installationPath

    if ($vsCpp) {
        $vsPath = $vsCpp
        $vsName = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property displayName
        $vsVer  = & $vswhere -version '[17.0,18.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationVersion
        Write-Ok "$vsName ($vsVer) avec les outils C++ : $vsPath"
    }
    elseif ($vsAny) {
        Write-Miss "Visual Studio 2022 est installe mais SANS les outils C++" `
                   "Visual Studio Installer > Modifier > cocher la charge de travail 'Developpement Desktop en C++'"
    }
    else {
        $vs2026 = & $vswhere -version '[18.0,19.0)' -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -latest -property installationPath
        if ($vs2026) {
            Write-Info "Visual Studio 2026 avec C++ detecte ($vs2026) : utilisable avec 'cmake --preset vs2026' (CMake 4.2+)"
        }
        Write-Miss "Visual Studio 2022 introuvable" $vsInstallCmd
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
        if ($ver -ge [version]'3.22.0') {
            Write-Ok "CMake $ver : $($cmake.Source)"
        }
        else {
            Write-Miss "CMake $ver est trop ancien (3.22 minimum pour JUCE 8)" "winget install Kitware.CMake  (puis rouvrir le terminal)"
        }
    }
}
else {
    $vsCmake = $null
    if ($vsPath) { $vsCmake = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' }

    if ($vsCmake -and (Test-Path $vsCmake)) {
        Write-Miss "CMake n'est pas dans le PATH (celui fourni avec Visual Studio existe : $vsCmake)" `
                   "Utiliser le terminal 'Developer PowerShell for VS 2022', ou : winget install Kitware.CMake"
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
