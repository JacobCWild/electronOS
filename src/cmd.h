#ifndef CMD_H
#define CMD_H

void parse_command(const char *input);
void handle_echo(const char *args);
void handle_ls(void);
void handle_cd(const char *path);
void handle_whoami(void);

#endif // CMD_H
