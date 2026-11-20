"""tools/cdat_scan.py: what the C-DAT scan flags in a game's C++ code, what it leaves alone, and the cdat-ok waiver.

python -m unittest tests/test_cdat_scan.py
"""
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

path = Path(__file__).resolve().parents[1] / "tools/cdat_scan.py"
spec = importlib.util.spec_from_file_location("cdat_scan", path)
cdat = importlib.util.module_from_spec(spec)
sys.modules["cdat_scan"] = cdat   # (its dataclass looks its module up)
spec.loader.exec_module(cdat)


def rules(code):
    return [f.rule for f in cdat.scan_lines(code.splitlines(), "x.cpp")]


class CdatScanTests(unittest.TestCase):
    def test_element_creation(self):
        self.assertEqual(rules('auto* e = doc->CreateElement("div");'), ["element-creation"])
        self.assertEqual(rules("parent->AppendChild(std::move(e));"), ["element-creation"])
        self.assertEqual(rules('ui->set_markup(kDoc, "list", "<div class=\\"row\\">" + name + "</div>");'), ["element-creation"])
        # markup that comes from the localization file is the file's, not the code's
        self.assertEqual(rules('ui->set_markup(kDoc, "tip", UiText::get().text("TIP_1"));'), [])

    def test_markup_string(self):
        self.assertEqual(rules('const std::string row = "<span class=\'gold\'>" + money + "</span>";'), ["markup-string"])
        self.assertEqual(rules('LOG_INFO("a < b and c > d");'), [])

    def test_inline_style(self):
        self.assertEqual(rules('ui->set_property(kDoc, "fill", "width", width);'), ["inline-style"])
        self.assertEqual(rules('ui->set_property(kDoc, "name",\n    "color", "#ff0000");'), ["inline-style", "color-literal"])
        self.assertEqual(rules('e->SetProperty("left", "10px");'), ["inline-style"])
        self.assertEqual(rules('ui->set_attribute(kDoc, "box", "style", "top: 4dp");'), ["inline-style"])
        # an attribute that is data (an image), not a style
        self.assertEqual(rules('ui->set_attribute(kDoc, "portrait", "src", path);'), [])
        self.assertEqual(rules('ui->set_class(kDoc, "menu", "touch", true);'), [])

    def test_color_literal(self):
        self.assertEqual(rules('const char* gold = "#ffcf40";'), ["color-literal"])
        self.assertEqual(rules('std::string c = "rgba(255, 0, 0, 128)";'), ["color-literal"])
        self.assertEqual(rules("uint32_t ring = 0xff40ffa0;"), ["color-literal"])
        self.assertEqual(rules("const uint32_t kZone = 0xc0ffd040u;"), ["color-literal"])
        # a mask is not a color
        self.assertEqual(rules("return (color & 0xff000000) | rest;"), [])
        self.assertEqual(rules("int id = 0x1f;"), [])
        self.assertEqual(rules('ui->set_text(kDoc, "tag", "#1");'), [])

    def test_player_text(self):
        self.assertEqual(rules('ui->set_text(kDoc, "title", "Game Over");'), ["player-text"])
        self.assertEqual(rules('ui->set_text(kDoc, "toggle", open ? "Hide" : "-");'), ["player-text"])
        self.assertEqual(rules('ui->set_text(kDoc, "money", std::to_string(money));'), [])
        self.assertEqual(rules('ui->set_text(kDoc, "time", text.format("CLOCK", {{"t", "1:20"}}));'), [])
        self.assertEqual(rules('ui->set_text(kDoc, "fps", "--");'), [])

    def test_comments_are_not_scanned(self):
        self.assertEqual(rules('// ui->set_text(kDoc, "title", "Game Over");'), [])
        self.assertEqual(rules('/* "#ff0000"\n   CreateElement */ int x = 0;'), [])

    def test_waiver(self):
        self.assertEqual(rules('ui->set_text(kDoc, "fps", "FPS");   // cdat-ok: debug overlay (C-DAT-5)'), [])
        self.assertEqual(rules('// cdat-ok: debug overlay, listed in DESIGN.md\nui->set_text(kDoc, "fps", "FPS");'), [])
        # a waiver without a reason waives nothing
        self.assertEqual(rules('ui->set_text(kDoc, "fps", "FPS");   // cdat-ok:'), ["player-text"])
        # the line above waives only when it is a comment alone
        self.assertEqual(rules('int a = 0;   // cdat-ok: its own\nui->set_text(kDoc, "fps", "FPS");'), ["player-text"])

    def test_scan_paths(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root / "src").mkdir()
            (root / "tests").mkdir()
            (root / "src" / "Hud.cpp").write_text('ui->set_text(1, "a", "Wave");\nui->set_text(1, "b", count);\n', encoding="utf-8")
            (root / "src" / "notes.txt").write_text('set_text(1, "a", "Wave")', encoding="utf-8")
            (root / "tests" / "HudTests.cpp").write_text('ui->set_text(1, "a", "Wave");', encoding="utf-8")
            findings = cdat.scan([root])
            self.assertEqual([(Path(f.path).name, f.line, f.rule) for f in findings], [("Hud.cpp", 1, "player-text")])
            self.assertEqual(cdat.counts(findings)["player-text"], 1)
            self.assertEqual(cdat.main(["--summary", "--report-only", str(root)]), 0)
            self.assertEqual(cdat.main(["--summary", str(root)]), 1)


if __name__ == "__main__":
    unittest.main()
