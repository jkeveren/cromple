#include <iostream>
#include <format>
#include <expected>
#include <vector>
#include <map>
#include <filesystem>
#include <regex>
#include <chrono>

#include <cstring>

#include <clang-c/Index.h>

#include "arguments.hpp"
#include "error.hpp"
#include "translation_unit.hpp"
#include "compiler.hpp"

int main(int argc, char *argv[]) {
	// Usage: <this executable> [--compiler COMPILER=g++] [--source SOURCE_DIRECTORY=src] [--objects OBJECT_DIRECTORY=objects] [-o OUTPUT_FILE=a.out] [COMPILER_OPTIONS]
	// Order independent.
	// TODO: add help and usage print on error.

	pgm::error error;
	
	pgm::arguments arguments = pgm::arguments::parse(argc, argv);

	// Assert that arguments.source_directory exists and is a directory.
	if (!std::filesystem::is_directory(arguments.source_directory)) {
		return pgm::error(std::format("Source directory \"{}\" is not a directory. Create it or change it with the \"--source\" argument.", arguments.source_directory.string())).print();
	}

	// Assert that arguments.object_directory exists and is a directory.
	if (!std::filesystem::is_directory(arguments.object_directory)) {
		return pgm::error(std::format("Object directory \"{}\" is not a directory. Create it or change it with the \"--objects\" argument.", arguments.object_directory.string())).print();
	}

	// Find translation units.
	std::vector<pgm::translation_unit> units = pgm::translation_unit::find_all(arguments.source_directory, arguments.object_directory, error);
	if (error) {
		return error.print();
	}

	// Compiler.
	pgm::compiler compiler(arguments.compiler, arguments.compiler_arguments);

	// Find units that have changed.
	std::vector<pgm::translation_unit> changed_units = pgm::translation_unit::find_changed(units, compiler, error);
	if (error) {
		return error.print();
	}

	// Compile objects.
	for (const pgm::translation_unit &unit : changed_units) {
		compiler.compile(unit, error);
		if (error) {
			return error.print();
		}
	}

	if (units.size() > 0) {
		compiler.link(units, arguments.out_file, error);
		if (error) {
			return error.print();
		}
	}

	return 0;
}
