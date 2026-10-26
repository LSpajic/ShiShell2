#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <termios.h>
#define MAX_STRING 32
#define MAX_PID 32
struct ProcessInformation
{
    pid_t pid;
    char *name;
};
struct ProcessInformation processList[MAX_PID];
int nProcess = 0;
void add_process(pid_t pid, char *name)
{
    processList[nProcess].pid = pid;
    processList[nProcess].name = name;
    nProcess++;
}
void remove_process(pid_t pid)
{
    for (int i = 0; i < nProcess; i++)
    {
        if (processList[i].pid == pid)
        {
            for (int j = i; j < nProcess - 1; j++)
            {
                processList[j] = processList[j + 1];
            }
            nProcess--;
            return;
        }
    }
}
void ps_command()
{
    if (fork() == 0)
    {
        add_process(getpid(), "ps");
        printf("PID\tName\n");
        for (int i = 0; i < nProcess; i++)
        {
            printf("%d\t%s\n", processList[i].pid, processList[i].name);
        }
        remove_process(getpid());
        exit(0);
    }
    else
    {
        wait(NULL);
    }
    return;
}
void kill_command(char *argument)
{
    if (!*argument || !argument)
    {
        fprintf(stderr, "Usage: kill <pid>\n");
        return;
    }
    int pid = atoi(argument);
    if (pid == 0)
    {
        fprintf(stderr, "kill: invalid argument\n");
        return;
    }
    if (kill(pid, SIGKILL) == -1)
    {
        perror("kill");
    }
    remove_process(pid);
    return;
}
void exit_command()
{
    for (int i = 1; i < nProcess; i++)
    {
        kill(processList[i].pid, SIGHUP);
    }
    kill(processList[0].pid, SIGKILL);
}
