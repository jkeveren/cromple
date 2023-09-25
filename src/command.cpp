#pragma once

namespace pgm {
	class command {
		private:
		bool running = false;
		::FILE *pipe = NULL;

		public:
		enum error_reasons {
			error_reason_command_already_running = pgm::error::custom_reason_start,
		};

		std::string command_string;
		const char *type; // passed to popen.

		command(std::string command_string, const char *type) : command_string{command_string}, type{type} {}

		// Starts process and returns pipe. This does not wait for the process to exit.
		std::expected<::FILE *, pgm::error>
		start() {
			if (running) {
				return std::unexpected(pgm::error(
					std::format("Command is already running: \"{}\".", command_string),
					error_reason_command_already_running
				));
			}

			running = true;

			pipe = popen(command_string.c_str(), type);
			if (pipe == NULL) {
				return std::unexpected(pgm::error::strerror().append(std::format("Error running command: \"{}\" with type: \"{}\"", command_string, type)));
			}
			return pipe;
		}

		// Waits for process to exit and returns status code.
		std::expected<int, pgm::error>
		wait() {
			// Wait for process to exit one way or another.
			// Sometimes? returns an incorrect status code that can be decoded with WEXITSTATUS later. Why is it this complex?
			int code = pclose(pipe);
			if (code == -1) {
				return std::unexpected(pgm::error::strerror().append("Error closing compilation command."));
			}

			// Allow the command to be run again.
			running = false;

			// If terminated by a signal.
			if (WIFEXITED(code) == 0) {
				// Should return throw? Is it an error.
				// Inside a generic command runner class this should probably just be returned.
				return std::unexpected(pgm::error::strerror().append(std::format("Compilation command exited abnormally.")));
			}

			// Get the actual status code.
			int status = WEXITSTATUS(code);

			return status;
		}

		// Runs the command to completion and returns status code.
		std::expected<int, pgm::error> run() {
			std::expected<::FILE *, pgm::error> pipe = start();
			if (!pipe) {
				return std::unexpected(pipe.error().append("Error starting command.", pipe.error().reason));
			}
			std::expected<int, pgm::error> status = wait();
			if (!status) {
				return std::unexpected(status.error().append("Error wating for command to complete.", pipe.error().reason));
			}
			return status;
		}
	};
}