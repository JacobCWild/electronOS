/* ===========================================================================
 * electronOS Shell — Commands Header
 * ===========================================================================
 * Defines the 30 common commands displayed on login and the built-in
 * command infrastructure for the electronOS shell.
 * =========================================================================== */

#ifndef ELECTRONOS_SHELL_COMMANDS_H
#define ELECTRONOS_SHELL_COMMANDS_H

#include <stdbool.h>

/* ---- Command table entry ------------------------------------------------ */
typedef struct {
    const char  *name;          /* Command name                              */
    const char  *description;   /* Short description                         */
    const char  *category;      /* Category for grouping                     */
    bool         builtin;       /* true = handled internally                 */
    int        (*handler)(int argc, char **argv);  /* Builtin handler        */
} command_entry_t;

/* ---- Command count ------------------------------------------------------ */
#define NUM_COMMANDS  30

/* ---- Global command table ----------------------------------------------- */
extern const command_entry_t COMMANDS[NUM_COMMANDS];

/* ---- Built-in command handlers ------------------------------------------ */
int cmd_help(int argc, char **argv);
int cmd_cd(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_exit(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_whoami(int argc, char **argv);
int cmd_history(int argc, char **argv);
int cmd_alias(int argc, char **argv);
int cmd_export(int argc, char **argv);
int cmd_sysinfo(int argc, char **argv);

/* ---- Utility functions -------------------------------------------------- */
void commands_print_table(void);
void commands_print_welcome(void);
const command_entry_t *commands_find(const char *name);

#endif /* ELECTRONOS_SHELL_COMMANDS_H */
