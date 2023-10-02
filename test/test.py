#!/usr/bin/python

import os
import subprocess
import pathlib

print("Testing...")

repo_root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
subject_executable = os.path.join(repo_root, "bin", "cromple")

# Test directories.
test_root = os.path.join(repo_root, "test")
source_directory = os.path.join(test_root, "src")
object_directory = os.path.join(test_root, "objects")
include_directory = os.path.join(test_root, "include")

# Test files.
main_source = os.path.join(source_directory, "main.cpp")
main_object = os.path.join(object_directory, "main.cpp.o")
test_executable = os.path.join(test_root, "executable")

# Delete object files generated by previous tests.
directory_iterator = os.scandir(object_directory)
for entry in directory_iterator:
	if entry.name.endswith(".o"):
		os.remove(entry.path)

# Delete executable generated by previous tests.
if os.path.isfile(test_executable):
	os.remove(test_executable)

command = [subject_executable, "--compiler", "/usr/bin/g++", "--source", source_directory, "--objects", object_directory, "-I", include_directory, "-o", test_executable]
print("Compilation command used in testing:", " ".join(command))

def compile():
	popen = subprocess.Popen(command)
	popen.wait()
	if popen.returncode != 0:
		raise SystemExit(f"Process exited with non-zero return code {popen.returncode}.")

compile()

print("Test that object files are created.")
object_files = ["main.cpp.o", "symlink.c.o"]
for object_file in object_files:
	object_path = os.path.join(object_directory, object_file)
	if not os.path.isfile(object_path):
		raise SystemExit(f"Object file was not created or is not a file: {object_path!r}.")

print("Test that an executable is created.")
if not os.path.isfile(test_executable):
	raise SystemExit(f"Executable was not generated: {test_executable!r}.");

print("Test that object files are not recompiled when nothing has changed.")
# It does not matter if the executable is recompiled unneccessarily because it is only linking and should be fast.
# The command will only really be used when something has changed anyway and if anything has changed then linking will be required.
main_object_time = os.stat(main_object).st_mtime
compile()
if os.stat(main_object).st_mtime != main_object_time:
	raise SystemExit("main.cpp was unnecessarily recompiled when nothing was touched.")

print("Test that object file and executable is recompiled when a source file is touched.")
main_object_time = os.stat(main_object).st_mtime
executable_time = os.stat(test_executable).st_mtime
pathlib.Path(main_source).touch()
compile()
if os.stat(main_object).st_mtime == main_object_time:
	raise SystemExit("main.cpp was not recompiled when touched.");
if os.stat(test_executable).st_mtime == executable_time:
	raise SystemExit("Executable was not recompiled when source touched.");

print("Test that the corresponding object files are recompiled when a header file is touched.")
mod_time = os.stat(main_object).st_mtime
pathlib.Path(os.path.join(include_directory, "touch header.hpp")).touch()
compile()
if os.stat(main_object).st_mtime == mod_time:
	raise SystemExit("main.cpp was not recompiled when \"touch header.hpp\" was touched.")

print("Test that object file is recompiled when nested headers are touched.")
mod_time = os.stat(main_object).st_mtime
pathlib.Path(os.path.join(include_directory, "deep_touch_header.hpp")).touch()
compile()
if os.stat(main_object).st_mtime == mod_time:
	raise SystemExit("main.cpp was not recompiled when deep_touch_header.hpp was touched.")

print("Test that executable works.")
popen = subprocess.Popen(test_executable)
popen.wait()
if popen.returncode != 0:
	raise SystemExit(f"Test executable exited with non-zero return code {popen.returncode}.")

print("All tests passed.")
