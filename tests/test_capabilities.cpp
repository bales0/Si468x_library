#include "../Si468x.h"
#include <stdint.h>
#include <string.h>

using namespace si468x;

struct Fake {
    uint8_t cmd;
    uint8_t args[32];
    uint16_t len;
    uint32_t now;
};
static bool wr(void* c,uint8_t cmd,const uint8_t* a,uint16_t n){
    Fake* f=(Fake*)c; f->cmd=cmd; f->len=n; if(n) memcpy(f->args,a,n); return true;
}
static bool rd(void* c,uint8_t* d,uint16_t n){ (void)c; memset(d,0,n); if(n>=1)d[0]=0x80; return true; }
static uint32_t tm(void* c){ return ((Fake*)c)->now += 100; }

int main(){
    Capabilities c4=capabilitiesForPart(4684);
    if(!c4.fm || !c4.rds || !c4.dab || !c4.dabPlus || c4.am || c4.hdFm || c4.hdAm) return 1;
    Capabilities c9=capabilitiesForPart(4689);
    if(!c9.fm || !c9.rds || !c9.am || !c9.hdFm || !c9.hdAm || !c9.dab || !c9.dabPlus) return 2;

    Fake f={0,{0},0,0}; HostInterface h; h.context=&f; h.writeCommand=wr; h.readReply=rd; h.timeUs=tm;
    Si468x r(h);
    if(r.setMute(true,false)!=Result::Ok) return 3;
    if(f.cmd!=(uint8_t)Command::SET_PROPERTY || f.len!=5 || f.args[3]!=1 || f.args[4]!=0) return 4;
    if(r.setMute(false,true)!=Result::Ok) return 5;
    if(f.args[3]!=2 || f.args[4]!=0) return 6;
    return 0;
}
