#include "lispy.h"

char* lval_t_name(lval_t t) {
  switch (t) {
    case LVAL_FUN:
      return "Function";
    case LVAL_NUM:
      return "Number";
    case LVAL_ERR:
      return "Error";
    case LVAL_SYM:
      return "Symbol";
    case LVAL_SEXPR:
      return "S-Expression";
    case LVAL_QEXPR:
      return "Q-Expression";
    default:
      return "Unknown";
  }
}

lval* lval_num(double x) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_NUM;
  v->num = x;
  return v;
}

lval* lval_sym(char* s) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SYM;
  v->sym = malloc(strlen(s) + 1);
  strcpy(v->sym, s);
  return v;
}

lval* lval_sexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_SEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_qexpr(void) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_QEXPR;
  v->count = 0;
  v->cell = NULL;
  return v;
}

lval* lval_fun(lbuiltin fun) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_FUN;
  v->fun = fun;
  return v;
}

lval* lval_err(char* fmt, ...) {
  lval* v = malloc(sizeof(lval));
  v->type = LVAL_ERR;

  va_list va;
  va_start(va, fmt);

  v->err = malloc(512);

  // print error string with max of 511 chars
  vsnprintf(v->err, 511, fmt, va);

  // reallocateto number of bytes actually used
  v->err = realloc(v->err, strlen(v->err) + 1);

  // cleanup our va list
  va_end(va);

  return v;
}

lenv* lenv_new(void) {
  lenv* e = malloc(sizeof(lenv));
  e->count = 0;
  e->syms = NULL;
  e->vals = NULL;
  return e;
}

void lenv_del(lenv* e) {
  for (int i = 0; i < e->count; ++i) {
    free(e->syms[i]);
    lval_del(e->vals[i]);
  }
  free(e->syms);
  free(e->vals);
  free(e);
}

lval* lenv_get(lenv* e, lval* k) {
  for (int i = 0; i < e->count; ++i)
    if (strcmp(e->syms[i], k->sym) == 0)
      return lval_copy(e->vals[i]);

  return lval_err("unbound symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
  for (int i = 0; i < e->count; ++i)
    if (strcmp(e->syms[i], k->sym) == 0) {
      // delete the old value
      lval_del(e->vals[i]);
      // assign a new value
      e->vals[i] = lval_copy(v);
      return;
    }

  e->count++;
  e->vals = realloc(e->vals, sizeof(lval*) * e->count);
  e->syms = realloc(e->syms, sizeof(char*) * e->count);

  // copy contents of the lval and symbol
  e->vals[e->count - 1] = lval_copy(v);
  e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
  strcpy(e->syms[e->count - 1], k->sym);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin fun) {
  lval* k = lval_sym(name);
  lval* v = lval_fun(fun);
  lenv_put(e, k, v);
  lval_del(k);
  lval_del(v);
}

lval* lval_read_num(mpc_ast_t* t) {
  errno = 0;
  double x = strtod(t->contents, NULL);
  return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
  if (strstr(t->tag, "number"))
    return lval_read_num(t);
  if (strstr(t->tag, "symbol"))
    return lval_sym(t->contents);

  lval* e = NULL;
  if (strcmp(t->tag, ">") == 0)
    e = lval_sexpr();
  if (strstr(t->tag, "sexpr"))
    e = lval_sexpr();
  if (strstr(t->tag, "qexpr"))
    e = lval_qexpr();

  for (int i = 0; i < t->children_num; ++i) {
    if (strcmp(t->children[i]->contents, "(") == 0)
      continue;
    if (strcmp(t->children[i]->contents, ")") == 0)
      continue;
    if (strcmp(t->children[i]->contents, "}") == 0)
      continue;
    if (strcmp(t->children[i]->contents, "{") == 0)
      continue;
    if (strcmp(t->children[i]->tag, "regex") == 0)
      continue;
    e = lval_add(e, lval_read(t->children[i]));
  }

  return e;
}

void lval_del(lval* v) {
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
    case LVAL_QEXPR:
      for (int i = 0; i < v->count; ++i)
        lval_del(v->cell[i]);
      free(v->cell);
      break;
    case LVAL_FUN:
      break;
  }

  free(v);
}

lval* lval_add(lval* v, lval* c) {
  v->count += 1;
  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  v->cell[v->count - 1] = c;
  return v;
}

lval* lval_pop(lval* v, int i) {
  lval* x = v->cell[i];

  // shift the memory after the item at "i" over the top
  // note: "i" could be at the center of the list, so,
  // where copying the element next to it??
  memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval*) * (v->count - i - 1));

  v->count -= 1;

  v->cell = realloc(v->cell, sizeof(lval*) * v->count);
  return x;
}

