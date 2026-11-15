"""Check the actual Inter font bounds of every app name inside the Home ring."""
from pathlib import Path
import math
import json
import re
import struct
from PIL import ImageFont

root = Path(__file__).resolve().parents[1]
source = (root / "src/scene_view.cpp").read_text()
home = source[source.index("void home("):source.index("void row(")]
names = re.findall(r'"([^"]+)"', re.search(r'const char\* names\[\]=\{([^}]+)\}', home)[1])
assert names == ["Casa", "Note", "Badge", "Orologio", "Spotify", "Ponte", "Sistema", "Focus"]
label = re.search(r'names\[menu\.app\(\)\]\),233,(\d+),ui::Font::Body,ui::accent,ui::black,(\d+)\)', home)
layout = (root / "include/ui_layout.h").read_text()
ring = re.search(r'lround\((\d+)\*std::sin\(angle\)\)\),233-int\(std::lround\(\1\*std::cos\(angle\)\)\),(\d+)\}', layout)
offset, maximum, clear = int(label[1]) - 233, int(label[2]), int(ring[1]) - int(ring[2])
size = 22
font = ImageFont.truetype(str(root / "assets/fonts/InterVariable.ttf"), size)
font.set_variation_by_axes([400 if a["name"] == b"Weight" else min(32, a["maximum"]) for a in font.get_variation_axes()])
font_source = (root / "src/ui_fonts.h").read_text()
header = re.search(r"inter"+str(size)+r"\[\].*?\{\s*([^\n]+)", font_source, re.S)[1]
raw = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]+)", header))
_, _, _, _, ascent, descent = struct.unpack(">6i", raw[:24])
bottom = offset + (ascent + descent) / 2 + 2
# The label must fit the chord of the clear disc inside the ring at its lowest line.
chord = 2 * math.sqrt(clear * clear - bottom * bottom)
assert maximum <= chord, (maximum, chord)
english = json.loads((root / "locales/en.json").read_text())
for name in names + [english.get(name, name) for name in names]:
    width = sum(round(font.getlength(char)) for char in name)
    assert width <= maximum, (name, width, maximum)
print("OK: tutti gli otto nomi leggibili per intero al centro dell'anello")
