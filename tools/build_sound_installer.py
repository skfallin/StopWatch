"""Build the one-time sound installer with the project's existing, pinned libraries."""
from pathlib import Path
import configparser
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
output = Path(sys.argv[1] if len(sys.argv) > 1 else "/tmp/stopwatch-sound-installer").resolve()
if output == root:
    raise SystemExit("Choose an installer output folder different from the project root.")
output.mkdir(parents=True, exist_ok=True)
config = configparser.ConfigParser(interpolation=None)
config.read(root / "platformio.ini")
config["platformio"]["build_dir"] = str(output / "build")
config["platformio"]["core_dir"] = str(root / ".platformio")
config["platformio"]["libdeps_dir"] = str(root / ".pio/libdeps")
config["env:stopwatch"]["build_flags"] += "\n-DSFX_SEED_ARCHIVE"
path = output / "platformio.ini"
with path.open("w") as file:
    config.write(file)
subprocess.run([str(root / ".venv/bin/pio"), "run", "-d", str(root), "-c", str(path)], check=True)
print(f"Installer built: {output / 'build/stopwatch/firmware.bin'}")
print(f"Upload with: .venv/bin/pio run -d '{root}' -c '{path}' -t upload --upload-port <port>")
print("Wait for [audio-store] pronto=1 cache=1 before flashing the normal firmware.")
