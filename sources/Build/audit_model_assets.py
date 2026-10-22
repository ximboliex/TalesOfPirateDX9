#!/usr/bin/env python3
"""Read existing LGO/LMO containers without loading a renderer or changing assets.

Validate geometry/material block boundaries and extract legacy 208-byte texture
records for the native-pointer conversion regression test. Mesh/animation
payloads are deliberately not interpreted by this format-boundary audit.
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import struct

VERSIONS = {0, *range(0x1000, 0x1006)}
GEOMETRY_HEADER = 116
TEXTURE_RECORD = 208
MATERIAL_PREFIX = 8 + 68 + 96
MATERIAL_RECORD = MATERIAL_PREFIX + 4 * TEXTURE_RECORD


def u32(data, offset):
    if offset < 0 or offset + 4 > len(data):
        raise ValueError(f'Truncated DWORD at {offset}')
    return struct.unpack_from('<I', data, offset)[0]


def require_range(data, begin, length, end=None):
    end = len(data) if end is None else min(end, len(data))
    if begin < 0 or length < 0 or begin > end or length > end - begin:
        raise ValueError(f'Block outside file/container: offset={begin}, size={length}, end={end}')


def geometry(data, begin, end, version, records):
    require_range(data, begin, GEOMETRY_HEADER, end)
    sizes = struct.unpack_from('<4I', data, begin + 100)
    require_range(data, begin, GEOMETRY_HEADER + sum(sizes), end)
    material_size = sizes[0]
    if not material_size:
        return 1
    material = begin + GEOMETRY_HEADER
    material_version = version
    if version == 0:
        material_version = u32(data, material)
        material += 4
    count = u32(data, material)
    material += 4
    if material_version >= 0x1000 or material_version == 2:
        record_size, prefix = MATERIAL_RECORD, MATERIAL_PREFIX
    elif material_version == 1:
        record_size, prefix = 8 + 68 + 128 + 4 * 240, None
    elif material_version == 0:
        record_size, prefix = 68 + 128 + 4 * 208, None
    else:
        raise ValueError(f'Unknown material version: {material_version:#x}')
    if material + count * record_size != begin + GEOMETRY_HEADER + material_size:
        raise ValueError('Material block size disagrees with the Win32 disk layout')
    if prefix is not None:
        for index in range(count):
            start = material + index * record_size + prefix
            records.extend(data[start:start + 4 * TEXTURE_RECORD])
    return 1


def audit_file(data, extension):
    version = u32(data, 0)
    if version not in VERSIONS:
        raise ValueError(f'Unknown model version: {version:#x}')
    records = bytearray()
    geometries = 0
    if extension.lower() == '.lgo':
        geometries = geometry(data, 4, len(data), version, records)
    else:
        count = u32(data, 4)
        require_range(data, 8, count * 12)
        for index in range(count):
            kind, address, length = struct.unpack_from('<3I', data, 8 + index * 12)
            require_range(data, address, length)
            if address < 8 + count * 12:
                raise ValueError('Object overlaps the container header')
            if kind == 1:
                begin = address + (4 if version == 0 else 0)
                geometries += geometry(data, begin, address + length, version, records)
            elif kind != 2:
                raise ValueError(f'Unknown model object kind: {kind}')
    return version, geometries, records


def audit_tree(root, record_file=None):
    counts, versions, issues = Counter(), Counter(), []
    stream = record_file.open('wb') if record_file else None
    try:
        for path in sorted(root.rglob('*')):
            if path.suffix.lower() not in ('.lgo', '.lmo') or not path.is_file():
                continue
            counts['files'] += 1
            try:
                version, geometries, records = audit_file(path.read_bytes(), path.suffix)
                versions[f'{version:#x}'] += 1
                counts['valid_boundaries'] += 1
                counts['geometries'] += geometries
                counts['texture_records'] += len(records) // TEXTURE_RECORD
                if stream:
                    stream.write(records)
            except (ValueError, OSError, struct.error) as error:
                issues.append(dict(file=str(path), reason=str(error)))
    finally:
        if stream:
            stream.close()
    return dict(counts=dict(counts), versions=dict(versions), issues=issues,
                scope='Geometry/material boundaries only; no mesh, animation or rendering validation')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('--texture-records', type=Path)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    report = audit_tree(args.root, args.texture_records)
    text = json.dumps(report, indent=2)
    if args.json:
        args.json.write_text(text + '\n')
    print(json.dumps(report['counts'], indent=2))
    print(f"Files needing review: {len(report['issues'])}")
    return int(bool(report['issues']))


if __name__ == '__main__':
    raise SystemExit(main())
