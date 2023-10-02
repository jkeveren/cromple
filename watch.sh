#!/usr/bin/bash

ls src/* compile.sh test/test.py | entr -crs "./build.sh && ./test/test.py"
