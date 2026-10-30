"""Exercise the production optional Home/Slide configuration validation atomically."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'src/settings.cpp').read_text()
a=source.index('    if(!input["presentation_page_keys"].isUnbound())')
b=source.index('    String ssidValue',a)
harness=r'''
#include "home_layout.h"
#include <ArduinoJson.h>
#include <cstring>
#include <string>
#include <cassert>
struct Settings { std::array<int,launcher::appCount> homeApps=launcher::defaultOrder;size_t homeAppCount=launcher::appCount;bool presentationPageKeys=false; } settings;
std::string error;
bool update(JsonObjectConst input) { Settings next=settings;
'''+source[a:b]+r'''
settings=next;return true; }
int main() {
 JsonDocument document;
 for(auto bad:{R"({"home_apps":null})",R"({"home_apps":[]})",R"({"home_apps":["home"]})",R"({"home_apps":["system","system"]})",R"({"home_apps":["system",1]})",R"({"home_apps":["system","missing"]})",R"({"home_apps":["system\u0000bad"]})",R"({"presentation_page_keys":"true"})"}) {
  assert(!deserializeJson(document,bad));assert(!update(document.as<JsonObjectConst>()));assert(settings.homeApps==launcher::defaultOrder && !settings.presentationPageKeys);
 }
 assert(!deserializeJson(document,R"({"home_apps":["system","focus","home"],"presentation_page_keys":true})"));
 assert(update(document.as<JsonObjectConst>()));assert(settings.homeAppCount==3 && settings.homeApps[0]==7 && settings.homeApps[1]==8 && settings.presentationPageKeys);
 assert(!deserializeJson(document,"{}"));assert(update(document.as<JsonObjectConst>()));assert(settings.homeAppCount==3 && settings.presentationPageKeys);
}
'''
with tempfile.TemporaryDirectory(prefix='stopwatch-launcher-') as folder:
    path=Path(folder);(path/'test.cpp').write_text(harness)
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'include'),'-I'+str(root/'.pio/libdeps/stopwatch/ArduinoJson/src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('OK: Home/Slide, ID validi, unicità, Sistema sempre visibile, aggiornamento atomico e client precedenti')
