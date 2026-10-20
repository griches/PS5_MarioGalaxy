/*
 * Super Mario Galaxy for the PlayStation 5 - libc names no module a title loads exports.
 *
 * The SDK's stubs let these link, but the import is null at run time and its
 * first call jumps to address 0. tools/link-title.sh binds each name to its
 * ps5dl_ answer here (its `shims` list must name every function below).
 *
 * SPDX-License-Identifier: MIT
 */
#include <errno.h>

/* Nothing a title writes to is a terminal: its output goes to klog. */
int
ps5dl_isatty(int fd)
{
   (void)fd;
   errno = ENOTTY;
   return 0;
}

/* One user, and no privilege to drop (SDL asks whether the process is setuid). */
int
ps5dl_getresuid(unsigned *real, unsigned *effective, unsigned *saved)
{
   *real = *effective = *saved = 0;
   return 0;
}

int
ps5dl_getresgid(unsigned *real, unsigned *effective, unsigned *saved)
{
   *real = *effective = *saved = 0;
   return 0;
}

/* No list of loaded modules to walk (Borealis's crash report names the module
 * of each frame with it): it reports addresses, which run-title.py symbolises. */
int
ps5dl_dl_iterate_phdr(int (*callback)(void *, unsigned long, void *), void *data)
{
   (void)callback;
   (void)data;
   return 0;
}

/* Files belong to no one in particular. */
int
ps5dl_fchown(int fd, unsigned owner, unsigned group)
{
   (void)fd;
   (void)owner;
   (void)group;
   return 0;
}

/* A title is one process: nothing can be started from it (SDL's process API,
 * Borealis's "restart" and "open a URL"). */
int
ps5dl_fork(void)
{
   errno = ENOSYS;
   return -1;
}

int
ps5dl_vfork(void)
{
   errno = ENOSYS;
   return -1;
}

int
ps5dl_setsid(void)
{
   errno = ENOSYS;
   return -1;
}

/* FreeBSD's _PC_NAME_MAX and _PC_PATH_MAX; no other limit is known. */
long
ps5dl_pathconf(const char *path, int name)
{
   (void)path;
   switch (name) {
   case 4:
      return 255;
   case 5:
      return 1024;
   default:
      errno = EINVAL;
      return -1;
   }
}

/* The network is not used: addresses have no names. */
int
ps5dl_getnameinfo(const void *address, unsigned length, char *host, unsigned host_size, char *service,
                  unsigned service_size, int flags)
{
   (void)address;
   (void)length;
   (void)host;
   (void)host_size;
   (void)service;
   (void)service_size;
   (void)flags;
   return 4; /* EAI_FAIL */
}

const char *
ps5dl_gai_strerror(int code)
{
   (void)code;
   return "name resolution is not available";
}

/* lstat does not work for a title (anything but a missing file's ENOENT made
 * std::filesystem::symlink_status report an occupied path, so Aurora refused to
 * create the memory card's save, and SQLite refused to open its databases). A
 * title's folders hold no symbolic links: stat answers the same. */
struct stat;
int stat(const char *path, struct stat *out);

int
ps5dl_lstat(const char *path, struct stat *out)
{
   return stat(path, out);
}
