/* ===========================================================================
 * electronOS Shell — Command Implementations
 * ===========================================================================
 * Implements built-in commands and the command reference table showing
 * the 30 most common system commands.
 * =========================================================================== */

#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <errno.h>

/* ---- History (simple ring buffer) --------------------------------------- */
#define HISTORY_SIZE 100
static char *history_entries[HISTORY_SIZE];
static int   history_count = 0;
static int   history_pos   = 0;

void history_add(const char *line) {
    if (!line || !line[0]) return;

    if (history_entries[history_pos]) {
        free(history_entries[history_pos]);
    }
    history_entries[history_pos] = strdup(line);
    history_pos = (history_pos + 1) % HISTORY_SIZE;
    if (history_count < HISTORY_SIZE) history_count++;
}

/* ---- Aliases (simple key=value store) ----------------------------------- */
#define MAX_ALIASES 64
static struct { char *name; char *value; } aliases[MAX_ALIASES];
static int alias_count = 0;

/* ---- Environment exports ------------------------------------------------ */
/* We delegate to setenv/getenv, but track user additions */

/* ---- Command table ------------------------------------------------------ */
/* The 30 most common commands, organized by category */

const command_entry_t COMMANDS[NUM_COMMANDS] = {
    /* ---- File Management ---- */
    { "ls",        "List directory contents",       "Files",    false, NULL },
    { "cd",        "Change working directory",      "Files",    true,  cmd_cd },
    { "pwd",       "Print working directory",       "Files",    true,  cmd_pwd },
    { "cp",        "Copy files and directories",    "Files",    false, NULL },
    { "mv",        "Move or rename files",          "Files",    false, NULL },
    { "rm",        "Remove files or directories",   "Files",    false, NULL },
    { "mkdir",     "Create directories",            "Files",    false, NULL },
    { "cat",       "Display file contents",         "Files",    false, NULL },
    { "find",      "Search for files",              "Files",    false, NULL },
    { "chmod",     "Change file permissions",       "Files",    false, NULL },

    /* ---- Text Processing ---- */
    { "grep",      "Search text with patterns",     "Text",     false, NULL },
    { "head",      "Show first lines of a file",    "Text",     false, NULL },
    { "tail",      "Show last lines of a file",     "Text",     false, NULL },
    { "less",      "Page through file contents",    "Text",     false, NULL },
    { "nano",      "Simple text editor",            "Text",     false, NULL },

    /* ---- System ---- */
    { "ps",        "List running processes",        "System",   false, NULL },
    { "top",       "Interactive process viewer",    "System",   false, NULL },
    { "kill",      "Terminate a process",           "System",   false, NULL },
    { "df",        "Show disk space usage",         "System",   false, NULL },
    { "du",        "Show directory space usage",    "System",   false, NULL },
    { "free",      "Display memory usage",          "System",   false, NULL },
    { "systemctl", "Manage system services",        "System",   false, NULL },

    /* ---- Networking ---- */
    { "ping",      "Test network connectivity",     "Network",  false, NULL },
    { "ip",        "Show/manage network config",    "Network",  false, NULL },
    { "ssh",       "Secure remote shell",           "Network",  false, NULL },
    { "scp",       "Secure file copy (remote)",     "Network",  false, NULL },
    { "wget",      "Download files from the web",   "Network",  false, NULL },
    { "tar",       "Archive and compress files",    "Network",  false, NULL },

    /* ---- Shell Built-ins ---- */
    { "clear",     "Clear the terminal screen",     "Shell",    true,  cmd_clear },
    { "help",      "Show this command reference",   "Shell",    true,  cmd_help },
};

/* ---- Built-in: help ----------------------------------------------------- */
int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    commands_print_table();
    return 0;
}

