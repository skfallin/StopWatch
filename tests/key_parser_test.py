"""Compile the real CanoKey RP parser and display adapter, without hardware."""
from pathlib import Path
import os
import sys
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
lib = root / "lib/CanokeyFido"
directories = ["port", "vendor/include", "vendor/applets/ctap", "vendor/canokey-crypto/include", "vendor/littlefs", "vendor/tinycbor/src"]
sources = [root / "tests/key_parser_test.c", lib / "port/core.c", lib / "vendor/applets/ctap/ctap-parser.c",
           lib / "vendor/tinycbor/src/cborencoder.c", lib / "vendor/tinycbor/src/cborparser.c"]
with tempfile.TemporaryDirectory(prefix="stopwatch-key-parser-") as temporary:
    binary = str(Path(temporary) / "test")
    subprocess.run(["clang", "-std=c11", "-ffunction-sections", "-fdata-sections", *( ["-Wl,-dead_strip"] if sys.platform == "darwin" else ["-Wl,--gc-sections"] ),
                    "-Wno-macro-redefined", "-fsanitize=address,undefined", "-DCBOR_NO_FLOATING_POINT",
                    "-DCBOR_PARSER_MAX_RECURSIONS=8", *["-I"+str(lib / d) for d in directories],
                    *map(str, sources), "-o", binary], check=True)
    subprocess.run([binary], env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"}, check=True)
