#include "../Si468x.h"
#include <cassert>
#include <cstring>
using namespace si468x;

struct Capture {
    DabServiceListHeader header;
    DabServiceEntry service;
    DabComponentEntry component;
    int headers, services, components;
    Capture() : headers(0), services(0), components(0) { std::memset(&header,0,sizeof(header)); std::memset(&service,0,sizeof(service)); std::memset(&component,0,sizeof(component)); }
};
static void onHeader(void* c,const DabServiceListHeader& x){ Capture* p=(Capture*)c; p->header=x; ++p->headers; }
static void onService(void* c,const DabServiceEntry& x){ Capture* p=(Capture*)c; p->service=x; ++p->services; }
static void onComponent(void* c,const DabComponentEntry& x){ Capture* p=(Capture*)c; p->component=x; ++p->components; }

int main(){
    uint8_t rds[20]={0x80,0,0,0,0x1B,0x1B,0x25,0,0x34,0x12,1,0, 'A','B','C','D','E','F','G','H'};
    FmRdsGroup g; assert(Si468x::parseFmRdsStatus(rds,20,g)==Result::Ok);
    assert(g.tpPtyInterrupt && g.piInterrupt && g.syncInterrupt && g.fifoInterrupt);
    assert(g.tpPtyValid && g.piValid && g.sync && g.fifoLost);
    assert(g.pi==0x1234); assert(g.fifoUsed==1); assert(g.block[0]==0x4241);

    uint8_t fmrsq[17]={0x80,0,0,0,0x0F,0xB3,0xBA,0x27,0xFE,0xF0,12,33,0x34,0x12,0,50,40};
    FmRsqStatus fr; assert(Si468x::parseFmRsqStatus(fmrsq,sizeof(fmrsq),fr)==Result::Ok);
    assert(fr.snrHighInterrupt && fr.snrLowInterrupt && fr.rssiHighInterrupt && fr.rssiLowInterrupt);
    assert(fr.bandLimit && fr.hdDetected && fr.filteredHdDetected && fr.afcRail && fr.valid);

    uint8_t fmacf[9]={0x80,0,0,0,0x07,0x77,0x1F,100,0xE4};
    FmAcfStatus fa; assert(Si468x::parseFmAcfStatus(fmacf,sizeof(fmacf),fa)==Result::Ok);
    assert(fa.blendInterrupt && fa.highCutInterrupt && fa.softMuteInterrupt);
    assert(fa.blendConverged && fa.highCutConverged && fa.softMuteConverged);
    assert(fa.blendActive && fa.highCutActive && fa.softMuteActive);

    uint8_t amrsq[17]={0x80,0,0,0,0x0F,0xB3,0x40,0x06,0,20,10,80,0,0,0,40,30};
    AmRsqStatus ar; assert(Si468x::parseAmRsqStatus(amrsq,sizeof(amrsq),ar)==Result::Ok);
    assert(ar.snrHighInterrupt && ar.snrLowInterrupt && ar.rssiHighInterrupt && ar.rssiLowInterrupt);

    // Fixed HD protocol replies are decoded by the core, while external SIS/PSD
    // payload semantics deliberately remain raw.
    uint8_t hd[23]={0x80,0,0,0,0xEF,0xEF,0x95,44,0x1D,0x83,0x03,0x80,
                    1,0,0,0, 2,0,0,0, 7,5,13};
    HdDigradStatus hdr; assert(Si468x::parseHdDigradStatus(hd,sizeof(hd),hdr)==Result::Ok);
    assert(hdr.hdLogoInterrupt && hdr.sourceAnalogInterrupt && hdr.sourceDigitalInterrupt);
    assert(hdr.audioAcquisitionInterrupt && hdr.acquisitionInterrupt && hdr.cdnrHighInterrupt && hdr.cdnrLowInterrupt);
    assert(hdr.hdLogo && hdr.sourceAnalog && hdr.sourceDigital && hdr.audioAcquired && hdr.acquired);
    assert(hdr.blendControl==2u && hdr.digitalAudioQuality==0x15u && hdr.cdnr==44u);
    assert(hdr.txGain==-3);
    assert(hdr.audioProgramsAvailable==0x83u && hdr.audioProgramsPlaying==0x03u && hdr.audioConditionalAccess==0x80u);
    assert(hdr.coreAudioErrors==1u && hdr.enhancedAudioErrors==2u && hdr.pty==7u && hdr.primaryServiceMode==5u && hdr.codecMode==13u);

    uint8_t hev[18]={0x80,0,0,0,0xDF,0xCF,0x34,0x12,0x78,0x56,0x0F,0x3F,0x7F,0xFF,0x07,9,10,11};
    HdEventStatus he; assert(Si468x::parseHdEventStatus(hev,sizeof(hev),he)==Result::Ok);
    assert(he.dataInfoInterrupt() && he.audioInfoInterrupt() && he.alertInterrupt() && he.psdInterrupt() && he.sisInterrupt());
    assert(he.dataServiceListInterrupt() && he.audioServiceListInterrupt());
    assert(he.dataInfoAvailable() && he.audioInfoAvailable() && he.psdAvailable() && he.sisAvailable());
    assert(he.dataServiceListAvailable() && he.audioServiceListAvailable());
    assert(he.audioServiceListVersion==0x1234u && he.dataServiceListVersion==0x5678u);
    assert(he.alertFrameCount==9u && he.alertMessageId==10u && he.alertCrc7==11u);

    uint8_t ber[44]={0x80,0,0,0};
    for (uint8_t i=0;i<10;++i) writeLe32(ber+4u+(size_t)i*4u,(uint32_t)i+1u);
    HdBerInfo hb; assert(Si468x::parseHdBerInfo(ber,sizeof(ber),hb)==Result::Ok);
    assert(hb.pidsBlockErrors==1u && hb.pidsBlocksTested==2u && hb.pidsBitErrors==3u && hb.pidsBitsTested==4u);
    assert(hb.p3BitErrors==5u && hb.p3BitsTested==6u && hb.p2BitErrors==7u && hb.p2BitsTested==8u && hb.p1BitErrors==9u && hb.p1BitsTested==10u);

    uint8_t dab[23]={0x80,0,0,0,0x1F,0x1D,0xC0,18,99,17,2,0,0x80,0xD6,0x02,0,3,0xFE,0x20,0,0x34,0x12,7};
    DabDigradStatus dr; assert(Si468x::parseDabDigradStatus(dab,sizeof(dab),dr)==Result::Ok);
    assert(dr.hardMuteInterrupt && dr.ficErrorInterrupt && dr.acquisitionInterrupt && dr.rssiHighInterrupt && dr.rssiLowInterrupt);
    assert(dr.hardMute && dr.ficError && dr.acquired && dr.valid);

    uint8_t ds[24]={0x80,0,0,0,7,2,0,0x80, 1,2,3,4, 5,6,7,8, 0,0, 4,0, 2,0, 3,0};
    DsrvHeader h; assert(Si468x::parseDsrvHeader(ds,24,h)==Result::Ok); assert(h.dataSource==2); assert(h.dataReady() && h.overflow() && h.physicalError()); assert(h.serviceId==0x04030201UL); assert(h.byteCount==4);

    // DAB service-list streaming test. Feed deliberately odd chunk sizes to
    // verify that no full-list RAM buffer is required.
    uint8_t list[8+24+4]; std::memset(list,0,sizeof(list));
    writeLe16(list+0,34);          // list payload size for this synthetic example
    writeLe16(list+2,7);           // list version
    list[4]=1;                     // one service
    writeLe32(list+8,0x11223344UL);
    list[12]=0x0A;                 // PTY=5, programme service, no linking
    list[13]=0x01;                 // one component
    list[14]=0x00;                 // charset 0
    const char label[16]={'T','e','s','t',' ','S','e','r','v','i','c','e',' ',' ',' ',' '};
    std::memcpy(list+16,label,16);
    // Exact four-byte component entry: raw field 0xE123 (TMId=3, DG=1,
    // SCId=0x123), component-info byte 0x45, valid-flags byte 0x01.
    list[32]=0x23; list[33]=0xE1; list[34]=0x45; list[35]=0x01;

    Capture cap; DabServiceListSink sink; sink.context=&cap; sink.onHeader=onHeader; sink.onService=onService; sink.onComponent=onComponent;
    DabServiceListParser parser; parser.setSink(sink);
    assert(parser.feed(list,3)==Result::Ok);
    assert(parser.feed(list+3,11)==Result::Ok);
    assert(parser.feed(list+14,sizeof(list)-14)==Result::Ok);
    assert(parser.complete()); assert(!parser.error());
    assert(cap.headers==1 && cap.services==1 && cap.components==1);
    assert(cap.service.serviceId==0x11223344UL);
    assert(cap.component.componentId==0x0145E123UL);
    assert(cap.component.rawComponentField==0xE123u);
    assert(cap.component.transportModeId==3u);
    assert(cap.component.dataGroupFlag);
    assert(cap.component.componentReference==0x0123u);
    assert(cap.component.componentType==(0x45u>>2));
    assert(cap.component.conditionalAccess);
    assert(cap.component.userApplicationInfoValid);

    // AN649 defines M < 15 components per service; reject the reserved value 15.
    uint8_t invalidList[8+24]; std::memset(invalidList,0,sizeof(invalidList));
    writeLe16(invalidList,30); invalidList[4]=1; invalidList[13]=0x0F;
    DabServiceListParser bad; assert(bad.feed(invalidList,sizeof(invalidList))==Result::MalformedReply); assert(bad.error());
    return 0;
}
