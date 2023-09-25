#pragma once

#include "error.cpp"
#include "command.cpp"

namespace pgm {
	class compiler {
		std::string executable; // "g++", "clang" etc.
		std::vector<std::string> arguments;
		
		public:
		compiler(std::string executable, const std::vector<std::string> &arguments) : executable{executable}, arguments{arguments} {}

		pgm::error compile(const pgm::translation_unit &unit) const {
			// Pertinent args coppied directly from "gcc --help" command:
			// -c                       Compile and assemble, but do not link.
			// -o <file>                Place the output into <file>.
			std::string command_string = std::format("{} -c {} -o {}", executable, unit.root_path.string(), unit.object_path.string());
			for (const std::string &argument : arguments) {
				command_string += " " + argument;
			}

			pgm::command command(command_string, "r");
			std::expected<int, pgm::error> expected_status = command.run();
			if (!expected_status) {
				return expected_status.error().append(std::format(
					"Error compiling source file \"{}\" to object file \"{}\" with command \"{}\".",
					unit.root_path.string(), unit.object_path.string(), command_string
				));
			}

			if (expected_status.value() != 0) {
				return pgm::error(std::format(
					"Exit code {} when compiling source file \"{}\" to object file \"{}\" with command \"{}\".",
					expected_status.value(), unit.root_path.string(), unit.object_path.string(), command_string
				));
			}

			return pgm::error();
		}

		pgm::error link(const std::vector<pgm::translation_unit> &units, std::string out_file) {
			// Build link command.
			std::string command_string = std::format("{} -o {}", executable, out_file);
			for (const std::string &argument : arguments) {
				command_string += " " + argument;
			}
			// Add all object files as input to compiler.
			for (const pgm::translation_unit &unit : units) {
				command_string += " " + unit.object_path.string();
			}

			// Perform linking.
			pgm::command command(command_string, "r");
			std::expected<int, pgm::error> expected_status = command.run();
			if (!expected_status) {
				return expected_status.error().append(std::format(
					"Error compiling {} object files into a final binary \"{}\" with command \"{}\".",
					out_file, units.size(), command_string
				));
			}

			if (expected_status.value() != 0) {
				return pgm::error(std::format(
					"Exit code {} when compiling {} object files into a final binary executable or library \"{}\" with command \"{}\".",
					expected_status.value(), units.size(), out_file, command_string
				));
			}
			return pgm::error();
		}

		// std::expected<std::filesystem::path, pgm::error> get_recursively_included_files(const std::filesystem::path &file) {

		// }
	};
}