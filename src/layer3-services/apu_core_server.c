#include "apu_core_server.h"
#include "apu_messages.h"
#include "nameserver.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/accel.h"
#include "../layer1-processes/apu_completion.h"

static int apu_server_tids[3] = { -1, -1, -1 };

int APU1ServerTid(void) { return apu_server_tids[0]; }
int APU2ServerTid(void) { return apu_server_tids[1]; }
int APU3ServerTid(void) { return apu_server_tids[2]; }

int APUCoreServerTid(int core_id)
{
	if (core_id < 1 || core_id > 3)
		return -1;
	return apu_server_tids[core_id - 1];
}

/*
 * Server loop for one APU core.  job_ram[] is stack-allocated workspace —
 * callers may pass a pointer into their message buffer; we copy the opaque
 * arg pointer through but keep fn/arg stable for the duration of the job.
 */
static void apu_core_server_loop(int core_id, const char *server_name, int *tid_slot)
{
	int client_tid;
	ApuMsg   m;
	ApuReply r;
	char     job_ram[APU_JOB_STACK_RAM];

	(void)job_ram;  /* reserved for typed job payloads (opcode + params) */

	*tid_slot = MyTid();
	RegisterAs(server_name);

	for (;;) {
		Receive(&client_tid, (char *)&m, (int)sizeof(m));

		r.status = 0;
		r.n      = 0;
		for (int i = 0; i < APU_BATCH_MAX; i++)
			r.cores_used[i] = -1;

		if (m.type != APU_MSG_DISPATCH) {
			r.status = -1;
			Reply(client_tid, (char *)&r, (int)sizeof(r));
			continue;
		}

		void (*fn)(void *) = m.jobs[0].fn;
		void *arg          = m.jobs[0].arg;

		accel_dispatch(core_id, fn, arg);

		/* Yield on CPU0 until Core N fires completion SGI (see apu_completion.c).
		 * Re-check accel_is_busy after each wake — handles fast jobs and queued SGIs. */
		int event = apu_done_event(core_id);
		while (accel_is_busy(core_id)) {
			(void)AwaitEvent(event);
			__asm__ volatile("dmb ish" ::: "memory");
		}

		r.cores_used[0] = core_id;
		r.n             = 1;
		Reply(client_tid, (char *)&r, (int)sizeof(r));
	}
}

void apu1_server_entry(void)
{
	apu_core_server_loop(1, APU1_SERVER_NAME, &apu_server_tids[0]);
}

void apu2_server_entry(void)
{
	apu_core_server_loop(2, APU2_SERVER_NAME, &apu_server_tids[1]);
}

void apu3_server_entry(void)
{
	apu_core_server_loop(3, APU3_SERVER_NAME, &apu_server_tids[2]);
}
