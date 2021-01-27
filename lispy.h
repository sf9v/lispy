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

#define LASSERT(args, cond, err)                                               \
  if (!(cond)) {                                                               \
    lval_del(args);                                                            \
    return lval_err(err);                                                      \
  }

enum lval_t { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR };

typedef struct lval {
  enum lval_t type;
  double num;
  char *err;
  char *sym;
  int count;
  struct lval **cell;
} lval;

// constructors
lval *lval_num(double x);
lval *lval_err(char *m);
lval *lval_sym(char *s);
lval *lval_sexpr(void);
lval *lval_qexpr(void);

// ast to lval
lval *lval_read_num(mpc_ast_t *t);
lval *lval_read(mpc_ast_t *t);

// lval manipulations
void lval_del(lval *v);
lval *lval_add(lval *v, lval *c);
lval *lval_pop(lval *v, int i);
lval *lval_take(lval *v, int i);
lval *lval_join(lval *x, lval *y);

// evaluators
lval *lval_eval_sexpr(lval *v);
lval *lval_eval(lval *v);

// operators
lval *builtin(lval *a, char *func);
lval *builtin_op(lval *a, char *op);
lval *builtin_head(lval *a);
lval *builtin_tail(lval *a);
lval *builtin_list(lval *a);
lval *builtin_eval(lval *a);
lval *builtin_join(lval *a);
lval *builtin_cons(lval *a);

// outputs
void lval_expr_print(lval *v, char open, char close);
void lval_print(lval *v);
void lval_print(lval *v);
void lval_println(lval *v);
void lval_debug(lval *v);
