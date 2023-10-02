#pragma once

#include <vector>
#include <filesystem>
#include <system_error>

#include "error.hpp"
#include "compiler.hpp"

namespace pgm {
	class compiler;

	// Manages a translation unit unit and it's object file.
	// A translaation unit typically refers to a source file after it has been pre-processed so all includes are resolved.
	// This class manages a source file, all other files that it includes and the object file that the translation unit will be compiled to.
	class translation_unit {
		public:
		// Translation units are a file after pre-processing, so will include all header contents.
		// root_path refers to the source file that, optionally, #include all of those headers.
		const std::filesystem::path root_path;
		const std::filesystem::path object_path; // Path of the object file that compilation should generate.

		translation_unit(const std::filesystem::path &root_path, const std::filesystem::path &object_directory);

		// Converts a source path to an object path
		static std::filesystem::path
		source_to_object(const std::filesystem::path &root_path, const std::filesystem::path &object_directory);

		// Checks if a translation units object file is out of date or non-existant.
		bool
		object_is_outdated(const pgm::compiler &compiler, error &error) const;

		// Find all translation_units in source_directory.
		static
		std::vector<pgm::translation_unit>
		find_all(const std::filesystem::path &source_directory, const std::filesystem::path &object_directory, error &error);

		// Find changed translation_units in units.
		// compiler is used to parse #include directives from translation units.
		static
		std::vector<pgm::translation_unit>
		find_changed(const std::vector<pgm::translation_unit> &units, const pgm::compiler &compiler, error &error);
	};
}