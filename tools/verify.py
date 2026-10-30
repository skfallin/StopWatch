"""Run the public-beta checks without flashing hardware or contacting user services."""
from pathlib import Path
import argparse
import os
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def run(*args, env=None):
    print('+', ' '.join(map(str, args)), flush=True)
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True, env=env)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--firmware', action='store_true', help='Build normal firmware and install pinned build dependencies')
    parser.add_argument('--lab', action='store_true', help='Also compile the opt-in FIDO LAB firmware')
    parser.add_argument('--swift', action='store_true', help='Run Companion model/protocol tests on macOS')
    args = parser.parse_args()
    run(sys.executable, 'tools/generate_localizations.py', '--check')
    run(sys.executable, 'tools/check_public_repo.py')
    if args.firmware:
        run(sys.executable, '-m', 'platformio', 'run', '-e', 'stopwatch')
    if args.lab:
        run(sys.executable, '-m', 'platformio', 'run', '-e', 'stopwatch-lab')
    dependency = ROOT / '.pio/libdeps/stopwatch/ArduinoJson/src'
    if not dependency.is_dir():
        raise SystemExit('Run with --firmware once to install the pinned ArduinoJson headers.')
    with tempfile.TemporaryDirectory(prefix='stopwatch-checks-') as directory:
        temporary = Path(directory)
        compiler = os.environ.get('CXX', 'clang++')
        flags = ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', '-Iinclude', '-I'+str(dependency)]
        for name in ['core', 'clock', 'relay', 'http_transfer', 'gradient', 'light', 'productivity', 'usb_input', 'security_key', 'regional']:
            binary = temporary / name
            run(compiler, *flags, f'tests/{name}_test.cpp', '-o', binary)
            run(binary)
        run(compiler, *flags, '-I'+str(dependency), 'tests/music_test.cpp', '-o', temporary/'music')
        run(temporary/'music')
        for test in sorted((ROOT/'tests').glob('*_test.py')):
            run(sys.executable, test)
        if platform.system() == 'Darwin':
            for installer in [False, True]:
                binary = temporary / ('audio-installer' if installer else 'audio')
                run(compiler, *flags, '-Wno-deprecated-declarations', '-Itests/audio_support', '-Isrc',
                    *(['-DSFX_SEED_ARCHIVE'] if installer else []), 'tests/audio_archive_test.cpp', 'src/ui_sounds.cpp', '-lz', '-o', binary)
                run(binary, 'assets/sounds/ui-sounds.bin')
        else:
            print('SKIP: CommonCrypto audio checks require macOS; covered by the macOS CI job.')
        if args.swift:
            env = os.environ.copy()
            env['CLANG_MODULE_CACHE_PATH'] = str(temporary/'clang-cache')
            env['SWIFTPM_MODULECACHE_OVERRIDE'] = str(temporary/'swift-cache')
            env['STOPWATCH_BADGE_FIXTURE'] = str(temporary/'badge.swb')
            run('xcrun', 'swift', 'test', '--package-path', 'ios', '--scratch-path', temporary/'swift', env=env)
            run(compiler, *flags, 'tests/badge_test.cpp', '-lz', '-o', temporary/'badge')
            run(temporary/'badge', temporary/'badge.swb')
    print('All requested checks passed. Hardware and iOS UI checks are separate.')

if __name__ == '__main__':
    main()
