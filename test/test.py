#!/usr/bin/python

import os
import subprocess

print("Testing...")

repo_root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
executable = os.path.join(repo_root, "bin", "main")
test_src = os.path.join(repo_root, "test", "src", "main.cpp")

# Run the executable with appropriate args.
popen = subprocess.Popen([executable, test_src, "-I./test/include"])
popen.wait()
if popen.returncode != 0:
	raise SystemExit(f"Process exited with return code {popen.returncode}.")

# # Check that it created an executable.
# process = subprocess.run([])
# if process.returncode != 0:
# 	raise Exception(f"Process exited with return code {process.returncode}")

print("Finished testing.")
