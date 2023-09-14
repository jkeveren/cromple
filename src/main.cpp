#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <format>
#include <expected>

#include <cstring>

// Using wrapper namespace to avoid global namespace because it is poluted by c crap that gets included by c++ includes.
// Access all global names with "::<name>" e.g. "::FILE" or "::error_t".
namespace program_n {

	class error_t {
		public:

		// int error = 0;
		// TODO: implement machine readable error identifiers.
		// Make errors stackable so very specifc errors can be caught.
		std::string message_stack = "";

		error_t(std::string message) : message_stack{message} {}

		error_t append(std::string message) {
			message_stack += message + "\n";
			return *this;
		}
	};

	class command_t {
		private:
		bool running = false;
		::FILE *pipe = NULL;

		public:
		// enum class error {
		// 	no_error = 0,
		// 	command_already_running,
		// };

		std::string command_string;
		const char *type; // passed to popen.

		command_t(std::string command_string, const char *type) : command_string{command_string}, type{type} {}

		// Starts process and returns nothing. This does not wait for the process to exit.
		std::expected<::FILE *, error_t>
		start() {
			if (running) {
				return std::unexpected(error_t(std::format("Command is already running: \"{}\".", command_string)));
			}

			running = true;

			pipe = popen(command_string.c_str(), type);
			if (pipe == NULL) {
				return std::unexpected(error_t(std::format("popen(\"{0}\", \"{1}\"): {2}.\nError running command: \"{0}\", type: \"{1}\"\n", command_string, type, std::strerror(errno))));
			}
			return pipe;
		}

		// Waits for process to exit and returns status code.
		std::expected<int, error_t>
		wait() {
			// Wait for process to exit one way or another.
			// Sometimes? returns an incorrect status code that can be decoded with WEXITSTATUS later. Why is it this complex?
			int code = pclose(pipe);
			if (code == -1) {
				return std::unexpected(error_t(std::format("Error closing compilation command: {}", std::strerror(errno))));
			}

			// Allow the command to be run again.
			running = false;

			// If terminated by a signal.
			if (WIFEXITED(code) == 0) {
				throw std::runtime_error(std::format("Compilation command exited abnormally: {}", std::strerror(errno)));
			}

			// Get the actual status code.
			int status = WEXITSTATUS(code);

			return status;
		}

		// Runs the command to completion and returns status code.
		std::expected<int, error_t> run() {
			std::expected<FILE *, error_t> pipe = start();
			if (!pipe) {
				return std::unexpected(pipe.error().append("Error starting command."));
			}
			std::expected<int, error_t> status = wait();
			if (!status) {
				return std::unexpected(status.error().append("Error wating for command to complete."));
			}
			return status;
		}
	};

	int main(int argc, char *argv[]) {
		std::filesystem::path origin_file_path;

		// Parse arguments.
		// Start at 1 because the 0th arg is this executable.
		for (int i = 1; i < argc; i++) {
			std::string arg = std::string(argv[i]);

			// // Ignore options for now.
			// if (arg[0] == '-') {
			// 	continue;
			// }

			origin_file_path = arg;
		}

		if (origin_file_path.empty()) {
			std::cerr << "Origin file was not provided." << std::endl;
			return 1;
		}

		// Compile file.
		command_t command(std::string("g++ ") + origin_file_path.string(), "r");
		std::expected<int, error_t> status = command.run();
		if (!status) {
			std::cerr << status.error().message_stack << "Error running compilation command." << std::endl;
			return 1;
		}

		std::cout << "Compiler exited with status: " << status.value() << std::endl;

		return 0;
	}
}

int main(int argc, char *argv[]) {
	return program_n::main(argc, argv);
}
