#pragma once

#include <vector>
#include <filesystem>
#include <system_error>

#include "error.hpp"
#include "compiler.hpp"

namespace pgm {
	class compiler;

	// Manages a translation unit unit and it's object file.
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

		static
		std::vector<pgm::translation_unit>
		find_all(const std::filesystem::path &source_directory, const std::filesystem::path &object_directory, error &error);

		static
		std::vector<pgm::translation_unit>
		find_changed(const std::vector<pgm::translation_unit> &units, const pgm::compiler &compiler, error &error);
	};
}