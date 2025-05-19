#include "translation_unit.hpp"

#include <format>
#include <iostream>

pgm::translation_unit::translation_unit(const std::filesystem::path &root_path, const std::filesystem::path &object_directory) : root_path{root_path}, object_path{source_to_object(root_path, object_directory)} {}

std::filesystem::path
pgm::translation_unit::source_to_object(const std::filesystem::path &root_path, const std::filesystem::path &object_directory) {
	return object_directory / (root_path.filename().string() + ".o");
}

bool
pgm::translation_unit::object_is_outdated(const pgm::compiler &compiler, error &error) const {
	std::filesystem::file_time_type object_time;
	do {
		// If object file does not exist.
		if (!std::filesystem::exists(object_path)) {
			// Add source and object to compilation vector.
			return true;
		}

		// Get object write time.
		try {
			object_time = std::filesystem::last_write_time(object_path);
		} catch (const std::filesystem::filesystem_error &filesystem_error) {
			// If object file does not exist.
			if (filesystem_error.code() == std::errc::no_such_file_or_directory) {
				return true;
			}

			error.append(filesystem_error.what()).append(std::format("Error getting modification time of object file \"{}\".", object_path.string()));
			break;
		}

		// Get headers that are #included in root file.
		std::vector<std::string> prerequisites = compiler.get_make_prerequisites(root_path.string(), error);
		if (error) {
			break;
		}

		// Check if any are newer than object.
		for (const std::string &prerequisite : prerequisites) {
			std::filesystem::file_time_type time;
			try {
				time = std::filesystem::last_write_time(prerequisite);
			} catch (const std::filesystem::filesystem_error &filesystem_error) {
				error.append(filesystem_error.what()).append(std::format("Error getting modification time for prerequisite \"{}\".", prerequisite));
				break;
			}
			// std::cout << prerequisite << " " << time.time_since_epoch().count() << " " << object_time.time_since_epoch().count() << std::endl;
			if (time > object_time) {
				return true;
			}
		}
		if (error) {
			break;
		}
		return false;
	} while (false);

	std::chrono::sys_time sys_time = std::chrono::file_clock::to_sys(object_time);
	std::time_t time_t = std::chrono::system_clock::to_time_t(sys_time);
	error.append(std::format(
		"Error checking if file \"{}\" or it's includes were modified since {}.",
		root_path.string(), std::ctime(&time_t)
	));
	return false;
}

std::vector<pgm::translation_unit>
pgm::translation_unit::find_all(const std::filesystem::path &source_directory, const std::filesystem::path &object_directory, error &error) {
	std::vector<pgm::translation_unit> units;

	do {
		// Get source_directory iterator.
		std::filesystem::directory_iterator iterator;
		try {
			iterator = std::filesystem::directory_iterator(source_directory);
		} catch (std::exception &e) {
			error.append(e.what()).append(std::format("Error getting directory iterator for \"{}\".", source_directory.string()));
			break; 
		}

		// Iterate over source_directory entries.
		for (const std::filesystem::directory_entry &entry : iterator) {
			// Skip non-readable entries. Do not skip symlinks (!is_regular_file would make us skip symlinks).
			if (entry.is_directory()) {
				continue;
			}

			std::filesystem::path root_path(entry.path());

			// Skip non-source files.
			// https://gcc.gnu.org/onlinedocs/gcc-4.4.1/gcc/Overall-Options.html#index-file-name-suffix-71
			static std::vector<std::string> valid_extensions {
				".c",
				".cc",
				".cp",
				".cxx",
				".cpp",
				".c++",
				".C"
			};
			const std::string actual_extension(root_path.extension().string());
			bool actual_is_invalid = true;
			for (const std::string &valid_extension : valid_extensions) {
				if (actual_extension == valid_extension) {
					actual_is_invalid = false;
					break;
				}
			}
			if (actual_is_invalid) {
				continue;
			}
			// Alternative looser regex implementation
			// static std::regex c_source_file_regex(".+\\.c[^\\.]*$");
			//  if (!std::regex_match(root_path.string(), c_source_file_regex)) {
			// 	 continue;
			// }

			units.emplace_back(root_path, object_directory);
		}
	} while (false);

	if (error) {
		error.append(std::format("Error getting translation units from source directory \"{}\". ", source_directory.string()));
	}
	return units;
}

std::vector<pgm::translation_unit>
pgm::translation_unit::find_changed(const std::vector<pgm::translation_unit> &units, const pgm::compiler &compiler, error &error) {
	std::vector<pgm::translation_unit> changed_units;

	for (const pgm::translation_unit &unit : units) {
		// Check if object is outdated.
		bool object_is_outdated = unit.object_is_outdated(compiler, error);
		if (error) {
			error.append("Error finding changed translation units.");
			return changed_units;
		}

		// If object is up to date.
		if (object_is_outdated) {
			changed_units.push_back(unit);
		}
	}
	return changed_units;
}
