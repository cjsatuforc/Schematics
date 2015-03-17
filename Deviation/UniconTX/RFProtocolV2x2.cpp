#include <SPI.h>
#include "RFProtocolV2x2.h"
#include "utils.h"

u8 RFProtocolV2x2::getCheckSum(u8 *data)
{
    u8 sum = 0;
    for (u8 i = 0; i < MAX_PACKET_SIZE - 1;  ++i) 
        sum += data[i];
    return sum;
}

u8 RFProtocolV2x2::checkStatus()
{
    u8 stat = mDev.readReg(NRF24L01_07_STATUS);

    switch (stat & (BV(NRF24L01_07_TX_DS) | BV(NRF24L01_07_MAX_RT))) {
    case BV(NRF24L01_07_TX_DS):
        return PKT_ACKED;
    case BV(NRF24L01_07_MAX_RT):
        return PKT_TIMEOUT;
    }
    return PKT_PENDING;
}

u8 RFProtocolV2x2::getChannel(CH_T id)
{
    s32 ch = RFProtocol::getControl(id);
    if (ch < CHAN_MIN_VALUE) {
        ch = CHAN_MIN_VALUE;
    } else if (ch > CHAN_MAX_VALUE) {
        ch = CHAN_MAX_VALUE;
    }

    u8 ret =  (u8) (((ch * 0xFF / CHAN_MAX_VALUE) + 0x100) >> 1);
    return ret;
}

void RFProtocolV2x2::getControls(u8* throttle, u8* rudder, u8* elevator, u8* aileron, u8* flags, u16 *led_blink)
{
    // Protocol is registered AETRG, that is
    // Aileron is channel 0, Elevator - 1, Throttle - 2, Rudder - 3
    // Sometimes due to imperfect calibration or mixer settings
    // throttle can be less than CHAN_MIN_VALUE or larger than
    // CHAN_MAX_VALUE. As we have no space here, we hard-limit
    // channels values by min..max range
    u8 a;

    *throttle = getChannel(CH_THROTTLE);    

    a         = getChannel(CH_RUDDER);
    *rudder   = a < 0x80 ? 0x7f - a : a;
    
    a         = getChannel(CH_ELEVATOR);
    *elevator = a < 0x80 ? 0x7f - a : a;

    a        = getChannel(CH_AILERON);
    *aileron = a < 0x80 ? 0x7f - a : a;


    // Channel 5
    // 512 - slow blinking (period 4ms*2*512 ~ 4sec), 64 - fast blinking (4ms*2*64 ~ 0.5sec)
    u16 nNewLedBlinkCtr;
    s32 ch = RFProtocol::getControl(CH_AUX1);
    if (ch == CHAN_MIN_VALUE) {
        nNewLedBlinkCtr = BLINK_COUNT_MAX + 1;
    } else if (ch == CHAN_MAX_VALUE) {
        nNewLedBlinkCtr = BLINK_COUNT_MIN - 1;
    } else {
        nNewLedBlinkCtr = (BLINK_COUNT_MAX+BLINK_COUNT_MIN)/2 -
            ((s32) RFProtocol::getControl(CH_AUX1) * (BLINK_COUNT_MAX-BLINK_COUNT_MIN) / (2*CHAN_MAX_VALUE));
    }
    if (*led_blink != nNewLedBlinkCtr) {
        if (mBindCtr > nNewLedBlinkCtr) 
            mBindCtr = nNewLedBlinkCtr;
        *led_blink = nNewLedBlinkCtr;
    }

    // Channel 6
    if (RFProtocol::getControl(CH_AUX2) <= 0)
        *flags &= ~V2x2_FLAG_FLIP;
    else
        *flags |= V2x2_FLAG_FLIP;

    // Channel 7
    if (RFProtocol::getControl(CH_AUX3) <= 0)
        *flags &= ~V2x2_FLAG_CAMERA;
    else
        *flags |= V2x2_FLAG_CAMERA;

    // Channel 8
    if (RFProtocol::getControl(CH_AUX4) <= 0)
        *flags &= ~V2x2_FLAG_VIDEO;
    else
        *flags |= V2x2_FLAG_VIDEO;
}

