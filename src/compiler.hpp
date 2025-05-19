#pragma once

#include <vector>
#include <string>

#include "error.hpp"
#include "translation_unit.hpp"


namespace pgm {
	class translation_unit;
	
	class compiler {
		// Vector of compiler and arguments to run the compiler.
		std::vector<std::string> command_parts;
		
		public:
		compiler(std::string executable, const std::vector<std::string> &arguments);

		// Compiles source at unit.root_path to unit.object_path.
		void
		compile(const pgm::translation_unit &unit, error &error) const;

		// Links objects at all object_paths in units to an output binary at out_file.
		void
		link(const std::vector<pgm::translation_unit> &units, std::string out_file, error &error);

		// Get the make rule prerequisites generated from compiler -MM option.
		std::vector<std::string>
		get_make_prerequisites(const std::string &file, error &error) const;
	};
}
