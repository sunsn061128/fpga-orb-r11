import importlib.util
import pathlib
import shutil
import sys
import tempfile
import unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from compare import compare_file

class CompareFailureTests(unittest.TestCase):
    def test_pixel_reports_first_position(self):
        with tempfile.TemporaryDirectory() as tmp:
            a=pathlib.Path(tmp)/'L0_fast.u8';b=pathlib.Path(tmp)/'actual.u8'
            a.write_bytes(bytes(range(20)));raw=bytearray(a.read_bytes());raw[7]^=1;raw[15]^=2;b.write_bytes(raw)
            r=compare_file(a,b,5)
            self.assertEqual((r['offset'],r['x'],r['y']),(7,2,1));self.assertFalse(r['equal'])
    def test_field_vs_format_and_extra_row(self):
        with tempfile.TemporaryDirectory() as tmp:
            a=pathlib.Path(tmp)/'moments.csv';b=pathlib.Path(tmp)/'actual.csv'
            a.write_text('index,m10,m01\n0,-17,23\n');b.write_text('index,m10,m01\n00,-17,23\n')
            self.assertTrue(compare_file(a,b)['equal'])
            b.write_text('index,m10,m01\n0,-16,23\n');r=compare_file(a,b)
            self.assertEqual((r['row'],r['field']),(0,'m10'))
            b.write_text(a.read_text()+'1,0,0\n');self.assertEqual(compare_file(a,b)['field'],'row count')
    def test_descriptor_bit(self):
        with tempfile.TemporaryDirectory() as tmp:
            a=pathlib.Path(tmp)/'L0_descriptors.u8';b=pathlib.Path(tmp)/'actual.u8'
            a.write_bytes(b'\0'*64);raw=bytearray(a.read_bytes());raw[35]=32;b.write_bytes(raw)
            r=compare_file(a,b);self.assertEqual((r['record'],r['byte'],r['bit']),(1,3,5))
    def test_packet_corruption(self):
        manifest=ROOT/'vectors/manifest.json'
        if manifest.exists():
            import json
            entry=next(r for r in json.loads(manifest.read_text())['expected'] if r['case']=='random_seed_73' and r['role_name']=='normal')
            source=ROOT/entry['expected']/'packet.orb'
        else:source=next((ROOT/'delivery/vectors').glob('random_seed_73/L1_normal/packet.orb'))
        with tempfile.TemporaryDirectory() as tmp:
            target=pathlib.Path(tmp)/'packet.orb';raw=bytearray(source.read_bytes());self.assertGreater(len(raw),128)
            raw[96]^=128;target.write_bytes(raw);r=compare_file(source,target)
            self.assertEqual((r['record'],r['field'],r['byte'],r['bit']),(0,'descriptor',0,7))
            target.write_bytes(raw[:-1])
            with self.assertRaises(ValueError):compare_file(source,target)

if __name__=='__main__':unittest.main()
