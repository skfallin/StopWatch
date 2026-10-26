"""Render the firmware clock views on the host; requires clang++ and existing Pillow."""
from pathlib import Path
import json
import subprocess
import sys
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/stopwatch-clock-preview")
output.mkdir(parents=True, exist_ok=True)
music = "--music" in sys.argv
quick = "--quick" in sys.argv
source = (root / "src/ui_drawing.cpp").read_text()
start, end = source.index("void icon("), source.index("void dots(")
(output / "drawing_primitives.inc").write_text(source[start:end])
binary = output / "clock-render"
subprocess.run([
    "clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
    "-I" + str(root / "tests/render"), "-I" + str(root / "include"),
    "-I" + str(root / "src"), "-I" + str(output),
    str(root / ("src/quick_settings_view.cpp" if quick else "src/music_view.cpp" if music else "src/clock_view.cpp")),
    str(root / ("tests/render/quick_render.cpp" if quick else "tests/render/music_render.cpp" if music else "tests/render/clock_render.cpp")),
    "-o", str(binary)
], check=True)
subprocess.run([str(binary), str(output)], check=True)

scale = 2
sizes = (18, 22, 28, 36, 68, 96)
fonts, metrics = {}, {}
font_path = str(root / "assets/fonts/InterVariable.ttf")
for size in sizes:
    native = ImageFont.truetype(font_path, size)
    large = ImageFont.truetype(font_path, size * scale)
    for font in (native, large):
        font.set_variation_by_axes([400 if a["name"] == b"Weight" else min(32, a["maximum"]) for a in font.get_variation_axes()])
    chars = sorted(set(range(32, 127)) | set(range(160, 256)) | {0x2013, 0x2014, 0x2019, 0x2026})
    if size >= 68:
        chars = sorted(set(map(ord, " 0123456789:%-+./ONFs")))
    bounds = [native.getbbox(chr(code), anchor="ls") for code in chars]
    metrics[size] = (-min(b[1] for b in bounds), max(b[3] for b in bounds))
    fonts[size] = (native, large)

def color(value):
    return (value >> 16 & 255, value >> 8 & 255, value & 255)

truncated = []
for path in sorted(output.glob("*.jsonl")):
    image = Image.new("RGB", (466 * scale, 466 * scale), "black")
    draw = ImageDraw.Draw(image)
    for line in path.read_text().splitlines():
        kind, *values = json.loads(line)
        if kind == "rect":
            x, y, w, h, radius, rgb = values
            draw.rounded_rectangle((x * scale, y * scale, (x + w) * scale, (y + h) * scale), radius * scale, fill=color(rgb))
        elif kind == "circle":
            x, y, radius, rgb = values
            draw.ellipse(((x-radius)*scale, (y-radius)*scale, (x+radius)*scale, (y+radius)*scale), fill=color(rgb))
        elif kind == "line":
            x0, y0, x1, y1, radius, rgb = values
            draw.line((x0*scale, y0*scale, x1*scale, y1*scale), fill=color(rgb), width=max(1, round(radius*2*scale)))
            for x, y in ((x0, y0), (x1, y1)):
                draw.ellipse(((x-radius)*scale, (y-radius)*scale, (x+radius)*scale, (y+radius)*scale), fill=color(rgb))
        elif kind == "text":
            text, x, y, font_index, rgb, background, maximum = values
            size = sizes[font_index]
            native, large = fonts[size]
            width = lambda value: sum(round(native.getlength(char)) for char in value)
            fitted = text
            if width(fitted) > maximum:
                while fitted and width(fitted + "…") > maximum:
                    fitted = fitted[:-1]
                fitted += "…"
                truncated.append({"frame": path.stem, "text": text, "fitted": fitted})
            ascent, descent = metrics[size]
            baseline = y + (ascent-descent)/2
            left = x-width(fitted)/2
            # VLW advances are per glyph without kerning, as in the generated firmware font.
            for char in fitted:
                draw.text((left*scale, baseline*scale), char, font=large, anchor="ls", fill=color(rgb))
                left += round(native.getlength(char))
    mask = Image.new("L", image.size)
    ImageDraw.Draw(mask).ellipse((0, 0, image.width-1, image.height-1), fill=255)
    result = Image.new("RGB", image.size, "#141819")
    result.paste(image, mask=mask)
    result.resize((466, 466), Image.Resampling.LANCZOS).save(path.with_suffix(".png"))

def sheet(name, frames):
    tile, gap = 340, 28
    result = Image.new("RGB", (3*(tile+gap)+gap, 2*(tile+66)+gap), "#141819")
    draw = ImageDraw.Draw(result)
    font = fonts[22][0]
    for index, (frame, caption) in enumerate(frames):
        x, y = gap+(index%3)*(tile+gap), gap+(index//3)*(tile+66)
        image = Image.open(output/(frame+".png")).resize((tile, tile), Image.Resampling.LANCZOS)
        result.paste(image, (x, y))
        draw.text((x+tile/2, y+tile+24), caption, font=font, anchor="mm", fill="#f4f6f0")
    result.save(output/name)

if quick:
    sheet("quick-settings.png", [("quick-off", "Disattivato"), ("quick-on", "Silenzio completo"),
          ("quick-pending", "Salvataggio dopo la nota"), ("quick-failed", "Errore di salvataggio"),
          ("quick-buttons", "Palette dei pulsanti"), ("quick-violet", "Palette viola")])
elif music:
    sheet("music.png", [("playing", "In ascolto"), ("paused", "In pausa"), ("devices", "Lettori Spotify Connect"),
          ("offline", "Connessione interrotta"), ("waiting", "Invio comando"), ("empty", "Nessun brano")])
    sheet("music-library.png", [("music-menu", "Playlist e dispositivi"), ("playlists", "Playlist dell’account"),
          ("playlists-scroll", "Scorrimento con A"), ("playlists-last", "Pagine successive"),
          ("playlists-loading", "Caricamento"), ("playlists-empty", "Libreria vuota")])
else:
    names = ("Digitale", "Analogico", "Halo", "Doppio", "Orbita", "Segmenti")
    for prefix in ("", "violet-"):
        sheet(prefix+"clock-faces.png", [(prefix+f"face-{i}", name) for i, name in enumerate(names)])
    sheet("clock-functions.png", [("menu", "Tre funzioni"), ("timer-edit", "Durata al touch"),
        ("timer-running", "Timer in corso"), ("timer-done", "Tempo scaduto"),
        ("stopwatch-running", "Cronometro e giri"), ("controls", "Comandi del quadrante")])
(output/"truncation.json").write_text(json.dumps(truncated, ensure_ascii=False, indent=2))
print(f"Rendered {len(list(output.glob('*.jsonl')))} production views in {output}; truncated labels: {len(truncated)}")
if truncated:
    print(json.dumps(truncated, ensure_ascii=False))
