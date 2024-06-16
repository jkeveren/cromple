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
	1. Build with "./build.sh".
	2. Install with "./install.sh" (copies "bin/cromple" to "/usr/local/bin").
		This will ask for password. Read the script first (it's one line).

Updating
	1. Pull updates with "git pull".
	2. Build with "./build.sh".
	3. Install again with "./install.sh" (replaces old binary).

Automated Tests
	Tests are in the "test" directory. Run "test/test.py".
	"./watch.sh" uses entr (https://eradman.com/entrproject) to rebuild and run tests when relevant files change.
