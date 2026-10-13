param([string]$Zig = 'zig')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $build 'zig-global'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $build 'zig-local'
$options = @('-target', 'x86_64-windows-gnu', '-O2', '-Wall', '-Wextra', '-Werror')
& $Zig cc @options "$PSScriptRoot\test_native.c" -o "$build\test-native.exe" -lkernel32
if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
& "$build\test-native.exe"
if ($LASTEXITCODE -ne 0) { throw 'Native tests failed' }
& $Zig cc @options -shared "$PSScriptRoot\native.c" -o "$root\BabyProofDynamites\Scripts\BabyProofDynamitesNative.dll" -lkernel32
if ($LASTEXITCODE -ne 0) { throw 'DLL compilation failed' }
