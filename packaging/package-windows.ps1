[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$SourceDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ArtifactsDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$')]
    [string]$Version,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9A-Za-z._-]+$')]
    [string]$Label,

    [Parameter(Mandatory = $true)]
    [string]$InnoCompiler
)

$ErrorActionPreference = 'Stop'

$sourcePath = (Resolve-Path -LiteralPath $SourceDirectory).Path
$compilerPath = (Resolve-Path -LiteralPath $InnoCompiler).Path
$mainExecutable = Join-Path $sourcePath 'bin\spectra_qt.exe'
if (-not (Test-Path -LiteralPath $mainExecutable -PathType Leaf)) {
    throw "The installed application is missing: $mainExecutable"
}

New-Item -ItemType Directory -Force -Path $ArtifactsDirectory | Out-Null
$artifactsPath = (Resolve-Path -LiteralPath $ArtifactsDirectory).Path
$packageBaseName = "Spectra-$Label-windows-x64"
$archivePath = Join-Path $artifactsPath "$packageBaseName.zip"
$installerBaseName = "$packageBaseName-setup"
$installerPath = Join-Path $artifactsPath "$installerBaseName.exe"

Compress-Archive -LiteralPath $sourcePath -DestinationPath $archivePath -Force

$innoArguments = @(
    "/DAppVersion=$Version"
    "/DSourceDir=$sourcePath"
    "/DOutputDir=$artifactsPath"
    "/DOutputBaseFilename=$installerBaseName"
    (Join-Path $PSScriptRoot 'Spectra.iss')
)
& $compilerPath @innoArguments
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $installerPath -PathType Leaf)) {
    throw "The installer was not created: $installerPath"
}

function Write-Sha256File {
    param([Parameter(Mandatory = $true)][string]$FilePath)

    $hash = (Get-FileHash -LiteralPath $FilePath -Algorithm SHA256).Hash.ToLowerInvariant()
    $fileName = Split-Path -Leaf $FilePath
    "$hash  $fileName" | Set-Content -LiteralPath "$FilePath.sha256" -Encoding ascii
}

Write-Sha256File -FilePath $archivePath
Write-Sha256File -FilePath $installerPath

Get-Item -LiteralPath $archivePath, "$archivePath.sha256", $installerPath, "$installerPath.sha256"
