#!/bin/sh
# Eternal Darkness for the PlayStation 5 - compile with an init array (a compiler launcher).
#
#   cc-init-array.sh COMPILER ARGUMENT...
#
# Clang's PS5 target writes constructors to .ctors, last to first, for a
# start-up that walks them backwards, and its driver refuses -fuse-init-array.
# PS5_Vulkan's CRT walks the merged array forwards, so a unit's initialisers
# ran in reverse: Dusklight's inline tables (one initialiser each, the later
# built from the earlier) read a std::map that was not constructed yet.
#
# The switch is the driver's alone: it passes -fno-use-init-array to the
# compiler proper. This asks the driver for that command (-###), drops the
# flag and runs it, which gives .init_array in the order the source has.
# Anything that is not one compile (-c) runs unchanged.
#
# SPDX-License-Identifier: MIT
case " $* " in
*" -c "*) ;;
*) exec "$@" ;;
esac
jobs=$("$@" '-###' 2>&1 | grep -- '^ *"') || exec "$@"
# one job, the compiler proper: a .S file is two (the preprocessor, then the assembler)
[ "$(printf '%s\n' "$jobs" | wc -l)" -eq 1 ] || exec "$@"
job=$(printf '%s\n' "$jobs" | grep -- ' "-cc1" ') || exec "$@"
case $job in
*'$'* | *'`'*) exec "$@" ;; # nothing the shell would expand
esac
eval "exec $(printf '%s' "$job" | sed 's/ "-fno-use-init-array"//')"
