"""Check the actual Inter font bounds in every Scene app circle."""
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
assert names == ["Casa", "Note", "Badge", "Orologio", "Spotify", "Ponte", "Chiave", "Sistema", "Focus", "Slide"]
offsets = re.search(r'circle.y\+\(slot==0\?(\d+):(\d+)\)', home)
widths = re.search(r'color,color,slot==0\?(\d+):(\d+)', home)
for size, slot, radius in [(28, 1, 79), (28, 1, 84), (18, 2, 54)]:
    offset, maximum = int(offsets[slot]), int(widths[slot])
    font = ImageFont.truetype(str(root / "assets/fonts/InterVariable.ttf"), size)
    font.set_variation_by_axes([400 if a["name"] == b"Weight" else min(32, a["maximum"]) for a in font.get_variation_axes()])
    font_source = (root / "src/ui_fonts.h").read_text()
    header = re.search(r"inter"+str(size)+r"\[\].*?\{\s*([^\n]+)", font_source, re.S)[1]
    raw = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]+)", header))
    _, _, _, _, ascent, descent = struct.unpack(">6i", raw[:24])
    bottom = offset + (ascent + descent) / 2 + 2
    chord = 2 * math.sqrt(radius * radius - bottom * bottom)
    assert maximum <= chord, (size, maximum, chord)
    for name in names + [json.loads((root / 'locales/en.json').read_text()).get(name, name) for name in names]:
        width = sum(round(font.getlength(char)) for char in name)
        assert width <= maximum, (name, size, width, maximum)
print("OK: tutti i dieci nomi leggibili per intero e contenuti nei cerchi principale e laterali")
