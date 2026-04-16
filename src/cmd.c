// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmd.h"
#include "utils.h"

#define READ		0
#define WRITE		1

static bool shell_cd(word_t *dir)
{
    if (dir == NULL) return false;
    char *path = get_word(dir);
    if (!path) return false;
    char *clean_path = path;
    int len = strlen(path);
    if (len >= 2 && (path[0] == '"' || path[0] == '\'')) {
        clean_path = path + 1;
        if (path[len - 1] == '"' || path[len - 1] == '\'') {
            path[len - 1] = '\0';
        }
    }
    len = strlen(clean_path);
    if (len > 1 && clean_path[len - 1] == '/') {
        clean_path[len - 1] = '\0';
    }
    int x = chdir(clean_path);
    free(path); 
    return (x == 0);
}

static int shell_exit(void)
{
	return SHELL_EXIT; 
}

static int parse_simple(simple_command_t *s, int level, command_t *father)
{
    if (s == NULL || s->verb == NULL)
        return 1;
    if (!strcmp(s->verb->string, "exit") || !strcmp(s->verb->string, "quit"))
        return shell_exit();
    if (s->verb->next_part && !strcmp(s->verb->next_part->string, "=")) {
        char *name = s->verb->string;
        char *value = get_word(s->verb->next_part->next_part);
        int res = setenv(name, value, 1);
        free(value);
        return res;
    }
    if (!strcmp(s->verb->string, "true"))
        return 0;
    if (!strcmp(s->verb->string, "false"))
        return 1;
    if (!strcmp(s->verb->string, "cd")) {
        if (!s->in && !s->out && !s->err) {
            return shell_cd(s->params) ? 0 : 1;
        }
    }
    pid_t pid = fork();
    if (pid < 0) {
        return 1;
    }
    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        if (s->in) {
            char *fname = get_word(s->in);
            int fd = open(fname, O_RDONLY);
            free(fname);
            if (fd < 0) exit(1);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (s->out) {
            char *fname = get_word(s->out);
            int flags = O_WRONLY | O_CREAT | ((s->io_flags & IO_OUT_APPEND) ? O_APPEND : O_TRUNC);
            int fd = open(fname, flags, 0644);
            free(fname);
            if (fd < 0) exit(1);
            dup2(fd, STDOUT_FILENO);
            if (s->err && !strcmp(s->err->string, s->out->string)) {
                dup2(fd, STDERR_FILENO);
            }
            close(fd);
        }

        if (s->err && (!s->out || strcmp(s->err->string, s->out->string))) {
            char *fname = get_word(s->err);
            int flags = O_WRONLY | O_CREAT | ((s->io_flags & IO_ERR_APPEND) ? O_APPEND : O_TRUNC);
            int fd = open(fname, flags, 0644);
            free(fname);
            if (fd < 0) exit(1);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }

        if (!strcmp(s->verb->string, "cd")) {
            bool res = shell_cd(s->params);
            exit(res ? 0 : 1);
        }

        int argc;
        char **argv = get_argv(s, &argc);
        execvp(s->verb->string, argv);

        fprintf(stderr, "Execution failed for '%s'\n", s->verb->string);
        free(argv);
        exit(127);

    } else {
        int status;
        waitpid(pid, &status, 0);
        if (!strcmp(s->verb->string, "cd")) {
            return shell_cd(s->params) ? 0 : 1;
        }
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        return 1;
    }
}


static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	pid_t pid1, pid2;
	int rc1, rc2, status1, status2;
	pid1 = fork();
	if (pid1 == 0) {
		rc1 = parse_command(cmd1, level+1, father);
		exit(rc1);
	}
	pid2 = fork();
	if (pid2 == 0) {
		rc2 = parse_command(cmd2, level+1, father);
		exit(rc2);
	}
	waitpid(pid1, &status1, 0);
	waitpid(pid2, &status2, 0);
	return true; 
}

static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
		command_t *father)
{
	if (!cmd1 || !cmd2)
		return false;
	int pipefd[2];
	pid_t pid;
	if (pipe(pipefd) < 0)
		return false;
	pid = fork();
	if (pid == 0) {
		close(pipefd[WRITE]);
		dup2(pipefd[READ], 0);
		close(pipefd[READ]);
		exit(parse_command(cmd2, level + 1, father));
	} else {
		int stdout_copy = dup(1);
		close(pipefd[READ]);
		dup2(pipefd[WRITE], 1);
		close(pipefd[WRITE]);
		int rc1 = parse_command(cmd1, level + 1, father);

		dup2(stdout_copy, 1);
		close(stdout_copy);

		int status;

		waitpid(pid, &status, 0);
		int rc2 = 1;

		if (WIFEXITED(status))
			rc2 = WEXITSTATUS(status);
		return rc2 == 0;
	}
}

int parse_command(command_t *c, int level, command_t *father)
{
	int rc;
	if (c == NULL)
		return shell_exit();
	rc = SHELL_EXIT;
	if (c->op == OP_NONE) {
		rc = parse_simple(c->scmd, level, father);
		if (rc == shell_exit())
			return rc;
		if (rc) {
			printf("Execution failed for '%s'\n", c->scmd->verb->string);
			fflush(stdout);
			return 1;
		}
		return 0; 
	}

	switch (c->op) {
	case OP_SEQUENTIAL:
		rc = parse_command(c->cmd1, level + 1, c);
		rc = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PARALLEL:
		rc = 1;
		if (run_in_parallel(c->cmd1, c->cmd2, level + 1, c) == true)
			rc = 0;
		break;

	case OP_CONDITIONAL_NZERO:
		 rc = parse_command(c->cmd1, level + 1, c);
		if (rc)
			rc = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_CONDITIONAL_ZERO:
		 rc = parse_command(c->cmd1, level + 1, c);
		if (!rc)
			rc = parse_command(c->cmd2, level + 1, c);
		break;

	case OP_PIPE:
		 rc = 1;
		if (run_on_pipe(c->cmd1, c->cmd2, level + 1, c) == true)
			rc = 0;
		break;

	default:
		return SHELL_EXIT;
	}

	return rc; 
}
