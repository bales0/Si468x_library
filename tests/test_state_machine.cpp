#include "../Si468x.h"

#include <cassert>
#include <cstdint>
#include <cstring>

using namespace si468x;

struct MockTransport {
    uint32_t now;
    uint32_t writeCount;
    uint32_t readCount;
    uint32_t statusReadCount;
    uint32_t payloadReadCount;
    bool cts;
    bool commandError;
    bool failRead;
    uint8_t errorReason;
    uint8_t payloadByte;

    MockTransport()
        : now(0), writeCount(0), readCount(0), statusReadCount(0),
          payloadReadCount(0), cts(false), commandError(false),
          failRead(false), errorReason(0), payloadByte(0x5A) {}
};

static uint32_t mockTime(void* context) {
    return static_cast<MockTransport*>(context)->now;
}

static bool mockWrite(void* context, uint8_t, const uint8_t*, uint16_t) {
    ++static_cast<MockTransport*>(context)->writeCount;
    return true;
}

static bool mockRead(void* context, uint8_t* destination, uint16_t length) {
    MockTransport* mock=static_cast<MockTransport*>(context);
    ++mock->readCount;
    if (length==4) ++mock->statusReadCount;
    else ++mock->payloadReadCount;
    if (mock->failRead) return false;
    std::memset(destination,0,length);
    if (length) destination[0]=static_cast<uint8_t>(
        (mock->cts ? 0x80u : 0u) | (mock->commandError ? 0x40u : 0u));
    if (length>4) destination[4]=mock->commandError ? mock->errorReason : mock->payloadByte;
    return true;
}

static HostInterface hostFor(MockTransport& mock, bool withTimer=true) {
    HostInterface host;
    host.context=&mock;
    host.writeCommand=mockWrite;
    host.readReply=mockRead;
    host.timeUs=withTimer ? mockTime : 0;
    return host;
}

static void testResultValues() {
    static_assert(static_cast<int>(Result::Ok)==0,"Result::Ok changed");
    static_assert(static_cast<int>(Result::Pending)==1,"Result::Pending changed");
    static_assert(static_cast<int>(Result::Busy)==2,"Result::Busy changed");
    static_assert(static_cast<int>(Result::InvalidArgument)==-1,"Result::InvalidArgument changed");
    static_assert(static_cast<int>(Result::NoTransport)==-2,"Result::NoTransport changed");
    static_assert(static_cast<int>(Result::NoTimer)==-3,"Result::NoTimer changed");
    static_assert(static_cast<int>(Result::Timeout)==-4,"Result::Timeout changed");
    static_assert(static_cast<int>(Result::TransportError)==-5,"Result::TransportError changed");
    static_assert(static_cast<int>(Result::DeviceError)==-6,"Result::DeviceError changed");
    static_assert(static_cast<int>(Result::BufferTooSmall)==-7,"Result::BufferTooSmall changed");
    static_assert(static_cast<int>(Result::Unsupported)==-8,"Result::Unsupported changed");
    static_assert(static_cast<int>(Result::MalformedReply)==-9,"Result::MalformedReply changed");
    static_assert(static_cast<int>(Result::EndOfData)==-10,"Result::EndOfData changed");
    static_assert(static_cast<int>(Result::Aborted)==-11,"Unexpected Result::Aborted value");
}

static void testAsyncBusyCompletionAndReplyOwnership() {
    MockTransport mock;
    mock.now=100;
    Si468x radio(hostFor(mock));
    uint8_t reply[5]={0};
    assert(radio.startCommand(Command::GET_SYS_STATE,0,0,reply,sizeof(reply),5000)==Result::Pending);
    assert(radio.busy());
    assert(radio.startCommand(Command::GET_PART_INFO,0,0)==Result::Busy);
    assert(mock.writeCount==1);
    assert(radio.service()==Result::Pending);
    assert(reply[4]==0);
    mock.cts=true;
    radio.notifyInterrupt();
    mock.now=200;
    assert(radio.service()==Result::Ok);
    assert(!radio.busy());
    assert(reply[4]==mock.payloadByte);
}

