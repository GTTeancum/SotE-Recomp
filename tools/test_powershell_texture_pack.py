"""Check the Windows PowerShell publisher against the Python reference."""
import json
import subprocess
import unittest

import test_texture_pack
from build_user_texture_pack import build
from sote_texture_pack import write_png
from pathlib import Path


class PowerShellPackTests(unittest.TestCase):
    setUp = test_texture_pack.PackTests.setUp
    tearDown = test_texture_pack.PackTests.tearDown
    def test_powershell_matches_reference(self):
        script = Path(__file__).with_name('Build-HD-Pack.ps1').resolve()
        for name, edited in [('stock', False), ('edited', True)]:
            if edited:
                write_png(self.source / 'stock.png', 2, 2, bytes([20, 80, 200, 128]) * 4)
            expected, actual = self.root / (name + '-python'), self.root / (name + '-powershell')
            build(self.source, expected)
            subprocess.run(['powershell', '-NoProfile', '-ExecutionPolicy', 'Bypass',
                            '-File', str(script), '-Source', str(self.source),
                            '-Output', str(actual)], check=True)
            for filename, array_key in [('rt64.json', 'textures'), ('sote_slots.json', 'slots')]:
                a = json.loads((actual / filename).read_text())
                b = json.loads((expected / filename).read_text())
                a[array_key] = sorted(a[array_key], key=lambda row: json.dumps(row, sort_keys=True))
                b[array_key] = sorted(b[array_key], key=lambda row: json.dumps(row, sort_keys=True))
                self.assertEqual(a, b)
            if edited:
                self.assertEqual((actual / 'stock.png').read_bytes(), (expected / 'stock.png').read_bytes())


if __name__ == '__main__':
    unittest.main()
