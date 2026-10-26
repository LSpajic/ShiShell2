#ifndef HISTORY_H
#define HISTORY_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include "BuiltInCommands.h"
#define MAX_STRING 32
#define MAX_INPUT 1024
extern char **history;
extern int history_count;
extern char input[MAX_STRING];
void clear_history();
void history_command(char *argument);
void load_history();
void save_history();
void add_history(char *line);
#endif 
