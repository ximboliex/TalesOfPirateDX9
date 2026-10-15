#!/usr/bin/env python3
"""64-bit host regressions using production code; not a Windows server build."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}

def read(path):
    return (ROOT / path).read_bytes().decode('latin1')

def function(text, name):
    match = re.search(r'(?m)^[^\n{}]*\b' + re.escape(name) + r'\([^;{}]*\)\s*\{', text)
    assert match, name
    start, depth, end = match.start(), 1, match.end()
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

def check_projects():
    solution = read('sources/server-x64.sln')
    entries = re.findall(r'Project\("[^\"]+"\) = "([^\"]+)", "([^\"]+)", "([^\"]+)"', solution)
    assert len(entries) == 12, len(entries)
    for name, rel, guid in entries:
        project = ET.parse(ROOT / 'sources' / rel.replace('\\', '/'))
        configs = {e.attrib['Include'] for e in project.findall('.//m:ProjectConfiguration', NS)}
        for mode in ('Debug', 'Release'):
            config = ('Long_Struct_' if name == 'Common' else '') + mode + '|x64'
            assert config in configs, (name, config)
            assert f'{guid}.{mode}|x64.ActiveCfg = {config}' in solution
            assert f'{guid}.{mode}|x64.Build.0 = {config}' in solution
        assert any('ServerX64.props' in e.attrib.get('Project', '') for e in project.findall('m:Import', NS))
        if name != 'cryptlib':
            for item in project.findall('m:ItemGroup/m:ClCompile', NS):
                for metadata in item:
                    condition = metadata.attrib.get('Condition', '')
                    if re.search(r"=='(?:Debug|Release)\|Win32'", condition):
                        expected = condition.replace('|Win32', '|x64')
                        assert any(e.tag == metadata.tag and e.text == metadata.text and
                                   e.attrib.get('Condition') == expected for e in item), (name, item.attrib)
        for e in project.findall('m:ItemDefinitionGroup', NS):
            if '|x64' in e.attrib.get('Condition', ''):
                assert not e.findall('.//m:TargetMachine[.="MachineX86"]', NS)
    props = ET.parse(ROOT / 'build/server-x64/ServerX64.props')
    paths = props.findall('.//m:AdditionalLibraryDirectories', NS)
    assert len(paths) == 2 and all('Win32' not in (p.text or '') for p in paths)
    print('PASS: 12 projects, 24 genuine x64 mappings, isolated link directories')

def check_stacktrace_preprocessor():
    # Preprocess the real UTF-16 source after decoding. Empty SDK headers are
    # enough to validate branch selection; this is not a Windows compile.
    with tempfile.TemporaryDirectory(prefix='stacktrace-x64-') as temp:
        directory = Path(temp)
        for header in ('tchar.h', 'windows.h', 'dbghelp.h', 'eh.h'):
            (directory / header).write_text('')
        original = ROOT / 'sources/Libraries/Util/src/Stacktrace.cpp'
        decoded = directory / 'Stacktrace.cpp'
        decoded.write_text(original.read_text(encoding='utf-16'))
        output = subprocess.check_output([
            os.environ.get('CXX', 'g++'), '-E', '-x', 'c++', '-D_WIN64', '-D_M_X64',
            '-D_MSC_VER=1930', '-I', str(directory),
            '-I', str(ROOT / 'sources/Libraries/Util/include'),
            '-I', str(original.parent), str(decoded)], text=True)
        assert 'StackWalk64(IMAGE_FILE_MACHINE_AMD64' in output
        assert 'class sym_engine' not in output
    print('PASS: x64 stack trace preprocessor excludes the x86 symbol engine')

check_projects()
check_stacktrace_preprocessor()
algo = read('sources/Libraries/Util/src/algo.cpp')
algo = algo[algo.index('// Portable byte operations:'):]
packet = read('sources/Libraries/Server/sdk/src/Packet.cpp')
source = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>
#include "WireHandleRegistry.h"
#define INL inline
using LONG64 = std::int64_t;
'''
source += '\n'.join(function(algo, f) for f in ('rol_byte_off', 'ror_byte_off', 'xor_byte_off', 'encrypt_B'))
source += '\n' + function(packet, 'join_uint32_to_uint64') + '\n' + function(packet, 'split_uint64_to_uint32')
source += r'''
namespace dbc {
struct WPacket {
    std::vector<std::uint32_t> words;
    bool WriteLong(std::uint32_t n) { words.push_back(n); return true; }
    bool WriteLongLong(LONG64 ll);
};
struct RPacket {
    std::vector<std::uint32_t> words;
    size_t front = 0, back = 0;
    std::uint32_t ReadLong() { return words.at(front++); }
    std::uint32_t ReverseReadLong() { return words.at(words.size() - ++back); }
    LONG64 ReadLongLong();
    LONG64 ReverseReadLongLong();
};
}
using dbc::WPacket;
using dbc::RPacket;
'''
source += '\n'.join(function(packet, f) for f in ('WPacket::WriteLongLong', 'RPacket::ReadLongLong', 'dbc::RPacket::ReverseReadLongLong'))
source += r'''
int main() {
    static_assert(sizeof(void*) == 8, "Run with a 64-bit compiler");
    dbc::WireHandleRegistry registry;
    int object = 42;
    assert(registry.Insert<int>(nullptr) == 0);
    assert(registry.Resolve<int>(0) == nullptr);
    const auto first = registry.Insert(&object);
    assert(reinterpret_cast<std::uintptr_t>(&object) > UINT32_MAX);
    assert(first > 0 && first <= INT32_MAX);
    assert(registry.Insert(&object) == first);
    assert(registry.Resolve<int>(first) == &object);
    assert(registry.Resolve<double>(first) == nullptr);
    bool typeRejected = false;
    try { registry.Insert(reinterpret_cast<double*>(&object)); }
    catch (const std::logic_error&) { typeRejected = true; }
    assert(typeRejected);
    registry.Erase(&object);
    registry.Erase(&object);
    assert(registry.Resolve<int>(first) == nullptr);
    auto previous = first;
    for (int i = 0; i < 10000; ++i) {
        const auto current = registry.Insert(&object);
        assert(current > previous);
        assert(registry.Resolve<int>(previous) == nullptr);
        assert(registry.Resolve<int>(current) == &object);
        registry.Erase(&object);
        previous = current;
    }
    const auto shared = registry.Insert(&object);
    std::array<std::thread, 8> threads;
    for (auto& thread : threads) thread = std::thread([&] {
        int local = 0;
        for (int n = 0; n < 1000; ++n) {
            assert(registry.Insert(&object) == shared);
            assert(registry.Resolve<int>(shared) == &object);
            const auto own = registry.Insert(&local);
            assert(registry.Resolve<int>(own) == &local);
            registry.Erase(&local);
            assert(registry.Resolve<int>(own) == nullptr);
        }
    });
    for (auto& thread : threads) thread.join();
    registry.Erase(&object);
    std::cout << "PASS: high addresses, null/type checks, 10000 pool reuses, 8 concurrent threads\n";
    for (unsigned value = 0; value < 256; ++value) {
        for (unsigned bits = 0; bits < 256; ++bits) {
            unsigned char bytes[] = {0xa5, static_cast<unsigned char>(value), 0x5a};
            const bool ok = rol_byte_off(bytes, 1, static_cast<unsigned char>(bits));
            assert(ok == (bits < 8 || bits >= 128));
            if (ok) {
                unsigned expected = value;
                for (unsigned n = 0; n < (bits & 7); ++n) expected = ((expected * 2) & 255) + (expected >= 128);
                assert(bytes[1] == expected);
                assert(ror_byte_off(bytes, 1, static_cast<unsigned char>(bits)));
                assert(bytes[1] == value);
            } else {
                assert(bytes[1] == value);
                assert(!ror_byte_off(bytes, 1, static_cast<unsigned char>(bits)));
            }
            assert(bytes[0] == 0xa5 && bytes[2] == 0x5a);
        }
        for (unsigned mask = 0; mask < 256; ++mask) {
            auto b = static_cast<unsigned char>(value);
            assert(xor_byte_off(&b, 0, static_cast<unsigned char>(mask)));
            assert(b == (value ^ mask));
        }
    }
    // Independent byte-by-byte reference for complete blocks and remainders.
    for (unsigned length = 0; length < 100; ++length) {
        std::vector<char> data(length), reference(length);
        char key[] = {'k', 'e', 'y', '1'};
        for (unsigned i = 0; i < length; ++i) data[i] = reference[i] = static_cast<char>(i * 13);
        for (unsigned i = 0; i < length; ++i) {
            unsigned b = static_cast<unsigned char>(reference[i]) ^ key[i % 4];
            for (int n = 0; n < key[i % 4] % 4 + 1; ++n) b = ((b * 2) & 255) + (b >= 128);
            reference[i] = static_cast<char>(b);
        }
        assert(encrypt_B(data.data(), length, key, 4, true));
        assert(data == reference);
        assert(encrypt_B(data.data(), length, key, 4, false));
        for (unsigned i = 0; i < length; ++i) assert(data[i] == static_cast<char>(i * 13));
    }
    std::cout << "PASS: 65536 rotation inputs, 65536 XOR inputs, 100 cipher lengths\n";
    for (std::uint64_t value : {UINT64_C(0), UINT64_C(1), UINT64_C(0x123456789abcdef0),
                               UINT64_C(0x100000001), UINT64_C(0x7fffffffffffffff)}) {
        WPacket writer;
        assert(writer.WriteLongLong(static_cast<LONG64>(value)));
        assert(writer.words[0] == (value >> 32));
        assert(writer.words[1] == static_cast<std::uint32_t>(value));
        RPacket forward{writer.words}, reverse{writer.words};
        assert(static_cast<std::uint64_t>(forward.ReadLongLong()) == value);
        assert(static_cast<std::uint64_t>(reverse.ReverseReadLongLong()) == value);
    }
    std::cout << "PASS: 64-bit packet word order and reverse decoding\n";
}
'''
with tempfile.TemporaryDirectory(prefix='server-x64-') as temp:
    path = Path(temp)
    cpp = path / 'regressions.cpp'
    cpp.write_text(source)
    executable = path / 'regressions'
    compiler = os.environ.get('CXX', 'g++')
    subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation', '-pthread',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie',
                    '-I', str(ROOT / 'sources/Libraries/Server/sdk/include'),
                    str(cpp), '-o', str(executable)], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1')
    subprocess.run([str(executable)], check=True, env=env)
