<#
.SYNOPSIS
    build.ps1 — Compile FF777NavStudio sans Qt Creator (Windows).

.DESCRIPTION
    Équivalent Windows / PowerShell de build.sh, calé sur la configuration
    imposée pour les utilisateurs Windows du repo :

        - Qt 6.11 (ou plus récent), composant "MinGW 64-bit"
        - Toolchain "MinGW 64-bit" installée via le Maintenance Tool Qt
          (Qt Creator/Qt Online Installer -> Additional Libraries / Developer
          and Designer Tools -> MinGW 13.x 64-bit)
        - CMake : soit celui installé avec Qt Creator (Developer Tools),
          soit un CMake système dans le PATH.

    Le script détecte automatiquement le kit "mingw_64" sous C:\Qt, ajoute
    temporairement le compilateur MinGW correspondant (et CMake/Ninja s'ils
    sont fournis par Qt) au PATH du process, puis configure et compile.

.USAGE
    .\build.ps1                                # config + compilation + déploiement des DLL Qt
    .\build.ps1 -Clean                         # supprime le dossier de build
    $env:QT_PATH="C:\Qt\6.11.0\mingw_64"; .\build.ps1   # force un kit Qt précis
    .\build.ps1 -Generator Ninja                # utilise Ninja au lieu de MinGW Makefiles

    Le résultat se trouve dans build2\FF777NavStudio.exe.

.PREREQUIS (à documenter dans le README du repo)
    1. Installer Qt (https://www.qt.io/download-qt-installer) et sélectionner :
         - Qt 6.11.x -> MinGW 64-bit
         - Developer and Designer Tools -> MinGW 13.x 64-bit
         - (optionnel mais recommandé) Developer and Designer Tools -> CMake
    2. Vérifier que l'installation se trouve sous C:\Qt (emplacement par
       défaut) ou définir $env:QT_PATH vers le dossier du kit mingw_64.
#>

[CmdletBinding()]
param(
    [switch]$Clean,
    [switch]$Help,
    [ValidateSet("MinGW Makefiles", "Ninja")]
    [string]$Generator = "MinGW Makefiles",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$ProjectDir = $PSScriptRoot
$BuildDir   = Join-Path $ProjectDir "build2"
$Jobs       = if ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { 4 }

# --- Aide --------------------------------------------------------------
if ($Help) {
    Get-Help $PSCommandPath -Full
    exit 0
}

# --- Nettoyage -----------------------------------------------------------
if ($Clean) {
    if (Test-Path $BuildDir) {
        Remove-Item -Recurse -Force $BuildDir
    }
    Write-Host ">> $BuildDir supprimé."
    exit 0
}

# --- Détection du kit Qt "mingw_64" ----------------------------------------
function Find-QtMingwKit {
    if ($env:QT_PATH)            { return $env:QT_PATH }
    if ($env:QT_INSTALL_PREFIX)  { return $env:QT_INSTALL_PREFIX }

    $roots = @("C:\Qt", "$env:USERPROFILE\Qt")
    $candidates = @()

    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }

        $versionDirs = Get-ChildItem -Path $root -Directory -Filter "6.*" -ErrorAction SilentlyContinue |
            Sort-Object { [version]($_.Name -replace '[^0-9.].*$','') } -Descending

        foreach ($verDir in $versionDirs) {
            $kit = Join-Path $verDir.FullName "mingw_64"
            if (Test-Path (Join-Path $kit "lib\cmake\Qt6")) {
                $candidates += $kit
            }
        }
    }

    if ($candidates.Count -gt 0) { return $candidates[0] }
    return $null
}

$QtPrefix = Find-QtMingwKit
if (-not $QtPrefix) {
    Write-Error "Kit Qt 'mingw_64' introuvable sous C:\Qt. Installez Qt 6.11+ avec le composant MinGW 64-bit, ou définissez `$env:QT_PATH."
    exit 1
}
Write-Host ">> Qt utilisée (MinGW 64-bit) : $QtPrefix"

# --- Mise en PATH du toolchain MinGW / CMake / Ninja fournis par Qt --------
# C:\Qt\6.11.0\mingw_64  ->  C:\Qt\6.11.0  ->  C:\Qt
$QtVersionDir = Split-Path $QtPrefix -Parent
$QtRoot       = Split-Path $QtVersionDir -Parent
$ToolsDir     = Join-Path $QtRoot "Tools"

