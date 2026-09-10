import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from build_dynamic_texture_pack import slot_alias
from build_user_texture_pack import build
from sote_texture_pack import write_png
from extract_rom_textures import decode_and_hash


class PackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.key = [3,0x2bc380,32,32,2,1,4,0,0,32768,32,0,0,2,1,1,0x1a4740]
        self.alias = slot_alias(self.key)
        write_png(self.source / "stock.png", 1, 1, bytes([255,0,0,128]))
        (self.source / "source_baseline.json").write_text(json.dumps({"version":1,"images":{
            "stock.png":hashlib.sha256((self.source / "stock.png").read_bytes()).hexdigest()}}))
        (self.source / "rt64.json").write_text(json.dumps({"configuration":{},"textures":[
            {"hashes":{"rt64":"1234567890abcdef"},"path":"stock"}]}))
        (self.source / "slot_catalog.json").write_text(json.dumps({"slots":[
            {"key":self.key,"hash":self.alias,"path":"stock"}]}))

    def tearDown(self):
        self.temp.cleanup()

    def test_stock_is_passthrough(self):
        self.assertEqual(build(self.source,self.root/"pack"),(0,0,0))
        self.assertEqual(json.loads((self.root/"pack/sote_slots.json").read_text())["slots"],[])

    def test_edit_binds_content_and_source_and_preserves_alpha(self):
        write_png(self.source/"stock.png",2,1,bytes([0,255,0,63])*2)
        self.assertEqual(build(self.source,self.root/"pack"),(1,2,1))
        self.assertEqual((self.source/"stock.png").read_bytes(),(self.root/"pack/stock.png").read_bytes())
        with self.assertRaises(ValueError): build(self.source,self.root/"pack")

    def test_bad_art_is_rejected_before_output(self):
        (self.source/"stock.png").write_bytes(b"not a png")
        with self.assertRaises(ValueError): build(self.source,self.root/"pack")
        self.assertFalse((self.root/"pack").exists())

    def test_source_escape_is_rejected(self):
        (self.source/"source_baseline.json").write_text(json.dumps({"version":1,"images":{"../escape.png":"x"}}))
        with self.assertRaises(ValueError): build(self.source,self.root/"pack")

    def test_alias_matches_cpp_and_separates_palettes(self):
        self.assertEqual(self.alias,"30e6c4ff00118fa4")
        other=self.key.copy(); other[-1]+=32
        self.assertNotEqual(slot_alias(other),self.alias)

    def test_rom_sprite_odd_rows_are_unswizzled(self):
        tmem=bytearray(4096)
        tmem[:16]=bytes(list(range(8))+[12,13,14,15,8,9,10,11])
        for i in range(16):
            tmem[2048+i*8:2048+i*8+8]=(((i+1)<<1)|1).to_bytes(2,'big')*4
        result=decode_and_hash({"tile":{"fmt":2,"siz":1,"line":1,"tmem":0,"palette":0},
            "width":8,"height":2,"tlut":32768,"tmem":bytes(tmem),
            "source":("fixture",0,16),"container":"fixture"})
        self.assertEqual(list(result["pixels"][2::4]),[(i<<3)|(i>>2) for i in range(1,17)])
        self.assertEqual(set(result["pixels"][3::4]),{255})


if __name__ == "__main__": unittest.main()
