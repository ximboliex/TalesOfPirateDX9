import importlib.util
from pathlib import Path
import struct
import unittest

spec = importlib.util.spec_from_file_location('model_audit', Path(__file__).resolve().parents[1]/'audit_model_assets.py')
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def model(texture_size=208):
    material = bytes(172 + 4 * texture_size)
    block = struct.pack('<I', 1) + material
    header = bytes(100) + struct.pack('<4I', len(block), 0, 0, 0)
    return struct.pack('<I', 0x1005) + header + block


class ModelBoundaries(unittest.TestCase):
    def test_native_pointer_size_cannot_change_disk_records(self):
        _, count, records = audit.audit_file(model(), '.lgo')
        self.assertEqual((count, len(records)), (1, 4 * 208))
        with self.assertRaises(ValueError):
            audit.audit_file(model(216), '.lgo')

    def test_container_geometry(self):
        geometry = model()[4:]
        container = struct.pack('<5I', 0x1005, 1, 1, 20, len(geometry)) + geometry
        self.assertEqual(audit.audit_file(container, '.lmo')[1], 1)

    def test_truncated_and_oversized_blocks(self):
        for data in (b'', model()[:-1], struct.pack('<2I', 0x1005, 0xffffffff)):
            with self.subTest(length=len(data)):
                with self.assertRaises(ValueError):
                    audit.audit_file(data, '.lmo' if len(data) == 8 else '.lgo')

    def test_container_header_overlap(self):
        with self.assertRaises(ValueError):
            audit.audit_file(struct.pack('<5I', 0x1005, 1, 1, 8, 0), '.lmo')
