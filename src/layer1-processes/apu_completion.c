#include "apu_completion.h"
#include "gic.h"

int apu_done_event(int core_id)
{
	switch (core_id) {
	case 1: return APU1_DONE_EVENT;
	case 2: return APU2_DONE_EVENT;
	case 3: return APU3_DONE_EVENT;
	default: return APU1_DONE_EVENT;
	}
}

void apu_signal_completion(int core_id)
{
	gic_send_sgi((uint32_t)apu_done_event(core_id), 0x01u);  /* target CPU0 */
}

void apu_completion_init(void)
{
	for (int core = 1; core <= 3; core++) {
		uint32_t evt = (uint32_t)apu_done_event(core);
		route_interrupt(evt, 0);
		enable_interrupt(evt);
	}
}
