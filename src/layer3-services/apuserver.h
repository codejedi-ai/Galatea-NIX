#ifndef APUSERVER_H
#define APUSERVER_H

/*
 * APU server — owns cores 1-3 (the "auxiliary processing units").
 *
 * Same shape as LinkServer / DisplayServer:
 *   - one task that calls RegisterAs(APU_SERVER_NAME) and Receives requests
 *   - clients use the apu_client.h stubs (APUDispatch, APUBatch)
 *
 * The server is the only task allowed to call accel_dispatch / accel_wait.
 * It serializes single-job requests and parallelizes batched requests across
 * the three secondary cores. Hot rendering paths (display, screenbuf diff,
 * canvas clear, tetris) all go through this server now.
 */

#define APU_SERVER_PRIORITY 4
#define APU_SERVER_NAME     "APUServer"

void apu_server_entry(void);   /* RegisterAs(APU_SERVER_NAME); created from main.c */
int  APUServerTid(void);       /* static handle (valid once the server has run) */

#endif
