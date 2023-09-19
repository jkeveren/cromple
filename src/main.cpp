#include <iostream>
#include <format>
#include <expected>
#include <clang-c/Index.h>

// #include <cstdio>
#include <cstring>

// Using wrapper namespace to avoid global namespace because it is poluted by c crap that gets included by c++ includes.
// Access all global names with "::<name>" e.g. "::FILE" or "::error_t".
namespace program_n {

	// Error handling strategy.
	// This is a lot of comments. Just the way I like it. Explain every decision so it sticks.
	// Use std::expect<`return-type`, error_t> as return value for all functions that can error in order to remove the nead to try-catch every errorable function call just to add more error information at each level.
	// The goal is for a deep error caused by a high level mistake to be easy to debug just by seeing the error message. All relavent variable values should be in the error messages.
	// However deep errors should not be possible to branch on. Only the highest level error. This allows the implementation of the unit to be changed without fear of breaking changes.
	// This error handling strategy has overhead because of all the string building so don't use this for things that you expect to error a lot, or at least just use a const string.
	// If this is used in a performace critical application that is expected to error a lot then this is not ideal.
	// If it's performance critical and simple you can re-implement whatever uses this, or it's complex and string building is probably not that much of an overhead.
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
		// Defaults to other, not none. Better to have false positives than false negatives. (Should be overridden by constructors anyway).
		int reason = reason_other;

		// Common reasons are define here.
		// When defining your own reasons, make sure to start the enum at custom_reason_start!
		// Using a regular enum for implicit conversion to int.
		enum reasons {
			reason_none = 0, // None means no error occurred. This is not the default
			reason_other = 1, // Other should be used when an error occured but you don't want to make the machine readable reason part of the external API. Mostly for implementation details that can change.
			// reason_none and reason_other being 0 and 1 makes them also work as sensible exit codes so you can just exit with the error reason which defaults to other which is 1. Not a great strat if you want unique exit codes for different circumstances.
			custom_reason_start, // Custom reasons should start at this int to avoid colliding with the previous reasons.
		};
		
		// Default constructor.
		// Constructs an empty error. Indicates that an error has not occurred.
		error_t() : reason{reason_none} {}

		// Constructs an error with message and reason. Indicates that an error has occurred.
		error_t(const std::string &message, int reason = reason_other) {
			append(message, reason);
		}

		// Returns true if an error has occurred.
		operator bool() const {
			return reason != reason_none;
		}

		// Appends the highest level error message to the message stack and overrides the reason.
		// Returns self for chaining.
		error_t append(const std::string &message, int reason = reason_other) {
			this->reason = reason;
			message_stack += message + "\n";
			return *this;
		}

		// Prints the message_stack and returns reason int (handy for exiting program e.g. "int main() {...; return something.error().append("OOPSIE WOOPSIE!! Uwu We made a fucky wucky!!").print();}").
		// message_stack is only accessible through printing to prevent easy access to the user unit for the reasons stated above in the section about message_stack etc.
		int print(std::FILE *output_stream = stderr) const {
			std::fputs(message_stack.c_str(), output_stream);
			return reason;
		}

		// Constructs an error_t with message from errno in a thread safe way (using strerror_r instead of strerror).
		static error_t strerror() {
			// This looks weird because the GNU version of strerror_r is weird and this is compatible.
			char *error_reason = (char *)alloca(100);
			error_reason = strerror_r(errno, error_reason, sizeof(error_reason));
			return error_t(error_reason);
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
				return std::unexpected(error_t::strerror().append(std::format("Error running command: \"{}\" with type: \"{}\"", command_string, type)));
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
				return std::unexpected(error_t::strerror().append("Error closing compilation command."));
			}

			// Allow the command to be run again.
			running = false;