/* ---- Built-in: cd ------------------------------------------------------- */
int cmd_cd(int argc, char **argv) {
    const char *path;

    if (argc < 2) {
        /* cd with no args goes to HOME */
        path = getenv("HOME");
        if (!path) {
            struct passwd *pw = getpwuid(getuid());
            path = pw ? pw->pw_dir : "/";
        }
    } else {
        path = argv[1];
    }

    if (chdir(path) != 0) {
        fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
        return 1;
    }
    return 0;
}

/* ---- Built-in: pwd ------------------------------------------------------ */
int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("%s\n", cwd);
    } else {
        perror("pwd");
        return 1;
    }
    return 0;
}

/* ---- Built-in: exit ----------------------------------------------------- */
int cmd_exit(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("Goodbye.\n");
    exit(0);
    return 0;  /* unreachable */
}

/* ---- Built-in: clear ---------------------------------------------------- */
int cmd_clear(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("\033[2J\033[H");
    fflush(stdout);
    return 0;
}

/* ---- Built-in: whoami --------------------------------------------------- */
int cmd_whoami(int argc, char **argv) {
    (void)argc; (void)argv;
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        printf("%s\n", pw->pw_name);
    } else {
        printf("uid=%d\n", getuid());
    }
    return 0;
}

/* ---- Built-in: history -------------------------------------------------- */
int cmd_history(int argc, char **argv) {
    (void)argc; (void)argv;
    int start = (history_pos - history_count + HISTORY_SIZE) % HISTORY_SIZE;
    for (int i = 0; i < history_count; i++) {
        int idx = (start + i) % HISTORY_SIZE;
        if (history_entries[idx]) {
            printf("  %4d  %s\n", i + 1, history_entries[idx]);
        }
    }
    return 0;
}

/* ---- Built-in: alias ---------------------------------------------------- */
int cmd_alias(int argc, char **argv) {
    if (argc < 2) {
        /* List aliases */
        for (int i = 0; i < alias_count; i++) {
            printf("alias %s='%s'\n", aliases[i].name, aliases[i].value);
        }
        return 0;
    }

    /* Parse name=value */
    char *eq = strchr(argv[1], '=');
    if (!eq) {
        fprintf(stderr, "alias: usage: alias name=value\n");
        return 1;
    }

    *eq = '\0';
    const char *name = argv[1];
    const char *value = eq + 1;

    /* Update existing or add new */
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].value);
            aliases[i].value = strdup(value);
            return 0;
        }
    }

    if (alias_count < MAX_ALIASES) {
        aliases[alias_count].name = strdup(name);
        aliases[alias_count].value = strdup(value);
        alias_count++;
    } else {
        fprintf(stderr, "alias: maximum aliases reached\n");
        return 1;
    }
    return 0;
}

/* ---- Built-in: export --------------------------------------------------- */
int cmd_export(int argc, char **argv) {
    if (argc < 2) {
        /* List environment */
        extern char **environ;
        for (char **env = environ; *env; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }

    char *eq = strchr(argv[1], '=');
    if (!eq) {
        /* Export existing variable */
        const char *val = getenv(argv[1]);
        if (val) {
            printf("%s=%s\n", argv[1], val);
        }
        return 0;
    }

    *eq = '\0';
    if (setenv(argv[1], eq + 1, 1) != 0) {
        perror("export");
        return 1;
    }
    return 0;
}

/* ---- Built-in: sysinfo ------------------------------------------------- */
int cmd_sysinfo(int argc, char **argv) {
    (void)argc; (void)argv;

    struct utsname uts;
    struct sysinfo si;

    printf("\n");
    printf("  ┌─────────────────────────────────────┐\n");
    printf("  │        electronOS System Info       │\n");
    printf("  └─────────────────────────────────────┘\n\n");

    if (uname(&uts) == 0) {
        printf("  OS:       %s %s\n", uts.sysname, uts.release);
        printf("  Host:     %s\n", uts.nodename);
        printf("  Arch:     %s\n", uts.machine);
    }

    if (sysinfo(&si) == 0) {
        long uptime_h = si.uptime / 3600;
        long uptime_m = (si.uptime % 3600) / 60;
        printf("  Uptime:   %ldh %ldm\n", uptime_h, uptime_m);
        printf("  RAM:      %lu MB / %lu MB\n",
               (si.totalram - si.freeram) / (1024*1024),
               si.totalram / (1024*1024));
        printf("  Procs:    %d\n", si.procs);
    }

    /* Current user */
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        printf("  User:     %s\n", pw->pw_name);
    }

    printf("\n");
    return 0;
}

