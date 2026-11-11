#!/bin/sh
# Vile terminal banner — printed once when foot opens under Vile,
# then hands over to the user's interactive shell.
#
# The compositor launches foot with this script (see vile_spawn_foot()
# in src/main.c) instead of plain `foot`, so every terminal — startup,
# Windows+Enter, dock click — opens with the same short banner.
# The banner is cosmetic only: after printing it the script execs the
# user's own shell, interactive, so typing and job control work
# exactly as they would in a plain foot.
printf '\033[1;35m  VILE OS\033[0m  ·  v0.5.6\n'
printf '\033[2m  Windows+Enter new terminal · Windows+Q close · Windows+L lock\033[0m\n'
printf '\033[2m  Windows+J/K switch window · Windows+F fullscreen · Windows+Escape unfocus\033[0m\n'
printf '\n'
exec "${SHELL:-/bin/bash}" -i
