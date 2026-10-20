/* Super Mario Galaxy for the PlayStation 5 - vkd3d's configuration for the
 * console (FreeBSD's libc in the payload SDK, no dynamic loader: Vulkan comes
 * from the RADV linked into the title). What configure finds in the Linux VM,
 * less the loader, gettid, thread names, program_invocation_name and strtof_l.
 * SPDX-License-Identifier: GPL-3.0-only */
#define HAVE_ATOMIC_EXCHANGE_N 1
#define HAVE_BUILTIN_ADD_OVERFLOW 1
#define HAVE_BUILTIN_CLZ 1
#define HAVE_BUILTIN_CTZ 1
#define HAVE_BUILTIN_POPCOUNT 1
#define HAVE_DECL_PROGRAM_INVOCATION_NAME 0
#define HAVE_INTTYPES_H 1
#define HAVE_PTHREAD_H 1
#define HAVE_SPIRV_UNIFIED1_GLSL_STD_450_H 1
#define HAVE_SPIRV_UNIFIED1_SPIRV_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYNC_ADD_AND_FETCH 1
#define HAVE_SYNC_BOOL_COMPARE_AND_SWAP 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_UNISTD_H 1
#define HAVE_VULKAN_VULKAN_H 1
#define HAVE_WCHAR_H 1
#define PACKAGE "vkd3d"
#define PACKAGE_NAME "vkd3d"
#define PACKAGE_STRING "vkd3d 2.1"
#define PACKAGE_TARNAME "vkd3d"
#define PACKAGE_VERSION "2.1"
#define SONAME_LIBVULKAN "libvulkan.so.1"
#define VERSION "2.1"
