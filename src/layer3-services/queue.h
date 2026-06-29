#ifndef QUEUE_H
#define QUEUE_H

/*
 * Layer 3 — Global Queue Server (Kafka-like pub/sub for all I/O and events).
 *
 * Topics: named event streams (e.g., "input:keyboard", "input:timer", "game:update")
 * Producers: publish events to topics
 * Consumers: subscribe to topics, receive events via blocking calls
 *
 * Usage:
 *   queue_publish("input:keyboard", key_code);
 *   queue_subscribe("input:keyboard", my_tid);
 *   while (1) { int msg = queue_receive(my_tid); }  // blocks until message
 */

/* max topics, subscriptions, and queue depth per topic */
#define QUEUE_MAX_TOPICS 32
#define QUEUE_MAX_SUBS   128
#define QUEUE_DEPTH      256

typedef struct {
	char name[32];
	int queue[QUEUE_DEPTH];
	int head, tail;
	int num_subscribers;
} QueueTopic;

typedef struct {
	char topic_name[32];
	int consumer_tid;
	int queue[QUEUE_DEPTH];
	int head, tail;
} QueueSubscription;

int QueueServerTid(void);
void queue_publish(const char *topic, int message);
int  queue_subscribe(const char *topic);        /* returns subscription id */
int  queue_receive(int sub_id);                 /* blocking: wait for message */
int  queue_trybuffer(int sub_id);               /* non-blocking: peek if available */

#endif
