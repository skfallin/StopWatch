"""Exercise the actual transport choice, including failure after an MQTT dispatch."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/mqtt_bridge.cpp").read_text()
start = source.index("bool MqttBridge::dispatch(")
body = source[start:source.index("\n}", start) + 2]
harness = r'''
#include "phone_relay.h"
#include <cassert>
#include <iostream>
using String=std::string;
struct Companion {
    int sends=0;bool accepts=true;
    bool queueRelay(phone::Kind,const String&) { ++sends;return accepts; }
};
struct MqttBridge {
    Companion* relay_=nullptr;
    String base_="stopwatch/test",topic;
    int sends=0;bool online=true,accepts=true;
    bool connected() const { return online; }
    bool publish(const String& value,const String&,bool retain,int qos) {
        assert(!retain && qos==0);topic=value;++sends;return accepts;
    }
    bool dispatch(phone::Kind,const String&,bool&);
};
'''
harness += body + r'''
int main() {
    Companion phone;MqttBridge mqtt;mqtt.relay_=&phone;bool viaPhone=true;
    assert(mqtt.dispatch(phone::Kind::Music,"request",viaPhone));
    assert(!viaPhone && mqtt.sends==1 && phone.sends==0 && mqtt.topic=="stopwatch/test/music/request");
    mqtt.accepts=false;
    assert(!mqtt.dispatch(phone::Kind::Favorite,"request",viaPhone));
    assert(!viaPhone && mqtt.sends==2 && phone.sends==0);
    mqtt.online=false;
    assert(mqtt.dispatch(phone::Kind::Favorite,"request",viaPhone));
    assert(viaPhone && mqtt.sends==2 && phone.sends==1);
    phone.accepts=false;
    assert(!mqtt.dispatch(phone::Kind::Music,"request",viaPhone));
    assert(mqtt.sends==2 && phone.sends==2);
    mqtt.relay_=nullptr;
    assert(!mqtt.dispatch(phone::Kind::Music,"request",viaPhone));
    std::cout<<"OK: Wi-Fi preferito, ponte in assenza del broker, nessun reinvio dopo un errore\n";
}
'''
with tempfile.TemporaryDirectory(prefix="stopwatch-relay-choice-") as temporary:
    output = Path(temporary)
    (output / "test.cpp").write_text(harness)
    binary = output / "test"
    subprocess.run(["clang++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-I" + str(root / "include"),
                    str(output / "test.cpp"), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
