param([switch]$Force, [string]$TargetDirectory)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($TargetDirectory)) {
    $tools = Join-Path $projectRoot 'tools'
} elseif ([IO.Path]::IsPathRooted($TargetDirectory)) {
    $tools = $TargetDirectory
} else {
    $tools = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot $TargetDirectory))
}
New-Item -ItemType Directory -Path $tools -Force | Out-Null
function Get-VerifiedFile([string]$Uri, [string]$ChecksumUri, [string]$Name, [switch]$Manifest) {
    $target = Join-Path $tools $Name
    $staging = "$target.download"
    $checksumFile = "$target.sha256"
    Invoke-WebRequest -Uri $Uri -OutFile $staging -TimeoutSec 240
    Invoke-WebRequest -Uri $ChecksumUri -OutFile $checksumFile -TimeoutSec 60
    if ($Manifest) {
        $line = Get-Content -LiteralPath $checksumFile | Where-Object { $_ -match ('\s' + [regex]::Escape($Name) + '$') }
        $expected = ($line -split '\s+')[0]
    } else {
        $expected = [regex]::Match((Get-Content -LiteralPath $checksumFile -Raw), '(?i)[a-f0-9]{64}').Value
    }
    $actual = (Get-FileHash -LiteralPath $staging -Algorithm SHA256).Hash
    if (!$expected -or $actual -ne $expected) { throw "SHA-256 mismatch: $Name. Existing executable was not changed." }
    Move-Item -LiteralPath $staging -Destination $target -Force
}
if ($Force -or !(Test-Path -LiteralPath (Join-Path $tools 'yt-dlp.exe'))) {
    Get-VerifiedFile -Uri 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe' -ChecksumUri 'https://github.com/yt-dlp/yt-dlp/releases/latest/download/SHA2-256SUMS' -Name 'yt-dlp.exe' -Manifest
}
if ($Force -or !(Test-Path -LiteralPath (Join-Path $tools 'ffmpeg.exe')) -or !(Test-Path -LiteralPath (Join-Path $tools 'ffprobe.exe'))) {
    Get-VerifiedFile -Uri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip' -ChecksumUri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip.sha256' -Name 'ffmpeg-release-essentials.zip'
    $package = Join-Path $tools 'ffmpeg-package'
    Expand-Archive -LiteralPath (Join-Path $tools 'ffmpeg-release-essentials.zip') -DestinationPath $package -Force
    $binary = Get-ChildItem -LiteralPath $package -Filter ffmpeg.exe -Recurse | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (!$binary) { throw 'ffmpeg.exe missing from package' }
    Copy-Item -LiteralPath $binary.FullName -Destination (Join-Path $tools 'ffmpeg.exe') -Force
    Copy-Item -LiteralPath (Join-Path $binary.DirectoryName 'ffprobe.exe') -Destination (Join-Path $tools 'ffprobe.exe') -Force
}
& (Join-Path $tools 'yt-dlp.exe') --version
& (Join-Path $tools 'ffmpeg.exe') -version | Select-Object -First 1
Write-Output 'Tools ready. Node.js 22+ must be available on PATH or configured in the app.'