void RFProtocolV2x2::sendPacket(u8 bind)
{
    if (bind) {
        mAuxFlag      = V2x2_FLAG_BIND;
        mPacketBuf[0] = 0;
        mPacketBuf[1] = 0;
        mPacketBuf[2] = 0;
        mPacketBuf[3] = 0;
        mPacketBuf[4] = 0;
        mPacketBuf[5] = 0;
        mPacketBuf[6] = 0;
    } else {
        getControls(&mPacketBuf[0], &mPacketBuf[1], &mPacketBuf[2], &mPacketBuf[3], &mAuxFlag, &mLedBlinkCtr);
        // Trims, middle is 0x40
        mPacketBuf[4] = 0x40;   // yaw
        mPacketBuf[5] = 0x40;   // pitch
        mPacketBuf[6] = 0x40;   // roll
    }
        // TX id
    mPacketBuf[7] = mTXID[0];
    mPacketBuf[8] = mTXID[1];
    mPacketBuf[9] = mTXID[2];
    // empty
    mPacketBuf[10] = 0x00;
    mPacketBuf[11] = 0x00;
    mPacketBuf[12] = 0x00;
    mPacketBuf[13] = 0x00;
    //
    mPacketBuf[14] = mAuxFlag;
    mPacketBuf[15] = getCheckSum(mPacketBuf);

    mPacketSent = 0;
    // Each packet is repeated twice on the same
    // channel, hence >> 1
    // We're not strictly repeating them, rather we
    // send new packet on the same frequency, so the
    // receiver gets the freshest command. As receiver
    // hops to a new frequency as soon as valid packet
    // received it does not matter that the packet is
    // not the same one repeated twice - nobody checks this
    u8 rf_ch = mChannelBuf[mCurChan >> 1];
    mCurChan = (mCurChan + 1) & 0x1F;
    mDev.writeReg(NRF24L01_05_RF_CH, rf_ch);
    mDev.flushTx();
    mDev.writePayload(mPacketBuf, sizeof(mPacketBuf));
    ++mPacketCtr;
    mPacketSent = 1;

//    radio.ce(HIGH);
//    delayMicroseconds(15);
    // It saves power to turn off radio after the transmission,
    // so as long as we have pins to do so, it is wise to turn
    // it back.
//    radio.ce(LOW);

    // Check and adjust transmission power. We do this after
    // transmission to not bother with timeout after power
    // settings change -  we have plenty of time until next
    // mPacketBuf.
//    if (!mCurChan && tx_power != Model.tx_power) {
        //Keep transmit power updated
//        tx_power = Model.tx_power;
//        mDev.setPower((tx_power);
//    }
}

void RFProtocolV2x2::initRxTxAddr(void)
{
    u32 lfsr = getControllerID();

    // Pump zero bytes for LFSR to diverge more
    for (u8 i = 0; i < sizeof(lfsr); ++i)
      rand32_r(&lfsr, 0);

    setTxID(lfsr);
    printf(F("ID:%08lx\n"), lfsr);
}

