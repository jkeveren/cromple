#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace pgm {
	class arguments {
		public:
		std::filesystem::path source_directory;
		std::filesystem::path object_directory;
		std::filesystem::path out_file;
		std::string compiler;
		std::vector<std::string> compiler_arguments;
		bool help = false;

		// Parse program arguments into an instance of arguments.
		static pgm::arguments
		parse(int argc, char **argv);
	};
}