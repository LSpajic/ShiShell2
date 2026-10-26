#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <ctype.h>
#define MAX_STRING 32
#define PATH_MAX 256
#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"
#define MAX_INPUT 1024
void cd_command(char *path)
{
    if (path == NULL)
    {
        fprintf(stderr, "cd: missing argument\n");
        return;
    }
    if (chdir(path) == -1)
    {
        perror("ERROR");
    }
}
void print_logo()
{
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int columns = w.ws_col;
    FILE *file = fopen("logo_large", "r"); 
    if (columns < 70)
    {
        file = fopen("logo", "r"); 
    }
    else if (columns < 80)
    {
        file = fopen("logo_small", "r"); 
    }
    else if (columns < 120)
    {
        file = fopen("logo_medium", "r"); 
    }
    if (file == NULL)
    {
        return;
    }
    printf("\n"); 
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), file) != NULL)
    {
        printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET, buffer);
    }
}
