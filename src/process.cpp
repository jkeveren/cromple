#include "process.hpp"

#include <list>
#include <format>
#include <iostream>

#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

pgm::process::child::child() {}
pgm::process::child::child(pid_t pid, int stdin, int stdout, int stderr) : pid{pid}, stdin{stdin}, stdout{stdout}, stderr{stderr} {}

std::vector<std::byte>
pgm::process::child::read_all(const int file_descriptor, error &error) {
	constexpr size_t chunk_size = 11;

	// Use a list for linear time complexity.
	// This avoids reallocation as a vector grows.
	std::list<std::vector<std::byte>> chunks;
	ssize_t bytes_read;

	while (true) {
		// Create vector with size. This will not init the elements.
		std::vector<std::byte> chunk(chunk_size);
		// Read bytes.
		bytes_read = ::read(file_descriptor, chunk.data(), chunk.size());
		if (bytes_read == -1) {
			error.strerror().append("Error reading all data from child process pipe.");
			return std::vector<std::byte>();
		} else if (bytes_read == 0) {
			break;
		}
		// Resize so chunk.end() is accurate.
		chunk.resize(static_cast<unsigned>(bytes_read));
		// Add chunk to chunks list.
		chunks.push_back(std::move(chunk));
	};

	// Final buffer to return.
	// Use a vector for memory management and runtime variable size.
	unsigned full_size_chunks = chunks.size();
	if (full_size_chunks > 0) {
		full_size_chunks -= 1;
	}
	unsigned total_bytes = full_size_chunks * chunk_size + static_cast<unsigned>(bytes_read); // bytes_read is guaranteed to be a positive number at this point.
	std::vector<std::byte> result;
	result.reserve(total_bytes);

	// Copy chunks to result.
	for (const std::vector<std::byte> &chunk : chunks) {
		result.insert(result.end(), chunk.begin(), chunk.end());
	}

	return result;
}

std::string
pgm::process::child::read_all_string(const int file_descriptor, error &error) {
	// Read all as a vector.
	std::vector<std::byte> output = read_all(file_descriptor, error);
	if (error) {
		return std::string();
	}
	
	// Create a string from said buffer.
	std::string string;
	string.reserve(output.size());

	// Cast bytes to characters and assign to string.
	for (const std::byte &byte : output) {
		string.push_back(static_cast<char>(byte));
	}
	return string;
}

ssize_t
pgm::process::child::read_stdout(void *buffer, size_t buffer_size, error &error) const {
	ssize_t bytes_read = ::read(stdout, buffer, buffer_size);
	if (bytes_read == -1) {
		error.append("Error reading stdout from child.");
	}
	return bytes_read;
}

ssize_t
pgm::process::child::read_stderr(void *buffer, size_t buffer_size, error &error) const {
	ssize_t bytes_read = ::read(stderr, buffer, buffer_size);
	if (bytes_read == -1) {
		error.append("Error reading stderr from child.");
	}
	return bytes_read;
}

std::vector<std::byte>
pgm::process::child::read_all_stdout(error &error) const {
	std::vector<std::byte> output = read_all(stdout, error);
	if (error) {
		error.append("Error reading all of child process stdout.");
		return std::vector<std::byte>();
	}
	return output;
}

std::string
pgm::process::child::read_all_stdout_string(error &error) const {
	std::string output = read_all_string(stdout, error);
	if (error) {
		error.append("Error reading all of child process stdout.");
		return std::string();
	}
	return output;
}

std::vector<std::byte>
pgm::process::child::read_all_stderr(error &error) const {
	std::vector<std::byte> output = read_all(stderr, error);
	if (error) {
		error.append("Error reading all of child process stderr.");
		return std::vector<std::byte>();
	}
	return output;
}

std::string
pgm::process::child::read_all_stderr_string(error &error) const {
	std::string output = read_all_string(stderr, error);
	if (error) {
		error.append("Error reading all of child process stderr.");
		return std::string();
	}
	return output;
}

