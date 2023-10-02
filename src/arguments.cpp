#include "arguments.hpp"

#include <map>

pgm::arguments
pgm::arguments::parse(int argc, char **argv) {
	pgm::arguments arguments;

	std::string source_directory;
	std::string object_directory;
	std::string out_file;

	std::map<std::string, std::string *> argument_pointers {
		{"--source", &source_directory}, // source_directory must be provided by a named argument because we can't know which arguments belong to a previous compiler argument like "library" in "-l library". We can't accurately parse all compiler args.
		{"--objects", &object_directory},
		{"-o", &out_file},
		{"--compiler", &arguments.compiler},
	};

	// Points to where to store the next option. When finding "--compiler" point this to compiler so it gets set in the next loop.
	std::string *argument_pointer = nullptr;

	// Start at 1st arg because the 0th arg is this executable.
	for (int i = 1; i < argc; i++) {
		std::string arg = std::string(argv[i]);

		// Store second part of non-compiler key-value arguments, e.g. "--compiler /usr/bin/g++" or "-o out"
		if (argument_pointer != nullptr) {
			*argument_pointer = arg;
			argument_pointer = nullptr;
			continue;
		}

		// Parse 
		if (arg[0] == '-') {
			// Point argument_pointer to place to store next argument.
			std::map<std::string, std::string *>::iterator flag_iterator = argument_pointers.find(arg);
			// Is this a non-compiler key-value argument?
			if (flag_iterator != argument_pointers.end()) {
				argument_pointer = flag_iterator->second;
				continue;
			}

			if (arg == "--help" || arg == "-h" || arg == "-?") {
				arguments.help = true;
			}
		}

		arguments.compiler_arguments.push_back(arg);
		continue;
	}

	// Default arguments.
	if (source_directory.empty()) {source_directory = "src";}
	if (object_directory.empty()) {object_directory = "obj";}
	if (out_file.empty()) {out_file = "a.out";}
	if (arguments.compiler.empty()) {arguments.compiler = "/usr/bin/g++";}

	// Convert to paths.
	arguments.source_directory = std::filesystem::path(source_directory);
	arguments.object_directory = std::filesystem::path(object_directory);
	arguments.out_file = std::filesystem::path(out_file);

	return arguments;
}