lval* lval_take(lval* v, int i) {
  lval* x = lval_pop(v, i);
  lval_del(v);
  return x;
}

lval* lval_join(lval* x, lval* y) {
  // add each cell from 'y' to 'x'
  while (y->count)
    x = lval_add(x, lval_pop(y, 0));

  lval_del(y);
  return x;
}

lval* lval_copy(lval* v) {
  lval* x = malloc(sizeof(lval));
  x->type = v->type;

  switch (v->type) {
    case LVAL_FUN:
      x->fun = v->fun;
      break;
    case LVAL_NUM:
      x->num = v->num;
      break;

    case LVAL_ERR:
      x->err = malloc(strlen(v->err) + 1);
      strcpy(x->err, v->err);
      break;

    case LVAL_SYM:
      x->sym = malloc(strlen(v->sym) + 1);
      strcpy(x->sym, v->sym);
      break;

    case LVAL_SEXPR:
    case LVAL_QEXPR:
      // recursively copy sub-expressions
      x->count = v->count;
      x->cell = malloc(sizeof(lval*) * x->count);
      for (int i = 0; i < x->count; ++i)
        x->cell[i] = lval_copy(v->cell[i]);
      break;
  }

  return x;
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
  for (int i = 0; i < v->count; ++i)
    v->cell[i] = lval_eval(e, v->cell[i]);

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

  // first element must be a function
  lval* f = lval_pop(v, 0);
  if (f->type != LVAL_FUN) {
    lval_del(v);
    lval_del(f);
    return lval_err("first element is not a function. expecting %s but got %s",
                    lval_t_name(LVAL_FUN), lval_t_name(f->type));
  }

  // call the function
  lval* result = f->fun(e, v);
  lval_del(f);
  return result;
}

lval* lval_eval(lenv* e, lval* v) {
  if (v->type == LVAL_SYM) {
    lval* x = lenv_get(e, v);
    lval_del(v);
    return x;
  }

  if (v->type == LVAL_SEXPR)
    return lval_eval_sexpr(e, v);
  return v;
}

void lenv_add_builtins(lenv* e) {
  // list functions
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "cons", builtin_cons);
  lenv_add_builtin(e, "def", builtin_def);

  // math functions
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

lval* builtin_op(lenv* e, lval* a, char* op) {
  UNUSED(e);
  // ensure arguments are numbers
  for (int i = 0; i < a->count; ++i)
    if (a->cell[i]->type != LVAL_NUM) {
      // lval_del(a);
      return lval_err("cannot operate on a non-number. expecting %s but got %s",
                      lval_t_name(LVAL_NUM), lval_t_name(a->cell[i]->type));
    }

  lval* x = lval_pop(a, 0);

  // if no arguments and a minus symbol, do unary negation
  if ((strcmp(op, "-") == 0) && a->count == 0)
    x->num = -x->num;

  while (a->count > 0) {
    lval* y = lval_pop(a, 0);
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
    }

    lval_del(y);
  }

  lval_del(a);
  return x;
}

lval* builtin_add(lenv* e, lval* a) {
  return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_head(lenv* e, lval* a) {
  UNUSED(e);

  // check error
  LASSERT(a, a->count == 1,
          "Function 'head': too many arguments. expecting %i but got %i", 1,
          a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'head': incorrect type. expecting %s but got %s",
          lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[0]->type));
  LASSERT(a, a->cell[0]->count != 0, "Function 'head': empty q-expression");

  lval* v = lval_take(a, 0);
  // delete all non-head elements
  while (v->count > 1)
    lval_del(lval_pop(v, 1));
  return v;
}

lval* builtin_tail(lenv* e, lval* a) {
  UNUSED(e);

  // check error
  LASSERT(a, a->count == 1,
          "Function 'tail': too many arguments. expecting %i but got %i", 1,
          a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'tail': incorrect type. expecting %s but got %s",
          lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[0]->type));
  LASSERT(a, a->cell[0]->count != 0, "Function 'tail': empty q-expression");

  lval* v = lval_take(a, 0);
  // delete first element
  lval_del(lval_pop(v, 0));
  return v;
}

lval* builtin_list(lenv* e, lval* a) {
  UNUSED(e);

  a->type = LVAL_QEXPR;
  return a;
}

lval* builtin_eval(lenv* e, lval* a) {
  LASSERT(a, a->count == 1,
          "Function 'eval': too many arguments. expecting %i but got %i", 1,
          a->count);
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'eval': incorrect type. expecting %i but got %i",
          lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[0]->type));

  lval* x = lval_take(a, 0);
  x->type = LVAL_SEXPR;
  return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
  UNUSED(e);

  for (int i = 0; i < a->count; ++i)
    LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
            "Function 'join': incorrect type. expecting %s but got %s",
            lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[i]->type));

  lval* x = lval_pop(a, 0);
  while (a->count) {
    x = lval_join(x, lval_pop(a, 0));
  }

  lval_del(a);
  return x;
}

