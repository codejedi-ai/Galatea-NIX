#include "apps.h"
#include "rps.h"

/* The RPS app (Layer 5) is the human-vs-computer client; the RPS server it talks
 * to is a Layer 4 service. */
void app_rps(void)
{
	rps_play_human();
}
