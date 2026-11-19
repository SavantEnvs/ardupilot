/*
 * RC receiver protocols: every decoder behind AP_RCProtocol (SBUS, DSM, SUMD, SRXL, SRXL2, CRSF,
 * ST24, FPort, FPort2, IBUS, GHST, PPMSum, ...), fed through the public AP_RCProtocol API the way
 * the HAL's RC input driver feeds it.
 *
 * Input layout:
 *   byte 0      bit 7 set: pulse mode, else byte mode; bits 0-2: UART baud rate (byte mode)
 *   byte mode:  records of [len][len bytes]; the receiver line is idle for 5 ms between records
 *               (a frame gap) and each byte takes its real time on the wire
 *   pulse mode: little-endian uint16 pairs (high width, low width) in microseconds
 * The HAL clock is driven from the input (stop_clock), so frame timing is reproducible.
 */
#include "fuzz_common.h"
#include <AP_RCProtocol/AP_RCProtocol.h>
#include <AP_Math/AP_Math.h>
#include <GCS_MAVLink/GCS_Dummy.h>
#include <RC_Channel/RC_Channel.h>

const AP_HAL::HAL& hal = AP_HAL::get_HAL();

static GCS_Dummy _gcs;

// the decoders consult the vehicle's RC channel options (rc()), so provide the channel set
class RC_Channel_Fuzz : public RC_Channel {};
class RC_Channels_Fuzz : public RC_Channels {
public:
    const RC_Channel *channel(uint8_t chan) const override { return &obj_channels[chan]; }
    RC_Channel *channel(uint8_t chan) override { return &obj_channels[chan]; }
    RC_Channel_Fuzz obj_channels[NUM_RC_CHANNELS];
private:
    int8_t flight_mode_channel_number() const override { return -1; }
};
#define RC_CHANNELS_SUBCLASS RC_Channels_Fuzz
#define RC_CHANNEL_SUBCLASS RC_Channel_Fuzz
#include <RC_Channel/RC_Channels_VarInfo.h>
static RC_Channels_Fuzz _rc;

static uint64_t fuzz_now_us;

static void advance_clock(uint32_t usec)
{
    fuzz_now_us += usec;
    hal.scheduler->stop_clock(fuzz_now_us);
}

static void drain_input(AP_RCProtocol &rc)
{
    if (rc.new_input()) {
        uint16_t pwm[32];
        const uint8_t n = MIN(rc.num_channels(), (uint8_t)ARRAY_SIZE(pwm));
        rc.read(pwm, n);
        ::printf("%s: %u channels, ch1=%u\n", rc.detected_protocol_name(), (unsigned)n, n > 0 ? (unsigned)pwm[0] : 0U);
    }
}

static void fuzz_one(const std::vector<uint8_t> &d)
{
    if (d.empty()) {
        return;
    }
    static const uint32_t bauds[8] = { 100000, 115200, 416666, 420000, 57600, 19200, 9600, 230400 };
    AP_RCProtocol &rc = AP::RC();
    fuzz_now_us = AP_HAL::micros64() + 1000000;
    hal.scheduler->stop_clock(fuzz_now_us);

    const uint8_t mode = d[0];
    size_t i = 1;
    if (mode & 0x80) {
        while (i + 4 <= d.size()) {
            const uint32_t w0 = d[i] | (d[i+1] << 8);
            const uint32_t w1 = d[i+2] | (d[i+3] << 8);
            i += 4;
            advance_clock(w0 + w1);
            rc.process_pulse(w0, w1);
            drain_input(rc);
        }
        return;
    }
    const uint32_t baud = bauds[mode & 7];
    const uint32_t byte_us = 11000000UL / baud + 1;
    while (i < d.size()) {
        const uint8_t len = d[i++];
        advance_clock(5000);
        for (uint8_t k = 0; k < len && i < d.size(); k++) {
            advance_clock(byte_us);
            rc.process_byte(d[i++], baud);
            drain_input(rc);
        }
    }
}
