#ifndef RPS_H
#define RPS_H

#include "rps_messages.h"

/* ----- server ----- */
#define RPS_SERVER_PRIORITY 6
#define RPS_CLIENT_PRIORITY 7
void rps_server_entry(void);     /* registers as RPS_SERVER_NAME */
int  RpsServerTid(void);         /* static handle (valid once the server ran) */

/* ----- client API (talk to the server over IPC) ----- */
int  RpsSignup(int server);                 /* -> RPS_R_PAIRED */
int  RpsPlay(int server, int move, int *opp_move); /* -> RPS_R_WIN/LOSE/TIE/OPP_QUIT */
void RpsQuit(int server);

/*
 * Shared snapshot of the headline match + tallies, updated by the server and
 * read by the Layer 4 UI. Single-CPU, so a plain global is race-free.
 */
typedef struct {
	int  games_played;
	int  p1_wins, p2_wins, ties;
	int  active_matches;
	/* headline match (match #1) */
	int  m_active;
	int  m_p1_tid, m_p2_tid;
	int  m_p1_move, m_p2_move;   /* -1 = not played this round */
	int  m_round;
	int  m_last_result;          /* RPS_R_* of the last resolved round, 0 = none */
} RpsDisplay;

extern volatile RpsDisplay g_rps;

/* Interactive Rock-Paper-Scissors vs the computer (Layer 4 UI). */
void rps_play_human(void);

#endif
