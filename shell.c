#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAX_N_PROGS 100

int get_prog_names(char* line, char* prog_names[]) {
	int num_progs = 0;
	const char delim[2] = " |";
	char* token = strtok(line, delim);
	while (token != NULL) {
		prog_names[num_progs] = token;
		token = strtok(NULL, " |");
		num_progs++;
	}
	return num_progs;
}

// wrapper of close API with self-contained checking
void close_checked(int fd) {
	if (close(fd) == -1) {
		perror("close");
		exit(-1);
	}
}

void execute_line(char* line) {
    char* prog_names[MAX_N_PROGS];
	int num_progs = get_prog_names(line, prog_names);

	pid_t pid;
	int wstatus;
	char* argv[2];
	int pipefd[2];
	int read_end, write_end;

    // use NULL to mark the end of argv
	// we don't need to support command-line arguments
	argv[1] = '\0';

    // fork and execute programs, redirect I/O to pipes
	for (int i = 0; i < num_progs; i++) {
		argv[0] = prog_names[i];
		// if not the first one, write_end fd must be valid and used, thus free here 
		if (i > 0) {
			close_checked(write_end);
		}
		// if not the last one, allocate pipe for communication between this one and next one
		if (i < num_progs - 1) {
			if (pipe(pipefd) == -1) {
				perror("pipe");
				exit(-1);
			}
			// only updating write_end, since read_end allocated previously is intended
			// to be used this time
			write_end = pipefd[1];
		}
		pid = fork();
		if (pid == -1) {
			perror("fork");
			exit(-1);
		}
		if (pid == 0) { // child process
		    // if not first one, replace stdin with read_end of previous pipe
			if (i > 0) {
				if (dup2(read_end, STDIN_FILENO) == -1) {
					perror("dup2");
					exit(-1);
				}
				close_checked(read_end);
			}
			// if not the last one, replace stdout with write_end of this pipe
			if (i < num_progs - 1) {
				if (dup2(write_end, STDOUT_FILENO) == -1) {
					perror("dup2");
					exit(-1);
				}
				close_checked(write_end);
			}
			// load program image
			if (execvp(prog_names[i], argv) == -1) {
				perror("execvp");
			}
			// if not exited normally in program, child process will exit with -1
			exit(-1);
		}
		// if not last one, read_end is used, thus free here,
		// and will be updated to this pipe's read end
		if (i < num_progs - 1) {
			read_end = pipefd[0];
		}
	}
	while ((pid = wait(&wstatus)) != -1) {
		// do nothing, waiting
	}
	// if wait exited *not* because the children has all terminated, report error
	if (errno != ECHILD) {
		perror("wait");
		exit(-1);
	}
}

int main() {
	char* line = readline("> ");
	while (line != NULL) {
		execute_line(line);
		free(line);
		line = readline("> ");
	}
}