			// If terminated by a signal.
			if (WIFEXITED(code) == 0) {
				// Should return throw? Is it an error.
				// Inside a generic command runner class this should probably just be returned.
				return std::unexpected(error_t::strerror().append(std::format("Compilation command exited abnormally.")));
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

	std::expected<std::string, error_t> get_cursor_string(CXCursor cursor, std::FILE *file) {
		error_t error;
		do {
			CXSourceRange range = clang_getCursorExtent(cursor);
			CXSourceLocation start_location = clang_getRangeStart(range);
			CXSourceLocation end_location = clang_getRangeEnd(range);
			unsigned start = 0;
			unsigned end = 0;
			CXFile clang_file;
			clang_getSpellingLocation(start_location, &clang_file, nullptr, nullptr, &start);
			clang_getSpellingLocation(end_location, nullptr, nullptr, nullptr, &end);
			unsigned length = end - start;

			// Seek to start of cursor.
			if (std::fseek(file, start, SEEK_SET) != 0) {
				error = error_t::strerror().append(std::format("Error seeking to position {}.", start));
				break;
			}

			// Read cursor string.
			char *buffer = (char *)alloca(length + 1);
			if (std::fread(buffer, sizeof(char), length, file) != length) {
				error = error_t::strerror().append(std::format("Error reading {} char from file from position {} to {}. ferror() = {}; feof() = {}.", length, start, end, ferror(file), feof(file)));
				break;
			}

			return std::string(buffer);
		} while (false);
		return std::unexpected(error.append("Error getting cursor string."));
	}

	int main(int argc, char *argv[]) {
		std::string origin_file_path;

		// Parse arguments.
		// Start at 1 because the 0th arg is this executable.
		for (int i = 1; i < argc; i++) {
			std::string arg = std::string(argv[i]);

			// Ignore options for now.
			if (arg[0] == '-') {
				continue;
			}

			origin_file_path = arg;
			break;
		}

		if (origin_file_path.empty()) {
			return error_t("Origin file was not provided.").print();
		}

		CXIndex index = clang_createIndex(0, 0);

		CXTranslationUnit translation_unit;
		unsigned options = CXTranslationUnit_DetailedPreprocessingRecord
			| CXTranslationUnit_SkipFunctionBodies
		;

		const char *arguments = "-I./test/include"; // TODO: NO HARDCODE. HARDCODE BAD!
		CXErrorCode clang_error = clang_parseTranslationUnit2(index, origin_file_path.c_str(), &arguments, 1, nullptr, 0, options, &translation_unit);
		if (clang_error != 0) {
			return error_t(std::format("Clang error code {}.\nError parsing file \"{}\".", (int)clang_error, origin_file_path)).print();
		}

		CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);

		std::FILE *file = fopen(origin_file_path.c_str(), "r");
		if (file == nullptr) {
			return error_t::strerror().append(std::format("Error opening file \"{}\".", origin_file_path)).print();
		}

		struct visitor_data_t {
			error_t error;
			std::FILE *file;
		};

		CXCursorVisitor visitor = [](CXCursor cursor, [[maybe_unused]] CXCursor parent, [[maybe_unused]] CXClientData client_data) -> CXChildVisitResult {
			visitor_data_t &visitor_data = *(visitor_data_t *)client_data;
			do {
				if (
					// Cursor is on an include directive.
					clang_getCursorKind(cursor) == CXCursor_InclusionDirective
					// Cursor is in the currnet file and has not recursed into another include.
					&& clang_Location_isFromMainFile(clang_getCursorLocation(cursor))
				) {
					// Get full include directive
					std::expected<std::string, error_t> include = get_cursor_string(cursor, visitor_data.file);
					if (!include) {
						visitor_data.error = include.error();
						break;
					}

					std::puts(include.value().c_str());
					
					// Get contents of brackets or quotes from include directive.
					CXString clang_str = clang_getCursorSpelling(cursor);
					std::string include_target = clang_getCString(clang_str);
					clang_disposeString(clang_str);

					std::puts(include_target.c_str());
				}

				return CXChildVisit_Continue;
			} while (false);
			visitor_data.error.append("Error parsing headers.");
			return CXChildVisit_Break;
		};
		
		visitor_data_t visitor_data;
		visitor_data.file = file;
		clang_visitChildren(cursor, visitor, &visitor_data);
		if (visitor_data.error) {
			return visitor_data.error.append(std::format("Error parsing headers from file \"{}\".", origin_file_path)).print();
		}

		clang_disposeTranslationUnit(translation_unit);
		clang_disposeIndex(index);
		
		// // Compile file.
		// std::expected<int, error_t> status = command_t(std::string("g++ ") + origin_file_path.string(), "r").run();
		// if (!status) {
		// 	if (status.error().reason == command_t::error_reason_command_already_running) {
		// 		std::cout << "ur dumb" << std::endl;
		// 	}
		// 	return status.error().append("Error running compilation command.").print();
		// }

		// std::cout << "Compiler exited with status: " << status.value() << std::endl;

		return 0;
	}
}

int main(int argc, char *argv[]) {
	return program_n::main(argc, argv);
}
