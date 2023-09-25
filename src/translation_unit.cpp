#pragma once

#include <vector>
#include <filesystem>
#include <system_error>

namespace pgm {
	// Manages a translation unit unit and it's object file.
	class translation_unit {
		public:
		// Translation units are a file after pre-processing, so will include all header contents.
		// root_path refers to the source file that, optionally, #include all of those headers.
		const std::filesystem::path root_path;
		const std::filesystem::path object_path; // Path of the object file that compilation should generate.

		translation_unit(const std::filesystem::path &root_path, const std::filesystem::path &object_directory) : root_path{root_path}, object_path{source_to_object(root_path, object_directory)} {}

		// Converts a source path to an object path
		static std::filesystem::path source_to_object(const std::filesystem::path &root_path, const std::filesystem::path &object_directory) {
			return object_directory / (root_path.filename().string() + ".o");
		}

		// Returns true if included files have a later modification time than time.
		std::expected<bool, pgm::error> includes_are_newer(std::filesystem::file_time_type time, std::vector<std::string> compiler_arguments) const {
			// This function should only return at the end, outside the do{}while() statement.
			// To exit the function, set error or result and break from the do{}while() statement.
			pgm::error error;
			bool newer = false;

			CXTranslationUnit translation_unit;
			CXIndex index;
			do {
				index = clang_createIndex(0, 0); // Maybe we should be sharing this across translation unit parses. Performance boost?

				unsigned options = CXTranslationUnit_DetailedPreprocessingRecord // Required. Makes clang record include directives.
					// Optional optimizations.
					| CXTranslationUnit_Incomplete // Supress semantic analysis.
					| CXTranslationUnit_SkipFunctionBodies // Skip function bodies.
					| CXTranslationUnit_SingleFileParse // Not sure what this does internally but it makes thigs a lot faster.
				;

				// Transform compiler_arguments from vector of strings to array of char arrays.
				size_t argument_count = compiler_arguments.size();
				char **arguments = (char **)calloc(argument_count, sizeof(char *));
				for (size_t i = 0; i < argument_count; i++) {
					std::string argument = compiler_arguments[i];
					arguments[i] = (char *)calloc(argument.length() + 1, sizeof(char)); // +1 for null termination.
					strcpy(arguments[i], argument.c_str());
				}

				CXErrorCode clang_error = clang_parseTranslationUnit2(index, root_path.c_str(), arguments, argument_count, nullptr, 0, options, &translation_unit);
				if (clang_error != CXError_Success) {
					error = pgm::error(std::format("Clang CXErrorCode {}.", (int)clang_error));

					CXDiagnosticSet diagnostic_set = clang_getDiagnosticSetFromTU(translation_unit);
					if (diagnostic_set == nullptr) {
						error.append("Could not get clang CXDiagnosticSet.");
					}

					error.append(std::format(
						"Error parsing translation unit \"{}\".",
						root_path.string()
					));
					break;
				}

				CXCursor cursor = clang_getTranslationUnitCursor(translation_unit);

				std::FILE *file = fopen(root_path.c_str(), "r");
				if (file == nullptr) {
					error = pgm::error::strerror().append(std::format("Error opening file \"{}\".", root_path.string()));
					break;
				}

				struct visitor_data_t {
					pgm::error &error;
					std::FILE *file;
					const std::filesystem::file_time_type &time;
					bool &newer;
				};

				CXCursorVisitor visitor = [](CXCursor cursor, [[maybe_unused]] CXCursor parent, [[maybe_unused]] CXClientData client_data) -> CXChildVisitResult {
					// To exit this iteration, set visitor_data.error or visitor_data.result and return CXChildVisit_Break.

					visitor_data_t &visitor_data = *(visitor_data_t *)client_data;
					
					do {
						if (
							// Cursor is on an include directive.
							clang_getCursorKind(cursor) == CXCursor_InclusionDirective
							// Cursor is in the currnet file and has not recursed into another include.
							// && clang_Location_isFromMainFile(clang_getCursorLocation(cursor))
							// Do not recurse into system includes.
							&& !clang_Location_isInSystemHeader(clang_getCursorLocation(cursor))
						) {
							// Get full include directive.
							CXSourceRange range = clang_getCursorExtent(cursor);
							CXSourceLocation start_location = clang_getRangeStart(range);
							CXSourceLocation end_location = clang_getRangeEnd(range);
							unsigned start = 0;
							unsigned end = 0;
							CXFile clang_file;
							clang_getSpellingLocation(start_location, &clang_file, nullptr, nullptr, &start);
							clang_getSpellingLocation(end_location, nullptr, nullptr, nullptr, &end);
							unsigned length = end - start;

							// Seek to start of file.
							if (std::fseek(visitor_data.file, start, SEEK_SET) != 0) {
								visitor_data.error = pgm::error::strerror().append(std::format("Error seeking to position {}.", start));
								return CXChildVisit_Break;
							}

							// Read cursor string.
							char *buffer = (char *)alloca(length + 1); // +1 for null termination.
							if (std::fread(buffer, sizeof(char), length, visitor_data.file) != length) {
								visitor_data.error = pgm::error::strerror().append(std::format("Error reading {} char from file from position {} to {}.", length, start, end));
								return CXChildVisit_Break;
							}
							buffer[length] = (char)NULL; // Null terminate the buffer before parsing into std::string
							std::string include_directive(buffer);

							// Skip system includes. (#include <stdio.h>)
							// If last character is ">".
							if (include_directive[include_directive.length() - 1] == '>') {
								return CXChildVisit_Continue;
							}

							// Get included file path.
							CXFile included_cxfile = clang_getIncludedFile(cursor);
							CXString included_cxname = clang_getFileName(included_cxfile);
							const char *included_cstring = clang_getCString(included_cxname);
							std::filesystem::path included_path = included_cstring;
							clang_disposeString(included_cxname);

							// Check header mod time.
							std::filesystem::file_time_type included_file_write_time = std::filesystem::last_write_time(included_path);
							if (included_file_write_time > visitor_data.time) {
								// Header is newer so return
								visitor_data.newer = true;
								return CXChildVisit_Break;
							}

							// std::expected<bool, pgm::error> expected_includes_are_newer = are_includes_newer(included_path, visitor_data.time, visitor_data.compiler_arguments);
							// if (!expected_includes_are_newer) {
							// 	visitor_data.error = expected_includes_are_newer.error().append("Error recursively getting header modification times.");
							// 	return CXChildVisit_Break;
							// }

							// if (expected_includes_are_newer.value()) {
							// 	visitor_data.includes_are_newer = true;
							// 	return CXChildVisit_Break;
							// }
						}

						return CXChildVisit_Continue;
					} while (false);
					visitor_data.error.append("Error parsing includes.");
					return CXChildVisit_Break;
				};
				
				visitor_data_t visitor_data {
					.error = error,
					.file = file,
					.time = time,
					.newer = newer,
				};
				clang_visitChildren(cursor, visitor, &visitor_data);
			} while (false);

			clang_disposeTranslationUnit(translation_unit);
			clang_disposeIndex(index);

			if (error) {
				return std::unexpected(error.append(std::format("Error checking if includes were newer than \"{}\".", std::chrono::file_clock::to_sys(time))));
			} else {
				return newer;
			}
		}

