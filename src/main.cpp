#include <iostream>
#include <format>
#include <expected>
#include <vector>
#include <map>
#include <filesystem>
#include <thread>

#include <cstring>

#include <clang-c/Index.h>

// Using wrapper namespace to avoid global namespace because it is poluted by c crap that gets included by c++ includes like error_t. The audacity to use such a generic type name in global scope, honestly.
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
				error = error_t::strerror().append(std::format("Error reading {} char from file from position {} to {}.", length, start, end));
				break;
			}

			return std::string(buffer);
		} while (false);
		return std::unexpected(error.append("Error getting cursor string."));
	}

	// Returns a list of header files included in file at given path.
	// compiler_arguments must contain include arguments for correct header file resolution but can contain all valid compiler arguments.
	std::expected<std::filesystem::file_time_type, error_t> get_latest_header_write_time(std::string path, std::vector<std::string> compiler_arguments) {
		error_t error;
		std::filesystem::file_time_type result;

		CXTranslationUnit translation_unit;
		CXIndex index;
		do {
			index = clang_createIndex(0, 0);

			unsigned options = CXTranslationUnit_DetailedPreprocessingRecord
				| CXTranslationUnit_SkipFunctionBodies
			;

			// Transform compiler_arguments from vector of strings to array of char arrays.
			size_t argument_count = compiler_arguments.size();
			char **arguments = (char **)calloc(argument_count, sizeof(char *));
			for (size_t i = 0; i < argument_count; i++) {
				std::string argument = compiler_arguments[i];
				arguments[i] = (char *)calloc(argument.length(), sizeof(char));
				strcpy(arguments[i], argument.c_str());
			}

			CXErrorCode clang_error = clang_parseTranslationUnit2(index, path.c_str(), arguments, argument_count, nullptr, 0, options, &translation_unit);
			if (clang_error != 0) {
				error = error_t(std::format("Clang error code {}.\nError parsing file \"{}\".", (int)clang_error, path));
				break;
			}

			CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);

			std::FILE *file = fopen(path.c_str(), "r");
			if (file == nullptr) {
				error = error_t::strerror().append(std::format("Error opening file \"{}\".", path));
				break;
			}

			struct visitor_data_t {
				error_t error;
				std::FILE *file;
				std::filesystem::file_time_type latest_header_write_time = std::filesystem::file_time_type::min(); // defaults to epoch (0);
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
						std::expected<std::string, error_t> expected_include = get_cursor_string(cursor, visitor_data.file);
						if (!expected_include) {
							visitor_data.error = expected_include.error();
							break;
						}
						std::string include = expected_include.value();

						// Skip system headers. (#include <stdio.h>)
						if (include[include.length() - 1] == '>') {
							return CXChildVisit_Continue;
						}

						// Get included file path
						CXFile included_cxfile = clang_getIncludedFile(cursor);
						CXString included_cxname = clang_getFileName(included_cxfile);
						const char *included_cstring = clang_getCString(included_cxname);
						if (included_cstring == nullptr) {
							puts(include.c_str());
						}
						std::filesystem::path included_path = included_cstring;
						clang_disposeString(included_cxname);

						std::filesystem::file_time_type last_write_time = std::filesystem::last_write_time(included_path);

						if (last_write_time > visitor_data.latest_header_write_time) {
							visitor_data.latest_header_write_time = last_write_time;
						}
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
				error = visitor_data.error.append(std::format("Error parsing headers from file \"{}\".", path));
				break;
			}

			result = visitor_data.latest_header_write_time;
			break;
		} while (false);

		clang_disposeTranslationUnit(translation_unit);
		clang_disposeIndex(index);

		if (error) {
			return std::unexpected(error);
		} else {
			return result;
		}
	}

	int main(int argc, char *argv[]) {
		// Usage: <this executable> [--compiler COMPILER] [--source SOURCE_DIRECTORY=src] [--objects OBJECT_DIRECTORY=objects] [-o OUTPUT_FILE=a.out] [COMPILER_OPTIONS]
		// Order independent.

		// Parse arguments
		// Argument variables (defaults below parsing)
		std::string source_directory;
		std::string object_directory;
		std::string out_file;
		std::string compiler;
		std::vector<std::string> compiler_arguments;

		std::map<std::string, std::string *> argument_pointers {
			{"--source", &source_directory}, // source_directory must be provided by a named argument because we can't know which arguments belong to a previous compiler argument like "library" in "-l library". We can't accurately parse all compiler args.
			{"--objects", &object_directory},
			{"-o", &out_file},
			{"--compiler", &compiler},
		};

		// Points to where to store the next option. When finding "--compiler" point this to compiler so it gets set in the next loop.
		std::string *argument_pointer = nullptr;

		// Start at 1st arg because the 0th arg is this executable.
		for (int i = 1; i < argc; i++) {
			std::string arg = std::string(argv[i]);

			// Store second part of non-compiler key-value arguments, e.g. "--compiler g++" or "-o out"
			if (argument_pointer != nullptr) {
				*argument_pointer = arg;
				argument_pointer = nullptr;
				continue;
			}

			// Parse 
			if (arg[0] == '-') {
				// Point argument_pointer to place to store next argument.
				std::map<std::string, std::string *>::iterator flag_iterator = argument_pointers.find(arg);
				// Is this a non-compiler key-value argument?
				if (flag_iterator != argument_pointers.end()) {
					argument_pointer = flag_iterator->second;
					continue;
				}
			}

			compiler_arguments.push_back(arg);
			continue;
		}

		// Default arguments.
		if (source_directory.empty()) {source_directory = "src";}
		constexpr std::string_view object_directory_default = "objects";
		if (object_directory.empty()) {object_directory = object_directory_default;}
		if (out_file.empty()) {out_file = "a.out";}
		if (compiler.empty()) {compiler = "gcc";}

		// Convert to paths.
		std::filesystem::path source_directory_path(source_directory);
		std::filesystem::path object_directory_path(object_directory);
		std::filesystem::path out_file_path(out_file);

		// Assert that object_directory exists and is a directory.
		if (!std::filesystem::is_directory(object_directory)) {
			return error_t(std::format("Object directory \"{}\" is not a directory. Create it or change it with the \"--objects\" argument (defaults to \"{}\").", object_directory, object_directory_default)).print();
		}

		// Vector of pairs of source and object files to be compiled.
		std::vector<std::pair<std::filesystem::path, std::filesystem::path>> source_object_pairs;
		std::vector<std::string> object_files;

		// Get source_directory iterator.
		std::filesystem::directory_iterator iterator;
		try {
			iterator = std::filesystem::directory_iterator(source_directory_path);
		} catch (std::exception &e) {
			return error_t(e.what()).append(std::format("Error iterating directory \"{}\".", source_directory)).print();
		}

		// Find which source files need compiling.
		// Iterate over source_directory entries.
		for (const std::filesystem::directory_entry &entry : iterator) {
			// Skip non-readable files. Do not skip symlinks (!is_regular_file would make us skip symlinks).
			if (entry.is_directory()) {
				continue;
			}

			// Get source and object paths.
			std::filesystem::path source_path = entry.path();
			std::filesystem::path object_path = object_directory_path / (source_path.filename().string() + ".o");
			object_files.push_back(object_path);

			// Make pair to store in vector.
			std::pair<std::filesystem::path, std::filesystem::path> pair {source_path, object_path};

			// If object file does not exist.
			if (!std::filesystem::exists(object_path)) {
				// Add source and object to compilation vector.
				source_object_pairs.push_back(pair);
				continue;
			}

			// Get write times.
			std::filesystem::file_time_type source_time = std::filesystem::last_write_time(source_path);
			std::filesystem::file_time_type object_time = std::filesystem::last_write_time(object_path);

			// If source is newer than object.
			if (source_time > object_time) {
				source_object_pairs.push_back(pair);
				continue;
			}

			// Get headers and latest write time.
			std::expected<std::filesystem::file_time_type, error_t> latest_header_write_time = get_latest_header_write_time(source_path, compiler_arguments);
			if (!latest_header_write_time) {
				return latest_header_write_time.error().append("Error finding source files that need recompiling").print();
			}

			// If headers are newer than object.
			if (latest_header_write_time.value() > object_time) {
				source_object_pairs.push_back(pair);
				continue;
			}
		}

		// Compile objects.
		{
			for (const std::pair<std::filesystem::path, std::filesystem::path> &pair : source_object_pairs) {
				std::filesystem::path source_path = pair.first;
				std::filesystem::path object_path = pair.second;

				// Pertinent args coppied directly from "gcc --help" command:
				// -c                       Compile and assemble, but do not link.
				// -o <file>                Place the output into <file>.
				std::string command_string = std::format("{} -c {} -o {}", compiler, source_path.string(), object_path.string());
				// Pass all other arguments on to compiler. Linker arguments are ignored.
				for (const std::string &argument : compiler_arguments) {
					command_string += " " + argument;
				}

				command_t command(command_string, "r");
				std::expected<int, error_t> expected_status = command.run();
				if (!expected_status) {
					return expected_status.error().append(std::format(
						"Error compiling source file \"{}\" to object file \"{}\" with command \"{}\".",
						source_path.string(), object_path.string(), command_string
					)).print();
				}

				if (expected_status.value() != 0) {
					return error_t(std::format(
						"Exit code {} when compiling source file \"{}\" to object file \"{}\" with command \"{}\".",
						expected_status.value(), source_path.string(), object_path.string(), command_string
					)).print();
				}
			}
		}

		// Link final binary.
		{
			// Not checking if linking is explicitly required.
			// Just link the executable every time.
			// KISS. No need to add complexity here.
			// This program will usually only be run when something changes.
			// Linking should be pretty fast.
			// Checking modtimes and comparing etc adds complexity for almost nothing.
			// What if the user want to re-link?

			// Build link command.
			std::string command_string = std::format("{} -o {}", compiler, out_file);
			// Pass all other arguments on to compiler. Linker arguments are ignored.
			for (const std::string &argument : compiler_arguments) {
				command_string += " " + argument;
			}
			// Add all object files as input to compiler.
			for (const std::string &object_file : object_files) {
				command_string += " " + object_file;
			}

			// Perform linking.
			command_t command(command_string, "r");
			std::expected<int, error_t> expected_status = command.run();
			if (!expected_status) {
				return expected_status.error().append(std::format(
					"Error compiling {} object files into a final binary \"{}\" with command \"{}\".",
					out_file, object_files.size(), command_string
				)).print();
			}

			if (expected_status.value() != 0) {
				return error_t(std::format(
					"Exit code {} when compiling {} object files into a final binary executable or library \"{}\" with command \"{}\".",
					expected_status.value(), object_files.size(), out_file, command_string
				)).print();
			}
		}

		return 0;
	}
}

int main(int argc, char *argv[]) {
	return program_n::main(argc, argv);
}
