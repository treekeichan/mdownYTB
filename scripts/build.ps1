param([string]$QtRoot = 'C:\Qt\6.11.0\mingw_64', [switch]$Package)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$toolRoot = Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) 'Tools'
$cmake = Join-Path $toolRoot 'CMake_64\bin\cmake.exe'
$ninja = Join-Path $toolRoot 'Ninja\ninja.exe'
$compiler = Join-Path $toolRoot 'mingw1310_64\bin'
$env:PATH = "$QtRoot\bin;$compiler;$env:PATH"
$buildDir = Join-Path $projectRoot 'build'
& $cmake -S $projectRoot -B $buildDir -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_PREFIX_PATH=$QtRoot" '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_TESTING=ON'
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
& $cmake --build $buildDir --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
& (Join-Path $toolRoot 'CMake_64\bin\ctest.exe') --test-dir $buildDir --output-on-failure
if ($LASTEXITCODE -ne 0) {
    Get-ChildItem -LiteralPath $buildDir -Filter '*-tests-results.txt' | ForEach-Object { Get-Content -LiteralPath $_.FullName }
    throw 'Tests failed'
}
if ($Package) {
    $projectVersion = [regex]::Match((Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw), 'project\(mdownYTB VERSION ([0-9.]+)').Groups[1].Value
    if (!$projectVersion) { throw 'Project version missing' }
    $dist = Join-Path $projectRoot "dist\mdownYTB-v$projectVersion"
    New-Item -ItemType Directory -Path $dist -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildDir 'mdownYTB.exe') -Destination $dist
    & (Join-Path $QtRoot 'bin\windeployqt.exe') --release --no-translations --no-system-d3d-compiler --no-opengl-sw (Join-Path $dist 'mdownYTB.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
    $distTools = Join-Path $dist 'tools'
    New-Item -ItemType Directory -Path $distTools -Force | Out-Null
    foreach ($name in @('yt-dlp.exe', 'ffmpeg.exe', 'ffprobe.exe')) {
        $source = Join-Path $projectRoot "tools\$name"
        if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $distTools }
    }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $dist
    Copy-Item -LiteralPath (Join-Path $projectRoot 'THIRD_PARTY.md') -Destination $dist
    $licenses = Join-Path $dist 'licenses'
    New-Item -ItemType Directory -Path $licenses -Force | Out-Null
    $qtLicense = Join-Path (Split-Path (Split-Path $QtRoot -Parent) -Parent) 'Licenses\LICENSE'
    if (Test-Path -LiteralPath $qtLicense) { Copy-Item -LiteralPath $qtLicense -Destination (Join-Path $licenses 'Qt-LICENSE.txt') -Force }
    $ffmpegPackage = Join-Path $projectRoot 'tools\ffmpeg-package'
    if (Test-Path -LiteralPath $ffmpegPackage) {
        $ffLicense = Get-ChildItem -LiteralPath $ffmpegPackage -Recurse -File -Filter LICENSE | Select-Object -First 1
        if ($ffLicense) { Copy-Item -LiteralPath $ffLicense.FullName -Destination (Join-Path $licenses 'FFmpeg-LICENSE.txt') -Force }
    }
    Write-Output "Packaged app: $dist\mdownYTB.exe"
}
