#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

/* Fake readline function */
char *readline(char *prompt) {
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

enum lval_t { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

typedef struct lval {
  enum lval_t type;
  double num;
  char *err;
  char *sym;
  int count;
  struct lval **cell;
} lval;

// lval functions
lval *lval_num(double x);
lval *lval_err(char *m);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
void lval_del(lval *v);
lval *lval_read_num(mpc_ast_t *t);
lval *lval_add(lval *v, lval *c);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *lval_read(mpc_ast_t *t);
lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);
lval *builtin_op(lval *a, char *op);

// output functions
void lval_debug(lval *v);
void lval_expr_print(lval *v, char open, char close);
void lval_print(lval *v);
void lval_print(lval *v);
void lval_println(lval *v);
