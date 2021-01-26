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

enum lval_t
{
    LVAL_NUM,
    LVAL_ERR
};

enum err_t
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

typedef struct lval
{
    int type;
    double num;
    int err;
} lval;

lval lval_num(double x)
{
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}

lval lval_err(int x)
{
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}

void lval_print(lval v)
{
    switch (v.type)
    {
    case LVAL_NUM:
        printf("%f", v.num);
        break;
    case LVAL_ERR:
        if (v.err == LERR_DIV_ZERO)
            printf("Error: Division By Zero!");
        if (v.err == LERR_BAD_OP)
            printf("Error: Invalid Operator!");
        if (v.err == LERR_BAD_NUM)
            printf("Error: Invalid Number!");
        break;
    }
}

void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char *op, lval y)
{

    if (x.type == LVAL_ERR)
        return x;
    if (y.type == LVAL_ERR)
        return y;

    if (strcmp(op, "+") == 0)
        return lval_num(x.num + y.num);
    if (strcmp(op, "-") == 0)
        return lval_num(x.num - y.num);
    if (strcmp(op, "*") == 0)
        return lval_num(x.num * y.num);
    if (strcmp(op, "/") == 0)
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    if (strcmp(op, "^") == 0)
        return lval_num(pow(x.num, y.num));
    if (strcmp(op, "min") == 0)
        return lval_num(MIN(x.num, y.num));
    if (strcmp(op, "max") == 0)
        return lval_num(MAX(x.num, y.num));
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t)
{
    // a number
    if (strstr(t->tag, "number"))
    {
        errno = 0;
        double x = strtod(t->contents, NULL);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // the operator is always the second child
    char *op = t->children[1]->contents;

    // evaluate the third child
    lval x = eval(t->children[2]);

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
    number   : /-?[0-9.]+/ ; \
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
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        }
        else
        {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
