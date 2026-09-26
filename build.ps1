param([switch]$SkipTests, [switch]$StaticRuntime, [switch]$Shared)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -prerelease -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Install Visual Studio Desktop development with C++.' }
$vcVersion = (Get-Content (Join-Path $installation 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')).Trim()
$vc = Join-Path $installation "VC\Tools\MSVC\$vcVersion"
$sdkRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10'
$sdkVersion = Get-ChildItem (Join-Path $sdkRoot 'Include') -Directory |
    Where-Object { (Test-Path (Join-Path $_.FullName 'um\Windows.h')) -and (Test-Path (Join-Path $_.FullName 'ucrt\stdio.h')) } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1 -ExpandProperty Name
if (-not $sdkVersion) { throw 'Install the Windows SDK.' }
$cmakeRoot = Join-Path $installation 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmake = Join-Path $cmakeRoot 'CMake\bin\cmake.exe'
$ctest = Join-Path $cmakeRoot 'CMake\bin\ctest.exe'
$ninja = Join-Path $cmakeRoot 'Ninja\ninja.exe'
$buildDir = Join-Path $taskRoot 'build\ninja'
if ($Shared) { $buildDir = Join-Path $taskRoot 'build\shared' }
if ($Shared -and $StaticRuntime) { throw 'Use -Shared without -StaticRuntime.' }
$savedInclude = $env:INCLUDE
$savedLib = $env:LIB
$savedPath = $env:PATH
$savedLanguage = $env:VSLANG
try {
    $env:VSLANG = '1033'
    $env:INCLUDE = "$vc\include;$sdkRoot\Include\$sdkVersion\ucrt;$sdkRoot\Include\$sdkVersion\shared;$sdkRoot\Include\$sdkVersion\um;$sdkRoot\Include\$sdkVersion\winrt"
    $env:LIB = "$vc\lib\x64;$sdkRoot\Lib\$sdkVersion\ucrt\x64;$sdkRoot\Lib\$sdkVersion\um\x64"
    $env:PATH = "$vc\bin\Hostx64\x64;$sdkRoot\bin\$sdkVersion\x64;$savedPath"
    New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    $probe = Join-Path $buildDir 'include-prefix.cpp'
    Set-Content -LiteralPath $probe -Value '#include <stddef.h>' -Encoding ascii
    $probeOutput = & "$vc\bin\Hostx64\x64\cl.exe" /nologo /showIncludes /c /TP $probe "/Fo$buildDir\include-prefix.obj" 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'Compiler probe failed.' }
    $includePrefix = $null
    foreach ($line in $probeOutput) {
        if ([string]$line -match '^(.*?:)\s+[A-Za-z]:[\\/]') { $includePrefix = $Matches[1]; break }
    }
    if (-not $includePrefix) { throw 'Could not detect MSVC include prefix.' }
    $prefixFile = Join-Path $buildDir 'include-prefix.txt'
    [IO.File]::WriteAllText($prefixFile, $includePrefix, [Text.UTF8Encoding]::new($false))
    $runtimeOption = if ($StaticRuntime) { 'ON' } else { 'OFF' }
    $sharedOption = if ($Shared) { 'ON' } else { 'OFF' }
    & $cmake -S $taskRoot -B $buildDir -G Ninja '-DCMAKE_BUILD_TYPE=Release' "-DVGUI_BUILD_SHARED=$sharedOption" "-DVGUI_STATIC_RUNTIME=$runtimeOption" "-DVGUI_MSVC_INCLUDE_PREFIX_FILE=$prefixFile" "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$vc\bin\Hostx64\x64\cl.exe"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $buildDir
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if (-not $SkipTests) {
        & $ctest --test-dir $buildDir --output-on-failure
        if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
    }
    # Keep the existing demo link working after a rebuild.
    if (-not $Shared) {
        Copy-Item -LiteralPath (Join-Path $buildDir 'vgui_demo.exe') -Destination (Join-Path $taskRoot 'build\vgui_demo.exe') -Force
    }
    Write-Host "Demo: $buildDir\vgui_demo.exe"
} finally {
    $env:INCLUDE = $savedInclude
    $env:LIB = $savedLib
    $env:PATH = $savedPath
    $env:VSLANG = $savedLanguage
}
