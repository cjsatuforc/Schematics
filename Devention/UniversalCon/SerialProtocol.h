#ifndef _SERIAL_PROTOCOL_H_
#define _SERIAL_PROTOCOL_H_

#include "Common.h"
#include "utils.h"

#define MAX_RX_BUF  32

class SerialProtocol
{
    
public:
    typedef enum {
        CMD_READY,
        CMD_SET_RFPROTOCOL,     // data u32  : (module << 16 | proto << 8 | option)
        CMD_START_RF,
        CMD_STOP_RF,
        CMD_INJECT_CONTROLS,    // 12ch data : throttle, rudder, elevator, aileron, aux1..8
        CMD_GET_INFO,
        CMD_TEST = 110,
    } CMD_T;

    SerialProtocol();
    ~SerialProtocol();

    void handleRX(void);
    void sendResponse(bool ok, u8 cmd, u8 *data, u8 size);
    void evalCommand(u8 cmd, u8 *data, u8 size);
    void setCallback(u32 (*callback)(u8 cmd, u8 *data, u8 size));


private:
    typedef enum
    {
        STATE_IDLE,
        STATE_HEADER_START,
        STATE_HEADER_M,
        STATE_HEADER_ARROW,
        STATE_HEADER_SIZE,
        STATE_HEADER_CMD
    } STATE_T;
    // 

    // variables
    u8 mRxBuf[MAX_RX_BUF];
    
    u8   mState;
    u8   mOffset;
    u8   mDataSize;
    u8   mCheckSum;
    u8   mCmd;
    u32  (*mCallback)(u8 cmd, u8 *data, u8 size);
};

#endif