int
pgm::process::child::wait(error &error) const {
	do {
		int wstatus;
		pid_t maybe_pid = waitpid(pid, &wstatus, 0);
		if (maybe_pid == -1) {
			error.strerror();
			break;
		}

		bool exited_normally = WIFEXITED(wstatus);
		if (!exited_normally) {
			error.append("Process exited abnormally. Did not call exit() or return from main.", error_reason_exited_abnormally);
			break;
		}

		int exit_status = WEXITSTATUS(wstatus);
		return exit_status;
	} while (false);

	error.append(std::format("Error waiting for child process \"{}\".", pid));
	return 0;
}

template<typename data_type>
pgm::process::child
pgm::process::fork(process::child_function<data_type> child_function, data_type data, error &error) {
	enum pipe_ends {
		read = 0,
		write,
	};

	do {
		int stdin_pipe[2]; // [read, write]
		int stdout_pipe[2];
		int stderr_pipe[2];
		if (
			::pipe(stdin_pipe) == -1
			|| ::pipe(stdout_pipe) == -1
			|| ::pipe(stderr_pipe) == -1
		) {
			error.strerror().append("Error creating pipes.");
			break;
		}

		pid_t child_pid = ::fork();
		if (child_pid == -1) {
			error.strerror().append("Error forking process.");
			break;
		}

		// If this process is the child
		if (child_pid == 0) {
			if (
				// Reassign stdin etc to pipes.
				::dup2(stdin_pipe[read], STDIN_FILENO) == -1
				|| ::dup2(stdout_pipe[write], STDOUT_FILENO) == -1
				|| ::dup2(stderr_pipe[write], STDERR_FILENO) == -1

				// Close parent side of pipes.
				|| ::close(stdin_pipe[write]) == -1
				|| ::close(stdout_pipe[read]) == -1
				|| ::close(stderr_pipe[read]) == -1

				// Close child end of pipes that have been reassigned.
				|| ::close(stdin_pipe[read]) == -1
				|| ::close(stdout_pipe[write]) == -1
				|| ::close(stderr_pipe[write]) == -1
			) {
				std::exit(pgm::error().strerror().append("Error reassigning and closing pipes.").print());
			}

			// Run the child program.
			child_function(data);
			std::exit(0);
		}

		if (
			::close(stdin_pipe[read]) == -1
			|| ::close(stdout_pipe[write]) == -1
			|| ::close(stderr_pipe[write]) == -1
		) {
			error.strerror().append("Error closing child end of pipes.");
			break;
		}

		return process::child(child_pid, stdin_pipe[write], stdout_pipe[read], stderr_pipe[read]);
	} while (false);

	error.append("Error forking process.");
	return process::child();
}

pgm::process::child
pgm::process::exec(std::vector<std::string> command_parts, error &error) {
	process::child_function<std::vector<std::string>> child_function = [](std::vector<std::string> command_parts) {
		// Transform arguments std::vector of std::strings to array of c strings.
		char **argv = (char **)alloca((command_parts.size() + 1) * sizeof(char *)); // +1 for nullptr termination.
		argv[command_parts.size()] = nullptr;
		for (std::vector<std::string>::size_type i = 0; i < command_parts.size(); i++) {
			std::string part = command_parts[i];
			long unsigned int length = part.length();
			// Allocate on stack.
			argv[i] = (char *)alloca(length + 1 * sizeof(char));
			// Null terminate.
			argv[i][length] = (char)NULL;

			// Copy value because c_str() returns const and we need mutable.
			memcpy(argv[i], part.data(), part.length());
		}

		// Replace current program with the new program.
		// A call to execve terminates the current program and replaces it with a new one so this should not return, unless an error occurrs.
		int error = execve(command_parts[0].c_str(), argv, environ);
		if (error == -1) {
			std::string command_string;
			for (const std::string &part : command_parts) {
				command_string += " " + part;
			}
			std::exit(pgm::error().strerror().append(std::format("Error running command: \"{}\".", command_string)).print());
		}
	};
	
	process::child child = process::fork(child_function, command_parts, error);
	if (error) {
		std::string command_string;
		for (const std::string &part : command_parts) {
			command_string += " " + part;
		}
		error.append(std::format("Error running command: \"{}\".", command_string));
		return process::child();
	}
	return child;
}