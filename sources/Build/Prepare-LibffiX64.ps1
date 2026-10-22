param(
    [string]$DependenciesRoot = "$PSScriptRoot\..\Dependencies\x64",
    [string]$CacheRoot = "$PSScriptRoot\out\vcpkg",
    [string]$Python = 'python'
)
$ErrorActionPreference = 'Stop'
# Pin both the port recipe and its upstream source hash instead of following HEAD.
$revision = '67972e92edd181d433927b42d8ab4bfa52ac164a'
$CacheRoot = [IO.Path]::GetFullPath($CacheRoot)
$DependenciesRoot = [IO.Path]::GetFullPath($DependenciesRoot)
if (!(Test-Path "$CacheRoot\.git")) {
    New-Item -ItemType Directory -Force $CacheRoot | Out-Null
    & git -C $CacheRoot init
    if ($LASTEXITCODE -ne 0) { throw 'Cannot initialize the isolated vcpkg checkout.' }
    & git -C $CacheRoot remote add origin https://github.com/microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) { throw 'Cannot configure vcpkg origin.' }
}
& git -C $CacheRoot fetch --depth 1 origin $revision
if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch the pinned vcpkg revision.' }
& git -C $CacheRoot checkout --detach $revision
if ($LASTEXITCODE -ne 0) { throw 'Cannot select the pinned vcpkg revision.' }
& "$CacheRoot\bootstrap-vcpkg.bat" -disableMetrics
if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed.' }
& "$CacheRoot\vcpkg.exe" install libffi:x64-windows-static --disable-metrics
if ($LASTEXITCODE -ne 0) { throw 'Building libffi for the Windows x64 static CRT failed.' }

$installed = "$CacheRoot\installed\x64-windows-static"
New-Item -ItemType Directory -Force "$DependenciesRoot\include" | Out-Null
Copy-Item "$installed\include\ffi.h", "$installed\include\ffitarget.h" "$DependenciesRoot\include"
foreach ($configuration in @('Release', 'Debug')) {
    $source = if ($configuration -eq 'Debug') { "$installed\debug\lib" } else { "$installed\lib" }
    $libraries = @(Get-ChildItem $source -Filter '*ffi*.lib')
    if ($libraries.Count -ne 1) { throw "Expected one libffi static library in $source." }
    New-Item -ItemType Directory -Force "$DependenciesRoot\lib\$configuration" | Out-Null
    $library = "$DependenciesRoot\lib\$configuration\libffi.lib"
    Copy-Item $libraries[0].FullName $library
    & $Python "$PSScriptRoot\check_client_x64.py" --projects-only --binary $library
    if ($LASTEXITCODE -ne 0) { throw "Staged libffi is not AMD64: $library" }
}
Copy-Item "$installed\share\libffi\copyright" "$DependenciesRoot\libffi-COPYRIGHT.txt"
Write-Host "Prepared libffi headers and Debug/Release AMD64 static libraries in $DependenciesRoot"
