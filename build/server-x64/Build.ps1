[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [ValidateSet('All','AccountServer','GateServer','GroupServer','GameServer')][string]$Target = 'All',
    [string]$DependencyRoot = '',
    [switch]$BuildLua
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (!$DependencyRoot) { $DependencyRoot = Join-Path $repo 'dependencies/server-x64' }
$DependencyRoot = [IO.Path]::GetFullPath($DependencyRoot).TrimEnd('\', '/') + '\'
foreach ($tool in @('cl.exe','lib.exe','dumpbin.exe','msbuild.exe')) {
    if (!(Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool missing. Run from the VS 2022 x64 Native Tools command prompt."
    }
}
if ($env:VSCMD_ARG_TGT_ARCH -ne 'x64') { throw 'The compiler environment must target x64.' }
# Block an unreviewed ABI conversion instead of silently disabling billing.
if ($Target -in @('All','AccountServer')) {
    throw 'AccountServer x64 is blocked: integrate an x64 BTI SDK with a pointer-sized window-handle ABI, or an out-of-process x86 bridge. The supplied BTI header takes LONG. See README.md.'
}
$libDir = Join-Path $DependencyRoot "$Configuration/lib"
New-Item -ItemType Directory -Force $libDir | Out-Null
if ($BuildLua) {
    $luaWork = Join-Path $repo "sources/out/server/dependencies/luajit/x64/$Configuration"
    New-Item -ItemType Directory -Force $luaWork | Out-Null
    foreach ($folder in @('src','dynasm')) {
        Copy-Item (Join-Path $repo "sources/Libraries/LuaJIT/$folder") $luaWork -Recurse -Force
    }
    $luaSrc = Join-Path $luaWork 'src'
    # The bundled static builder defaults to /MT in both modes. Explicitly set
    # the debug CRT through CL for /MTd projects; build generated VM code in x64.
    # The bundled script prints an error but does not reliably return failure.
    # Correct only the isolated build copy; never accept a stale bundled .lib.
    $builder = Join-Path $luaSrc 'msvcbuild.bat'
    $batch = [IO.File]::ReadAllText($builder)
    $batch = [regex]::Replace($batch, '(?m)^:(BAD|FAIL)\r?$', ":`$1`r`n@exit /b 1")
    [IO.File]::WriteAllText($builder, $batch, [Text.Encoding]::ASCII)
    Remove-Item (Join-Path $luaSrc 'lua51.lib') -ErrorAction SilentlyContinue
    $savedCL = $env:CL
    try {
        if ($Configuration -eq 'Debug') { $env:CL = "$savedCL /MTd" }
        else { $env:CL = "$savedCL /MT" }
        Push-Location $luaSrc
        try {
            $luaArguments = if ($Configuration -eq 'Debug') { 'debug static' } else { 'static' }
            & cmd.exe /d /c "msvcbuild.bat $luaArguments"
            if ($LASTEXITCODE -ne 0) { throw 'LuaJIT build failed.' }
        } finally { Pop-Location }
        Copy-Item (Join-Path $luaSrc 'lua51.lib') $libDir -Force
    } finally { $env:CL = $savedCL }
}
$icu = if ($Configuration -eq 'Debug') { @('icudt.lib','icuucd.lib','icuind.lib') } else { @('icudt.lib','icuuc.lib','icuin.lib') }
$required = @($icu)
if ($Target -in @('GameServer','GroupServer')) { $required += 'lua51.lib' }
foreach ($name in $required) {
    $path = Join-Path $libDir $name
    if (!(Test-Path -LiteralPath $path)) { throw "Missing x64 dependency: $path. Supply matching ICU 3.8 libraries; use -BuildLua for LuaJIT." }
    $headers = & dumpbin.exe /nologo /headers $path 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -or $headers -notmatch '(?im)^\s*8664 machine' -or $headers -match '(?im)^\s*(14C|1C0|AA64) machine') {
        throw "Dependency is not exclusively AMD64: $path"
    }
}
$output = Join-Path $repo "sources/out/server/x64/$Configuration"
New-Item -ItemType Directory -Force $output | Out-Null
$targetArg = "/t:$Target"
$binlog = Join-Path $output "$Target.binlog"
& msbuild.exe (Join-Path $repo 'sources/server-x64.sln') $targetArg /m "/p:Configuration=$Configuration" /p:Platform=x64 "/p:ServerDependencyRoot=$DependencyRoot" "/bl:$binlog" /nologo
if ($LASTEXITCODE -ne 0) { throw "Server build failed. Inspect $binlog" }
$exe = Join-Path $output "$Target$(if ($Configuration -eq 'Debug') { '_D' }).exe"
$headers = & dumpbin.exe /nologo /headers $exe 2>&1 | Out-String
if ($LASTEXITCODE -ne 0 -or $headers -notmatch '(?im)^\s*8664 machine') { throw "Output is not AMD64: $exe" }
Write-Host "Verified AMD64 output: $exe"
