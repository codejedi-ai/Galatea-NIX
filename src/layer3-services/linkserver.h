#ifndef LINKSERVER_H
#define LINKSERVER_H

/*
 * Link server (Layer 4) — owns the AUX mini-UART (the container link) and lets
 * tasks exchange one-line messages with the virtual hardware running in the
 * docker container. Same CS452 shape as the clock server: a Receive/Reply
 * server plus a notifier task that polls the UART and forwards bytes.
 *
 *   client --LinkSend("PING")--> [link server] --"PING\n"--> AUX UART
 *                                                     |
 *   container vhw.py  <--- serial1 chardev <----------+
 *   container vhw.py  ---"PONG\n"---> AUX UART --> notifier --> server --> client
 */
#define LINK_SERVER_PRIORITY    6
#define LINK_NOTIFIER_PRIORITY  4
#define LINK_SERVER_NAME        "LinkServer"

#define LINK_CMD_MAX    120     /* longest command a client may send */
#define LINK_REPLY_MAX  240     /* longest reply line we keep        */

void link_server_entry(void);   /* RegisterAs(LINK_SERVER_NAME); owns the AUX UART */
int  LinkServerTid(void);       /* static handle (valid once the server has run)   */

/* Send one command line to the container's virtual hardware and wait for its
 * reply line. `out` receives a NUL-terminated response (never the trailing \n).
 * Returns the reply length (>= 0). Replies "(no response)" on timeout, "(busy)"
 * if another request is in flight, or "(no link)" if the server isn't up. */
int  LinkSend(const char *cmd, char *out, int outmax);

#endif
