"""Run the actual firmware output gates and panel routing with host hardware spies."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/main.cpp").read_text()

def function(signature):
    start = source.index(signature + " {")
    line_end = source.index("\n", start)
    if source[line_end - 1] == "}":
        return source[start:line_end]
    return source[start:source.index("\n}", start) + 2]

functions = [function(signature) for signature in (
    "bool canSave()", "void sound(sfx::Cue cue, bool preview=false)",
    "void toast(const String& text)", "void vibrate(uint32_t duration = 35, bool test = false)",
    "void applySettings()", "void showQuickPanel(bool open)", "void toggleQuiet()",
    "void playNote()", "bool remoteOutput(const String& action)",
    "void diagnosticAction()", "void startRecording(bool fromTouch)",
)]
touch = function("void handleTouch(const ui::Gesture& gesture)")
functions.append(touch[:touch.index("    if(noticeTouch)")] + "    ++underlyingTouches;\n}")
start = source.index("    if(events.recordStop) stopRecording();", source.index("void loop()"))
end = source.index("    if(M5.Touch.getCount()>1)", start)
functions.append("void buttons(const InputEvents& events) {\n" + source[start:end] + "}\n")
start = source.index("    if(quietSavePending && canSave())")
end = source.index("    const bool animating=", start)
functions.append("void flushQuiet() {\n" + source[start:end] + "}\n")
config = (root / "src/settings.cpp").read_text()
start = config.index('    if(!input["do_not_disturb"].isUnbound())')
end = config.index('    if(!input["note_volume"].isUnbound())', start)
functions.append("bool updateQuiet(JsonObjectConst input,String& error) {\n    auto next=settings;\n" +
                 config[start:end] + "    settings=next;return true;\n}")
line = next(line for line in config.splitlines() if 'out["do_not_disturb"]' in line)
functions.append("void encodeQuiet(JsonObject out,bool doNotDisturb) {\n" + line + "\n}")

with tempfile.TemporaryDirectory(prefix="stopwatch-quiet-") as temporary:
    output = Path(temporary)
    (output / "quiet_functions.inc").write_text("\n\n".join(functions))
    binary = output / "quiet-test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / "include"),
                    "-I" + str(root / ".pio/libdeps/stopwatch/ArduinoJson/src"),
                    "-I" + temporary, str(root / "tests/quiet_test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
