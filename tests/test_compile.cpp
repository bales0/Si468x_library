#include "../Si468x.h"
using namespace si468x;

static uint32_t tm(void*) { static uint32_t t=0; return ++t*1000u; }
static bool wr(void*,uint8_t,const uint8_t*,uint16_t){return true;}
static bool rd(void*,uint8_t* p,uint16_t n){
    for(uint16_t i=0;i<n;i++) p[i]=0;
    if(n) p[0]=0x80; // CTS
    return true;
}

int main(){
    HostInterface h; h.timeUs=tm; h.writeCommand=wr; h.readReply=rd;
    Si468x r(h); uint8_t buf[128]; r.setWorkspace(buf,sizeof(buf));
    uint16_t v=0; (void)r.getProperty(Property::AUDIO_ANALOG_VOLUME,v);
    FmRsqStatus f; (void)r.fmRsqStatus(f);
    DabServiceListParser p; (void)p;
    RdsDecoder d; (void)d;
    return 0;
}