$prependPaths = @()

# Compilateur MinGW (obligatoire)
$mingwDir = Get-ChildItem -Path $ToolsDir -Directory -Filter "mingw*_64" -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending | Select-Object -First 1
if ($mingwDir -and (Test-Path (Join-Path $mingwDir.FullName "bin\gcc.exe"))) {
    Write-Host ">> Toolchain MinGW détectée : $($mingwDir.FullName)"
    $prependPaths += (Join-Path $mingwDir.FullName "bin")
}
elseif (-not (Get-Command gcc.exe -ErrorAction SilentlyContinue)) {
    Write-Error "Toolchain MinGW introuvable sous $ToolsDir et absente du PATH. Installez le composant 'MinGW 64-bit' via le Maintenance Tool Qt."
    exit 1
}

# CMake fourni par Qt (utilisé seulement si aucun CMake système n'est déjà dans le PATH)
if (-not (Get-Command cmake.exe -ErrorAction SilentlyContinue)) {
    $qtCmake = Join-Path $ToolsDir "CMake_64\bin"
    if (Test-Path (Join-Path $qtCmake "cmake.exe")) {
        Write-Host ">> CMake (fourni par Qt) détecté : $qtCmake"
        $prependPaths += $qtCmake
    }
    else {
        Write-Error "CMake introuvable (ni système, ni sous $ToolsDir\CMake_64). Installez-le via le Maintenance Tool Qt ou séparément."
        exit 1
    }
}

# Ninja (uniquement si demandé via -Generator Ninja)
if ($Generator -eq "Ninja" -and -not (Get-Command ninja.exe -ErrorAction SilentlyContinue)) {
    $qtNinja = Join-Path $ToolsDir "Ninja"
    if (Test-Path (Join-Path $qtNinja "ninja.exe")) {
        Write-Host ">> Ninja (fourni par Qt) détecté : $qtNinja"
        $prependPaths += $qtNinja
    }
    else {
        Write-Error "Ninja introuvable. Installez-le via le Maintenance Tool Qt (Developer and Designer Tools -> Ninja) ou utilisez -Generator 'MinGW Makefiles'."
        exit 1
    }
}

if ($prependPaths.Count -gt 0) {
    $env:PATH = ($prependPaths -join ";") + ";" + $env:PATH
}

# --- Configuration et compilation ------------------------------------------
$cmakeArgs = @(
    "-S", $ProjectDir,
    "-B", $BuildDir,
    "-G", $Generator,
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DCMAKE_PREFIX_PATH=$QtPrefix"
)

& cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake --build $BuildDir --parallel $Jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# --- Déploiement des DLL Qt (équivalent de macdeployqt) --------------------
function Deploy-Windows {
    $windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe"
    $exePath = Join-Path $BuildDir "FF777NavStudio.exe"

    if (-not (Test-Path $exePath)) {
        Write-Warning "Exécutable FF777NavStudio.exe introuvable dans $BuildDir, déploiement ignoré."
        return $null
    }

    if (Test-Path $windeployqt) {
        Write-Host ">> Création du bundle autonome (windeployqt)..."
        & $windeployqt $exePath "--$($Config.ToLower())"

        # Nettoyage chirurgical APRÈS déploiement : on supprime les pilotes
        # SQL inutiles, mais on préserve qsqlite.dll !
        $sqlPluginsDir = Join-Path (Split-Path $exePath) "sqldrivers"
        if (Test-Path $sqlPluginsDir) {
            Write-Host ">> Nettoyage chirurgical des pilotes SQL inutilisés (Postgres, ODBC, Mimer, MySQL)..."
            @("qsqlodbc.dll", "qsqlpsql.dll", "qsqlmimer.dll", "qsqlmysql.dll") | ForEach-Object {
                $p = Join-Path $sqlPluginsDir $_
                if (Test-Path $p) { Remove-Item -Force $p }
            }
            Write-Host ">> Nettoyage SQL terminé. Pilote SQLite préservé."
        }
    }
    else {
        Write-Warning "windeployqt introuvable dans $QtPrefix\bin, DLL Qt non déployées."
    }

    return $exePath
}

$finalExe = Deploy-Windows

Write-Host ""
if ($finalExe) {
    Write-Host ">> Terminé. Lancez l'application : $finalExe"
} else {
    Write-Host ">> Terminé. Vérifiez le dossier $BuildDir pour l'exécutable."
}
