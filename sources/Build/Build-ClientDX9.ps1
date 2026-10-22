param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$DependenciesRoot = "$PSScriptRoot\..\Dependencies\x64",
    [string]$D3DXIncludeDir = '',
    [string]$D3DXLibraryDir = '',
    [string]$Python = 'python',
    [string]$MSBuild = ''
)
$ErrorActionPreference = 'Stop'
if (!$D3DXIncludeDir) {
    $D3DXIncludeDir = if ($env:DXSDK_DIR) { Join-Path $env:DXSDK_DIR 'Include' } else { "$PSScriptRoot\..\Libraries\DirectX\DX9\Include" }
}
if (!$D3DXLibraryDir) {
    $D3DXLibraryDir = if ($env:DXSDK_DIR) { Join-Path $env:DXSDK_DIR 'Lib\x64' } else { "$PSScriptRoot\..\Libraries\DirectX\DX9\Lib\x64" }
}
# Forward slashes also work in MSBuild on Windows and avoid a trailing backslash
# escaping the closing native-command quote when a directory contains spaces.
$DependenciesRoot = [IO.Path]::GetFullPath($DependenciesRoot).Replace('\', '/').TrimEnd('/') + '/'
$D3DXIncludeDir = [IO.Path]::GetFullPath($D3DXIncludeDir).Replace('\', '/').TrimEnd('/') + '/'
$D3DXLibraryDir = [IO.Path]::GetFullPath($D3DXLibraryDir).Replace('\', '/').TrimEnd('/') + '/'

& $Python "$PSScriptRoot\check_client_x64.py" --configuration $Configuration `
    --dependencies $DependenciesRoot --dx9-include $D3DXIncludeDir --dx9-lib $D3DXLibraryDir
if ($LASTEXITCODE -ne 0) { throw 'Resolve the dependency/preflight errors before compiling.' }

if (!$MSBuild) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 C++ tools, or pass -MSBuild.' }
    $MSBuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild `
        -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
}
if (!$MSBuild -or !(Test-Path $MSBuild)) { throw 'MSBuild.exe was not found.' }
New-Item -ItemType Directory -Force "$PSScriptRoot\out" | Out-Null
& $MSBuild "$PSScriptRoot\..\ClientDX9.sln" /m /t:Build `
    "/p:Configuration=$Configuration" /p:Platform=x64 `
    "/p:ClientDependenciesRoot=$DependenciesRoot" `
    "/p:ClientD3DXIncludeDir=$D3DXIncludeDir" "/p:ClientD3DXLibraryDir=$D3DXLibraryDir" `
    "/bl:$PSScriptRoot\out\ClientDX9-$Configuration.binlog"
if ($LASTEXITCODE -ne 0) { throw 'Client build failed; inspect the MSBuild binary log.' }

$suffix = if ($Configuration -eq 'Debug') { '_D' } else { '' }
$engine = if ($Configuration -eq 'Debug') { 'MindPower3D_D9D' } else { 'MindPower3D_D9R' }
$bin = "$PSScriptRoot\out\bin\x64\$Configuration"
& $Python "$PSScriptRoot\check_client_x64.py" --projects-only `
    --binary "$bin\Game$suffix.exe" --binary "$bin\$engine.dll" --binary "$bin\CaLua.dll"
if ($LASTEXITCODE -ne 0) { throw 'Output architecture validation failed.' }
Write-Host "Built AMD64 client and engine in $bin. Runtime/gameplay validation is still required."
