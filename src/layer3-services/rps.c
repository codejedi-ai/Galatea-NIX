#include "rps.h"
#include "rps_messages.h"
#include "nameserver.h"
#include "clockserver.h"
#include "clock_client.h"
#include "../layer2-messaging/messaging.h"
#include "../layer1-processes/syscall.h"
#include "../layer1-processes/rpi.h"
#include "../layer1-processes/config.h"

#define RPS_MAX_PLAYERS     NUMPROCS

static int rps_server_tid = -1;
volatile RpsDisplay g_rps;

int RpsServerTid(void) { return rps_server_tid; }

/* -------------------------------------------------- server state machine -- */

typedef struct {
	int in_use;
	int tid;
	int opp;     /* index of opponent slot, or -1 */
	int move;    /* current move, or -1 */
	int played;  /* 1 = Play received and held (not yet replied) */
} Slot;

static Slot slots[RPS_MAX_PLAYERS];

static int slot_find(int tid)
{
	for (int i = 0; i < RPS_MAX_PLAYERS; i++)
		if (slots[i].in_use && slots[i].tid == tid)
			return i;
	return -1;
}

static int slot_alloc(int tid)
{
	for (int i = 0; i < RPS_MAX_PLAYERS; i++) {
		if (!slots[i].in_use) {
			slots[i].in_use = 1;
			slots[i].tid = tid;
			slots[i].opp = -1;
			slots[i].move = -1;
			slots[i].played = 0;
			return i;
		}
	}
	return -1;
}

/* rock>scissors>paper>rock */
static int beats(int a, int b)
{
	return (a == RPS_ROCK && b == RPS_SCISSORS) ||
	       (a == RPS_SCISSORS && b == RPS_PAPER) ||
	       (a == RPS_PAPER && b == RPS_ROCK);
}

static void rps_display_move(int tid, int move)
{
	if (tid == g_rps.m_p1_tid) g_rps.m_p1_move = move;
	else if (tid == g_rps.m_p2_tid) g_rps.m_p2_move = move;
}

