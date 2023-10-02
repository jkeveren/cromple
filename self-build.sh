#!/usr/bin/bash

./bin/cromple \
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
-lclang \
`# Put it in the bin.`\
-o bin/cromple

status=$?

echo End.

exit $status