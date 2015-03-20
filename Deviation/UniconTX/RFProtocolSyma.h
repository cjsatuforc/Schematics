#ifndef _PROTOCOL_SYMA_H_
#define _PROTOCOL_SYMA_H_

#include "DeviceNRF24L01.h"
#include "RFProtocol.h"
#include "Timer.h"

class RFProtocolSyma : public RFProtocol
{
#define PAYLOADSIZE         10  // receive data pipes set to this size, but unused
#define MAX_PACKET_SIZE     16  // X11,X12,X5C-1 10-byte, X5C 16-byte
#define MAX_BIND_COUNT     345

#define PACKET_PERIOD_uS  4000
#define INITIAL_WAIT_uS    500
#define FIRST_PACKET_uS  12000

#define ADDR_BUF_SIZE        5
#define MAX_RF_CHANNELS     17

#define FLAG_FLIP         0x01
#define FLAG_VIDEO        0x02
#define FLAG_PICTURE      0x04

enum {
    SYMAX_INIT1 = 0,
    SYMAX_BIND2,
    SYMAX_BIND3,
    SYMAX_DATA  = 0x10
};

enum {
    FORMAT_OTHERS  = 0,
    FORMAT_X5C_X2  = 1,
};

public:
    RFProtocolSyma(u32 id):RFProtocol(id) { }
    ~RFProtocolSyma() { close(); }

// for protocol
    virtual int  init(void);
    virtual int  close(void);
    virtual int  reset(void);
    virtual int  getChannels(void);
    virtual int  getInfo(s8 id, u8 *data);
    virtual void test(s8 id);
    virtual u16  callState(void);

private:
    u8   getCheckSum(u8 *data);
    u8   checkStatus(void);
    u8   getChannel(CH_T id);
    void getControls(u8* throttle, u8* rudder, u8* elevator, u8* aileron, u8* flags);
    void buildPacketX5C(u8 bind);
    void buildPacket(u8 bind);
    void sendPacket(u8 bind);
    void initRxTxAddr(void);
    void init1(void);
    void init2(void);
    void init3(void);
    void setRFChannel(u8 address);

// variables
    DeviceNRF24L01  mDev;
    u32  mPacketCtr;
    u16  mBindCtr;
    u8   mRFChanBufs[MAX_RF_CHANNELS];
    u8   mPacketBuf[MAX_PACKET_SIZE];
    u8   mRxTxAddrBuf[ADDR_BUF_SIZE];
    
    u8   mCurChan;
    u8   mChannelCnt;
    u8   mPacketSize;
    u8   mAuxFlag;
    u8   mState;

protected:

};

#endif
