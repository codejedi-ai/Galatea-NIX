#include "idle.h"
#include "../layer1-processes/accel.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/config.h"

extern void wfi(void);

/*
 * Idle time measurement — updated here, read by TC1 status display.
 *
 * g_idle_us  : microseconds the CPU spent in WFI (idle) since boot.
 * g_total_us : total microseconds since boot.
 *
 * The ARM virtual counter (cntvct_el0) runs at cntfrq_el0 Hz
 * (54 MHz on RPi 4).  We convert to microseconds on read.
 */
volatile unsigned long long g_idle_us  = 0;
volatile unsigned long long g_total_us = 0;

static inline unsigned long long sys_cnt(void)
{
    unsigned long long v;
    __asm__ volatile ("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}

static inline unsigned long long sys_freq(void)
{
    unsigned long long f;
    __asm__ volatile ("mrs %0, cntfrq_el0" : "=r"(f));
    return f;
}

unsigned long long time_us_since_boot(void)
{
    unsigned long long v = sys_cnt();
    static unsigned long long boot = 0;
    static unsigned long long freq = 0;
    if (!freq)
        freq = sys_freq();
    if (!boot)
        boot = v;
    return (v - boot) * 1000000ULL / freq;
}

static unsigned g_cpu_ema;   /* tenths 0–1000, exponential moving average */
static unsigned g_apu_ema[3]; /* per APU core, same scale */
static unsigned apu_jobs_snap[3];

void cpu_util_sample(void)
{
	static unsigned long long idle_snap, total_snap;
	unsigned long long idle_now  = g_idle_us;
	unsigned long long total_now = time_us_since_boot();
	unsigned long long d_idle, d_total, busy;
	unsigned instant;

	if (total_snap == 0 && idle_snap == 0) {
		idle_snap  = idle_now;
		total_snap = total_now;
		for (int i = 0; i < 3; i++)
			apu_jobs_snap[i] = accel_jobs_done(i + 1);
		return;
	}

	d_idle  = idle_now  - idle_snap;
	d_total = total_now - total_snap;
	if (d_total < 10000ULL)   /* ≥10 ms between samples */
		return;

	busy = (d_total > d_idle) ? (d_total - d_idle) : 0;
	instant = (unsigned)(busy * 1000ULL / d_total);
	if (instant > 1000u) instant = 1000u;

	for (int i = 0; i < 3; i++) {
		unsigned j = accel_jobs_done(i + 1);
		unsigned delta = j - apu_jobs_snap[i];
		unsigned apu_inst = 0;

		apu_jobs_snap[i] = j;
		if (accel_is_busy(i + 1))
			apu_inst = 1000u;
		else if (delta > 0) {
			unsigned load = delta * 50u;
			if (load > 1000u) load = 1000u;
			apu_inst = load;
		}
		if (g_apu_ema[i] == 0)
			g_apu_ema[i] = apu_inst;
		else
			g_apu_ema[i] = (g_apu_ema[i] * 2u + apu_inst * 8u) / 10u;
	}

	if (g_cpu_ema == 0)
		g_cpu_ema = instant;
	else
		g_cpu_ema = (g_cpu_ema * 2u + instant * 8u) / 10u;

	idle_snap  = idle_now;
	total_snap = total_now;
}

unsigned cpu_util_tenths(void)
{
	return g_cpu_ema;
}

unsigned cpu_util_pct(void)
{
	return (g_cpu_ema + 5u) / 10u;
}

unsigned apu_util_tenths(int core_id)
{
	if (core_id < 1 || core_id > 3)
		return 0;
	return g_apu_ema[core_id - 1];
}

#define IDLE_WINDOW_TICKS 100   /* recent-idle window: 1 s (configurable) */

void idle_entry(void)
{
    uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m Idle task (TID=%d)\r\n",
                MyTid());

    unsigned long long freq = sys_freq();

    for (;;) {
#if CLOCKSERVERON == 1
        unsigned long long t0 = sys_cnt();
        wfi();
        unsigned long long t1 = sys_cnt();

        /* Convert counts to microseconds */
        g_idle_us  += (t1 - t0) * 1000000ULL / freq;
        g_total_us  = time_us_since_boot();
#else
        (void)freq;
        Yield();
#endif
    }
}
