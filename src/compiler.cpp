#include "compiler.hpp"

#include <iostream>
#include <format>

#include "process.hpp"

std::vector<std::string> command_parts;

pgm::compiler::compiler(std::string executable, const std::vector<std::string> &arguments) {
	command_parts.reserve(arguments.size() + 1);
	command_parts.push_back(executable);
	command_parts.insert(command_parts.end(), arguments.begin(), arguments.end());
}

void
pgm::compiler::compile(const pgm::translation_unit &unit, error &error) const {
	std::vector<std::string> command = command_parts;
	do {
		// Build command vector for exec.
		// Pertinent args copied directly from "gcc --help":
		// -c                       Compile and assemble, but do not link.
		// -o <file>                Place the output into <file>.
		command.insert(command.end(), {"-c", unit.root_path, "-o", unit.object_path});

		// Run command.
		process::child child = process::exec(command, error);
		if (error) {
			break;
		}
		int exit_status = child.wait(error);
		if (error) {
			break;
		}

		// Check exit status
		if (exit_status != 0) {
			error
				.append(child.read_all_stderr_string(error))
				.append(std::format("Exit status {}.", exit_status))
			;
			break;
		}

		return;
	} while (false);

	// Build command string for error.
	std::string command_string;
	for (const std::string &part : command) {
		command_string += " " + part;
	}
	error.append(std::format("Error compiling source file \"{}\" to object file \"{}\" with command \"{}\".", unit.root_path.string(), unit.object_path.string(), command_string));
}

void
pgm::compiler::link(const std::vector<translation_unit> &units, std::string out_file, error &error) {
	std::vector<std::string> command = command_parts;
	do {
		command.insert(command.end(), {"-o", out_file});
		for (const translation_unit &unit : units) {
			command.push_back(unit.object_path);
		}

		process::child child = process::exec(command, error);
		if (error) {
			break;
		}
		int exit_status = child.wait(error);
		if (error) {
			break;
		}

		// Check exit status
		if (exit_status != 0) {
			error
				.append(child.read_all_stderr_string(error))
				.append(std::format("Exit status {}.", exit_status))
			;
			break;
		}

		return;
	} while (false);

	// Build command string for error.
	std::string command_string;
	for (const std::string &part : command) {
		command_string += " " + part;
	}
	error.append(std::format("Error linking final binary executable or library \"{}\" from {} object files with command \"{}\".", out_file, units.size(), command_string));
}

std::vector<std::string>
pgm::compiler::get_make_prerequisites(const std::string &file, error &error) const {
	do {
		// Run compiler with -MM to output a makefile rule.
		// Use -MT "" to remove the target at the start for simpler parsing.
		std::vector<std::string> command = command_parts;
		command.insert(command.end(), {file, "-MM", "-MT", ""});

		// Run command
		process::child child = process::exec(command, error);
		int exit_status = child.wait(error);
		if (error) {
			break;
		}

		// Check status code.
		if (exit_status != 0) {
			std::string command_string;
			for (const std::string &part : command) {
				command_string += " " + part;
			}
			error
				.append(child.read_all_stderr_string(error))
				.append(std::format("Exit status {} from command \"{}\".", exit_status, command_string))
			;
			break;
		}

		// Read make rule from stdout.
		// Rule with escaped newlines and maybe other stuff.
		std::string escaped_rule = child.read_all_stdout_string(error);
		if (error) {
			break;
		}

		// Parse prerequisites from make rule.
		std::vector<std::string> prerequisites;
		std::string prerequisite;
		bool escaping = false;
		bool delimiting = true;
		constexpr char delimiter = ' ';
		constexpr char escape = '\\'; // Used to escape newlines and delimiters in the middle of prerequisites.
		for (std::string::iterator iterator = escaped_rule.begin() + 1 /* +1 to skip leading colon */; iterator != escaped_rule.end(); ++iterator) {
			char c = *iterator;

			if (c == escape) {
				escaping = true;
				continue;
			}

			if (escaping) {
				escaping = false;
				// Keep escaped escape characters "\\".
				if (c == escape) {
					prerequisite += c;
					continue;
				}
				// Keep escaped delimiters in the prerequisite.
				if (c == delimiter) {
					prerequisite += c;
					continue;
				}
				// Ignore escaped newlines and any else that is escaped.
				continue;
			}

			// Push prerequisiste once reached end of line.
			// Prerequisites are terminated with an unescaped newline.
			if (c == '\n') {
				prerequisites.push_back(prerequisite);
				prerequisite = std::string();
				break;
			}

			// Store prerequisite when we hit a delimiter, but not on multiple delimiters in a row.
			if (c == delimiter) {
				if (!delimiting) {
					prerequisites.push_back(prerequisite);
					prerequisite = std::string();
					delimiting = true;
				}
				continue;
			}
			delimiting = false;

			prerequisite += c;
		}

		// Push the last prerequisite that was not pushed yet because no delimiter at end.
		if (prerequisite.size() > 0) {
			prerequisites.push_back(prerequisite);
		}
		
		return prerequisites;
	} while (false);

	error.append(std::format("Error getting make prerequisites for file \"{}\".", file));
	return std::vector<std::string>();
}