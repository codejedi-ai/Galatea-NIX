#ifndef MARKLIN_H
#define MARKLIN_H

/*
 * Marklin train-control server (Layer 3 service).
 *
 * Implements the binary Marklin Digital protocol on two hardware targets:
 *
 *   QEMU demo  (MARKLIN_HW_UART3 = 0):  AUX mini-UART at 115200 baud → vhw.py mock.
 *   Waterloo Pi hat (MARKLIN_HW_UART3 = 1):  PL011 UART3 at 2400 baud → real Marklin.
 *
 * Binary protocol summary (same on both targets; vhw.py understands it too):
 *
 *   Train speed:  TX [speed 0-15][address]         (2 bytes, no response)
 *   Switch throw: TX [0x20|dir][address][0x00]      (3 bytes, no response)
 *   Sensor query: TX [0x80|N]                        (1 byte, N=1-5 modules)
 *                 RX N×2 bytes of S88 sensor data   (bulk, one round-trip)
 *   (Also accepts 0xC0|module for single-module reads; vhw.py handles both.)
 *
 * Speed encoding: 0 = stop, 1-14 = speeds, 15 = emergency stop.
 * Switch dir:     MARKLIN_SW_STRAIGHT (0x01) or MARKLIN_SW_CURVED (0x02).
 *
 * Sensor data: 5 modules (A-E), 16 sensors per module = 80 sensors total.
 * For each module's 2 response bytes, bit 7 of byte 0 = sensor 1,
 * bit 0 of byte 1 = sensor 16.
 */

#define MARKLIN_SERVER_NAME       "MarklinServer"
#define MARKLIN_SERVER_PRIORITY   6
#define MARKLIN_SERVER_NOTIF_PRI  4

#define MARKLIN_MODULES     5     /* A through E */
#define MARKLIN_SENS_PER_M 16     /* sensors per module */
#define MARKLIN_TOTAL_SENS (MARKLIN_MODULES * MARKLIN_SENS_PER_M)

/* Wire-protocol constants */
#define MARKLIN_SPEED_ESTOP   15
#define MARKLIN_SW_STRAIGHT   0x01
#define MARKLIN_SW_CURVED     0x02
#define MARKLIN_SENSOR_BASE   0x80   /* 0x80|N = read N modules, get N*2 bytes */
#define MARKLIN_SW_BASE       0x20   /* OR with dir byte */

/* ---- IPC message types ---- */
typedef enum {
    MARKLIN_MSG_SET_SPEED    = 1,   /* client: set train speed */
    MARKLIN_MSG_SET_SWITCH   = 2,   /* client: throw a turnout */
    MARKLIN_MSG_POLL_SENSORS = 3,   /* client: fetch all sensor bytes */
} MarklinkMsgType;

/* IPC request from client to server */
typedef struct {
    int type;
    int addr;    /* train or switch address */
    int speed;   /* SET_SPEED: 0-15 */
    int dir;     /* SET_SWITCH: MARKLIN_SW_STRAIGHT or MARKLIN_SW_CURVED */
} MarklinkMsg;

/* Raw sensor snapshot: 5 modules × 2 bytes = 10 bytes */
typedef struct {
    unsigned char data[MARKLIN_MODULES * 2];
} MarklinkSensors;

void marklin_server_entry(void);

int MarklinkServerTid(void);
void MarklinkSetSpeed(int train_addr, int speed);
void MarklinkSetSwitch(int sw_addr, int dir);
MarklinkSensors MarklinkPollSensors(void);

/* Decode one sensor from a snapshot.  module: 1-5, sensor: 1-16. */
static inline int marklin_sens_get(const MarklinkSensors *s, int module, int sensor)
{
    int bidx = (module - 1) * 2 + (sensor - 1) / 8;
    int bit  = 7 - ((sensor - 1) % 8);
    return (s->data[bidx] >> bit) & 1;
}

/* Fill out[4] with the sensor name, e.g. "A1" or "C12". */
static inline void marklin_sens_name(int module, int sensor, char out[4])
{
    int n = 0;
    out[n++] = (char)('A' + (module - 1));
    if (sensor >= 10) out[n++] = (char)('0' + sensor / 10);
    out[n++] = (char)('0' + sensor % 10);
    out[n] = 0;
}

#endif /* MARKLIN_H */
