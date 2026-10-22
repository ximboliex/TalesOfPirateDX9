"""Ensure that the dependency gate distinguishes real and mislabeled x64 libs."""
import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('preflight', Path(__file__).resolve().parents[1]/'check_client_x64.py')
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)


def coff(machine):
    return struct.pack('<H', machine) + bytes(18)


def archive(*members):
    result = b'!<arch>\n'
    for index, member in enumerate(members):
        header = (f'{index}.obj/'.ljust(16) + '0'.ljust(12) + '0'.ljust(6) +
                  '0'.ljust(6) + '0'.ljust(8) + str(len(member)).ljust(10) + '`\n').encode()
        result += header + member + (b'\n' if len(member) & 1 else b'')
    return result


class BinaryArchitectureTests(unittest.TestCase):
    def test_real_x64_archive(self):
        self.assertEqual(preflight.binary_machines(archive(coff(0x8664), coff(0x8664))), {0x8664})

    def test_mixed_archive_cannot_pass_as_amd64(self):
        self.assertEqual(preflight.binary_machines(archive(coff(0x8664), coff(0x14c))), {0x8664, 0x14c})

    def test_short_import_object(self):
        header = struct.pack('<HHHH', 0, 0xffff, 0, 0x14c) + bytes(12)
        self.assertEqual(preflight.binary_machines(archive(header)), {0x14c})

    def test_pe_image(self):
        dos = bytearray(64)
        dos[:2] = b'MZ'
        struct.pack_into('<I', dos, 0x3c, 64)
        self.assertEqual(preflight.binary_machines(dos+b'PE\0\0'+coff(0x8664)), {0x8664})

    def test_truncated_and_unknown_inputs_rejected(self):
        for value in (b'MZ', b'garbage', archive(coff(0x8664))[:-3]):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    preflight.binary_machines(value)

    def test_nested_archives_are_rejected(self):
        with self.assertRaises(ValueError):
            preflight.binary_machines(archive(archive(coff(0x8664))))

    def test_invalid_pe_offset_is_rejected(self):
        data = bytearray(68)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 0x3c, 64)
        data[64:] = b'PE\0\0'
        with self.assertRaises(ValueError):
            preflight.binary_machines(data)


if __name__ == '__main__':
    unittest.main()
