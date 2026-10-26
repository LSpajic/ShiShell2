
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include "history.h"
#include "ps.h"
#include "BuiltInCommands.h"
#include "parser.h"
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define COLOR_CYAN   "\x1b[36m"
#define COLOR_RESET  "\x1b[0m"
#define MAX_INPUT 1024
/* ------------------------------------------------------------------ */
static void info_command(char **argv, int argc);
static void bang_command(const char *arg, char *line_buf);
static int  execute_pipeline(Pipeline *pl);
/* ------------------------------------------------------------------ */
typedef struct {
    const char *name;
    void      (*fn)(char **argv, int argc);
    const char *description;
} BuiltIn;
static void wrap_exit   (char **argv, int argc) { (void)argv; (void)argc; save_history(); exit(0); }
static void wrap_cd     (char **argv, int argc) { cd_command(argc > 1 ? argv[1] : NULL); }
static void wrap_history(char **argv, int argc) { history_command(argc > 1 ? argv[1] : NULL); }
static void wrap_ps     (char **argv, int argc) { (void)argv; (void)argc; ps_command(); }
static void wrap_kill   (char **argv, int argc) { kill_command(argc > 1 ? argv[1] : NULL); }
static BuiltIn builtins[] = {
    { "exit",    wrap_exit,    "Exit the shell"                  },
    { "cd",      wrap_cd,      "Change the current directory"    },
    { "history", wrap_history, "Show the history of commands"    },
    { "ps",      wrap_ps,      "Show the current processes"      },
    { "kill",    wrap_kill,    "Kill a process"                  },
    { "info",    info_command, "Print system / command info"     },
};
static const int N_BUILTINS = (int)(sizeof(builtins) / sizeof(builtins[0]));
/* ------------------------------------------------------------------ */
static struct sigaction g_old_sigint; 
static pid_t            g_fg_pid = 0; 
/* ------------------------------------------------------------------ */
static void print_prompt(void)
{
    char cwd[PATH_MAX];
    struct utsname u;
    getcwd(cwd, sizeof(cwd));
    uname(&u);
    printf(COLOR_CYAN "%s#%s -> " COLOR_RESET, u.nodename, cwd);
    fflush(stdout);
}
/* ------------------------------------------------------------------ */
static void info_command(char **argv, int argc)
{
    if (argc < 2) {
        struct utsname u;
        uname(&u);
        printf("System name: %s\n", u.sysname);
        printf("Node name:   %s\n", u.nodename);
        printf("Release:     %s\n", u.release);
        printf("Version:     %s\n", u.version);
        printf("Machine:     %s\n", u.machine);
        printf("Shell:       ShiShell v2.0\n");
        return;
    }
    for (int i = 0; i < N_BUILTINS; i++) {
        if (strcmp(argv[1], builtins[i].name) == 0) {
            printf("%s: %s\n", builtins[i].name, builtins[i].description);
            return;
        }
    }
    fprintf(stderr, "info: unknown command '%s'\n", argv[1]);
}
/* ------------------------------------------------------------------ */
static void bang_command(const char *arg, char *line_buf)
{
    if (!arg || !*arg) {
        fprintf(stderr, "usage: !<number>\n");
        return;
    }
    int num = atoi(arg);
    if (num <= 0 || num > history_count) {
        fprintf(stderr, "shishell: !%d: event not found\n", num);
        line_buf[0] = '\0';
        return;
    }
    strncpy(line_buf, history[num - 1], MAX_INPUT - 1);
    line_buf[MAX_INPUT - 1] = '\0';
}
/* ------------------------------------------------------------------ */
static void exec_segment(Command *cmd, int fd_in, int fd_out)
{
    sigaction(SIGINT, &g_old_sigint, NULL);
    if (fd_in != STDIN_FILENO) {
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }
    if (fd_out != STDOUT_FILENO) {
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }
    if (cmd->redirect_in) {
        int fd = open(cmd->redirect_in, O_RDONLY);
        if (fd < 0) {
            perror(cmd->redirect_in);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (cmd->redirect_out) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->redirect_out, flags, 0644);
        if (fd < 0) {
            perror(cmd->redirect_out);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    execvp(cmd->argv[0], cmd->argv);
    fprintf(stderr, "shishell: %s: %s\n", cmd->argv[0], strerror(errno));
    exit(127);
}
/* ------------------------------------------------------------------ */
static int execute_pipeline(Pipeline *pl)
{
    /* ---- Single built-in (no pipe): run inline, no fork ---- */
    if (pl->ncmds == 1) {
        Command *cmd = &pl->cmds[0];
        if (cmd->argc == 0) return -1;
        for (int i = 0; i < N_BUILTINS; i++) {
            if (strcmp(cmd->argv[0], builtins[i].name) == 0) {
                builtins[i].fn(cmd->argv, cmd->argc);
                return -1;
            }
        }
    }
    /* ---- Fork pipeline ---- */
    int    n   = pl->ncmds;
    pid_t *pids = malloc(n * sizeof(pid_t));
    int    fd_in = STDIN_FILENO; 
    for (int i = 0; i < n; i++) {
        int pipefd[2] = {-1, -1};
        if (i < n - 1) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                free(pids);
                return -1;
            }
        }
        int fd_out = (i < n - 1) ? pipefd[1] : STDOUT_FILENO;
        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            free(pids);
            return -1;
        }
        if (child == 0) {
            setpgid(0, 0);
            if (!pl->background)
                tcsetpgrp(STDIN_FILENO, getpgrp());
            if (i < n - 1) close(pipefd[0]);
            exec_segment(&pl->cmds[i], fd_in, fd_out);
        }
        setpgid(child, pids[0] > 0 ? pids[0] : child); 
        pids[i] = child;
        add_process(child, pl->cmds[i].argv[0]);
        if (fd_in != STDIN_FILENO)  close(fd_in);
        if (fd_out != STDOUT_FILENO) close(fd_out);
        fd_in = (i < n - 1) ? pipefd[0] : -1; 
    }
    pid_t last = pids[n - 1];
    if (!pl->background) {
        g_fg_pid = pids[0];
        tcsetpgrp(STDIN_FILENO, getpgid(pids[0]));
        for (int i = 0; i < n; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            remove_process(pids[i]);
        }
        g_fg_pid = 0;
    }
    free(pids);
    return (int)last;
}
/* ------------------------------------------------------------------ */
static int dispatch(char *line_buf)
{
    if (!line_buf[0]) return 1;
    if (line_buf[0] == '!') {
        char new_line[MAX_INPUT] = {0};
        bang_command(line_buf + 1, new_line);
        if (!new_line[0]) return 1;
        printf("%s\n", new_line); 
        strncpy(line_buf, new_line, MAX_INPUT - 1);
    }
    add_history(line_buf);
    if (strcmp(line_buf, "exit") == 0) {
        save_history();
        exit(0);
    }
    Pipeline pl;
    if (parse_pipeline(line_buf, &pl) < 0)
        return 1;
    execute_pipeline(&pl);
    free_pipeline(&pl);
    return 1;
}
/* ------------------------------------------------------------------ */
static void handle_sigint(int sig)
{
    (void)sig;
    if (g_fg_pid > 0)
        kill(-g_fg_pid, SIGINT); 
    write(STDOUT_FILENO, "\n", 1);
}
static void handle_sigchld(int sig)
{
    (void)sig;
    pid_t p;
    while ((p = waitpid(-1, NULL, WNOHANG)) > 0)
        remove_process(p);
}
/* ------------------------------------------------------------------ */
int main(void)
{
    pid_t my_pid = getpid();
    add_process(my_pid, "ShiShell");
    load_history();
    /* ---- Signal setup ---- */
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;
    act.sa_handler = handle_sigint;
    sigaction(SIGINT, &act, &g_old_sigint);
    act.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &act, NULL);
    act.sa_handler = SIG_IGN;
    sigaction(SIGTTOU, &act, NULL); 
    sigaction(SIGTTIN, &act, NULL);
    struct termios shell_tmodes;
    tcgetattr(STDIN_FILENO, &shell_tmodes);
    /* ---- REPL ---- */
    char input[MAX_INPUT];
    while (1) {
        print_prompt();
        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            clearerr(stdin);
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        dispatch(input);
        tcsetpgrp(STDIN_FILENO, getpgrp());
        tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes);
    }
    save_history();
    return 0;
}
