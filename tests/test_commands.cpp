#include "../Si468x.h"
#include <cassert>
#include <cstring>
using namespace si468x;

struct Mock {
    uint8_t command;
    uint8_t args[32];
    uint16_t argLength;
    uint32_t ticks;
    bool commandError;
    uint8_t errorCode;
    char events[8];
    uint8_t eventCount;
    Mock():command(0),argLength(0),ticks(0),commandError(false),errorCode(0),eventCount(0){std::memset(args,0,sizeof(args));std::memset(events,0,sizeof(events));}
};
static uint32_t tm(void* c){ Mock* m=(Mock*)c; m->ticks+=1000u; return m->ticks; }
static bool wr(void* c,uint8_t cmd,const uint8_t* p,uint16_t n){
    Mock* m=(Mock*)c; m->command=cmd; m->argLength=n;
    if(n>sizeof(m->args)) return false;
    if(n) std::memcpy(m->args,p,n);
    return true;
}
static bool rd(void* c,uint8_t* p,uint16_t n){
    Mock* m=(Mock*)c; std::memset(p,0,n); if(n) p[0]=(uint8_t)(m->commandError?0xC0:0x80);
    if(m->commandError && n>4) p[4]=m->errorCode;
    return true;
}
static void rst(void* c,bool asserted){ Mock* m=(Mock*)c; if(m->eventCount<sizeof(m->events)) m->events[m->eventCount++]=asserted?'R':'r'; }
static void pwr(void* c,bool enabled){ Mock* m=(Mock*)c; if(m->eventCount<sizeof(m->events)) m->events[m->eventCount++]=enabled?'P':'p'; }

int main(){
    Mock m; HostInterface h; h.context=&m; h.timeUs=tm; h.writeCommand=wr; h.readReply=rd; h.setReset=rst; h.setPower=pwr;
    Si468x radio(h); uint8_t ws[128]; radio.setWorkspace(ws,sizeof(ws));
    uint8_t reply[4];

    // RSTB must be asserted before an optional board power transition.
    m.eventCount=0;
    assert(radio.hardwareReset(1000u,1000u,1000u)==Result::Ok);
    assert(m.eventCount==3u && m.events[0]=='R' && m.events[1]=='P' && m.events[2]=='r');

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

    // AN649 argument bounds are rejected instead of being silently masked.
    assert(radio.fmTune(10170,TuneMode::Reserved)==Result::InvalidArgument);
    assert(radio.fmTune(10170,TuneMode::AnalogOnly,Injection::Automatic,129)==Result::InvalidArgument);
    assert(radio.amTune(1000,TuneMode::AnalogOnly,Injection::Automatic,4097)==Result::InvalidArgument);
    assert(radio.dabTune(0,Injection::Automatic,129)==Result::InvalidArgument);
    assert(radio.dabTune(48)==Result::InvalidArgument);
    uint32_t freqs[49]={0}; assert(radio.dabSetFrequencyList(freqs,49)==Result::InvalidArgument);

    // GET_PROPERTY COUNT is exposed, not silently limited to one property.
    uint8_t propReply[8]={0};
    assert(radio.getProperties((uint16_t)Property::FM_RSQ_SNR_HIGH_THRESHOLD,2,propReply,sizeof(propReply))==Result::Ok);
    assert(m.command==(uint8_t)Command::GET_PROPERTY && m.argLength==3u && m.args[0]==2u);
    assert(readLe16(m.args+1)==(uint16_t)Property::FM_RSQ_SNR_HIGH_THRESHOLD);
    assert(radio.getProperties((uint16_t)Property::FM_RSQ_SNR_HIGH_THRESHOLD,0,propReply,sizeof(propReply))==Result::InvalidArgument);

    // Fixed HD command layouts and AN649 ranges.
    uint8_t hdReply[256]={0};
    assert(radio.hdGetStationInfo(0,hdReply,sizeof(hdReply))==Result::InvalidArgument);
    assert(radio.hdGetStationInfo(7,hdReply,sizeof(hdReply))==Result::InvalidArgument);
    assert(radio.hdGetStationInfo(1,hdReply,6)==Result::Ok);
    assert(m.command==(uint8_t)Command::HD_GET_STATION_INFO && m.argLength==1u && m.args[0]==1u);
    assert(radio.hdGetPsdDecode(8,0,hdReply,8)==Result::InvalidArgument);
    assert(radio.hdGetPsdDecode(0,7,hdReply,8)==Result::InvalidArgument);
    assert(radio.hdGetPsdDecode(0,0,hdReply,8)==Result::Ok);
    assert(m.command==(uint8_t)Command::HD_GET_PSD_DECODE && m.argLength==2u && m.args[0]==0u && m.args[1]==0u);
    assert(radio.hdPlayAlertTone()==Result::Ok);
    assert(m.command==(uint8_t)Command::HD_PLAY_ALERT_TONE && m.argLength==1u && m.args[0]==0u);
    uint16_t tooManyPorts[65]={0};
    assert(radio.hdSetEnabledPorts(tooManyPorts,65)==Result::InvalidArgument);
    uint16_t ports[2]={0x1234u,0xABCDu};
    assert(radio.hdSetEnabledPorts(ports,2)==Result::Ok);
    assert(m.command==(uint8_t)Command::HD_SET_ENABLED_PORTS && m.argLength==5u && m.args[0]==2u);
    assert(readLe16(m.args+1)==0x1234u && readLe16(m.args+3)==0xABCDu);

    // If the caller requested only the 4-byte status reply, DeviceError must still
    // retain ERR_CMD's fifth-byte reason through a re-read of the preserved reply.
    m.commandError=true; m.errorCode=0x04; uint8_t statusOnly[4]={0};
    assert(radio.executeRaw(0xEE,0,0,statusOnly,sizeof(statusOnly))==Result::DeviceError);
    assert(radio.lastDeviceError()==0x04);
    assert(radio.lastCommandErrorReason()==CommandErrorReason::NotSupported);
    m.errorCode=0x23;
    assert(radio.executeRaw(0xEE,0,0,statusOnly,sizeof(statusOnly))==Result::DeviceError);
    assert(radio.lastDeviceError()==0x23);
    assert(radio.lastCommandErrorReason()==CommandErrorReason::Unknown);
    m.commandError=false;

    // Public READ_OFFSET length must not overflow the logical reply length (data + 4 status bytes).
    assert(radio.readOffset(0,statusOnly,65532u)==Result::InvalidArgument);

    // Status-only diagnostic reads also preserve ERR_CMD reason through a five-byte re-read.
    m.commandError=true; m.errorCode=0x45; Status st;
    assert(radio.readStatus(st)==Result::DeviceError);
    assert(radio.lastDeviceError()==0x45); m.commandError=false;
    return 0;
}
