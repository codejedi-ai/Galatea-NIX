#ifndef RAMFS_H
#define RAMFS_H

/*
 * RAM filesystem — a fictional, in-RAM file space (no disk; disk is on hold).
 * Folders are just string path prefixes (no real directory objects); files hold
 * text stored as bytes. Entries are flat with absolute paths like "/notes/a.txt".
 */
#define RAMFS_MAX_ENTRIES 64
#define RAMFS_PATH_MAX    64
#define RAMFS_FILE_CAP    1024

void ramfs_init(void);

int  ramfs_mkdir(const char *path);                       /* 0 ok, <0 err */
int  ramfs_write(const char *path, const char *data, int len);  /* create/overwrite file */
int  ramfs_append(const char *path, const char *s);       /* append text (nano) */
const char *ramfs_read(const char *path, int *len);       /* NULL if missing/dir */
int  ramfs_remove(const char *path);                      /* file or empty dir */
int  ramfs_exists(const char *path);                      /* 1=file, 2=dir, 0=no */

/* enumerate entries directly inside `dir`; cb(name, is_dir, size) per child */
void ramfs_list(const char *dir, void (*cb)(const char *name, int is_dir, int size, void *u), void *u);

/* enumerate ALL entries (full paths), for `tree` */
void ramfs_each(void (*cb)(const char *path, int is_dir, int size, void *u), void *u);

#endif
