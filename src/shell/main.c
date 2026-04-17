/* ===========================================================================
 * electronOS Shell — Main Entry Point
 * ===========================================================================
 * A custom interactive shell for electronOS that:
 *   1. Displays a welcome banner with ASCII art
 *   2. Shows the 30 most common commands in a formatted table
 *   3. Provides a REPL with readline-based input and history
 *   4. Handles built-in commands (cd, pwd, help, clear, etc.)
 *   5. Forks/execs external commands
 *   6. Supports basic piping and redirection
 *
 * Build: make  (see Makefile)
 * =========================================================================== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <limits.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "commands.h"

/* ---- Constants ---------------------------------------------------------- */
#define MAX_ARGS    256
#define MAX_PIPES   16
#define PROMPT_SIZE 8192

/* ---- Globals ------------------------------------------------------------ */
static volatile sig_atomic_t g_child_pid = 0;

/* ---- Signal handlers ---------------------------------------------------- */
static void sigint_handler(int sig) {
    (void)sig;
    if (g_child_pid > 0) {
        kill(g_child_pid, SIGINT);
    } else {
        if (write(STDOUT_FILENO, "\n", 1) < 0) { /* ignore */ }
        rl_on_new_line();
        rl_replace_line("", 0);
        rl_redisplay();
    }
}

static void sigchld_handler(int sig) {
    (void)sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0) {
        /* Reap zombies */
    }
}

/* ---- Command-line parsing ----------------------------------------------- */

/* Split a line into argv tokens, respecting quotes */
static int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args - 1) {
        /* Skip whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        char *start;
        if (*p == '"' || *p == '\'') {
            /* Quoted string */
            char quote = *p++;
            start = p;
            while (*p && *p != quote) p++;
            if (*p) *p++ = '\0';
        } else {
            start = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }

        argv[argc++] = start;
    }
    argv[argc] = NULL;
    return argc;
}

/* ---- I/O redirection ---------------------------------------------------- */

typedef struct {
    char *infile;       /* stdin  redirect (<)  */
    char *outfile;      /* stdout redirect (>)  */
    char *appendfile;   /* stdout append   (>>) */
    char *errfile;      /* stderr redirect (2>) */
} redirect_t;

static void parse_redirects(int *argc, char **argv, redirect_t *redir) {
    memset(redir, 0, sizeof(*redir));
    int new_argc = 0;

    for (int i = 0; i < *argc; i++) {
        if (strcmp(argv[i], "<") == 0 && i + 1 < *argc) {
            redir->infile = argv[++i];
        } else if (strcmp(argv[i], ">>") == 0 && i + 1 < *argc) {
            redir->appendfile = argv[++i];
        } else if (strcmp(argv[i], ">") == 0 && i + 1 < *argc) {
            redir->outfile = argv[++i];
        } else if (strcmp(argv[i], "2>") == 0 && i + 1 < *argc) {
            redir->errfile = argv[++i];
        } else {
            argv[new_argc++] = argv[i];
        }
    }
    argv[new_argc] = NULL;
    *argc = new_argc;
}

