#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <format>
#include <expected>

#include <cstring>

// Using wrapper namespace to avoid global namespace because it is poluted by c crap that gets included by c++ includes.
// Access all global names with "::<name>" e.g. "::FILE" or "::error_t".
namespace program_n {

	// Error handling strategy.
	// Use std::expect<`return-type`, error_t> as return value for all functions that can error in order to remove the nead to try-catch every errorable function call just to add more error information at each level.
	// The goal is for a deep error caused by a high level mistake to be easy to debug just by seeing the error message. All relavent variable values should be in the error messages.
	// However deep errors should not be possible to branch on. Only the highest level error. This allows the implementation of the unit to be changed without fear of breaking changes.
	// This error handling strategy has overhead so might want to be neutered for performance-crittical applications that expect to error a lot. Perhaps by making .append() do nothing (this would maintain .reason functionality).
	// There's more comments here than actual code.
	class error_t {
		private:
		// message_stack is a string of error messages seperated by newlines.
		// Each message is appended in chronological order. This results in the lowest level messages being at the top of the stack and are the first ones printed.
		// Each message should be verbose and contain lots of information that could be useful in debugging the issue like labeled variable values.
		// message_stack is private because deep errors should not be machine readable.
		// Machine readable errors become part of a units external API.
		// This causes low level changes to the unit to effect the external API which makes them breaking changes.
		// If a user of a unit needs low level error access then the user should implement it and/or the unit should provide a way of injecting low level resources.
		std::string message_stack;

		public:
		// reason is a machine readable version of the latest error appended to message_stack.
		// This does not stack like message_stack for the same reasons message_stack is private.
		// It only stores the highest level reason for the error.
		// Defaults to other. If an error occurred, there was a reason but we can't assume what it is here.
		int reason = reason_other;

		// Common reasons are define here.
		// When defining your own reasons, make sure to start the enum at custom_reason_start!
		// Using a regular enum for implicit conversion to int.
		enum reasons {
			reason_none__do_not_use_this_reason_on_purpose = 0, // A reason of 0 could be interpreted accidentally as no reason so let's make that official so there are less mistakes. This should not be used on puropse.
			reason_other = 1, // Other should be used when an error occured but you don't want to make the machine readable reason part of the external API. Mostly for implementation details that can change.
			// reason_none and reason_other being 0 and 1 makes them also work as sensible exit codes so you can just exit with the error reason which defaults to other which is 1. Not a great strat if you want unique exit codes for different circumstances.
			custom_reason_start, // Custom reasons should start at this int to avoid colliding with the previous reasons.
		};
		
		// Constructor
		error_t(const std::string &message, int reason = reason_other) : message_stack{message}, reason{reason} {}

		// Appends the highest level error message to the message stack and overrides the reason.
		// Returns self for chaining.
		error_t append(const std::string &message, int reason = reason_other) {
			this->reason = reason;
			message_stack += message + "\n";
			return *this;
		}

		// Prints the message_stack and returns reason int (handy for exiting program e.g. "int main() {...; return something.error().append("OOPSIE WOOPSIE!! Uwu We made a fucky wucky!!").print();}").
		// message_stack is only accessible through printing to prevent easy access to the user unit for the reasons stated above in the section about message_stack etc.
		int print(std::ostream &output_stream = std::cerr) {
			output_stream << message_stack << std::flush;
			return reason;
		}
	};

	class command_t {
		private:
		bool running = false;
		::FILE *pipe = NULL;

		public:
		enum error_reasons {
			error_reason_command_already_running = error_t::custom_reason_start,
		};

		std::string command_string;
		const char *type; // passed to popen.

		command_t(std::string command_string, const char *type) : command_string{command_string}, type{type} {}

		// Starts process and returns pipe. This does not wait for the process to exit.
		std::expected<::FILE *, error_t>
		start() {
			if (running) {
				return std::unexpected(error_t(
					std::format("Command is already running: \"{}\".", command_string),
					error_reason_command_already_running
				));
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
				// Should return throw? Is it an error.
				// Inside a generic command runner class this should probably just be returned.
				return std::unexpected(error_t(std::format("Compilation command exited abnormally: {}", std::strerror(errno))));
			}

			// Get the actual status code.
			int status = WEXITSTATUS(code);

			return status;
		}

		// Runs the command to completion and returns status code.
		std::expected<int, error_t> run() {
			std::expected<::FILE *, error_t> pipe = start();
			if (!pipe) {
				return std::unexpected(pipe.error().append("Error starting command.", pipe.error().reason));
			}
			std::expected<int, error_t> status = wait();
			if (!status) {
				return std::unexpected(status.error().append("Error wating for command to complete.", pipe.error().reason));
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
			return error_t("Origin file was not provided.").print();
		}

		// Compile file.
		std::expected<int, error_t> status = command_t(std::string("g++ ") + origin_file_path.string(), "r").run();
		if (!status) {
			if (status.error().reason == command_t::error_reason_command_already_running) {
				std::cout << "ur dumb" << std::endl;
			}
			return status.error().append("Error running compilation command.").print();
		}

		std::cout << "Compiler exited with status: " << status.value() << std::endl;

		return 0;
	}
}

int main(int argc, char *argv[]) {
	return program_n::main(argc, argv);
}
