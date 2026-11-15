<#
    Brasier Suite - copie des .vst3 construits dans C:\Program Files\Common Files\VST3.

    Ce dossier est protege : le script se relance tout seul en administrateur.
    Ferme FL Studio avant (un plugin charge ne peut pas etre remplace).

    Usage (depuis le dossier brasier-suite) :
        powershell -ExecutionPolicy Bypass -File scripts\install-vst3.ps1
        (le dossier de build le plus recent, build\vs2022 ou build\vs2026, est choisi tout seul)
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string] $Config = 'Release',
    [string] $BuildDir = '',
    [switch] $Pause
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $env:CommonProgramFiles 'VST3'

# Dossier de build : celui passe en parametre, sinon le plus recent qui contient des plugins
if (-not $BuildDir) {
    $BuildDir = @('vs2026', 'vs2022') |
        ForEach-Object { Join-Path $root "build\$_" } |
        Where-Object { Test-Path (Join-Path $_ 'plugins') } |
        Sort-Object { (Get-Item (Join-Path $_ 'plugins')).LastWriteTime } -Descending |
        Select-Object -First 1
}

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Droits administrateur requis pour ecrire dans $dest : relance en administrateur..." -ForegroundColor Yellow
    $relaunchArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"", '-Config', $Config, '-Pause')
    if ($BuildDir) { $relaunchArgs += @('-BuildDir', "`"$BuildDir`"") }
    Start-Process powershell -Verb RunAs -ArgumentList $relaunchArgs -Wait
    exit
}

try {
    if (-not $BuildDir) {
        throw "Aucun dossier de build trouve : lance d'abord scripts\build.ps1"
    }

    $pluginsDir = Join-Path $BuildDir 'plugins'
    $bundles = Get-ChildItem $pluginsDir -Recurse -Directory -Filter '*.vst3' |
        Where-Object { $_.FullName -match "\\$Config\\VST3\\[^\\]+\.vst3$" }

    if (-not $bundles) {
        throw "Aucun .vst3 trouve dans $pluginsDir : lance d'abord scripts\build.ps1"
    }

    New-Item -ItemType Directory -Force -Path $dest | Out-Null

    foreach ($bundle in $bundles) {
        $target = Join-Path $dest $bundle.Name
        if (Test-Path $target) { Remove-Item $target -Recurse -Force }
        Copy-Item $bundle.FullName $target -Recurse
        Write-Host "Installe : $target" -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "Dans FL Studio : Options > Manage plugins > Find installed plugins (ou Start scan)." -ForegroundColor Cyan
}
catch {
    Write-Host "ERREUR : $_" -ForegroundColor Red
}
finally {
    if ($Pause) { Read-Host "Appuie sur Entree pour fermer" }
}