static int apply_redirects(const redirect_t *redir) {
    if (redir->infile) {
        int fd = open(redir->infile, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "electronos: %s: %s\n", redir->infile,
                    strerror(errno));
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (redir->outfile) {
        int fd = open(redir->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "electronos: %s: %s\n", redir->outfile,
                    strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (redir->appendfile) {
        int fd = open(redir->appendfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0) {
            fprintf(stderr, "electronos: %s: %s\n", redir->appendfile,
                    strerror(errno));
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    if (redir->errfile) {
        int fd = open(redir->errfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) {
            fprintf(stderr, "electronos: %s: %s\n", redir->errfile,
                    strerror(errno));
            return -1;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    return 0;
}

/* ---- Execute a single command ------------------------------------------- */
static int execute_command(int argc, char **argv) {
    if (argc == 0) return 0;

    /* Check built-in commands first */
    const char *cmd = argv[0];

    /* Extra built-ins not in the 30-command table */
    if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "logout") == 0) {
        return cmd_exit(argc, argv);
    }
    if (strcmp(cmd, "whoami") == 0) {
        return cmd_whoami(argc, argv);
    }
    if (strcmp(cmd, "history") == 0) {
        return cmd_history(argc, argv);
    }
    if (strcmp(cmd, "alias") == 0) {
        return cmd_alias(argc, argv);
    }
    if (strcmp(cmd, "export") == 0) {
        return cmd_export(argc, argv);
    }
    if (strcmp(cmd, "sysinfo") == 0) {
        return cmd_sysinfo(argc, argv);
    }

    /* Check the command table for built-ins */
    const command_entry_t *entry = commands_find(cmd);
    if (entry && entry->builtin && entry->handler) {
        return entry->handler(argc, argv);
    }

    /* Parse I/O redirects */
    redirect_t redir;
    parse_redirects(&argc, argv, &redir);

    /* External command: fork and exec */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child process */
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        if (apply_redirects(&redir) != 0) {
            _exit(1);
        }

        execvp(argv[0], argv);
        fprintf(stderr, "electronos: %s: command not found\n", argv[0]);
        _exit(127);
    }

    /* Parent: wait for child */
    g_child_pid = pid;
    int status;
    waitpid(pid, &status, 0);
    g_child_pid = 0;

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

/* ---- Execute a pipeline ------------------------------------------------- */
static int execute_pipeline(char *line) {
    /* Split by pipe characters */
    char *commands[MAX_PIPES];
    int num_cmds = 0;

    char *saveptr;
    char *token = strtok_r(line, "|", &saveptr);
    while (token && num_cmds < MAX_PIPES) {
        commands[num_cmds++] = token;
        token = strtok_r(NULL, "|", &saveptr);
    }

    if (num_cmds == 1) {
        /* No pipes: simple command */
        char *argv[MAX_ARGS];
        int argc = parse_args(commands[0], argv, MAX_ARGS);
        return execute_command(argc, argv);
    }

    /* Multi-stage pipeline */
    int prev_fd = -1;
    int status = 0;
    pid_t pids[MAX_PIPES];

    for (int i = 0; i < num_cmds; i++) {
        int pipefd[2] = {-1, -1};

        if (i < num_cmds - 1) {
            if (pipe(pipefd) != 0) {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /* Child */
            signal(SIGINT, SIG_DFL);

            if (prev_fd != -1) {
                dup2(prev_fd, STDIN_FILENO);
                close(prev_fd);
            }
            if (pipefd[1] != -1) {
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            if (pipefd[0] != -1) close(pipefd[0]);

            char *argv[MAX_ARGS];
            int argc = parse_args(commands[i], argv, MAX_ARGS);

            redirect_t redir;
            parse_redirects(&argc, argv, &redir);
            apply_redirects(&redir);

            if (argc > 0) {
                execvp(argv[0], argv);
                fprintf(stderr, "electronos: %s: command not found\n", argv[0]);
            }
            _exit(127);
        }

        pids[i] = pid;
        if (prev_fd != -1) close(prev_fd);
        if (pipefd[1] != -1) close(pipefd[1]);
        prev_fd = pipefd[0];
    }

    /* Wait for all children */
    for (int i = 0; i < num_cmds; i++) {
        int child_status;
        waitpid(pids[i], &child_status, 0);
        if (i == num_cmds - 1 && WIFEXITED(child_status)) {
            status = WEXITSTATUS(child_status);
        }
    }

    return status;
}

/* ---- Build prompt string ------------------------------------------------ */
static void build_prompt(char *buf, size_t bufsize) {
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    }

    /* Shorten home directory to ~ */
    const char *home = getenv("HOME");
    const char *display_cwd = cwd;
    char short_cwd[PATH_MAX];

    if (home && strncmp(cwd, home, strlen(home)) == 0) {
        snprintf(short_cwd, sizeof(short_cwd), "~%s", cwd + strlen(home));
        display_cwd = short_cwd;
    }

    struct passwd *pw = getpwuid(getuid());
    const char *user = pw ? pw->pw_name : "?";

    if (getuid() == 0) {
        snprintf(buf, bufsize,
                 "\001\033[1;31m\002%s\001\033[0m\002"
                 ":\001\033[1;34m\002%s\001\033[0m\002# ",
                 user, display_cwd);
    } else {
        snprintf(buf, bufsize,
                 "\001\033[1;32m\002%s\001\033[0m\002"
                 "@\001\033[1;36m\002electronos\001\033[0m\002"
                 ":\001\033[1;34m\002%s\001\033[0m\002$ ",
                 user, display_cwd);
    }
}

/* ---- Tab completion ----------------------------------------------------- */
static char *command_generator(const char *text, int state) {
    static int index, len;

    if (!state) {
        index = 0;
        len = strlen(text);
    }

    /* Complete from command table */
    while (index < NUM_COMMANDS) {
        const char *name = COMMANDS[index].name;
        index++;
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    /* Additional built-ins */
    static const char *extra_builtins[] = {
        "exit", "logout", "whoami", "history", "alias", "export", "sysinfo",
        NULL
    };
    static int extra_idx;
    if (index == NUM_COMMANDS) {
        extra_idx = 0;
        index++;
    }
    while (extra_builtins[extra_idx]) {
        const char *name = extra_builtins[extra_idx];
        extra_idx++;
        if (strncmp(name, text, len) == 0) {
            return strdup(name);
        }
    }

    return NULL;
}

static char **command_completion(const char *text, int start, int end) {
    (void)end;
    if (start == 0) {
        return rl_completion_matches(text, command_generator);
    }
    return NULL;  /* Fall back to filename completion */
}

/* ---- Main --------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    /* Set up signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    sa.sa_handler = sigint_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    /* Configure readline */
    rl_attempted_completion_function = command_completion;
    rl_readline_name = "electronos";

    /* Load history */
    const char *home = getenv("HOME");
    char histfile[PATH_MAX];
    if (home) {
        snprintf(histfile, sizeof(histfile), "%s/.electronos_history", home);
        read_history(histfile);
    }

    /* Display welcome screen */
    commands_print_welcome();

    /* Main REPL */
    char prompt[PROMPT_SIZE];

    while (1) {
        build_prompt(prompt, sizeof(prompt));

        char *line = readline(prompt);
        if (!line) {
            /* EOF (Ctrl+D) */
            printf("\nGoodbye.\n");
            break;
        }

        /* Skip empty lines */
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        if (*trimmed == '\0') {
            free(line);
            continue;
        }

        /* Add to history */
        add_history(trimmed);
        history_add(trimmed);

        /* Handle semicolon-separated commands */
        char *saveptr;
        char *cmd = strtok_r(trimmed, ";", &saveptr);
        while (cmd) {
            /* Skip leading whitespace */
            while (*cmd == ' ' || *cmd == '\t') cmd++;
            if (*cmd) {
                /* Check for background execution */
                int len = strlen(cmd);
                bool background = false;
                if (len > 0 && cmd[len-1] == '&') {
                    cmd[len-1] = '\0';
                    background = true;
                }

                if (background) {
                    pid_t pid = fork();
                    if (pid == 0) {
                        execute_pipeline(cmd);
                        _exit(0);
                    } else if (pid > 0) {
                        printf("[bg] %d\n", pid);
                    }
                } else {
                    execute_pipeline(cmd);
                }
            }
            cmd = strtok_r(NULL, ";", &saveptr);
        }

        free(line);
    }

    /* Save history */
    if (home) {
        write_history(histfile);
    }

    return 0;
}
