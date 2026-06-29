#ifndef APU_MESSAGES_H
#define APU_MESSAGES_H

/*
 * Internal wire protocol between APU clients and the APU server.
 * Public callers should use apu_client.h, not these structures.
 */

#define APU_BATCH_MAX 3   /* one slot per secondary core (1, 2, 3) */

typedef enum {
	APU_MSG_DISPATCH = 1,   /* run one job on a specific or any free core */
	APU_MSG_BATCH    = 2,   /* run up to 3 jobs in parallel, one per core */
} ApuMsgType;

typedef struct {
	void (*fn)(void *);     /* work item; runs at EL2 on the secondary core */
	void  *arg;
	int    core_id;         /* 1, 2, or 3 — only honored in BATCH (jobs[i] -> core i+1) */
} ApuJobSlot;

typedef struct {
	int        type;        /* ApuMsgType */
	int        n;           /* DISPATCH: 1; BATCH: 1..APU_BATCH_MAX */
	int        target;      /* DISPATCH: requested core (1..3) or -1 for any */
	ApuJobSlot jobs[APU_BATCH_MAX];
} ApuMsg;

typedef struct {
	int status;                       /* 0 = OK, negative = error */
	int n;                            /* number of jobs actually dispatched */
	int cores_used[APU_BATCH_MAX];    /* which core ran jobs[i]            */
} ApuReply;

#endif
