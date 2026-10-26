#ifndef PS_H
#define PS_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <termios.h>
#define MAX_STRING 32
struct ProcessInformation
{
    pid_t pid;
    char name[MAX_STRING];
};
extern struct ProcessInformation processList[MAX_STRING];
extern int nProcess;
void ps_command();
void kill_command(char *argument);
void add_process(pid_t pid, char *name);
void remove_process(pid_t pid);
void exit_command();
#endif 
