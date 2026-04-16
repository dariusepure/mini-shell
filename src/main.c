// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "../util/parser/parser.h"
#include "cmd.h"
#include "utils.h"

#define ANSI_COLOR_CYAN    "\001\x1b[1;36m\002"
#define ANSI_COLOR_GREEN   "\001\x1b[1;32m\002"
#define ANSI_COLOR_BLUE    "\001\x1b[1;34m\002"
#define ANSI_COLOR_RESET   "\001\x1b[0m\002"

void parse_error(const char *str, const int where)
{
    fprintf(stderr, "Parse error near %d: %s\n", where, str);
}

static char *ensure_balanced_quotes(char *line)
{
    if (!line) return NULL;

    int quotes = 0;
    for (int i = 0; line[i]; i++) {
        if (line[i] == '"') quotes++;
    }

    if (quotes % 2 != 0) {
        size_t len = strlen(line);
        char *new_line = malloc(len + 2);
        if (!new_line) return line;

        strcpy(new_line, line);
        new_line[len] = '"';
        new_line[len + 1] = '\0';

        free(line);
        return new_line;
    }

    return line;
}

static void start_shell(void)
{
    char *line;
    command_t *root;
    int ret;

    char hostname[HOST_NAME_MAX];
    char cwd[PATH_MAX];
    char prompt[PATH_MAX + HOST_NAME_MAX + 128];

    if (gethostname(hostname, HOST_NAME_MAX) != 0) {
        strcpy(hostname, "pc");
    }

    for (;;) {
        char *username = getenv("USER");
        if (!username) username = "user";
        
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            strcpy(cwd, "unknown");
        }
        snprintf(prompt, sizeof(prompt), 
                 ANSI_COLOR_CYAN "(Mini Shell) " ANSI_COLOR_GREEN "%s@%s" ANSI_COLOR_RESET ":" ANSI_COLOR_BLUE "%s" ANSI_COLOR_RESET "$ ", 
                 username, hostname, cwd);

        line = readline(prompt);

        if (line == NULL) {
            printf("\nExiting Mini Shell...\n");
            break;
        }

        line = ensure_balanced_quotes(line);

        if (*line != '\0') {
            add_history(line);
        }

        ret = 0;
        root = NULL;

        parse_line(line, &root);

        if (root != NULL) {
            ret = parse_command(root, 0, NULL);
        }

        free_parse_memory();
        free(line); 

        if (ret == SHELL_EXIT)
            break;
    }
}

int main(void)
{
    signal(SIGINT, SIG_IGN);
    rl_initialize();
    rl_bind_key('\t', rl_complete);
    rl_basic_word_break_characters = " \t\n\\@$><=;|&{(";
    rl_completer_quote_characters = "\"'";
    rl_filename_quote_characters = " ";
    rl_filename_quoting_desired = 1;
    rl_completion_append_character = ' '; 
    start_shell();
    clear_history();
    return EXIT_SUCCESS;
}
