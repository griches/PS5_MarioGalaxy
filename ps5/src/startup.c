/*
 * Super Mario Galaxy for the PlayStation 5 - what runs before main: standard
 * error into klog and the shell's splash dismissed, then /app0/env.txt, the
 * runtime's GALAXY_* options for a test run (the console starts a title with
 * no arguments or environment).
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

static void
load_env_file(void)
{
   char line[1024];
   FILE *file = fopen("/app0/env.txt", "r");

   if (file == NULL)
      return;
   while (fgets(line, sizeof(line), file) != NULL) {
      char *equals, *end = line + strcspn(line, "\r\n");

      *end = '\0';
      if (line[0] == '#' || (equals = strchr(line, '=')) == NULL || equals == line)
         continue;
      *equals = '\0';
      setenv(line, equals + 1, 1);
      fprintf(stderr, "env.txt: %s=%s\n", line, equals + 1);
   }
   fclose(file);
}

__attribute__((constructor)) static void
ps5_startup(void)
{
   platform_init(PS5_TITLE_NAME);
   load_env_file();
}
