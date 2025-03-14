#!/bin/sh
gdb -x memcry.gdb --args ./test -p "$@"
