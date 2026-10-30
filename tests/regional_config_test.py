"""Exercise production regional-field validation and stored-config migration."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'src/settings.cpp').read_text()
start = source.index('    Settings next = *this;')
end = source.index('    const auto number', start)
validation = source[start:end]
migration = '\n'.join(line for line in source.splitlines() if 'if(document["language"]' in line or 'if(document["time_zone"]' in line)
harness = r'''
#include "regional_settings.h"
#include "i18n.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstring>
#include <string>
using String=std::string;
bool stringField(JsonObjectConst input,const char* key,String& target,size_t limit,String&) {
    if(!input[key].is<const char*>()) return false;
    const auto value=input[key].as<JsonString>();
    if(value.size()>limit || std::strlen(value.c_str())!=value.size()) return false;
    target=value.c_str();return true;
}
struct Settings {
    String language="en",timeZone="UTC";
    bool update(JsonObjectConst input,String& error) {
'''+validation+r'''
        *this=next;return true;
    }
};
void migrate(JsonDocument& document) {
'''+migration+r'''
}
int main() {
    Settings settings;String error;JsonDocument input;
    assert(settings.language=="en" && settings.timeZone=="UTC");
    assert(settings.update(input.to<JsonObject>(),error));
    input["language"]="it";input["time_zone"]="Asia/Kolkata";
    assert(settings.update(input.as<JsonObjectConst>(),error));
    assert(settings.language=="it" && settings.timeZone=="Asia/Kolkata");
    input["language"]="en";input["time_zone"]="Invalid/Zone";
    assert(!settings.update(input.as<JsonObjectConst>(),error));
    assert(settings.language=="it" && settings.timeZone=="Asia/Kolkata");
    for(auto bad:{"null","true","1","[]","{}","\"fr\"","\"en\\u0000x\""}) {
        assert(!deserializeJson(input,std::string("{\"language\":")+bad+"}"));
        assert(!settings.update(input.as<JsonObjectConst>(),error));
    }
    input.clear();assert(settings.update(input.to<JsonObject>(),error));
    assert(settings.language=="it" && settings.timeZone=="Asia/Kolkata");
    migrate(input);assert(input["language"]=="it" && input["time_zone"]=="Europe/Rome");
    input["language"]="en";input["time_zone"]="UTC";migrate(input);
    assert(input["language"]=="en" && input["time_zone"]=="UTC");
}
'''
with tempfile.TemporaryDirectory(prefix='stopwatch-regional-') as folder:
    path = Path(folder)
    (path/'test.cpp').write_text(harness)
    subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root/'include'),'-I'+str(root/'.pio/libdeps/stopwatch/ArduinoJson/src'),str(path/'test.cpp'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('OK: regional defaults, atomic validation, omitted fields and legacy migration')