static void testAbortWaitAndIdle() {
    MockTransport mock;
    mock.now=1000;
    Si468x radio(hostFor(mock));
    uint8_t reply[5]={0};
    assert(radio.startCommand(Command::GET_SYS_STATE,0,0,reply,sizeof(reply))==Result::Pending);
    radio.notifyInterrupt();
    const uint32_t readsBefore=mock.readCount;
    const uint32_t writesBefore=mock.writeCount;
    assert(radio.abortCommand()==Result::Aborted);
    assert(radio.lastResult()==Result::Aborted);
    assert(!radio.busy());
    assert(mock.readCount==readsBefore && mock.writeCount==writesBefore);

    // The old IRQ was discarded, so an immediate idle service does not touch
    // the bus. A new command can start and complete on the first attempt.
    assert(radio.service()==Result::Ok);
    assert(mock.readCount==readsBefore);
    mock.cts=true;
    assert(radio.startCommand(Command::GET_PART_INFO,0,0)==Result::Pending);
    assert(radio.service()==Result::Ok);

    // Blocking wrappers use the same state machine and remain usable after an
    // explicit cancellation.
    mock.cts=false;
    assert(radio.startCommand(Command::GET_SYS_STATE,0,0)==Result::Pending);
    assert(radio.abortCommand()==Result::Aborted);
    mock.cts=true;
    assert(radio.executeCommand(Command::GET_PART_INFO,0,0)==Result::Ok);

    const uint32_t idleReads=mock.readCount;
    const uint32_t idleWrites=mock.writeCount;
    assert(radio.abortCommand()==Result::Ok);
    assert(!radio.busy());
    assert(mock.readCount==idleReads && mock.writeCount==idleWrites);
}

static void testTimeoutAndLateFinalCheck() {
    {
        MockTransport mock;
        mock.now=10;
        Si468x radio(hostFor(mock));
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0,0,0,100)==Result::Pending);
        mock.now=110;
        assert(radio.service()==Result::Timeout);
        assert(!radio.busy());
        assert(mock.statusReadCount==1);
        assert(radio.lastDeadlineLatenessUs()==0);
    }
    {
        MockTransport mock;
        mock.now=100;
        Si468x radio(hostFor(mock));
        uint8_t reply[5]={0};
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0,reply,sizeof(reply),100)==Result::Pending);
        mock.now=275;
        mock.cts=true;
        assert(radio.service()==Result::Ok);
        assert(!radio.busy());
        assert(mock.statusReadCount==1);
        assert(mock.payloadReadCount==1);
        assert(radio.lastDeadlineLatenessUs()==75);
        assert(radio.lastServiceGapUs()==175);
    }
    {
        MockTransport mock;
        mock.now=100;
        Si468x radio(hostFor(mock));
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0,0,0,100)==Result::Pending);
        mock.now=250;
        mock.failRead=true;
        assert(radio.service()==Result::TransportError);
        assert(!radio.busy());
        assert(mock.statusReadCount==1);
    }
}

static void testInterruptPollingNoTimerAndErrorReason() {
    {
        MockTransport mock;
        mock.now=100;
        Si468x radio(hostFor(mock));
        radio.setCtsPollIntervalUs(1000);
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0)==Result::Pending);
        assert(radio.service()==Result::Pending);
        const uint32_t reads=mock.readCount;
        mock.now=200;
        assert(radio.service()==Result::Pending);
        assert(mock.readCount==reads);
        mock.cts=true;
        radio.notifyInterrupt();
        assert(radio.service()==Result::Ok);
        assert(mock.readCount==reads+1);
    }
    {
        MockTransport mock;
        Si468x radio(hostFor(mock,false));
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0)==Result::Pending);
        assert(radio.service()==Result::Pending);
        mock.cts=true;
        assert(radio.service()==Result::Ok);
        assert(mock.statusReadCount==2);
    }
    {
        MockTransport mock;
        mock.cts=true;
        mock.commandError=true;
        mock.errorReason=0x04;
        Si468x radio(hostFor(mock));
        uint8_t reply[4]={0};
        assert(radio.startCommand(Command::GET_SYS_STATE,0,0,reply,sizeof(reply))==Result::Pending);
        assert(radio.service()==Result::DeviceError);
        assert(radio.lastDeviceError()==0x04);
        assert(radio.lastCommandErrorReason()==CommandErrorReason::NotSupported);
    }
}

static void testTimerWraparound() {
    MockTransport mock;
    mock.now=0xFFFFFFF0u;
    Si468x radio(hostFor(mock));
    radio.setCtsPollIntervalUs(0);
    assert(radio.startCommand(Command::GET_SYS_STATE,0,0,0,0,32)==Result::Pending);
    mock.now=0x0000000Fu;
    assert(radio.service()==Result::Pending);
    mock.now=0x00000010u;
    assert(radio.service()==Result::Timeout);
    assert(!radio.busy());
}

int main() {
    testResultValues();
    testAsyncBusyCompletionAndReplyOwnership();
    testAbortWaitAndIdle();
    testTimeoutAndLateFinalCheck();
    testInterruptPollingNoTimerAndErrorReason();
    testTimerWraparound();
    return 0;
}
