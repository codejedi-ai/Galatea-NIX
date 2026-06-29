#ifndef DISKFS_H
#define DISKFS_H

/*
 * Disk filesystem — persistent storage backed by the Docker container's real
 * filesystem, accessed over the AUX UART link (serial1) via vhw.py.
 *
 * Wire protocol (ASCII line, handled by vhw.py's FS command handler):
 *
 *   OS sends:          vhw.py responds:
 *   FS STAT /path   -> "FILE <size>" | "DIR" | "NOENT" | "ERR ..."
 *   FS LS /path     -> "<name1> <name2> ..."  (space-separated, empty if none)
 *   FS MKDIR /path  -> "OK" | "ERR ..."
 *   FS RM /path     -> "OK" | "ERR ..."
 *   FS TRUNC /path  -> "OK" | "ERR ..."       (create/truncate to zero)
 *   FS READ /path <off>  -> "<hex-data>" | "EOF" | "ERR ..."
 *                           up to DISKFS_CHUNK bytes per call, hex-encoded
 *   FS WRITE /path <hex> -> "OK <bytes>" | "ERR ..."
 *                           hex-encoded data appended to file
 *
 * All paths live under /disk/ on the container side (e.g. OS path "/hello.txt"
 * maps to container path "/disk/hello.txt").
 *
 * The server wraps LinkSend() — calls block the caller until vhw.py replies.
 */

#define DISKFS_PATH_MAX   60    /* max path length the OS sends               */
#define DISKFS_CHUNK      48    /* bytes per read/write chunk (96 hex chars)  */
#define DISKFS_BUF_MAX    256   /* scratch buffer for LinkSend replies         */

/* Return values shared across all calls */
#define DISKFS_OK         0
#define DISKFS_ERR        -1
#define DISKFS_NOENT      -2
#define DISKFS_NOSPACE    -3

/*
 * Stat a path.
 * Returns: 1=file, 2=directory, 0=does not exist, DISKFS_ERR on link error.
 * On file: *size is set to the file size in bytes.
 */
int DiskStat(const char *path, int *size);

/*
 * List directory entries. Names written space-separated into buf (NUL-term).
 * Returns number of entries, or DISKFS_ERR / DISKFS_NOENT.
 */
int DiskLs(const char *path, char *buf, int bufmax);

/* Create directory (parents must exist). Returns DISKFS_OK or DISKFS_ERR. */
int DiskMkdir(const char *path);

/* Remove file or empty directory. Returns DISKFS_OK, DISKFS_NOENT, DISKFS_ERR. */
int DiskRm(const char *path);

/*
 * Read file into buf. Reads up to bufmax-1 bytes, NUL-terminates.
 * Returns bytes read, DISKFS_NOENT if missing, DISKFS_ERR on error.
 */
int DiskRead(const char *path, char *buf, int bufmax);

/*
 * Write (overwrite) a file with data[0..len-1].
 * Returns bytes written, or DISKFS_ERR.
 */
int DiskWrite(const char *path, const char *data, int len);

/*
 * Append data[0..len-1] to file (creates if missing).
 * Returns bytes appended, or DISKFS_ERR.
 */
int DiskAppend(const char *path, const char *data, int len);

#endif
