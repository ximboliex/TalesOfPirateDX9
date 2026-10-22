param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$DependenciesRoot = "$PSScriptRoot\..\Dependencies\x64",
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
    throw 'Run from an x64 Native Tools/Developer PowerShell for Visual Studio 2022.'
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$root = [IO.Path]::GetFullPath("$PSScriptRoot\..")
$DependenciesRoot = [IO.Path]::GetFullPath($DependenciesRoot)
$work = "$PSScriptRoot\out\tests\x64\$Configuration"
New-Item -ItemType Directory -Force $work | Out-Null
$runtime = if ($Configuration -eq 'Debug') { '/MTd' } else { '/MT' }
$common = @('/nologo', '/Zi', '/Od', $runtime, '/D_CRT_SECURE_NO_WARNINGS')

function Invoke-CompileAndTest([string]$Name, [string[]]$Arguments) {
    & $compiler @common "/Fe:$Name.exe" @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $Name" }
    & ".\$Name.exe"
    if ($LASTEXITCODE -ne 0) { throw "Test failed: $Name" }
}

Push-Location $work
try {
    & $Python "$PSScriptRoot\check_client_x64.py" --projects-only
    if ($LASTEXITCODE -ne 0) { throw 'Project graph validation failed.' }
    & $Python -m unittest discover -s "$PSScriptRoot\tests" -p 'test_*.py'
    if ($LASTEXITCODE -ne 0) { throw 'Python regression tests failed.' }
    Invoke-CompileAndTest 'byte_transforms' @('/std:c++20', "/I$root\Libraries\Util\include", "$PSScriptRoot\tests\byte_transforms.cpp")
    Invoke-CompileAndTest 'texture_disk' @('/std:c++20', "/I$root\Engine\include", "$PSScriptRoot\tests\texture_disk.cpp")
    Invoke-CompileAndTest 'binary_io' @('/std:c++20', '/EHsc', "/I$root\Engine\include", "$PSScriptRoot\tests\binary_io.cpp")
    Invoke-CompileAndTest 'effect_pass' @('/std:c++20', '/EHsc', "/I$root\Engine\include", "$PSScriptRoot\tests\effect_pass.cpp")
    $assetIncludes = @("$root\Engine\include", "$root\Engine\src", "$root\Libraries\DirectX\DX9\Include",
        "$root\Libraries\Util\include", "$root\Libraries\common\include", "$root\Libraries\Server\sdk\include") |
        ForEach-Object { "/I$_" }
    # lwDirectX.h emits pragma-link directives; syntax-only compilation is
    # intentional here so layout validation does not require D3DX binaries.
    & $compiler @common /std:c++20 /EHsc /Zs @assetIncludes "$PSScriptRoot\tests\asset_layout_win64.cpp"
    if ($LASTEXITCODE -ne 0) { throw 'Real engine asset header/layout validation failed.' }
    Invoke-CompileAndTest 'allocator' @('/std:c++20', '/EHsc', "/I$root\Libraries\ICUHelper\inc", "$root\Libraries\ICUHelper\src\pi_Alloc.cpp", "$PSScriptRoot\tests\allocator.cpp")
    Invoke-CompileAndTest 'stacktrace' @('/std:c++20', '/EHa', "/I$root\Libraries\Util\include", "$root\Libraries\Util\src\Stacktrace.cpp", "$root\Libraries\Util\src\StacktraceWin64.cpp", "$PSScriptRoot\tests\stacktrace_win64.cpp", '/link', 'dbghelp.lib')

    $calua = "$root\Libraries\CaLua"
    $sources = @((Get-ChildItem "$calua\Lua\*.c").FullName) +
        @((Get-ChildItem "$calua\Lua\lib\*.c").FullName) +
        @((Get-ChildItem "$calua\CaLua\*.c").FullName)
    $arguments = @('/TC', '/DFFI_BUILDING', '/DFFI_STATIC_BUILD', '/DWIN32',
        "/I$DependenciesRoot\include", "/I$calua\include", "/I$calua\Lua", "/I$calua\CaLua")
    if ($Configuration -eq 'Debug') { $arguments += '/D_DEBUG' }
    $arguments += $sources
    $arguments += @("$PSScriptRoot\tests\calua_abi.c", '/link', "$DependenciesRoot\lib\$Configuration\libffi.lib")
    Invoke-CompileAndTest 'calua_abi' $arguments
    Write-Host 'Windows AMD64 compiler/ABI regression tests passed. Direct3D/gameplay still require runtime validation.'
}
finally { Pop-Location }
