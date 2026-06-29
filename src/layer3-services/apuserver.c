#include "apuserver.h"
#include "apu_messages.h"
#include "nameserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/accel.h"

static int apu_server_tid = -1;
int APUServerTid(void) { return apu_server_tid; }

/* ------------------------------------------------------------- server -----
 * Single task; receives one ApuMsg at a time. For DISPATCH we run one job on
 * one core (the server task itself WFEs in accel_wait — microseconds — so the
 * scheduler pause is negligible). For BATCH we dispatch all n jobs in parallel
 * (one per core) and accel_wait each. The server's pre-Reply dmb (inside
 * accel_wait) is the acquire barrier the client sees on return.
 */
void apu_server_entry(void)
{
	int tid;
	ApuMsg   m;
	ApuReply r;

	apu_server_tid = MyTid();
	RegisterAs(APU_SERVER_NAME);

	for (;;) {
		Receive(&tid, (char *)&m, (int)sizeof(m));

		r.status = 0;
		r.n      = 0;
		for (int i = 0; i < APU_BATCH_MAX; i++) r.cores_used[i] = -1;

		if (m.type == APU_MSG_DISPATCH) {
			int core = m.target;
			if (core < 1 || core > 3) core = 1;   /* APU_ANY: pick core 1 (server is serial, so it's free) */
			accel_dispatch(core, m.jobs[0].fn, m.jobs[0].arg);
			accel_wait(core);
			r.cores_used[0] = core;
			r.n             = 1;
		} else if (m.type == APU_MSG_BATCH) {
			int n = m.n;
			if (n < 1) n = 1;
			if (n > APU_BATCH_MAX) n = APU_BATCH_MAX;
			/* Convention: jobs[i] runs on core (i+1). */
			for (int i = 0; i < n; i++) {
				accel_dispatch(i + 1, m.jobs[i].fn, m.jobs[i].arg);
				r.cores_used[i] = i + 1;
			}
			for (int i = 0; i < n; i++)
				accel_wait(i + 1);
			r.n = n;
		} else {
			r.status = -1;
		}

		Reply(tid, (char *)&r, (int)sizeof(r));
	}
}
