#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

/* Fake readline function */
char* readline(char* prompt) {
  fputs(prompt, stdout);
  fgets(buffer, 2048, stdin);
  char* cpy = malloc(strlen(buffer) + 1);
  strcpy(cpy, buffer);
  cpy[strlen(cpy) - 1] = '\0';
  return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

#else
#include <editline/readline.h>
#endif

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define UNUSED(x) (void)(x)

#define LASSERT(args, cond, fmt, ...)         \
  if (!(cond)) {                              \
    lval* err = lval_err(fmt, ##__VA_ARGS__); \
    lval_del(args);                           \
    return err;                               \
  }

struct lval;
typedef struct lval lval;

struct lenv;
typedef struct lenv lenv;

typedef enum lval_t {
  LVAL_ERR,
  LVAL_NUM,
  LVAL_SYM,
  LVAL_FUN,
  LVAL_SEXPR,
  LVAL_QEXPR
} lval_t;

char* lval_t_name(lval_t t);

typedef lval* (*lbuiltin)(lenv*, lval*);

typedef struct lenv {
  int count;
  char** syms;
  lval** vals;
} lenv;

typedef struct lval {
  lval_t type;

  double num;
  char* err;
  char* sym;
  lbuiltin fun;

  int count;
  lval** cell;
} lval;

// lval constructors
lval* lval_num(double x);
lval* lval_sym(char* s);
lval* lval_sexpr(void);
lval* lval_qexpr(void);
lval* lval_fun(lbuiltin fun);
lval* lval_err(char* fmt, ...);

// lval destructor
void lval_del(lval* v);

// lval manipulations
lval* lval_add(lval* v, lval* c);
lval* lval_pop(lval* v, int i);
lval* lval_take(lval* v, int i);
lval* lval_join(lval* x, lval* y);
lval* lval_copy(lval* v);

// lenv constructor
lenv* lenv_new(void);
// lenv destructor
void lenv_del(lenv* e);

// lenv manupilations
lval* lenv_get(lenv* e, lval* k);
void lenv_put(lenv* e, lval* k, lval* v);

// ast to lval
lval* lval_read_num(mpc_ast_t* t);
lval* lval_read(mpc_ast_t* t);

// evaluators
lval* lval_eval_sexpr(lenv* e, lval* v);
lval* lval_eval(lenv* e, lval* v);

// builtin functions
void lenv_add_builtin(lenv* e, char* name, lbuiltin fun);
lval* builtin_op(lenv* e, lval* a, char* op);

lval* builtin_add(lenv* e, lval* a);
lval* builtin_sub(lenv* e, lval* a);
lval* builtin_mul(lenv* e, lval* a);
lval* builtin_div(lenv* e, lval* a);

lval* builtin_head(lenv* e, lval* a);
lval* builtin_tail(lenv* e, lval* a);
lval* builtin_list(lenv* e, lval* a);
lval* builtin_eval(lenv* e, lval* a);
lval* builtin_join(lenv* e, lval* a);
lval* builtin_cons(lenv* e, lval* a);

lval* builtin_def(lenv* e, lval* a);

// outputs
void lval_expr_print(lval* v, char open, char close);
void lval_print(lval* v);
void lval_print(lval* v);
void lval_println(lval* v);
