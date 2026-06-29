#ifndef APU_CORE_SERVER_H
#define APU_CORE_SERVER_H

/*
 * Per-core APU servers — one kernel task per secondary core (1, 2, 3).
 *
 * Each server owns a stack-allocated job RAM workspace (no heap). Clients
 * Send() a job descriptor; the server dispatches to its core, Yield()s via
 * AwaitEvent() until the APU fires a completion SGI, then Reply()s.
 */

#define APU_SERVER_PRIORITY  4

#define APU1_SERVER_NAME  "APU1_server"
#define APU2_SERVER_NAME  "APU2_server"
#define APU3_SERVER_NAME  "APU3_server"

/* Stack workspace per server — job params live here, not on the heap. */
#define APU_JOB_STACK_RAM  4096

void apu1_server_entry(void);
void apu2_server_entry(void);
void apu3_server_entry(void);

int APUCoreServerTid(int core_id);   /* 1..3 */
int APU1ServerTid(void);
int APU2ServerTid(void);
int APU3ServerTid(void);

#endif
