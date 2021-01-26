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

lval *lval_num(double x) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval *lval_err(char *m) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_ERR;
  v->err = malloc(strlen(m) + 1);
  strcpy(v->err, m);
  return v;
}

lval *lval_sym(char *s) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval *lval_sexpr(void) {
  lval *v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

void lval_del(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    break;
  case LVAL_ERR:
    free(v->err);
    break;
  case LVAL_SYM:
    free(v->sym);
    break;
  case LVAL_SEXPR:
    for (int i = 0; i < v->count; i++)
      lval_del(v->cell[i]);
    free(v->cell);
    break;
  }

  free(v);
}

lval *lval_read_num(mpc_ast_t *t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_add(lval *v, lval *c) {
  v->count += 1;
  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  v->cell[v->count - 1] = c;
  return v;
}

lval *lval_pop(lval *v, int i) {
  lval *x = v->cell[i];

  // shift the memory after the item at "i" over the top
  // note: "i" could be at the center of the list, so,
  // where copying the element next to it?
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));

  v->count -= 1;

  v->cell = realloc(v->cell, sizeof(lval *) * v->count);
  return x;
}

lval *lval_take(lval *v, int i) {
  lval *e = lval_pop(v, i);
  lval_del(v);
  return e;
}

lval *lval_read(mpc_ast_t *t) {
  if (strstr(t->tag, "number"))
    return lval_read_num(t);
  if (strstr(t->tag, "symbol"))
    return lval_sym(t->contents);

  lval *e = NULL;
  if (strcmp(t->tag, ">") == 0)
    e = lval_sexpr();
  if (strstr(t->tag, "sexpr"))
    e = lval_sexpr();

  for (int i = 0; i < t->children_num; ++i) {
    if (strcmp(t->children[i]->contents, "(") == 0)
      continue;
    if (strcmp(t->children[i]->contents, ")") == 0)
      continue;
    if (strcmp(t->children[i]->tag, "regex") == 0)
      continue;
    e = lval_add(e, lval_read(t->children[i]));
  }

  return e;
}

lval *lval_eval(lval *v);
lval *builtin_op(lval *a, char *op);
void lval_println(lval *v);

void lval_debug(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("LVAL_NUM: %.2f\n", v->num);
    break;
  case LVAL_ERR:
    printf("LVAL_ERR: %s\n", v->err);
    break;
  case LVAL_SYM:
    printf("LVAL_SYM: %s\n", v->sym);
    break;
  case LVAL_SEXPR:
    printf("LVAL_EXPR: 0\n");
    break;
  }
}

lval *lval_eval_sexpr(lval *v) {
  for (int i = 0; i < v->count; ++i)
    v->cell[i] = lval_eval(v->cell[i]);

  // check for errors
  for (int i = 0; i < v->count; ++i)
    if (v->cell[i]->type == LVAL_ERR)
      return lval_take(v, i);

  // empty experession
  if (v->count == 0)
    return v;

  // single expression
  if (v->count == 1)
    return lval_take(v, 0);

  // first element must be a symbol
  lval *e = lval_pop(v, 0);
  if (e->type != LVAL_SYM) {
    lval_del(e);
    lval_del(v);
    return lval_err("s-expression does not start with a symbol");
  }

  // evaluate
  lval *result = builtin_op(v, e->sym);
  lval_del(e);
  return result;
}

lval *lval_eval(lval *v) {
  // uncomment to enable debug
  // lval_debug(v);
  if (v->type == LVAL_SEXPR)
    return lval_eval_sexpr(v);
  return v;
}

lval *builtin_op(lval *a, char *op) {
  // ensure arguments are numbers
  for (int i = 0; i < a->count; ++i)
    if (a->cell[i]->type != LVAL_NUM) {
      lval_println(a->cell[i]);
      lval_del(a);
      return lval_err("cannot operate on a non-number");
    }

  lval *x = lval_pop(a, 0);

  // if no arguments and a minus symbol, do unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0)
    x->num = -x->num;

  while (a->count > 0) {
    lval *y = lval_pop(a, 0);
    if (strcmp(op, "+") == 0)
      x->num += y->num;
    else if (strcmp(op, "-") == 0)
      x->num -= y->num;
    else if (strcmp(op, "*") == 0)
      x->num *= y->num;
    else if (strcmp(op, "/") == 0) {
      if (y->num == 0) {
        lval_del(x);
        lval_del(y);
        x = lval_err("division by zero");
        break;
      }
      x->num /= y->num;
    } else if (strcmp(op, "%") == 0)
      x->num = fmod(x->num, y->num);
    else if (strcmp(op, "^") == 0)
      x->num = pow(x->num, y->num);
    else if (strcmp(op, "min") == 0)
      x->num = MIN(x->num, y->num);
    else if (strcmp(op, "max") == 0)
      x->num = MAX(x->num, y->num);

    lval_del(y);
  }

  lval_del(a);
  return x;
}

// forward declare lval_print
void lval_print(lval *v);

void lval_expr_print(lval *v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1))
      putchar(' ');
  }

  putchar(close);
}

void lval_print(lval *v) {
  switch (v->type) {
  case LVAL_NUM:
    printf("%.2f", v->num);
    break;
  case LVAL_ERR:
    printf("error: %s", v->err);
    break;
  case LVAL_SYM:
    printf("%s", v->sym);
    break;
  case LVAL_SEXPR:
    lval_expr_print(v, '(', ')');
    break;
  }
}

void lval_println(lval *v) {
  lval_print(v);
  putchar('\n');
}

int main(int argc, char **argv) {
  /* Create Some Parsers */
  mpc_parser_t *Number = mpc_new("number");
  mpc_parser_t *Symbol = mpc_new("symbol");
  mpc_parser_t *Sexpr = mpc_new("sexpr");
  mpc_parser_t *Expr = mpc_new("expr");
  mpc_parser_t *Lispy = mpc_new("lispy");

  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT, "\
    number  : /-?[0-9.]+/ ; \
    symbol  : '+' | '-' | '*' | '/' | '^' | \"min\" | \"max\" | '%' ; \
    sexpr   : '(' <expr>* ')' ; \
    expr    : <number> | <symbol> | <sexpr> ; \
    lispy   : /^/ <expr>* /$/ ; ",
            Number, Symbol, Sexpr, Expr, Lispy);

  puts("Lispy version 0.0.1");
  puts("Press ctrl+c to exit\n");

  while (1) {
    char *input = readline("lispy> ");
    add_history(input);

    /* Attempt to Parse the user Input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval *result = lval_eval(lval_read(r.output));
      lval_println(result);
      lval_del(result);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

  return 0;
}