void rps_server_entry(void)
{
	int tid;
	RpsMsg msg;
	RpsReply rep;
	int waiting = -1;   /* slot index of an unpaired signed-up player */

	rps_server_tid = MyTid();
	RegisterAs(RPS_SERVER_NAME);

	g_rps.games_played = g_rps.p1_wins = g_rps.p2_wins = g_rps.ties = 0;
	g_rps.active_matches = 0;
	g_rps.m_active = 0;
	g_rps.m_p1_move = g_rps.m_p2_move = -1;
	g_rps.m_round = 0;
	g_rps.m_last_result = 0;

	uart_printf(CONSOLE, "\033[1;32m[  OK  ]\033[0m RPS Server (TID=%d)\r\n", MyTid());

	for (;;) {
		Receive(&tid, (char *)&msg, (int)sizeof(msg));

		if (msg.type == RPS_SIGNUP) {
			int s = slot_alloc(tid);
			if (s < 0) {
				rep.result = RPS_R_ERR; rep.opp_move = -1;
				Reply(tid, (char *)&rep, (int)sizeof(rep));
				continue;
			}
			if (waiting < 0) {
				waiting = s;          /* hold until a partner arrives */
			} else {
				int o = waiting; waiting = -1;
				slots[s].opp = o; slots[o].opp = s;
				rep.result = RPS_R_PAIRED; rep.opp_move = -1;
				Reply(slots[o].tid, (char *)&rep, (int)sizeof(rep));
				Reply(slots[s].tid, (char *)&rep, (int)sizeof(rep));
				g_rps.active_matches++;
				g_rps.m_active = 1;
				g_rps.m_p1_tid = slots[o].tid;
				g_rps.m_p2_tid = slots[s].tid;
				g_rps.m_p1_move = g_rps.m_p2_move = -1;
				g_rps.m_round = 1;
				g_rps.m_last_result = 0;
			}
		} else if (msg.type == RPS_PLAY) {
			int s = slot_find(tid);
			if (s < 0) {
				rep.result = RPS_R_ERR; rep.opp_move = -1;
				Reply(tid, (char *)&rep, (int)sizeof(rep));
				continue;
			}
			int o = slots[s].opp;
			if (o < 0) {   /* partner already left */
				rep.result = RPS_R_OPP_QUIT; rep.opp_move = -1;
				Reply(tid, (char *)&rep, (int)sizeof(rep));
				continue;
			}
			slots[s].move = msg.move;
			slots[s].played = 1;
			rps_display_move(tid, msg.move);

			if (slots[o].played) {
				int rs, ro;
				if (slots[s].move == slots[o].move) { rs = ro = RPS_R_TIE; }
				else if (beats(slots[s].move, slots[o].move)) { rs = RPS_R_WIN; ro = RPS_R_LOSE; }
				else { rs = RPS_R_LOSE; ro = RPS_R_WIN; }

				RpsReply reps, repo;
				reps.result = rs; reps.opp_move = slots[o].move;
				repo.result = ro; repo.opp_move = slots[s].move;
				Reply(slots[s].tid, (char *)&reps, (int)sizeof(reps));
				Reply(slots[o].tid, (char *)&repo, (int)sizeof(repo));

				int p1res = (slots[s].tid == g_rps.m_p1_tid) ? rs : ro;
				g_rps.games_played++;
				if (p1res == RPS_R_WIN) g_rps.p1_wins++;
				else if (p1res == RPS_R_LOSE) g_rps.p2_wins++;
				else g_rps.ties++;
				g_rps.m_last_result = p1res;
				g_rps.m_round++;

				slots[s].played = slots[o].played = 0;
				slots[s].move = slots[o].move = -1;
				g_rps.m_p1_move = g_rps.m_p2_move = -1;
			}
			/* else: hold (no reply) until the opponent plays */
		} else { /* RPS_QUIT (or unknown) */
			int s = slot_find(tid);
			rep.result = 0; rep.opp_move = -1;
			Reply(tid, (char *)&rep, (int)sizeof(rep));
			if (s >= 0) {
				int qtid = slots[s].tid;
				int o = slots[s].opp;
				if (o >= 0 && slots[o].in_use) {
					if (slots[o].played) {
						RpsReply oq; oq.result = RPS_R_OPP_QUIT; oq.opp_move = -1;
						Reply(slots[o].tid, (char *)&oq, (int)sizeof(oq));
						slots[o].played = 0;
					}
					slots[o].opp = -1;
				}
				slots[s].in_use = 0;
				if (waiting == s) waiting = -1;
				if (g_rps.m_active &&
				    (qtid == g_rps.m_p1_tid || qtid == g_rps.m_p2_tid)) {
					if (g_rps.active_matches > 0) g_rps.active_matches--;
					g_rps.m_active = 0;
				}
			}
		}
	}
}

/* --------------------------------------------------------- client API ----- */

int RpsSignup(int server)
{
	RpsMsg m; RpsReply r;
	m.type = RPS_SIGNUP; m.move = 0;
	Send(server, (const char *)&m, (int)sizeof(m), (char *)&r, (int)sizeof(r));
	return r.result;
}

int RpsPlay(int server, int move, int *opp_move)
{
	RpsMsg m; RpsReply r;
	m.type = RPS_PLAY; m.move = move;
	Send(server, (const char *)&m, (int)sizeof(m), (char *)&r, (int)sizeof(r));
	if (opp_move) *opp_move = r.opp_move;
	return r.result;
}

void RpsQuit(int server)
{
	RpsMsg m; RpsReply r;
	m.type = RPS_QUIT; m.move = 0;
	Send(server, (const char *)&m, (int)sizeof(m), (char *)&r, (int)sizeof(r));
}

/* ------------------------------------------------ human vs computer (TUI) -- */

/* The computer opponent: signs up, then each round commits a pseudo-random move
 * (the server holds it secret until the human plays, so it's fair). Quits when
 * the human leaves. */
static void rps_computer_client(void)
{
	int server = RpsServerTid();
	int clock = ClockServerTid();

	if (RpsSignup(server) != RPS_R_PAIRED) { Exit(); }

	unsigned seed = (unsigned)Time(clock) * 2654435761u + 1u;
	for (;;) {
		int opp;
		seed = seed * 1103515245u + 12345u;        /* LCG; reseeded by clock drift */
		seed ^= (unsigned)Time(clock);
		int mv = (int)((seed >> 13) % 3u);
		if (RpsPlay(server, mv, &opp) == RPS_R_OPP_QUIT)
			break;
	}
	RpsQuit(server);
	Exit();
}

/* Interactive RPS — UI removed in DarcyOS APU terminal build. */
void rps_play_human(void)
{
	uart_printf(CONSOLE, "rps: use RPS server API (no TUI in terminal-only build)\r\n");
}