/* ---- Utility: find command by name -------------------------------------- */
const command_entry_t *commands_find(const char *name) {
    for (int i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(COMMANDS[i].name, name) == 0) {
            return &COMMANDS[i];
        }
    }
    return NULL;
}

/* ---- Utility: print command table --------------------------------------- */
void commands_print_table(void) {
    const char *current_category = NULL;

    printf("\n");
    printf("  ┌──────────────────────────────────────────────────────┐\n");
    printf("  │             electronOS Command Reference             │\n");
    printf("  └──────────────────────────────────────────────────────┘\n");

    for (int i = 0; i < NUM_COMMANDS; i++) {
        if (!current_category ||
            strcmp(current_category, COMMANDS[i].category) != 0) {
            current_category = COMMANDS[i].category;
            printf("\n  \033[1;36m── %s ──\033[0m\n", current_category);
        }
        printf("    \033[1;37m%-12s\033[0m %s\n",
               COMMANDS[i].name, COMMANDS[i].description);
    }

    printf("\n  \033[1;36m── Additional Built-ins ──\033[0m\n");
    printf("    \033[1;37m%-12s\033[0m %s\n", "whoami",  "Display current user");
    printf("    \033[1;37m%-12s\033[0m %s\n", "history", "Show command history");
    printf("    \033[1;37m%-12s\033[0m %s\n", "alias",   "Create command aliases");
    printf("    \033[1;37m%-12s\033[0m %s\n", "export",  "Set environment variables");
    printf("    \033[1;37m%-12s\033[0m %s\n", "sysinfo", "Show system information");
    printf("    \033[1;37m%-12s\033[0m %s\n", "exit",    "Log out of electronOS");
    printf("\n  Type any command to execute it. Use 'help' to see this list again.\n\n");
}

/* ---- Utility: print welcome screen -------------------------------------- */
void commands_print_welcome(void) {
    struct passwd *pw = getpwuid(getuid());
    const char *username = pw ? pw->pw_name : "user";

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%A, %B %d %Y at %I:%M %p", tm);

    printf("\033[2J\033[H");  /* Clear screen */
    printf("\n");
    printf("  \033[1;37m╔═══════════════════════════════════════════════════════╗\033[0m\n");
    printf("  \033[1;37m║                                                       ║\033[0m\n");
    printf("  \033[1;37m║\033[0m       \033[1;36m┌─┐┬  ┌─┐┌─┐┌┬┐┬─┐┌─┐┌┐┌  ┌─┐┌─┐\033[0m               \033[1;37m║\033[0m\n");
    printf("  \033[1;37m║\033[0m       \033[1;36m├┤ │  ├┤ │   │ ├┬┘│ ││││  │ │└─┐\033[0m               \033[1;37m║\033[0m\n");
    printf("  \033[1;37m║\033[0m       \033[1;36m└─┘┴─┘└─┘└─┘ ┴ ┴└─└─┘┘└┘  └─┘└─┘\033[0m               \033[1;37m║\033[0m\n");
    printf("  \033[1;37m║                                                       ║\033[0m\n");
    printf("  \033[1;37m║\033[0m                    \033[0;37mv1.0.0\033[0m                             \033[1;37m║\033[0m\n");
    printf("  \033[1;37m║                                                       ║\033[0m\n");
    printf("  \033[1;37m╚═══════════════════════════════════════════════════════╝\033[0m\n");
    printf("\n");
    printf("  Welcome, \033[1;32m%s\033[0m! %s\n", username, time_str);
    printf("\n");

    commands_print_table();
}
