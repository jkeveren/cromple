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

// I know, I know, don't include c(++) files.
// I'm doing this to keep the build script dumb as hell.
// It would be cool to not use another build tool to build this build tool.
// Maybe we will make this tool self building in the future.
// This does have cons but I think it's ok given how simple this program is.
#include "error.cpp"
#include "arguments.cpp"
#include "translation_unit.cpp"
#include "compiler.cpp"

int main(int argc, char *argv[]) {
	// Usage: <this executable> [--compiler COMPILER=g++] [--source SOURCE_DIRECTORY=src] [--objects OBJECT_DIRECTORY=objects] [-o OUTPUT_FILE=a.out] [COMPILER_OPTIONS]
	// Order independent.
	// TODO: add help and usage print on error.
	
	pgm::arguments arguments = pgm::arguments::parse(argc, argv);

	// Assert that arguments.object_directory exists and is a directory.
	if (!std::filesystem::is_directory(arguments.object_directory)) {
		return pgm::error(std::format("Object directory \"{}\" is not a directory. Create it or change it with the \"--objects\" argument.", arguments.object_directory.string())).print();
	}

	// Find translation units.
	std::expected<std::vector<pgm::translation_unit>, pgm::error> expected_units = pgm::translation_unit::find_all(arguments.source_directory, arguments.object_directory);
	if (!expected_units) {
		return expected_units.error().print();
	}
	std::vector<pgm::translation_unit> units = expected_units.value();

	// Find units that have changed.
	std::expected<std::vector<pgm::translation_unit>, pgm::error> expected_changed_units = pgm::translation_unit::find_changed(units, arguments.compiler_arguments);
	if (!expected_changed_units) {
		return expected_changed_units.error().print();
	}
	std::vector<pgm::translation_unit> changed_units = expected_changed_units.value();

	// Compiler.
	pgm::compiler compiler(arguments.compiler, arguments.compiler_arguments);

	// Compile objects.
	for (const pgm::translation_unit &unit : changed_units) {
		pgm::error error = compiler.compile(unit);
		if (error) {
			return error.print();
		}
	}

	if (units.size() > 0) {
		pgm::error error = compiler.link(units, arguments.out_file);
		if (error) {
			return error.print();
		}
	}

	return 0;
}
