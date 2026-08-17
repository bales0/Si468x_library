#include "../Si468x.h"
#include <cassert>
#include <cstring>
using namespace si468x;

struct Mock {
    uint8_t command;
    uint8_t args[32];
    uint16_t argLength;
    uint32_t ticks;
    Mock():command(0),argLength(0),ticks(0){std::memset(args,0,sizeof(args));}
};
static uint32_t tm(void* c){ Mock* m=(Mock*)c; m->ticks+=1000u; return m->ticks; }
static bool wr(void* c,uint8_t cmd,const uint8_t* p,uint16_t n){
    Mock* m=(Mock*)c; m->command=cmd; m->argLength=n;
    if(n>sizeof(m->args)) return false;
    if(n) std::memcpy(m->args,p,n);
    return true;
}
static bool rd(void*,uint8_t* p,uint16_t n){ std::memset(p,0,n); if(n) p[0]=0x80; return true; }

int main(){
    Mock m; HostInterface h; h.context=&m; h.timeUs=tm; h.writeCommand=wr; h.readReply=rd;
    Si468x radio(h); uint8_t ws[128]; radio.setWorkspace(ws,sizeof(ws));
    uint8_t reply[4];

    // GET_DIGITAL_SERVICE_LIST SERTYPE is a two-bit field; all 0..3 values
    // must be passed intact, not truncated to one bit.
    for(uint8_t t=0;t<4;++t){
        assert(radio.getDigitalServiceList(t,reply,sizeof(reply))==Result::Ok);
        assert(m.command==(uint8_t)Command::GET_DIGITAL_SERVICE_LIST);
        assert(m.argLength==1 && m.args[0]==t);
    }
    assert(radio.getDigitalServiceList(4,reply,sizeof(reply))==Result::InvalidArgument);

    // DAB convenience always writes SERTYPE=0 and preserves the exact 32-bit
    // component value returned by the service-list parser.
    assert(radio.startDabService(0x11223344UL,0x0145E123UL)==Result::Ok);
    assert(m.command==(uint8_t)Command::START_DIGITAL_SERVICE);
    assert(m.argLength==11 && m.args[0]==0);
    assert(readLe32(m.args+3)==0x11223344UL);
    assert(readLe32(m.args+7)==0x0145E123UL);

    // Non-blocking start helper writes the command immediately and leaves CTS
    // completion to service().
    Result x=radio.startFmTune(10170);
    assert(x==Result::Pending); assert(radio.busy());
    assert(m.command==(uint8_t)Command::FM_TUNE_FREQ);
    assert(readLe16(m.args+1)==10170u);
    assert(radio.service()==Result::Ok); assert(!radio.busy());
    return 0;
}
