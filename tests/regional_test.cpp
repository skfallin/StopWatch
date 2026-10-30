#include "regional_settings.h"
#include "i18n.h"
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <cstring>

int hour(const char* zone, time_t epoch) {
    assert(regional::rule(zone));
    setenv("TZ",regional::rule(zone),1);tzset();
    tm local{};localtime_r(&epoch,&local);return local.tm_hour;
}
int main() {
    assert(regional::language("en") && regional::language("it"));
    assert(!regional::language("EN") && !regional::language("fr"));
    assert(!regional::rule("not/a/zone") && !regional::rule(""));
    // 2026-01-01 and 2026-07-01 at 12:00 UTC; exercise both DST hemispheres.
    constexpr time_t winter=1767268800,summer=1782907200;
    assert(hour("UTC",winter)==12 && hour("UTC",summer)==12);
    assert(hour("Europe/Rome",winter)==13 && hour("Europe/Rome",summer)==14);
    assert(hour("America/New_York",winter)==7 && hour("America/New_York",summer)==8);
    assert(hour("Australia/Sydney",winter)==23 && hour("Australia/Sydney",summer)==22);
    assert(hour("Asia/Kolkata",winter)==17);
    tm local{};localtime_r(&winter,&local);assert(local.tm_min==30);
    assert(std::strcmp(i18n::text("Impostazioni"),"Settings")==0);
    i18n::italian=true;
    assert(std::strcmp(i18n::text("Impostazioni"),"Impostazioni")==0);
    assert(std::strcmp(i18n::text("User's own label"),"User's own label")==0);
}
