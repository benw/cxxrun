
// c++run - A C++ command interpreter.
//
// Copyright (c)2005 Ben Williamson

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <list>
#include <vector>
#include <iostream>

const char* usage_message =
	"\n"
	"Usage: %s [options] source.cpp [arguments]\n"
	"\n"
	"source.cpp is compiled to a temporary executable,\n"
	"with any options passed to g++. Then the executable\n"
	"is run with the arguments given.\n"
	"\n"
	"To make a C++ source file into a standalone executable,\n"
	"add the following on the first line:\n"
	"\n"
	"\t#!/usr/local/bin/c++run -Wall -Werror -O3\n"
	"\n";

int main(int argc, char* argv[])
{
	// Collect options that start with '-' for the compiler.
	// We concatenate them together and then split on spaces,
	// because when we're run as an interpreter they'll all
	// be concatenated together anyway.
	std::stringstream compile_stream;
	compile_stream << "g++";
	int src_index = 1;
	while ((src_index < argc) && (argv[src_index][0] == '-')) {
		compile_stream << " " << argv[src_index];
		src_index++;
	}

	// There should still be at least one thing left, the source name.
	if (src_index >= argc) {
		fprintf(stderr, usage_message, argv[0]);
		return 1;
	}

	// Make a temporary name for the executable
	const char* src_name = argv[src_index];
	char* exe_name = strdup("/tmp/c++run-XXXXXX");
	int exe_fd = mkstemp(exe_name);
	if (exe_fd < 0) {
		perror(src_name);
		return errno;
	}
	close(exe_fd);
	compile_stream << " -x c++ - -o " << exe_name;

	// Split the compiler command-line and make a null-terminated pointer array.
	std::list<std::string> compile_strings;
	std::vector<char*> compile_pointers;
	std::string option;
	while (compile_stream >> option) {
		compile_strings.push_back(option);
		char* s = &compile_strings.back()[0];
		compile_pointers.push_back(s);
	}
	compile_pointers.push_back(0);
	char** compile = &compile_pointers[0];

	// Open the source file, and skip the first line "#!/usr/bin/c++run"
	int src_fd = open(src_name, 0);
	if (src_fd < 0) {
		perror(src_name);
		return errno;
	}
	char c;
	ssize_t count;
	do {
		count = read(src_fd, &c, 1);
	} while ((count > 0) && (c != '\0') && (c != '\n') && (c != '\r'));
	if (count < 0) {
		perror(src_name);
		return errno;
	}

	// Make a pipe for sending the source to the compiler.
	int pipe_fds[2];
	if (pipe(pipe_fds) < 0) {
		perror(src_name);
		return errno;
	}
	int read_pipe = pipe_fds[0];
	int write_pipe = pipe_fds[1];
	
	pid_t child = fork();
	if (child == 0) {
		// This is the child.

		// Hook the pipe up to the compiler's stdin.
		close(0);
		dup(read_pipe);
		close(write_pipe);

		// Compile with g++ -x c++ - -o exe_name
#if 0
		execlp("g++", "g++", "-Wall", "-Werror", "-O3", "-x", "c++", "-",
			"-o", exe_name, 0);
#else
		execvp("g++", compile);
#endif
		return errno;
	}
	// This is the parent.

	// Read the rest of the source file, and pipe it to the child.
	char buf[4096];
	do {
		count = read(src_fd, buf, sizeof(buf));
		if (count < 0) {
			int result = errno;
			kill(child, SIGTERM);
			perror(src_name);
			return result;
		}
		const char* p = buf;
		ssize_t remaining = count;
		while (remaining) {
			ssize_t written = write(write_pipe, p, remaining);
			if (written < 0) {
				perror(src_name);
				return errno;
			}
			p += written;
			remaining -= written;
		}
	} while (count > 0);
	close(write_pipe);

	// Wait for the child to finish compiling.
	int status;
	if (waitpid(child, &status, 0) < 0) {
		perror(src_name);
		return errno;
	}
	if (status != 0) {
		return status;
	}

	// Fork and run the executable.
	child = fork();
	if (child == 0) {
		// This is the child.
		execv(exe_name, argv + src_index);
		return errno;
	}

	// When the executable exits, remove the exe.
	if (waitpid(child, &status, 0) < 0) {
		perror(src_name);
		return errno;
	}
	unlink(exe_name);
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return status;
}