lval* builtin_cons(lenv* e, lval* a) {
  UNUSED(e);

  // first arg must be a number
  // second arg must be a q-expression
  LASSERT(a, a->count == 2,
          "Function 'cons': expected number of arguments is %i but got %i", 2,
          a->count);
  LASSERT(a, a->cell[0]->type == LVAL_NUM,
          "Function 'cons': first argument must be a %s but got %s",
          lval_t_name(LVAL_NUM), lval_t_name(a->cell[0]->type));
  LASSERT(a, a->cell[1]->type == LVAL_QEXPR,
          "Function 'cons': second argument must be a %s but got %s",
          lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[1]->type));

  lval* x = lval_pop(a, 0);
  lval* y = lval_pop(a, 0);
  lval* z = lval_add(lval_qexpr(), x);
  while (y->count > 0) {
    z = lval_add(z, lval_pop(y, 0));
  }

  lval_del(a);
  return z;
}

lval* builtin_def(lenv* e, lval* a) {
  LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
          "Function 'def': first argument is expected to be %s but got %s",
          lval_t_name(LVAL_QEXPR), lval_t_name(a->cell[0]->type));

  // first arg is symbol list
  lval* syms = a->cell[0];

  // should have at least argument
  LASSERT(a, syms->count > 0,
          "Function 'def': expecting at least 1 symbol but got %i", syms->count)

  // elements must all be symbols
  for (int i = 0; i < syms->count; ++i)
    LASSERT(a, syms->cell[i]->type == LVAL_SYM,
            "Function 'def': expecting arguments to be %s, got %s",
            lval_t_name(LVAL_SYM), lval_t_name(syms->cell[i]->type));

  // check correct number of symbols and values
  LASSERT(a, syms->count == a->count - 1,
          "Function 'def': number of symbols and values should match. "
          "expecting %i values but got %i",
          syms->count, a->count - 1)

  for (int i = 0; i < syms->count; ++i)
    lenv_put(e, syms->cell[i], a->cell[i + 1]);

  lval_del(a);
  return lval_sexpr();
}

void lval_expr_print(lval* v, char open, char close) {
  putchar(open);
  for (int i = 0; i < v->count; ++i) {
    lval_print(v->cell[i]);
    if (i != (v->count - 1))
      putchar(' ');
  }

  putchar(close);
}

void lval_print(lval* v) {
  switch (v->type) {
    case LVAL_NUM:
      printf("%.0f", v->num);
      break;
    case LVAL_ERR:
      printf("Error: %s", v->err);
      break;
    case LVAL_SYM:
      printf("%s", v->sym);
      break;
    case LVAL_SEXPR:
      lval_expr_print(v, '(', ')');
      break;
    case LVAL_QEXPR:
      lval_expr_print(v, '{', '}');
      break;
    case LVAL_FUN:
      printf("<function>");
      break;
  }
}

void lval_println(lval* v) {
  lval_print(v);
  putchar('\n');
}

int main(void) {
  // create some parsers
  mpc_parser_t* Number = mpc_new("number");
  mpc_parser_t* Symbol = mpc_new("symbol");
  mpc_parser_t* Sexpr = mpc_new("sexpr");
  mpc_parser_t* Qexpr = mpc_new("qexpr");
  mpc_parser_t* Expr = mpc_new("expr");
  mpc_parser_t* Lispy = mpc_new("lispy");

  const char* language =
      "\
    number  : /-?[0-9]+/ ; \
    symbol  : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ; \
    sexpr   : '(' <expr>* ')' ; \
    qexpr   : '{' <expr>* '}' ; \
    expr    : <number> | <symbol> | <sexpr> | <qexpr> ; \
    lispy   : /^/ <expr>* /$/ ; ";

  // define them with the following language
  mpca_lang(MPCA_LANG_DEFAULT, language, Number, Symbol, Sexpr, Qexpr, Expr,
            Lispy);

  puts("Lispy version 0.1");
  puts("Press Ctrl+C to exit\n");

  // setup environment
  lenv* e = lenv_new();
  lenv_add_builtins(e);

  while (1) {
    char* input = readline("lispy> ");
    add_history(input);

    // attempt to parse the user input
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      lval* result = lval_eval(e, lval_read(r.output));
      lval_println(result);
      lval_del(result);

      mpc_ast_delete(r.output);
    } else {
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }

    free(input);
  }

  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

  return 0;
}
