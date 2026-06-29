#ifndef RPS_MESSAGES_H
#define RPS_MESSAGES_H

/*
 * CS452 K2 — Rock-Paper-Scissors server protocol.
 * Clients reach the server with Send(); the server only Receive()/Reply()s.
 *
 *   Signup() -> blocks until paired with another client (RPS_R_PAIRED).
 *   Play(m)  -> blocks until the opponent also plays; returns WIN/LOSE/TIE,
 *               or RPS_R_OPP_QUIT if the opponent left.
 *   Quit()   -> leaves the match; the opponent is told on its next action.
 */

#define RPS_SERVER_NAME "RPSServer"

/* request types */
enum {
	RPS_SIGNUP = 1,
	RPS_PLAY   = 2,
	RPS_QUIT   = 3,
};

/* moves */
enum {
	RPS_ROCK     = 0,
	RPS_PAPER    = 1,
	RPS_SCISSORS = 2,
};

/* reply results */
enum {
	RPS_R_ERR      = -1,
	RPS_R_PAIRED   = 1,
	RPS_R_WIN      = 2,
	RPS_R_LOSE     = 3,
	RPS_R_TIE      = 4,
	RPS_R_OPP_QUIT = 5,
};

typedef struct {
	int type;   /* RPS_SIGNUP / RPS_PLAY / RPS_QUIT */
	int move;   /* RPS_ROCK / RPS_PAPER / RPS_SCISSORS (PLAY only) */
} RpsMsg;

typedef struct {
	int result;    /* RPS_R_* */
	int opp_move;  /* opponent's move on a resolved round (for display) */
} RpsReply;

static inline const char *rps_move_name(int m)
{
	switch (m) {
	case RPS_ROCK:     return "Rock";
	case RPS_PAPER:    return "Paper";
	case RPS_SCISSORS: return "Scissors";
	default:           return "-";
	}
}

#endif
