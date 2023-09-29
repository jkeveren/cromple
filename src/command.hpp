#pragma once

#include <string>

#include <stdio.h>

#include "error.hpp"

namespace pgm {
	class command {
		private:
		::FILE *pipe = NULL;

		public:
		enum error_reasons {
			error_reason_command_already_running = pgm::error::custom_reason_start,
		};

		std::string command_string;
		const char *type; // passed to popen.

		command(std::string command_string, const char *type);

		// Starts process and returns pipe. This does not wait for the process to exit.
		::FILE *
		start(error &error);

		// Waits for process to exit and returns status code.
		int
		wait(error &error);

		// Runs the command to completion and returns status code.
		int
		run(error &error);
	};
}