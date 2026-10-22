#!/usr/bin/env python3
"""Exercise Win64 native calls on Linux and the architecture-independent helpers.

The FFI test uses GCC's ms_abi functions and libffi's FFI_WIN64 ABI. It does not
replace a Windows/MSVC build or a Direct3D runtime test.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def run(argv, **kwargs):
    result = subprocess.run(list(map(str, argv)), capture_output=True, text=True, **kwargs)
    if result.returncode:
        raise RuntimeError(f"Command failed ({result.returncode}): {' '.join(map(str, argv))}\n{result.stdout}\n{result.stderr}")
    if result.stdout:
        print(result.stdout.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ffi-include', type=Path, required=True, help='Directory containing ffi.h and ffitarget.h')
    parser.add_argument('--ffi-library', type=Path, required=True, help='Native Linux libffi shared library')
    parser.add_argument('--leak-check', action=argparse.BooleanOptionalAction, default=True, help='Enable LeakSanitizer for the CaLua lifecycle test (default: enabled)')
    parser.add_argument('--texture-records', type=Path, help='208-byte records extracted by audit_model_assets.py')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    tests = root / 'Build/tests'
    calua = root / 'Libraries/CaLua'
    with tempfile.TemporaryDirectory(prefix='mindpower-x64-') as directory:
        work = Path(directory)
        (work / 'crtdbg.h').write_text('/* MSVC CRT compatibility for the native Linux test only. */\n')
        (work / 'compat.h').write_text('''#include <malloc.h>
#include <strings.h>
#define __cdecl
#define _msize malloc_usable_size
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strdup strdup
#define _CrtCheckMemory() 1
''')
        run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'Libraries/Util/include'), tests/'byte_transforms.cpp', '-o', work/'byte_test'])
        run([work/'byte_test'])
        print('Exhaustive byte rotation/XOR tests passed')
        run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'Engine/include'), tests/'texture_disk.cpp', '-o', work/'texture_test'])
        run([work/'texture_test', *([args.texture_records] if args.texture_records else [])])
        print('Legacy 208-byte texture records roundtrip with native 64-bit pointers passed')
        run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'Engine/include'), tests/'binary_io.cpp', '-o', work/'binary_test'])
        run([work/'binary_test'])
        print('Bounded array reads and sparse file offsets above 2 GiB passed')
        run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror', '-I'+str(root/'Engine/include'), tests/'effect_pass.cpp', '-o', work/'effect_test'])
        run([work/'effect_test'])
        print('DX9 effect pass lifecycle and failure recovery passed')
        run(['g++', '-std=c++20', '-D_MAX_PATH=260', '-I'+str(root/'Libraries/ICUHelper/inc'),
             root/'Libraries/ICUHelper/src/pi_Alloc.cpp', tests/'allocator.cpp',
             '-fsanitize=address,undefined', '-g', '-o', work/'allocator_test'])
        run([work/'allocator_test'], cwd=work, env=dict(os.environ, ASAN_OPTIONS='detect_leaks=1', UBSAN_OPTIONS='halt_on_error=1'))
        print('64-bit allocation bookkeeping, overflow and constructor-failure cleanup passed')
        sources = sorted((calua/'Lua').glob('*.c')) + sorted((calua/'Lua/lib').glob('*.c')) + sorted((calua/'CaLua').glob('*.c'))
        run(['gcc', '-std=gnu11', '-g', '-O1', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
             '-DCLU_USE_LIBFFI', '-DCLU_FFI_ABI=FFI_WIN64', '-DCLU_TEST_MS_ABI', '-fsanitize=float-cast-overflow',
             '-include', work/'compat.h', '-I'+str(work), '-I'+str(args.ffi_include),
             '-I'+str(calua/'include'), '-I'+str(calua/'Lua'), '-I'+str(calua/'CaLua'),
             *sources, tests/'calua_abi.c', args.ffi_library, '-lm', '-o', work/'abi_test'])
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks='+str(int(args.leak_check)), UBSAN_OPTIONS='halt_on_error=1')
        run([work/'abi_test'], cwd=work, env=environment)
        print(f'Compiled and exercised {len(sources)} Lua/CaLua translation units with ASan/UBSan (leak checking {"enabled" if args.leak_check else "disabled"})')


if __name__ == '__main__':
    main()
