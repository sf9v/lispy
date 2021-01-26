#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char *unused) {}

#else
#include <editline/readline.h>
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

long eval_op(long x, char *op, long y)
{
    if (strcmp(op, "+") == 0)
        return x + y;
    if (strcmp(op, "-") == 0)
        return x - y;
    if (strcmp(op, "*") == 0)
        return x * y;
    if (strcmp(op, "/") == 0)
        return x / y;
    if (strcmp(op, "^") == 0)
        return pow(x, y);
    if (strcmp(op, "%%") == 0)
        return x % y;
    if (strcmp(op, "min") == 0)
        return MIN(x, y);
    if (strcmp(op, "max") == 0)
        return MAX(x, y);
    return 0;
}

long eval(mpc_ast_t *t)
{
    // a number
    if (strstr(t->tag, "number"))
    {
        return atoi(t->contents);
    }

    // the operator is always the second child
    char *op = t->children[1]->contents;

    // evaluate the third child
    long x = eval(t->children[2]);

    // iterate the remaining children and combining
    int i = 3;
    while (strstr(t->children[i]->tag, "expr"))
    {
        x = eval_op(x, op, eval(t->children[i]));
        ++i;
    }

    return x;
}

int main(int argc, char **argv)
{
    /* Create Some Parsers */
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");

    /* Define them with the following Language */
    mpca_lang(MPCA_LANG_DEFAULT, " \
    number   : /-?[0-9]+/ ; \
    operator : '+' | '-' | '*' | '/' | '^' | \"min\" | \"max\" ; \
    expr     : <number> | '(' <operator> <expr>+ ')' ; \
    lispy    : /^/ <operator> <expr>+ /$/ ; \
    ",
              Number,
              Operator, Expr, Lispy);

    puts("Lisp Version 0.0.1");
    puts("Press Ctrl+C to Exit\n");

    while (1)
    {
        char *input = readline("lispy> ");
        add_history(input);

        /* Attempt to Parse the user Input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r))
        {
            /* On Success Print the AST */
            mpc_ast_print(r.output);
            printf("%li\n", eval(r.output));
            mpc_ast_delete(r.output);
        }
        else
        {
            /* Otherwise Print the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}