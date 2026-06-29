#ifndef IDLE_H
#define IDLE_H

#define IDLE_PRIORITY 31

void idle_entry(void);

extern volatile unsigned long long g_idle_us;
extern volatile unsigned long long g_total_us;

/* Wall-clock microseconds since boot (EL0-safe after cntkctl is configured). */
unsigned long long time_us_since_boot(void);

/* Sample CPU utilisation into an EMA (call once per UI frame). */
void cpu_util_sample(void);

/* Last sampled utilisation (0–100 % / 0–1000 tenths). */
unsigned cpu_util_pct(void);
unsigned cpu_util_tenths(void);   /* 0–1000 (one decimal, e.g. 63 → 6.3 %) */
unsigned apu_util_tenths(int core_id); /* core_id 1–3, same scale as cpu_util_tenths */

#endif
