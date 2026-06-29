#ifndef TC1_H
#define TC1_H

/*
 * TC1 — Train Controller 1  (CS452/652 Spring 2026)
 *
 * Controls train address 1 on the Marklin track.
 * The controller:
 *   1. Locates the train by polling sensors.
 *   2. Tracks position and estimates velocity sensor-to-sensor.
 *   3. Accepts a "goto <sensor>" command from the shell.
 *   4. Routes the train (BFS, Track A) and sets turnouts.
 *   5. Sends a speed-0 command timed to stop at the target.
 *
 * Architecture:
 *   tc1_entry()         — main server task (RegisterAs "TC1")
 *   tc1_sensor_notif()  — polls sensors, sends TC1_MSG_SENSOR every 100 ms
 *   tc1_display_notif() — sends TC1_MSG_TICK every 1 s for status display
 *
 * State frames:
 *   OSC 452 (ESC]452;{JSON}BEL) emitted to console so screen_bridge.py
 *   can forward train state to /state-ws clients.
 */

#define TC1_SERVER_NAME      "TC1"
#define TC1_PRIORITY         10    /* lower = runs sooner; between Marklin(6) and shell(20) */
#define TC1_NOTIF_PRIORITY   9     /* sensor+display notifiers: just above TC1 */
#define TC1_TRAIN_ADDR       1
#define TC1_DEFAULT_SPEED    8
#define TC1_POLL_TICKS       10    /* 100 ms at 10 ms/tick */

/* Calibration: estimated velocity (mm/s) at each speed 0-14.
 * Speed 0 = stopped.  Tune these with offline measurements. */
#define TC1_VEL_TABLE \
    { 0, 80, 160, 230, 310, 380, 440, 500, 560, 610, 660, 710, 760, 810, 900 }

/* Stopping distance (mm) from speed 0 (deceleration ramp) at each speed. */
#define TC1_STOP_TABLE \
    { 0, 80, 150, 220, 300, 370, 430, 490, 550, 600, 650, 700, 750, 800, 900 }

/* ---- IPC message types ------------------------------------------- */

#define TC1_MSG_SENSOR  1   /* from sensor notifier */
#define TC1_MSG_TICK    2   /* from display notifier (1-second heartbeat) */
#define TC1_MSG_GOTO    3   /* from shell: goto <sensor> */
#define TC1_MSG_SPEED   4   /* from shell: set speed */
#define TC1_MSG_STOP    5   /* from shell: stop immediately */

#include "../layer3-services/marklin.h"

typedef struct {
    int type;
    union {
        MarklinkSensors sensors;   /* TC1_MSG_SENSOR */
        char dest[12];             /* TC1_MSG_GOTO: sensor name e.g. "A5" */
        int  speed;                /* TC1_MSG_SPEED */
    };
} TC1Msg;

void tc1_entry(void);           /* main controller task */
void tc1_sensor_notif(void);    /* sensor polling notifier */
void tc1_display_notif(void);   /* 1-second display heartbeat */

int  TC1Tid(void);              /* -1 if not running */
int  TC1Goto(const char *sensor_name);   /* send goto command */
int  TC1Speed(int speed);                /* change speed */

#endif /* TC1_H */
