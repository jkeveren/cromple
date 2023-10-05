#!/usr/bin/bash

# Just using a dumb slow build script for this because the purpose of this tool is to replace complicated build tools so trying to avoid using one here.

echo Compiling... This will take 15s or so. This is a purposefully dumb build script. We don\'t self build yet. If this is taking too long, remove the \"-O3 \\\" line from this build script. It will make a larger binary but I don\'t know how long it will take to do -O3 on low power hardware.

g++ \
-O3 \
-std=c++20 \
`# Stop on the first error to make it easier to read.`\
-Wfatal-errors \
`# Make all warnings error so we don't miss them.`\
-Werror \
`# Enable most warnings.`\
-Wall -Wextra -Wpedantic \
`# Enable some number warnings.`\
-Wfloat-equal -Wsign-conversion -Wfloat-conversion \
`# Stop "-Werror" from turning some warnings into errors.\
# Unused things should not exist in the final versions of code but are ok while working on things so warnings are approriate.`\
-Wno-error=unused-but-set-parameter \
-Wno-error=unused-but-set-variable \
-Wno-error=unused-function \
-Wno-error=unused-label \
-Wno-error=unused-local-typedefs \
-Wno-error=unused-parameter \
-Wno-error=unused-result \
-Wno-error=unused-variable \
-Wno-error=unused-value \
`# Put it in the bin.`\
-o bin/cromple \
`# Yea compile that one.`\
src/*.cpp

status=$?

echo End.

exit $status
