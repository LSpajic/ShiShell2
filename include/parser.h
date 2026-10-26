#ifndef PARSER_H
#define PARSER_H
#define MAX_ARGS   128
#define MAX_CMDS   32
typedef struct
{
    char *argv[MAX_ARGS];
    int   argc;
    char *redirect_in;   
    char *redirect_out;  
    int   append;        
} Command;
typedef struct
{
    Command cmds[MAX_CMDS];
    int     ncmds;
    int     background;
} Pipeline;
int  parse_pipeline(const char *input, Pipeline *pl);
void free_pipeline(Pipeline *pl);
#endif 
