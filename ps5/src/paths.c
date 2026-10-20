/*
 * Super Mario Galaxy for the PlayStation 5 - the title folder as the working folder.
 *
 * The runtime and its libraries may name files relative to where they run
 * (Nebula's settings and diagnostics; the game data is named in full). A title
 * on the console has no working folder it can use: chdir("/app0") is refused
 * (EPERM) and libc's getcwd is a null import (its __getcwd is only in
 * libkernel_sys). So the path functions the game and its libraries use are
 * wrapped at link time (tools/link-title.sh: --wrap), and a relative path is
 * taken from /app0, the title's folder. Absolute paths (the driver's caches,
 * /app0/... already) pass through. getcwd answers /app0, and chdir to it works.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TITLE_DIR "/app0"

/* The platform layer's (ps5platform/libc.h), bound by PS5_Vulkan's link recipe */
DIR *ps5_opendir(const char *path);
int ps5_access(const char *path, int mode);
char *ps5_realpath(const char *path, char *resolved);

static const char *
title_path(const char *path, char *buffer, size_t size)
{
   if (path == NULL || path[0] == '/' || path[0] == '\0')
      return path;
   while (path[0] == '.' && path[1] == '/')
      path += 2;
   if (path[0] == '.' && path[1] == '\0')
      return TITLE_DIR;
   snprintf(buffer, size, TITLE_DIR "/%s", path);
   return buffer;
}

#define PATH_BUFFER char buffer[1024]
#define TITLE_PATH(path) title_path((path), buffer, sizeof(buffer))

FILE *__real_fopen(const char *path, const char *mode);
FILE *
__wrap_fopen(const char *path, const char *mode)
{
   PATH_BUFFER;
   return __real_fopen(TITLE_PATH(path), mode);
}

int __real_open(const char *path, int flags, ...);
int
__wrap_open(const char *path, int flags, ...)
{
   PATH_BUFFER;
   mode_t mode = 0;
   if (flags & O_CREAT) {
      va_list arguments;
      va_start(arguments, flags);
      mode = (mode_t)va_arg(arguments, int);
      va_end(arguments);
   }
   return __real_open(TITLE_PATH(path), flags, mode);
}

int __real_stat(const char *path, struct stat *status);
int
__wrap_stat(const char *path, struct stat *status)
{
   PATH_BUFFER;
   return __real_stat(TITLE_PATH(path), status);
}

int __real_lstat(const char *path, struct stat *status);
int
__wrap_lstat(const char *path, struct stat *status)
{
   PATH_BUFFER;
   return __real_lstat(TITLE_PATH(path), status);
}

int __real_mkdir(const char *path, mode_t mode);
int
__wrap_mkdir(const char *path, mode_t mode)
{
   PATH_BUFFER;
   return __real_mkdir(TITLE_PATH(path), mode);
}

int __real_rmdir(const char *path);
int
__wrap_rmdir(const char *path)
{
   PATH_BUFFER;
   return __real_rmdir(TITLE_PATH(path));
}

int __real_unlink(const char *path);
int
__wrap_unlink(const char *path)
{
   PATH_BUFFER;
   return __real_unlink(TITLE_PATH(path));
}

int __real_remove(const char *path);
int
__wrap_remove(const char *path)
{
   PATH_BUFFER;
   return __real_remove(TITLE_PATH(path));
}

int __real_rename(const char *from, const char *to);
int
__wrap_rename(const char *from, const char *to)
{
   char buffer[1024], buffer2[1024];
   return __real_rename(title_path(from, buffer, sizeof(buffer)),
                        title_path(to, buffer2, sizeof(buffer2)));
}

DIR *
__wrap_opendir(const char *path)
{
   PATH_BUFFER;
   return ps5_opendir(TITLE_PATH(path));
}

int
__wrap_access(const char *path, int mode)
{
   PATH_BUFFER;
   return ps5_access(TITLE_PATH(path), mode);
}

char *
__wrap_realpath(const char *path, char *resolved)
{
   PATH_BUFFER;
   return ps5_realpath(TITLE_PATH(path), resolved);
}

/* The working folder, as far as anything asks: the title's */
char *
ps5ed_getcwd(char *buffer, size_t size)
{
   if (buffer == NULL) {
      size = size ? size : sizeof(TITLE_DIR);
      buffer = malloc(size);
      if (buffer == NULL)
         return NULL;
   }
   if (size < sizeof(TITLE_DIR)) {
      errno = ERANGE;
      return NULL;
   }
   memcpy(buffer, TITLE_DIR, sizeof(TITLE_DIR));
   return buffer;
}

int
ps5ed_chdir(const char *path)
{
   if (path != NULL && (strcmp(path, TITLE_DIR) == 0 || strcmp(path, TITLE_DIR "/") == 0 ||
                        strcmp(path, ".") == 0))
      return 0;
   errno = EPERM;
   return -1;
}
