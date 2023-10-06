#!/usr/bin/bash

# This is just like ./build.sh except that it uses cromple itself to manage the build.

./bin/cromple \
-O3 \
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
-o bin/cromple

status=$?

echo Done.

exit $status
