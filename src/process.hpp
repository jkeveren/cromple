#pragma once

#include <vector>
#include <cstddef>

#include <sys/types.h>

#include "error.hpp"

namespace pgm {
	class process {
		public:

		// Interface for parent to read and write to child.
		class child {
			private:
			friend process;

			// Constructor is private with "friend process" so it can only be constructed by process::fork.
			child();
			child(pid_t pid, int stdin, int stdout, int stderr);

			static
			std::vector<std::byte>
			read_all(const int file_descriptor, error &error);

			static
			std::string
			read_all_string(const int file_descriptor, error &error);

			public:

			enum error_reasons {
				error_reason_exited_abnormally = pgm::error::custom_reason_start,
			};

			const pid_t pid = 0;

			const int stdin = 0; // Writeable
			const int stdout = 0; // Readable
			const int stderr = 0; // Readable

			int exit_status = 0;

			void write_stdin() const;

			// Fills buffer from the child's stdout.
			// Returns number of byted read.
			ssize_t
			read_stdout(void *buffer, size_t buffer_size, error &error) const;

			// Same as read_stdout but for stderr file descriptor.
			ssize_t
			read_stderr(void *buffer, size_t buffer_size, error &error) const;

			std::vector<std::byte>
			read_all_stdout(error &error) const;

			std::string
			read_all_stdout_string(error &error) const;

			std::vector<std::byte>
			read_all_stderr(error &error) const;

			std::string
			read_all_stderr_string(error &error) const;

			// Waits for process to exit and returns the exit status.
			int
			wait(error &error) const;
		};

		template<typename data_type>
		using child_function = void (*)(data_type data);

		// Creates pipes to read from stdin and write to std{out,err}.
		// Forks the process.
		// Runs child_function(data) in the child process.
		template<typename data_type>
		static
		process::child
		fork(process::child_function<data_type> child_function, data_type data, error &error);

		// Uses process::fork to run a command and returns it's process::child.
		static
		process::child
		exec(std::vector<std::string> command_parts, error &error);
	};
}