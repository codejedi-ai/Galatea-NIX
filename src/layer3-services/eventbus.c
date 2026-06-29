#include "eventbus.h"

/* ================================================================ Topic registry == */

typedef struct {
    char    name[32];
    Subject subj;
    int     in_use;
} EvTopic;

static EvTopic g_topics[EVENTBUS_MAX_TOPICS];

static EvTopic *find_topic(const char *name)
{
    for (int i = 0; i < EVENTBUS_MAX_TOPICS; i++) {
        if (!g_topics[i].in_use) continue;
        int j = 0;
        while (name[j] && g_topics[i].name[j] == name[j]) j++;
        if (name[j] == '\0' && g_topics[i].name[j] == '\0')
            return &g_topics[i];
    }
    return 0;
}

static EvTopic *find_or_create_topic(const char *name)
{
    EvTopic *t = find_topic(name);
    if (t) return t;
    for (int i = 0; i < EVENTBUS_MAX_TOPICS; i++) {
        if (g_topics[i].in_use) continue;
        int j = 0;
        while (name[j] && j < 31) { g_topics[i].name[j] = name[j]; j++; }
        g_topics[i].name[j] = '\0';
        subject_init(&g_topics[i].subj);
        g_topics[i].in_use = 1;
        return &g_topics[i];
    }
    return 0;   /* topic pool full */
}

/* ===================================================== Observer pools == */

typedef struct { FnObserver  obs; int topic_idx; int in_use; } FnEntry;
typedef struct { TidObserver obs; int topic_idx; int in_use; } TidEntry;

static FnEntry  g_fn_pool [EVENTBUS_MAX_FN_OBS];
static TidEntry g_tid_pool[EVENTBUS_MAX_TID_OBS];

static int topic_index(const EvTopic *t)
{
    return (int)(t - g_topics);
}

/* ================================================================= API == */

void eventbus_init(void)
{
    for (int i = 0; i < EVENTBUS_MAX_TOPICS;  i++) g_topics[i].in_use = 0;
    for (int i = 0; i < EVENTBUS_MAX_FN_OBS;  i++) g_fn_pool[i].in_use  = 0;
    for (int i = 0; i < EVENTBUS_MAX_TID_OBS; i++) g_tid_pool[i].in_use = 0;
}

void eventbus_publish(const char *topic, int event, int value)
{
    EvTopic *t = find_topic(topic);
    if (!t) return;
    subject_notify(&t->subj, event, value);
}

int eventbus_subscribe_fn(const char *topic,
                          void (*fn)(void *ctx, int event, int value),
                          void *ctx)
{
    EvTopic *t = find_or_create_topic(topic);
    if (!t) return -1;

    for (int i = 0; i < EVENTBUS_MAX_FN_OBS; i++) {
        if (g_fn_pool[i].in_use) continue;
        fn_observer_init(&g_fn_pool[i].obs, fn, ctx);
        g_fn_pool[i].topic_idx = topic_index(t);
        g_fn_pool[i].in_use    = 1;
        return subject_attach(&t->subj, &g_fn_pool[i].obs.base);
    }
    return -1;  /* FnObserver pool full */
}

int eventbus_subscribe_tid(const char *topic, int tid)
{
    EvTopic *t = find_or_create_topic(topic);
    if (!t) return -1;

    for (int i = 0; i < EVENTBUS_MAX_TID_OBS; i++) {
        if (g_tid_pool[i].in_use) continue;
        tid_observer_init(&g_tid_pool[i].obs, tid);
        g_tid_pool[i].topic_idx = topic_index(t);
        g_tid_pool[i].in_use    = 1;
        return subject_attach(&t->subj, &g_tid_pool[i].obs.base);
    }
    return -1;  /* TidObserver pool full */
}

void eventbus_detach_fn(const char *topic,
                        void (*fn)(void *ctx, int event, int value),
                        void *ctx)
{
    EvTopic *t = find_topic(topic);
    if (!t) return;
    int idx = topic_index(t);
    for (int i = 0; i < EVENTBUS_MAX_FN_OBS; i++) {
        if (!g_fn_pool[i].in_use) continue;
        if (g_fn_pool[i].topic_idx != idx) continue;
        if (g_fn_pool[i].obs.fn != fn || g_fn_pool[i].obs.ctx != ctx) continue;
        subject_detach(&t->subj, &g_fn_pool[i].obs.base);
        g_fn_pool[i].in_use = 0;
        return;
    }
}

void eventbus_detach_tid(const char *topic, int tid)
{
    EvTopic *t = find_topic(topic);
    if (!t) return;
    int idx = topic_index(t);
    for (int i = 0; i < EVENTBUS_MAX_TID_OBS; i++) {
        if (!g_tid_pool[i].in_use) continue;
        if (g_tid_pool[i].topic_idx != idx) continue;
        if (g_tid_pool[i].obs.tid != tid) continue;
        subject_detach(&t->subj, &g_tid_pool[i].obs.base);
        g_tid_pool[i].in_use = 0;
        return;
    }
}
