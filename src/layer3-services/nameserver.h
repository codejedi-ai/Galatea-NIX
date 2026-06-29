#ifndef NAMESERVER_H
#define NAMESERVER_H

#include <stdint.h>

#define NAME_SERVER_PRIORITY  3
#define NS_NAME_MAX 32
#define NS_MAX_NAMES 16

typedef enum {
	NS_MSG_REGISTER = 1,
	NS_MSG_WHOIS = 2,
} NameServerMsgType;

typedef struct {
	int type;
	char name[NS_NAME_MAX];
} NameServerMsg;

void name_server_entry(void);
int NameServerTid(void);
void RegisterAs(const char *name);
int WhoIs(const char *name);

#endif
