
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <glob.h>
#include "parser.h"
/* ---------- internal token list ----------------------------------------- */
#define TOK_CAP 256
typedef struct {
    char **tokens;
    int    n;
    int    cap;
} TokenList;
static void tl_init(TokenList *tl)
{
    tl->cap    = TOK_CAP;
    tl->tokens = malloc(tl->cap * sizeof(char *));
    tl->n      = 0;
}
static void tl_push(TokenList *tl, char *s)
{
    if (tl->n >= tl->cap - 1) {
        tl->cap *= 2;
        tl->tokens = realloc(tl->tokens, tl->cap * sizeof(char *));
    }
    tl->tokens[tl->n++] = s;
}
static void tl_free(TokenList *tl)
{
    for (int i = 0; i < tl->n; i++)
        free(tl->tokens[i]);
    free(tl->tokens);
    tl->n = 0;
}
/* ---------- tokenizer ------------------------------------------------------ */
static int tokenize(const char *input, TokenList *tl)
{
    tl_init(tl);
    const char *p = input;
    while (*p) {
        while (*p && isspace((unsigned char)*p))
            p++;
        if (!*p) break;
        if (p[0] == '>' && p[1] == '>') {
            tl_push(tl, strdup(">>"));
            p += 2;
            continue;
        }
        if (*p == '|' || *p == '<' || *p == '>' || *p == '&') {
            char op[2] = { *p, '\0' };
            tl_push(tl, strdup(op));
            p++;
            continue;
        }
        char   chunk[4096];
        int    ci         = 0;
        int    has_quotes = 0;
        while (*p && !isspace((unsigned char)*p)
                   && *p != '|' && *p != '<' && *p != '>'
                   && *p != '&') {
            if (*p == '\'') {           
                has_quotes = 1;
                p++;
                while (*p && *p != '\'')
                    chunk[ci++] = *p++;
                if (*p == '\'') p++;
            } else if (*p == '"') {     
                has_quotes = 1;
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && (*(p+1) == '"' || *(p+1) == '\\'))
                        p++;
                    chunk[ci++] = *p++;
                }
                if (*p == '"') p++;
            } else {
                chunk[ci++] = *p++;
            }
        }
        chunk[ci] = '\0';
        if (!has_quotes && strpbrk(chunk, "*?[")) {
            glob_t gb;
            int    rc = glob(chunk, GLOB_NOCHECK | GLOB_TILDE, NULL, &gb);
            if (rc == 0) {
                for (size_t i = 0; i < gb.gl_pathc; i++)
                    tl_push(tl, strdup(gb.gl_pathv[i]));
                globfree(&gb);
            } else {
                tl_push(tl, strdup(chunk));   
            }
        } else {
            tl_push(tl, strdup(chunk));
        }
    }
    return 0;
}
/* ---------- pipeline builder ---------------------------------------------- */
int parse_pipeline(const char *input, Pipeline *pl)
{
    memset(pl, 0, sizeof(*pl));
    pl->ncmds = 0;
    pl->background = 0;
    TokenList tl;
    if (tokenize(input, &tl) < 0)
        return -1;
    if (tl.n == 0) {
        free(tl.tokens);
        return -1;
    }
    if (strcmp(tl.tokens[tl.n - 1], "&") == 0) {
        pl->background = 1;
        free(tl.tokens[--tl.n]);
        tl.tokens[tl.n] = NULL;
    }
    int cmd_idx = 0;
    Command *cur = &pl->cmds[cmd_idx];
    memset(cur, 0, sizeof(*cur));
    for (int i = 0; i < tl.n; i++) {
        char *tok = tl.tokens[i];
        if (strcmp(tok, "|") == 0) {
            cur->argv[cur->argc] = NULL;
            cmd_idx++;
            if (cmd_idx >= MAX_CMDS) {
                fprintf(stderr, "shishell: too many pipes\n");
                tl_free(&tl);
                return -1;
            }
            cur = &pl->cmds[cmd_idx];
            memset(cur, 0, sizeof(*cur));
            free(tok);
        } else if (strcmp(tok, "<") == 0) {
            i++;
            if (i >= tl.n) {
                fprintf(stderr, "shishell: expected filename after <\n");
                tl_free(&tl);
                return -1;
            }
            cur->redirect_in = tl.tokens[i]; 
        } else if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            cur->append = (tok[1] == '>') ? 1 : 0;
            free(tok);
            i++;
            if (i >= tl.n) {
                fprintf(stderr, "shishell: expected filename after >\n");
                tl_free(&tl);
                return -1;
            }
            cur->redirect_out = tl.tokens[i]; 
        } else {
            if (cur->argc >= MAX_ARGS - 1) {
                fprintf(stderr, "shishell: too many arguments\n");
                tl_free(&tl);
                return -1;
            }
            cur->argv[cur->argc++] = tok; 
        }
    }
    cur->argv[cur->argc] = NULL;
    pl->ncmds = cmd_idx + 1;
    for (int i = 0; i < pl->ncmds; i++) {
        if (pl->cmds[i].argc == 0) {
            fprintf(stderr, "shishell: empty command in pipeline\n");
            free(tl.tokens);
            return -1;
        }
    }
    free(tl.tokens); 
    return 0;
}
void free_pipeline(Pipeline *pl)
{
    for (int i = 0; i < pl->ncmds; i++) {
        Command *c = &pl->cmds[i];
        for (int j = 0; j < c->argc; j++)
            free(c->argv[j]);
        free(c->redirect_in);
        free(c->redirect_out);
    }
    memset(pl, 0, sizeof(*pl));
}
