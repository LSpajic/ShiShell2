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
#define MAX_INPUT 1024
char **history = NULL;
int history_count = 0;
char input[MAX_STRING];
void clear_history()
{
    for (int i = 0; i < history_count; i++)
    {
        free(history[i]);
    }
    free(history);
    history = NULL;
    history_count = 0;
}
void history_command(char *argument)
{
    if (argument == NULL)
    {
        for (int i = 0; i < history_count; i++)
        {
            printf("%d %s\n", i + 1, history[i]);
        }
        return;
    }
    else if (strcmp(argument, "-c") == 0 || strcmp(argument, "clear") == 0)
    {
        clear_history();
        return;
    }
}
void load_history()
{
    FILE *file = fopen("history.txt", "r");
    if (!file)
        return;
    char buffer[MAX_INPUT];
    while (fgets(buffer, sizeof(buffer), file))
    {
        buffer[strcspn(buffer, "\n")] = '\0';
        char *line = strdup(buffer);
        if (!line)
            continue;
        char **temp = realloc(history, (history_count + 1) * sizeof(char *));
        if (!temp)
        {
            free(line);
            continue;
        }
        history = temp;
        history[history_count++] = line;
    }
    fclose(file);
}
void add_history(char *line)
{
    char **temp = realloc(history, (history_count + 1) * sizeof(char *));
    if (!temp)
        return;
    history = temp;
    history[history_count++] = strdup(line);
}
void save_history()
{
    FILE *file = fopen("history.txt", "w");
    if (!file)
        return;
    for (int i = 0; i < history_count; i++)
    {
        fprintf(file, "%s\n", history[i]);
    }
    fclose(file);
}
