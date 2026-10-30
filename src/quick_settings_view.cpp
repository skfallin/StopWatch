#include "i18n.h"
#include "quick_settings_view.h"
#include <cstdio>
namespace ui {
void quickSettings(M5Canvas& c,bool quiet,bool pending,bool saveFailed,int page,int value) {
    c.fillSmoothRoundRect(211,62,44,5,2,line);
    c.fillSmoothRoundRect(quickPanel.x,quickPanel.y,quickPanel.width,quickPanel.height,40,surface);
    label(c,i18n::text("Controlli rapidi"),214,125,Font::Body,white,surface,218);
    circleButton(c,quickClose,Icon::Close,18,line,white);
    const Icon icons[]={Icon::Moon,Icon::Sun,Icon::Speaker};
    for(int i=0;i<3;++i) circleButton(c,{151+i*82,176,25},icons[i],23,i==page?accent:black,i==page?ink:muted);
    if(page==0) {
        const uint32_t fill=quiet?selectionSurface:black,color=quiet?accent:white;
        c.fillSmoothRoundRect(quietToggle.x,quietToggle.y,quietToggle.width,quietToggle.height,28,fill);
        circleButton(c,{141,253,28},Icon::Moon,32,quiet?accent:surface,quiet?ink:white);
        label(c,i18n::text("Non disturbare"),262,244,Font::Small,color,fill,180);
        label(c,quiet?i18n::text("Attivo"):i18n::text("Disattivato"),262,276,Font::Body,color,fill,175);
        label(c,saveFailed?i18n::text("Non salvato: riprova"):pending?i18n::text("Salvataggio in attesa"):quiet?i18n::text("Silenzio completo"):i18n::text("Suoni e vibrazione attivi"),233,360,Font::Small,muted,surface,292);
    } else {
        const int maximum=page==1?255:200,minimum=page==1?16:0;
        char percent[12];snprintf(percent,sizeof(percent),"%d%%",value*100/maximum);
        label(c,percent,233,250,Font::Clock,white,surface,265);
        label(c,page==1?i18n::text("Luminosità display"):i18n::text("Volume interazioni"),233,295,Font::Small,muted,surface,285);
        const int position=109+(std::clamp(value,minimum,maximum)-minimum)*248/(maximum-minimum);
        c.fillRoundRect(109,339,248,8,4,line);
        c.fillRoundRect(109,339,std::max(8,position-109),8,4,accent);
        c.fillSmoothCircle(position,343,15,accent);
        if(saveFailed || pending) label(c,saveFailed?i18n::text("Non salvato: riprova"):i18n::text("Salvataggio in attesa"),233,375,Font::Small,muted,surface,280);
    }
    label(c,i18n::text("A chiude · B cambia"),233,421,Font::Small,muted,black,280);
}
}