void RFProtocolV2x2::init1(void)
{
    u8 rx_tx_addr[] = {0x66, 0x88, 0x68, 0x68, 0x68};
    u8 rx_p1_addr[] = {0x88, 0x66, 0x86, 0x86, 0x86};
    
    mDev.initialize();

    // 2-bytes CRC, radio off
    mDev.writeReg(NRF24L01_00_CONFIG, BV(NRF24L01_00_EN_CRC) | BV(NRF24L01_00_CRCO)); 
    mDev.writeReg(NRF24L01_01_EN_AA, 0x00);                 // No Auto Acknoledgement
    mDev.writeReg(NRF24L01_02_EN_RXADDR, 0x3F);             // Enable all data pipes
    mDev.writeReg(NRF24L01_03_SETUP_AW, 0x03);              // 5-byte RX/TX address
    mDev.writeReg(NRF24L01_04_SETUP_RETR, 0xFF);            // 4ms retransmit t/o, 15 tries
    mDev.writeReg(NRF24L01_05_RF_CH, 0x08);                 // Channel 8
    mDev.setBitrate(NRF24L01_BR_1M);                        // 1Mbps
    mDev.setPower(TXPOWER_100mW);
    mDev.writeReg(NRF24L01_07_STATUS, 0x70);                // Clear data ready, data sent, and retransmit
//    mDev.writeReg(NRF24L01_08_OBSERVE_TX, 0x00);          // no write bits in this field
//    mDev.writeReg(NRF24L01_00_CD, 0x00);                  // same
    mDev.writeReg(NRF24L01_0C_RX_ADDR_P2, 0xC3);            // LSB byte of pipe 2 receive address
    mDev.writeReg(NRF24L01_0D_RX_ADDR_P3, 0xC4);
    mDev.writeReg(NRF24L01_0E_RX_ADDR_P4, 0xC5);
    mDev.writeReg(NRF24L01_0F_RX_ADDR_P5, 0xC6);
    mDev.writeReg(NRF24L01_11_RX_PW_P0, MAX_PACKET_SIZE);   // bytes of data payload for pipe 1
    mDev.writeReg(NRF24L01_12_RX_PW_P1, MAX_PACKET_SIZE);
    mDev.writeReg(NRF24L01_13_RX_PW_P2, MAX_PACKET_SIZE);
    mDev.writeReg(NRF24L01_14_RX_PW_P3, MAX_PACKET_SIZE);
    mDev.writeReg(NRF24L01_15_RX_PW_P4, MAX_PACKET_SIZE);
    mDev.writeReg(NRF24L01_16_RX_PW_P5, MAX_PACKET_SIZE);
    mDev.writeReg(NRF24L01_17_FIFO_STATUS, 0x00);           // Just in case, no real bits to write here

    mDev.writeRegisterMulti(NRF24L01_0A_RX_ADDR_P0, rx_tx_addr, 5);
    mDev.writeRegisterMulti(NRF24L01_0B_RX_ADDR_P1, rx_p1_addr, 5);
    mDev.writeRegisterMulti(NRF24L01_10_TX_ADDR, rx_tx_addr, 5);

    printf(F("init1 : %ld\n"), millis());
}

void RFProtocolV2x2::init2(void)
{
    mDev.flushTx();
    mDev.setTxRxMode(TX_EN);
    u8 config = BV(NRF24L01_00_EN_CRC) | BV(NRF24L01_00_CRCO) | BV(NRF24L01_00_PWR_UP);
    mDev.writeReg(NRF24L01_00_CONFIG, config);

    mCurChan    = 0;
    mPacketSent = 0;
    printf(F("init2 : %ld\n"), millis());
}

// This is frequency hopping table for V202 protocol
// The table is the first 4 rows of 32 frequency hopping
// patterns, all other rows are derived from the first 4.
// For some reason the protocol avoids channels, dividing
// by 16 and replaces them by subtracting 3 from the channel
// number in this case.
// The pattern is defined by 5 least significant bits of
// sum of 3 bytes comprising TX id
const PROGMEM u8 freq_hopping[][16] = {
 { 0x27, 0x1B, 0x39, 0x28, 0x24, 0x22, 0x2E, 0x36,
   0x19, 0x21, 0x29, 0x14, 0x1E, 0x12, 0x2D, 0x18 }, //  00
 { 0x2E, 0x33, 0x25, 0x38, 0x19, 0x12, 0x18, 0x16,
   0x2A, 0x1C, 0x1F, 0x37, 0x2F, 0x23, 0x34, 0x10 }, //  01
 { 0x11, 0x1A, 0x35, 0x24, 0x28, 0x18, 0x25, 0x2A,
   0x32, 0x2C, 0x14, 0x27, 0x36, 0x34, 0x1C, 0x17 }, //  02
 { 0x22, 0x27, 0x17, 0x39, 0x34, 0x28, 0x2B, 0x1D,
   0x18, 0x2A, 0x21, 0x38, 0x10, 0x26, 0x20, 0x1F }  //  03
};

