#!/usr/bin/bash

# Just using a dumb slow build script for this because the purpose of this tool is to replace complicated build tools so trying to avoid using one here.

echo Compiling... This will take 10s or so. This is a purposefully dumb build script.

g++ \
-std=c++20 \
-Wfatal-errors -Werror \
-Wall -Wextra -Wpedantic \
-Wfloat-equal -Wsign-conversion -Wfloat-conversion \
-Wno-error=unused-but-set-parameter \
-Wno-error=unused-but-set-variable \
-Wno-error=unused-function \
-Wno-error=unused-label \
-Wno-error=unused-local-typedefs \
-Wno-error=unused-parameter \
-Wno-error=unused-result \
-Wno-error=unused-variable \
-Wno-error=unused-value \
-o bin/cromple \
src/*.cpp

status=$?

echo End.

exit $status
