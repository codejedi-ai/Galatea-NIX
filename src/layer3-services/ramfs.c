#include "ramfs.h"
#include "../layer1-processes/rpi.h"

typedef struct {
	int  used;
	int  is_dir;
	int  len;
	char path[RAMFS_PATH_MAX];
	char data[RAMFS_FILE_CAP];
} Entry;

static Entry fs[RAMFS_MAX_ENTRIES];
static int fs_ready;

/* ---- tiny string helpers (no libc) ---- */
static int  r_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int  r_eq(const char *a, const char *b) { int i = 0; while (a[i] && a[i] == b[i]) i++; return a[i] == b[i]; }
static void r_cpy(char *d, const char *s, int max) { int i = 0; for (; s[i] && i < max - 1; i++) d[i] = s[i]; d[i] = 0; }

/* parent directory of path into out (e.g. "/a/b" -> "/a", "/a" -> "/") */
static void parent_of(const char *path, char *out)
{
	int last = 0, i;
	for (i = 0; path[i]; i++) if (path[i] == '/') last = i;
	if (last == 0) { out[0] = '/'; out[1] = 0; return; }
	for (i = 0; i < last; i++) out[i] = path[i];
	out[i] = 0;
}

static const char *name_of(const char *path)
{
	const char *n = path;
	for (int i = 0; path[i]; i++) if (path[i] == '/') n = &path[i + 1];
	return n;
}

static Entry *find(const char *path)
{
	for (int i = 0; i < RAMFS_MAX_ENTRIES; i++)
		if (fs[i].used && r_eq(fs[i].path, path)) return &fs[i];
	return 0;
}

static Entry *alloc_entry(void)
{
	for (int i = 0; i < RAMFS_MAX_ENTRIES; i++)
		if (!fs[i].used) return &fs[i];
	return 0;
}

void ramfs_init(void)
{
	if (fs_ready) return;
	fs_ready = 1;
	for (int i = 0; i < RAMFS_MAX_ENTRIES; i++) fs[i].used = 0;
	/* a couple of starter entries so `ls` shows something */
	ramfs_mkdir("/notes");
	ramfs_write("/readme.txt",
		"Welcome to the RAM filesystem.\nFolders are string paths; files live in RAM.\n", -1);
	ramfs_write("/notes/hello.txt", "hi there!\n", -1);
}

int ramfs_exists(const char *path)
{
	Entry *e = find(path);
	if (!e) return 0;
	return e->is_dir ? 2 : 1;
}

int ramfs_mkdir(const char *path)
{
	if (find(path)) return -1;
	Entry *e = alloc_entry();
	if (!e) return -2;
	e->used = 1; e->is_dir = 1; e->len = 0;
	r_cpy(e->path, path, RAMFS_PATH_MAX);
	return 0;
}

int ramfs_write(const char *path, const char *data, int len)
{
	if (len < 0) len = r_len(data);
	if (len >= RAMFS_FILE_CAP) len = RAMFS_FILE_CAP - 1;
	Entry *e = find(path);
	if (e && e->is_dir) return -1;
	if (!e) {
		e = alloc_entry();
		if (!e) return -2;
		e->used = 1; e->is_dir = 0;
		r_cpy(e->path, path, RAMFS_PATH_MAX);
	}
	for (int i = 0; i < len; i++) e->data[i] = data[i];
	e->data[len] = 0;
	e->len = len;
	return len;
}

int ramfs_append(const char *path, const char *s)
{
	Entry *e = find(path);
	if (!e) return ramfs_write(path, s, -1);
	if (e->is_dir) return -1;
	int sl = r_len(s);
	int i = 0;
	while (i < sl && e->len < RAMFS_FILE_CAP - 1) e->data[e->len++] = s[i++];
	e->data[e->len] = 0;
	return e->len;
}

const char *ramfs_read(const char *path, int *len)
{
	Entry *e = find(path);
	if (!e || e->is_dir) return 0;
	if (len) *len = e->len;
	return e->data;
}

int ramfs_remove(const char *path)
{
	Entry *e = find(path);
	if (!e) return -1;
	e->used = 0;
	return 0;
}

void ramfs_list(const char *dir, void (*cb)(const char *, int, int, void *), void *u)
{
	char par[RAMFS_PATH_MAX];
	for (int i = 0; i < RAMFS_MAX_ENTRIES; i++) {
		if (!fs[i].used) continue;
		parent_of(fs[i].path, par);
		if (r_eq(par, dir))
			cb(name_of(fs[i].path), fs[i].is_dir, fs[i].len, u);
	}
}

void ramfs_each(void (*cb)(const char *, int, int, void *), void *u)
{
	for (int i = 0; i < RAMFS_MAX_ENTRIES; i++)
		if (fs[i].used)
			cb(fs[i].path, fs[i].is_dir, fs[i].len, u);
}
