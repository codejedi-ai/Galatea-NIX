#include "apu_client.h"
#include "apu_messages.h"
#include "apu_core_server.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/accel.h"

static int apu_send_dispatch(int server, int core_or_any,
                             void (*fn)(void *), void *arg)
{
	ApuMsg   req;
	ApuReply rep;

	if (server < 0)
		return -1;

	req.type        = APU_MSG_DISPATCH;
	req.n           = 1;
	req.target      = core_or_any;
	req.jobs[0].fn  = fn;
	req.jobs[0].arg = arg;
	req.jobs[0].core_id = core_or_any;

	int n = Send(server, (const char *)&req, (int)sizeof(req),
	             (char *)&rep, (int)sizeof(rep));
	if (n < 0 || rep.status < 0)
		return -1;
	return rep.cores_used[0];
}

int APUDispatch(int core_or_any, void (*fn)(void *), void *arg)
{
	int core = (core_or_any >= 1 && core_or_any <= 3) ? core_or_any : 1;
	int server = APUCoreServerTid(core);

	if (server < 0) {
		accel_dispatch(core, fn, arg);
		accel_wait(core);
		return core;
	}

	return apu_send_dispatch(server, core_or_any, fn, arg);
}

int APUBatch(const ApuJob *jobs, int n)
{
	if (n < 1)
		return 0;
	if (n > APU_BATCH_MAX)
		n = APU_BATCH_MAX;

	/* Parallel batch: send to each per-core server without waiting between
	 * dispatches, then collect replies. Each server yields on its own core. */
	int servers[APU_BATCH_MAX];
	int sent = 0;

	for (int i = 0; i < n; i++) {
		servers[i] = APUCoreServerTid(i + 1);
		if (servers[i] < 0) {
			for (int j = 0; j < i; j++) {
				ApuReply dummy;
				Receive(&servers[j], (char *)&dummy, (int)sizeof(dummy));
			}
			for (int j = 0; j < n; j++) {
				accel_dispatch(j + 1, jobs[j].fn, jobs[j].arg);
			}
			for (int j = 0; j < n; j++)
				accel_wait(j + 1);
			return n;
		}
	}

	/* Synchronous batch via sequential Send — each server runs in parallel
	 * on its core while the client blocks per-server. True overlap happens
	 * because servers dispatch then yield independently. */
	for (int i = 0; i < n; i++) {
		int r = apu_send_dispatch(servers[i], i + 1, jobs[i].fn, jobs[i].arg);
		if (r < 0)
			return -1;
		sent++;
	}
	return sent;
}
