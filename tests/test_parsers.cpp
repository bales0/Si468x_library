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
    uint8_t rds[20]={0x80,0,0,0,0,0x1A,0x25,0,0x34,0x12,1,0, 'A','B','C','D','E','F','G','H'};
    FmRdsGroup g; assert(Si468x::parseFmRdsStatus(rds,20,g)==Result::Ok); assert(g.pi==0x1234); assert(g.fifoUsed==1); assert(g.block[0]==0x4241);

    uint8_t ds[24]={0x80,0,0,0,1,2,0,0x80, 1,2,3,4, 5,6,7,8, 0,0, 4,0, 2,0, 3,0};
    DsrvHeader h; assert(Si468x::parseDsrvHeader(ds,24,h)==Result::Ok); assert(h.dataSource==2); assert(h.serviceId==0x04030201UL); assert(h.byteCount==4);

    uint8_t dls[]={0x80,0x10,'H','i'}; DlsFrame f; assert(Si468x::parseDlsPayload(dls,4,f)==Result::Ok); assert(f.isMessage()); assert(f.charset==1); assert(f.bodyLength==2);

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
    return 0;
}
