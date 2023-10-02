Cromple
	C(++) compiler wrapper that manages compilation dependencies.
	- No need for a makefile.
	- Only recompiles source that has changed.
	- Parses source files and headers to determine dependencies.
	- Invalidates object files when included headers are changed; recursively too.

Usage
	Usage: cromple [--compiler COMPILER (default: /usr/bin/g++)] [--source SOURCE_DIRECTORY (default: src)] [--objects OBJECT_DIRECTORY (default: obj)] [-o OUTPUT_FILE (default: a.out)] [COMPILER_OPTIONS]

Building from source
	run "./build.sh".
	it outputs to "bin/cromple".

Installation
	1. clone to somewhere you want to keep (~/.opt/cromple).
	2. Build with "./build.sh"
	3. symlink "/usr/local/bin/cromple" to "bin/cromple".
			"ln -s "

Updating
	1. "git pull"
	2. "./build.sh", your symlink is already in the right place from the installation.

Automated Tests
	Tests are in the "test" directory. Run "test/test.py".
	"watch.sh" uses entr(https://eradman.com/entrproject) to rebuild and run tests.
