#!/usr/bin/env python3
"""Check the x64 client project graph and the architecture of staged binaries."""
import argparse
import json
from pathlib import Path
import re
import struct
import sys
import xml.etree.ElementTree as ET

NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
AMD64 = 0x8664
I386 = 0x14c


def binary_machines(data):
    """Read COFF, PE and import/static archives; do not trust directory names."""
    if data.startswith(b'MZ'):
        if len(data) < 64:
            raise ValueError('Truncated DOS header')
        offset = struct.unpack_from('<I', data, 0x3c)[0]
        if offset < 64 or offset + 24 > len(data) or data[offset:offset+4] != b'PE\0\0':
            raise ValueError('Invalid PE header')
        return {struct.unpack_from('<H', data, offset+4)[0]}
    if data.startswith(b'!<arch>\n'):
        position = 8
        machines = set()
        while position < len(data):
            header = data[position:position+60]
            if len(header) != 60 or header[58:60] != b'`\n':
                raise ValueError('Invalid archive member')
            length = int(header[48:58].strip())
            if length < 0:
                raise ValueError('Negative archive member size')
            name = header[:16].strip()
            begin = position + 60
            member = data[begin:begin+length]
            if len(member) != length:
                raise ValueError('Truncated archive member')
            if name not in (b'/', b'//', b'/SYM64/'):
                if member.startswith(b'!<arch>\n'):
                    raise ValueError('Nested archives are not linker object members')
                machines.update(binary_machines(member))
            position = begin + length + (length & 1)
        return machines
    if data.startswith(b'\0\0\xff\xff') and len(data) >= 20:
        # Short import header and anonymous/bigobj COFF both store Machine here.
        return {struct.unpack_from('<H', data, 6)[0]}
    if len(data) >= 20 and struct.unpack_from('<H', data)[0] in (AMD64, I386):
        return {struct.unpack_from('<H', data)[0]}
    raise ValueError('Unknown object format (inspect LTCG/bitcode libraries with dumpbin)')


def resolve_windows_path(parent, name):
    path = parent
    for part in name.replace('\\', '/').split('/'):
        if part in ('', '.'):
            continue
        if part == '..':
            path = path.parent
        else:
            candidate = path / part
            if not candidate.exists() and path.is_dir():
                candidate = next((p for p in path.iterdir() if p.name.casefold() == part.casefold()), candidate)
            path = candidate
    return path


def check_projects(root):
    errors = []
    solution = (root/'ClientDX9.sln').read_text()
    projects = re.findall(r'Project\("\{8BC9[^\"]+"\) = "([^\"]+)", "([^\"]+)", "([^\"]+)"', solution)
    if len(projects) != 13:
        errors.append(f'Expected 13 projects, found {len(projects)}')
    source_count = 0
    for name, relative, guid in projects:
        project = resolve_windows_path(root, relative)
        if not project.exists():
            errors.append(f'Missing project: {project}')
            continue
        tree = ET.parse(project)
        configs = {e.attrib['Include'] for e in tree.findall('.//m:ProjectConfiguration', NS)}
        for flavor in ('Debug', 'Release'):
            match = re.search(re.escape(guid+'.'+flavor+'|x64.ActiveCfg = ') + r'([^\r\n]+)', solution)
            if not match or match[1] not in configs or not match[1].endswith('|x64'):
                errors.append(f'{name}: missing real {flavor} x64 mapping')
            build = re.search(re.escape(guid+'.'+flavor+'|x64.Build.0 = ') + r'([^\r\n]+)', solution)
            if not build or not match or build[1] != match[1]:
                errors.append(f'{name}: {flavor} is not enabled in the x64 build')
        imports = [e.get('Project', '') for e in tree.findall('m:Import', NS)]
        if not any('ClientX64.props' in value for value in imports):
            errors.append(f'{name}: missing isolated x64 settings')
        for item in tree.findall('.//m:ClCompile[@Include]', NS):
            filename = item.attrib['Include']
            if '$(' not in filename:
                source_count += 1
                if not resolve_windows_path(project.parent, filename).exists():
                    errors.append(f'{name}: missing source {filename}')
        if name in ('Game', 'MindPower3D'):
            children = list(tree.getroot())
            names = next((i for i, e in enumerate(children) if 'ClientDx9Names.props' in e.get('Project', '')), len(children))
            definitions = next((i for i, e in enumerate(children) if e.tag.endswith('ItemDefinitionGroup')), len(children))
            if names >= definitions:
                errors.append(f'{name}: DX9 names must be defined before link metadata expands properties')
            if not any('ClientDx9.props' in value for value in imports):
                errors.append(f'{name}: missing DX9 settings')
            text = project.read_text()
            if 'd3dx8.lib' in text or 'd3d8.dll' in text:
                errors.append(f'{name}: explicit DX8 dependency')
            if not tree.findall('.//m:PrecompiledHeader[@Condition]', NS):
                errors.append(f'{name}: missing PCH creation settings')
    return dict(projects=len(projects), translation_units=source_count, errors=errors)


def check_dependencies(root, dependencies, dx9_include, dx9_lib, flavor, ffi_library):
    names = ['SDL.lib', 'SDL_mixer.lib', 'SDLmain.lib', 'discord-rpc.lib', 'icudt.lib', ffi_library]
    names += ['icuucd.lib', 'icuind.lib'] if flavor == 'Debug' else ['icuuc.lib', 'icuin.lib']
    required = [dependencies/'lib'/flavor/name for name in names]
    required += [dx9_lib/'d3dx9.lib']
    errors, inspected = [], {}
    for path in required:
        if not path.exists():
            errors.append(f'Missing x64 library: {path}')
            continue
        try:
            machines = binary_machines(path.read_bytes())
            inspected[str(path)] = [hex(n) for n in sorted(machines)]
            if machines != {AMD64}:
                errors.append(f'Expected AMD64 only: {path}: {sorted(machines)}')
        except (ValueError, struct.error) as error:
            errors.append(f'{path}: {error}')
    for path in (dx9_include/'d3dx9.h', dependencies/'include/ffi.h', dependencies/'include/ffitarget.h'):
        if not path.exists():
            errors.append(f'Missing header: {path}')
    return dict(libraries=inspected, errors=errors)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    root = Path(__file__).resolve().parents[1]
    parser.add_argument('--projects-only', action='store_true')
    parser.add_argument('--dependencies', type=Path, default=root/'Dependencies/x64')
    parser.add_argument('--dx9-include', type=Path, default=root/'Libraries/DirectX/DX9/Include')
    parser.add_argument('--dx9-lib', type=Path, default=root/'Libraries/DirectX/DX9/Lib/x64')
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Release')
    parser.add_argument('--ffi-library', default='libffi.lib')
    parser.add_argument('--json', type=Path)
    parser.add_argument('--binary', type=Path, action='append', default=[])
    args = parser.parse_args()
    result = dict(projects=check_projects(root))
    if not args.projects_only:
        result['dependencies'] = check_dependencies(root, args.dependencies, args.dx9_include, args.dx9_lib, args.configuration, args.ffi_library)
    if args.binary:
        errors = []
        for path in args.binary:
            try:
                if binary_machines(path.read_bytes()) != {AMD64}:
                    errors.append(f'Expected AMD64 only: {path}')
            except (OSError, ValueError, struct.error) as error:
                errors.append(f'{path}: {error}')
        result['binaries'] = dict(errors=errors)
    encoded = json.dumps(result, indent=2)
    print(encoded)
    if args.json:
        args.json.write_text(encoded+'\n')
    return int(any(value['errors'] for value in result.values()))


if __name__ == '__main__':
    sys.exit(main())
