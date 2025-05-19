Cromple

C(++) compiler wrapper that manages compilation dependencies.

Features and Benefits
- No need for a makefile.
- Only recompiles source files that have changed.
- Recompiles source files when included headers are changed.
- Parses source files and headers to determine dependencies using your compilers -M options.


Usage
  command: "cromple [OPTION]..."

  Options that are understood by cromple are used by cromple and the rest are
  passed on to the compiler. Option names have been chosen carefully to avoid
  collisions.

  Possible options that are used by cromple:

  --compiler COMPILER         Defaults to "/usr/bin/g++". Compiler to use when
                              compiling the source.
  --oource SOURCE_DIRECTORY   Defaults to "src". Directory that contains the
                              source files for the project.
  --objects OBJECTS_DIRECTORY Defaults to "obj". Directory to put object files
                              when compiling translation units.
  -o OUTPUT_FILE              Defaults to "a.out". This gets passed directly to
                              the compiler but only in the final linking step so
                              we intercept it but do no modification.

  All other options are passed directly to the compiler during both compilation
  and linking without modification.

  It's recommended to create a simple build script using the above command to
  store options. Here's an example that links some OpenGL libraries and
  specifies some error options:

  "
  #!/usr/bin/bash

  cromple \
  -lglfw -lGL -lGLEW \
  -std=c++23 \
  -o bin/program \
  -fdiagnostics-color \
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
  -Wno-error=unused-variable
  "

Building from source
  run "./build.sh". The executable is compiled to "bin/cromple".

Installation
1. Build with "./build.sh".
2. Install with "./install.sh" (copies "bin/cromple" to "/usr/local/bin").
   This will ask for password. Read the script first (it's one line).

Updating
1. Pull updates with "git pull".
2. Build with "./build.sh".
3. Install again with "./install.sh" (replaces old binary).

Automated Tests
- Tests are in the "test" directory. Run "test/test.py".
- "./watch.sh" uses entr (https://eradman.com/entrproject) to rebuild and run
  tests when relevant files change.