void RFProtocolV2x2::setTxID(u32 id)
{
    u8 sum;
    mTXID[0] = (id >> 16) & 0xFF;
    mTXID[1] = (id >> 8) & 0xFF;
    mTXID[2] = (id >> 0) & 0xFF;
    sum = mTXID[0] + mTXID[1] + mTXID[2];

    
    const u8 *fh_row = freq_hopping[sum & 0x03];        // Base row is defined by lowest 2 bits
    u8 increment = (sum & 0x1e) >> 2;                   // Higher 3 bits define increment to corresponding row
    
    for (u8 i = 0; i < 16; ++i) {
        u8 val = pgm_read_byte(fh_row[i] + increment);
        mChannelBuf[i] = (val & 0x0f) ? val : val - 3;  // Strange avoidance of channels divisible by 16
    }
}

u16 RFProtocolV2x2::callState(void)
{
    switch (mState) {
    case V202_INIT2:
        init2();
        mState = V202_BIND2;
        return 1;

    case V202_INIT2_NO_BIND:
        init2();
        mState   = V202_DATA;
        return 1;

    case V202_BIND1:
        sendPacket(1);
        if (getChannel(CH_THROTTLE) >= 240) 
            mState = V202_BIND2;
        break;

    case V202_BIND2:
        if (mPacketSent && checkStatus() != PKT_ACKED) {
            return PACKET_CHKTIME;
        }
        sendPacket(1);
        if (--mBindCtr == 0) {
            mState = V202_DATA;
            mBindCtr = mLedBlinkCtr;
            mAuxFlag = 0;
        }
        break;

    case V202_DATA:
        if (mLedBlinkCtr > BLINK_COUNT_MAX) {
            mAuxFlag |= V2x2_FLAG_LED;
        } else if (mLedBlinkCtr < BLINK_COUNT_MIN) {
            mAuxFlag &= ~V2x2_FLAG_LED;
        } else if (--mBindCtr == 0) {
            mBindCtr = mLedBlinkCtr;
            mAuxFlag ^= V2x2_FLAG_LED;
        }
        if (mPacketSent && checkStatus() != PKT_ACKED) {
            return PACKET_CHKTIME;
        }
        sendPacket(0);
        break;
    }
    return PACKET_PERIOD_MS;
}

void RFProtocolV2x2::test(s8 id)
{
}

void RFProtocolV2x2::handleTimer(s8 id)
{
    if (id == mTmrState) {
        u16 time = callState();
        mTmrState = after(time);
    }
}

void RFProtocolV2x2::loop(void)
{
    update();
}

int RFProtocolV2x2::init(void)
{
    mPacketCtr = 0;
    mAuxFlag   = 0;
    mTmrState  = -1;
    mLedBlinkCtr = BLINK_COUNT_MAX;

    init1();
    if (getProtocolOpt() == STARTBIND_YES) {
        mState   = V202_INIT2;
        mBindCtr = MAX_BIND_COUNT;
    } else {
        mState   = V202_INIT2_NO_BIND;
        mBindCtr = BLINK_COUNT;
    }
    initRxTxAddr();
    mTmrState = after(INITIAL_WAIT_MS);
    printf(F("init : %ld\n"), millis());
    return 0;
}

int RFProtocolV2x2::close(void)
{
    mDev.initialize();
    return (mDev.reset() ? 1L : -1L);
}

int RFProtocolV2x2::reset(void)
{
    return close();
}

int RFProtocolV2x2::getChannels(void)
{
    return 8;
}

int RFProtocolV2x2::setPower(int power)
{
    mDev.setPower(power);
    return 0;
}

int RFProtocolV2x2::getInfo(s8 id, u8 *data)
{
    u8 size = 0;
    switch (id) {
        case INFO_STATE:
            *data = mState;
            size = 1;
            break;

        case INFO_CHANNEL:
            *data = mChannelBuf[mCurChan];
            size = 1;
            break;

        case INFO_PACKET_CTR:
            size = sizeof(mPacketCtr);
            *((u32*)data) = mPacketCtr;
            break;
    }
    return size;
}
