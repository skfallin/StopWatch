#include "quick_settings_view.h"

namespace ui {
void quickSettings(M5Canvas& c,bool quiet,bool pending,bool saveFailed) {
    c.fillSmoothRoundRect(211,62,44,5,2,line);
    c.fillSmoothRoundRect(quickPanel.x,quickPanel.y,quickPanel.width,quickPanel.height,40,surface);
    label(c,"Controlli rapidi",214,125,Font::Body,white,surface,218);
    circleButton(c,quickClose,Icon::Close,18,line,white);
    const uint32_t fill=quiet?selectionSurface:black, color=quiet?accent:white;
    c.fillSmoothRoundRect(quietToggle.x,quietToggle.y,quietToggle.width,quietToggle.height,32,fill);
    circleButton(c,{233,207,33},Icon::Moon,38,quiet?accent:surface,quiet?ink:white);
    label(c,"Non disturbare",233,262,Font::Label,color,fill,250);
    c.fillSmoothRoundRect(209,286,48,28,14,quiet?accent:line);
    c.fillSmoothCircle(quiet?243:223,300,10,quiet?ink:white);
    label(c,saveFailed?"Non salvato: riprova":pending?"Salvataggio in attesa":quiet?"Attivo · silenzio completo":"Disattivato",
          233,350,Font::Small,saveFailed?white:muted,surface,292);
    label(c,"A chiude · B cambia",233,407,Font::Small,muted,black,280);
}
}
