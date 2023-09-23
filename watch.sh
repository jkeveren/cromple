#!/usr/bin/bash

ls src/* compile.sh test/test.py | entr -crs "./compile.sh && ./test/test.py"
