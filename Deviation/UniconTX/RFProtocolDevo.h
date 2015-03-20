#ifndef _PROTOCOL_DEVO_H
#define _PROTOCOL_DEVO_H

#include "DeviceCYRF6936.h"
#include "RFProtocol.h"
#include "Timer.h"

class RFProtocolDevo : public RFProtocol
{
#define PKTS_PER_CHANNEL        4
#define TELEMETRY_ENABLE     0x30
#define MAX_PACKET_SIZE        16
#define MAX_BIND_COUNT     0x1388
#define ADDR_BUF_SIZE           5
#define MFG_ID_SIZE             6

#define PACKET_PERIOD_uS     1200
#define INITIAL_WAIT_uS      2400

#define MAX_RF_CHANNELS         5

enum PktState {
    DEVO_BIND,
    DEVO_BIND_SENDCH,
    DEVO_BOUND,
    DEVO_BOUND_1,
    DEVO_BOUND_2,
    DEVO_BOUND_3,
    DEVO_BOUND_4,
    DEVO_BOUND_5,
    DEVO_BOUND_6,
    DEVO_BOUND_7,
    DEVO_BOUND_8,
    DEVO_BOUND_9,
    DEVO_BOUND_10,
};

public:
    RFProtocolDevo(u32 id):RFProtocol(id) { }
    ~RFProtocolDevo() { close(); }

// for protocol
    virtual int  init(void);
    virtual int  close(void);
    virtual int  reset(void);
    virtual int  getChannels(void);
    virtual int  getInfo(s8 id, u8 *data);
    virtual void test(s8 id);
    virtual u16  callState(void);

private:
    void init1(void);
    void buildScramblePacket(void);
    void addPacketSuffix(void);
    void buildBeaconPacket(int upper);
    void buildBindPacket(void);
    void buildDataPacket(void);
    s32  convFloatStr2Int(u8 *ptr);
    void parseTelemetryPacket(u8 *mPacketBuf);
    void setBoundSOPCodes(void);
    void setRadioChannels(void);
    void buildPacket(void);
    

// variables
    DeviceCYRF6936  mDev;
    u16  mBindCtr;
    u8   mRFChanBufs[MAX_RF_CHANNELS];
    u8   mPacketBuf[MAX_PACKET_SIZE];
    u8   mRxTxAddrBuf[ADDR_BUF_SIZE];
    u8   mMfgIDBuf[MFG_ID_SIZE];

    u8   mPacketCtr;    
    u8   mConChanIdx;
    u8   mConChanCnt;
    u8   mPacketSize;
    u8   mState;
    u8   mTxState;

    u8 mBoolFixedID;
    u8 failsafe_pkt;
    u32 mFixedID;
    u8 *mCurRFChPtr;

protected:

};

#endif