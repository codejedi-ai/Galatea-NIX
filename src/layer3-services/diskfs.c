#include "diskfs.h"
#include "linkserver.h"

/* ---- hex helpers (no libc) -------------------------------------------- */

static int h_val(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

static int hex_dec(const char *hex, char *out, int maxlen)
{
	int n = 0;
	while (hex[0] && hex[1] && n < maxlen) {
		int hi = h_val(hex[0]), lo = h_val(hex[1]);
		if (hi < 0 || lo < 0) break;
		out[n++] = (char)((hi << 4) | lo);
		hex += 2;
	}
	return n;
}

static void hex_enc(const char *data, int len, char *out)
{
	const char *t = "0123456789abcdef";
	for (int i = 0; i < len; i++) {
		unsigned char b = (unsigned char)data[i];
		out[i * 2]     = t[(b >> 4) & 0xF];
		out[i * 2 + 1] = t[b & 0xF];
	}
	out[len * 2] = 0;
}

/* ---- tiny string helpers ------------------------------------------------ */

static void d_cpy(char *d, const char *s, int max)
{
	int i = 0;
	for (; s[i] && i < max - 1; i++) d[i] = s[i];
	d[i] = 0;
}

static int d_starts(const char *s, const char *pfx)
{
	int i = 0;
	for (; pfx[i]; i++) if (s[i] != pfx[i]) return 0;
	return 1;
}

static int d_atoi(const char *s)
{
	int n = 0, neg = 0;
	if (*s == '-') { neg = 1; s++; }
	for (; *s >= '0' && *s <= '9'; s++) n = n * 10 + (*s - '0');
	return neg ? -n : n;
}

/* ---- build "FS <verb> <path>" command string ---------------------------- */

static int build_cmd(char *cmd, int max, const char *verb, const char *path,
                     const char *extra)
{
	int n = 0;
	const char *pfx = "FS ";
	for (int i = 0; pfx[i] && n < max - 1; i++) cmd[n++] = pfx[i];
	for (int i = 0; verb[i] && n < max - 1; i++) cmd[n++] = verb[i];
	cmd[n++] = ' ';
	for (int i = 0; path[i] && n < max - 1; i++) cmd[n++] = path[i];
	if (extra) {
		cmd[n++] = ' ';
		for (int i = 0; extra[i] && n < max - 1; i++) cmd[n++] = extra[i];
	}
	cmd[n] = 0;
	return n;
}

/* ---- uint-to-decimal ---------------------------------------------------- */

static int u2d(unsigned v, char *buf)
{
	if (v == 0) { buf[0] = '0'; buf[1] = 0; return 1; }
	char tmp[12]; int n = 0, p;
	while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
	for (p = 0; p < n; p++) buf[p] = tmp[n - 1 - p];
	buf[n] = 0;
	return n;
}

/* ======================================================================== */

int DiskStat(const char *path, int *size)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	build_cmd(cmd, sizeof(cmd), "STAT", path, 0);
	LinkSend(cmd, reply, sizeof(reply));

	if (d_starts(reply, "FILE ")) {
		if (size) *size = d_atoi(reply + 5);
		return 1;
	}
	if (d_starts(reply, "DIR"))   return 2;
	if (d_starts(reply, "NOENT")) return 0;
	return DISKFS_ERR;
}

int DiskLs(const char *path, char *buf, int bufmax)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	build_cmd(cmd, sizeof(cmd), "LS", path, 0);
	LinkSend(cmd, reply, sizeof(reply));

	if (d_starts(reply, "ERR")) return DISKFS_ERR;
	if (d_starts(reply, "NOENT")) return DISKFS_NOENT;

	d_cpy(buf, reply, bufmax);

	/* count space-separated tokens */
	int count = 0, in_tok = 0;
	for (int i = 0; reply[i]; i++) {
		if (reply[i] != ' ') { if (!in_tok) { count++; in_tok = 1; } }
		else in_tok = 0;
	}
	return count;
}

int DiskMkdir(const char *path)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	build_cmd(cmd, sizeof(cmd), "MKDIR", path, 0);
	LinkSend(cmd, reply, sizeof(reply));
	return d_starts(reply, "OK") ? DISKFS_OK : DISKFS_ERR;
}

int DiskRm(const char *path)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	build_cmd(cmd, sizeof(cmd), "RM", path, 0);
	LinkSend(cmd, reply, sizeof(reply));
	if (d_starts(reply, "NOENT")) return DISKFS_NOENT;
	return d_starts(reply, "OK") ? DISKFS_OK : DISKFS_ERR;
}

int DiskRead(const char *path, char *buf, int bufmax)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	char offstr[12];
	int total = 0, off = 0;

	/* Truncate first in case vhw.py doesn't do multiread */
	while (total < bufmax - 1) {
		u2d((unsigned)off, offstr);
		build_cmd(cmd, sizeof(cmd), "READ", path, offstr);
		LinkSend(cmd, reply, sizeof(reply));

		if (d_starts(reply, "ERR"))   return DISKFS_ERR;
		if (d_starts(reply, "NOENT")) return DISKFS_NOENT;
		if (d_starts(reply, "EOF"))   break;

		/* reply is hex-encoded chunk */
		int got = hex_dec(reply, buf + total, bufmax - 1 - total);
		if (got <= 0) break;
		total += got;
		off   += got;
	}
	buf[total] = 0;
	return total;
}

static int disk_write_chunk(const char *path, const char *data, int len,
                             int append)
{
	char cmd[LINK_CMD_MAX], reply[DISKFS_BUF_MAX];
	char hexbuf[DISKFS_CHUNK * 2 + 1];
	const char *verb = append ? "APPEND" : "WRITE";
	int total = 0;

	/* If overwriting, truncate first */
	if (!append) {
		build_cmd(cmd, sizeof(cmd), "TRUNC", path, 0);
		LinkSend(cmd, reply, sizeof(reply));
		if (!d_starts(reply, "OK")) return DISKFS_ERR;
	}

	while (total < len) {
		int chunk = len - total;
		if (chunk > DISKFS_CHUNK) chunk = DISKFS_CHUNK;

		hex_enc(data + total, chunk, hexbuf);

		/* build "FS APPEND /path <hexdata>" */
		int n = 0;
		const char *p;
		p = "FS "; for (int i = 0; p[i]; i++) cmd[n++] = p[i];
		p = verb;  for (int i = 0; p[i] && n < LINK_CMD_MAX - 1; i++) cmd[n++] = p[i];
		cmd[n++] = ' ';
		for (int i = 0; path[i] && n < LINK_CMD_MAX - 1; i++) cmd[n++] = path[i];
		cmd[n++] = ' ';
		for (int i = 0; hexbuf[i] && n < LINK_CMD_MAX - 1; i++) cmd[n++] = hexbuf[i];
		cmd[n] = 0;

		LinkSend(cmd, reply, sizeof(reply));
		if (!d_starts(reply, "OK")) return DISKFS_ERR;

		total += chunk;
		/* after first chunk, always append */
		verb = "APPEND";
	}
	return total;
}

int DiskWrite(const char *path, const char *data, int len)
{
	return disk_write_chunk(path, data, len, 0);
}

int DiskAppend(const char *path, const char *data, int len)
{
	return disk_write_chunk(path, data, len, 1);
}
