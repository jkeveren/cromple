#include "command.hpp"

#include <format>

pgm::command::command(std::string command_string, const char *type) : command_string{command_string}, type{type} {}

// Starts process and returns pipe. This does not wait for the process to exit.
::FILE *
pgm::command::start(error &error) {
	pipe = popen(command_string.c_str(), type);
	if (pipe == NULL) {
		error.strerror().append(std::format("Error running command: \"{}\" with type: \"{}\"", command_string, type));
		return nullptr;
	}
	return pipe;
}

// Waits for process to exit and returns status code.
int
pgm::command::wait(error &error) {
	// Wait for process to exit one way or another.
	// Sometimes? returns an incorrect status code that can be decoded with WEXITSTATUS later. Why is it this complex?
	int code = pclose(pipe);
	if (code == -1) {
		error.strerror().append("Error closing compilation command.");
		return 0;
	}

	// If terminated by a signal.
	if (WIFEXITED(code) == 0) {
		error.strerror().append(std::format("Compilation command exited abnormally."));
		return 0;
	}

	// Get the actual status code.
	int status = WEXITSTATUS(code);

	return status;
}

// Runs the command to completion and returns status code.
int
pgm::command::run(error &error) {
	[[maybe_unused]] ::FILE *pipe = start(error);
	if (error) {
		error.append("Error starting command.");
		return 0;
	}
	int status = wait(error);
	if (error) {
		error.append("Error wating for command to complete.");
		return 0;
	}
	return status;
}