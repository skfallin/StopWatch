#include "i18n.h"
#include "productivity_view.h"
#include <cstdio>
namespace productivity_ui {
namespace {
String time(uint64_t ms) {
    char text[20];snprintf(text,sizeof(text),"%02lu:%02lu",(unsigned long)(ms/60000),(unsigned long)(ms/1000%60));return text;
}
void header(M5Canvas& c,const char* title,ui::Icon icon) {
    ui::circleButton(c,ui::backButton,ui::Icon::Back,24,ui::surface,ui::navigation);
    ui::label(c,title,245,79,ui::Font::Body,ui::white,ui::black,220);
    ui::icon(c,icon,233,145,38,ui::accent);
}
}
void focus(M5Canvas& c,const clockwork::Focus& model) {
    header(c,"Focus",ui::Icon::Focus);
    const auto& timer=model.timer();
    const bool done=timer.state()==clockwork::Timer::State::Done,running=timer.state()==clockwork::Timer::State::Running;
    ui::label(c,model.phase()==clockwork::Focus::Phase::Work?i18n::text("Concentrazione"):model.phase()==clockwork::Focus::Phase::LongBreak?i18n::text("Pausa lunga"):i18n::text("Pausa breve"),233,197,ui::Font::Label,ui::white);
    ui::label(c,time(uint64_t(timer.remainingSeconds())*1000),233,260,ui::Font::Clock,ui::white);
    char sessions[40];snprintf(sessions,sizeof(sessions),model.sessions()==1?i18n::text("%u sessione completata"):i18n::text("%u sessioni completate"),model.sessions());
    ui::label(c,sessions,233,306,ui::Font::Small,ui::muted);
    ui::pill(c,233,358,248,done?i18n::text("Continua"):running?i18n::text("Pausa"):timer.state()==clockwork::Timer::State::Paused?i18n::text("Riprendi"):i18n::text("Inizia"),ui::accent,ui::ink);
    ui::label(c,running?i18n::text("25 / 5 min · pausa lunga ogni 4"):i18n::text("Azzera sessione"),233,413,ui::Font::Small,ui::muted,ui::black,295);
}
void slides(M5Canvas& c,uint64_t elapsed,bool usb) {
    header(c,"Slide",ui::Icon::Display);
    ui::label(c,time(elapsed),233,219,ui::Font::Clock,ui::white);
    ui::label(c,usb?i18n::text("Tempo presentazione"):i18n::text("Collega USB-C al computer"),233,262,ui::Font::Small,ui::muted,ui::black,300);
    ui::circleButton(c,{149,326,42},ui::Icon::Back,30,ui::surface,usb?ui::white:ui::muted);
    ui::circleButton(c,{317,326,42},ui::Icon::Next,30,ui::accent,usb?ui::ink:ui::muted);
    ui::pill(c,233,408,230,i18n::text("Oscura / riprendi"),ui::surface,ui::white);
}
}