		// Checks if a translation units object file is out of date or non-existant.
		std::expected<bool, pgm::error> object_is_outdated(const std::vector<std::string> &compiler_arguments) const {
			pgm::error error;
			bool outdated = false;

			std::filesystem::file_time_type object_time;
			do {
				// If object file does not exist.
				if (!std::filesystem::exists(object_path)) {
					// Add source and object to compilation vector.
					outdated = true;
					break;
				}

				// Get write times.
				std::filesystem::file_time_type root_time;
				try {
					root_time = std::filesystem::last_write_time(root_path);
				} catch (const std::filesystem::filesystem_error &e) {
					error = pgm::error(e.what()).append(std::format("Error getting modification time of source file \"{}\".", root_path.string()));
					break;
				}

				try {
					object_time = std::filesystem::last_write_time(object_path);
				} catch (const std::filesystem::filesystem_error &filesystem_error) {
					// If object file does not exist.
					if (filesystem_error.code() == std::errc::no_such_file_or_directory) {
						outdated = true;
						break;
					}
					
					error = pgm::error(filesystem_error.what()).append(std::format("Error getting modification time of object file \"{}\".", object_path.string()));
					break;
				}

				// If source is newer than object.
				if (root_time > object_time) {
					outdated = true;
					break;
				}

				// If includes are newer than object.
				std::expected<bool, pgm::error> expected_includes_are_newer = includes_are_newer(object_time, compiler_arguments);
				if (!expected_includes_are_newer) {
					error = expected_includes_are_newer.error();
					break;
				}

				// If includes are newer than object.
				if (expected_includes_are_newer.value()) {
					outdated = true;
					break;
				}
			} while (false);

			if (error) {
				return std::unexpected(error.append(std::format(
					"Error checking if includes included in file \"{}\" were modified since {}.",
					root_path.string(), std::chrono::file_clock::to_sys(object_time)
				)));
			} else {
				return outdated;
			}
		}

		static std::expected<std::vector<pgm::translation_unit>, pgm::error> find_all(const std::filesystem::path &source_directory, const std::filesystem::path &object_directory) {
			std::vector<pgm::translation_unit> units;
			pgm::error error;

			do {
				// Get source_directory iterator.
				std::filesystem::directory_iterator iterator;
				try {
					iterator = std::filesystem::directory_iterator(source_directory);
				} catch (std::exception &e) {
					error = pgm::error(e.what()).append(std::format("Error getting directory iterator for \"{}\".", source_directory.string()));
					break;
				}

				// Iterate over source_directory entries.
				for (const std::filesystem::directory_entry &entry : iterator) {
					// Skip non-readable files. Do not skip symlinks (!is_regular_file would make us skip symlinks).
					if (entry.is_directory()) {
						continue;
					}

					std::filesystem::path root_path = entry.path();

					// Skip non-"*.c*" source files.
					static std::regex c_source_file_regex(".+\\.c.*?$");
					if (!std::regex_match(root_path.string(), c_source_file_regex)) {
						continue;
					}

					units.emplace_back(root_path, object_directory);
				}
			} while (false);

			if (error) {
				return std::unexpected(error.append(std::format("Error getting translation units from source directory \"{}\". ", source_directory.string())));
			} else {
				return units;
			}
		}

		static std::expected<std::vector<pgm::translation_unit>, pgm::error> find_changed(std::vector<pgm::translation_unit> units, std::vector<std::string> compiler_arguments) {
			pgm::error error;
			std::vector<pgm::translation_unit> changed_units;

			for (const pgm::translation_unit &unit : units) {
				// Check if object is outdated.
				std::expected<bool, pgm::error> expected_object_is_outdated = unit.object_is_outdated(compiler_arguments);
				if (!expected_object_is_outdated) {
					error = expected_object_is_outdated.error();
					break;
				}

				// If object is up to date.
				if (expected_object_is_outdated.value()) {
					changed_units.push_back(unit);
				}
			}

			if (error) {
				return std::unexpected(error.append("Error finding changed translation units."));
			}
			return changed_units;
		}
	};
}