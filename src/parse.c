#include "9cc.h"

static VarList *locals;
static VarList *globals;
static VarList *scope;

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static Var *find_var(Token *tok) {
  for (VarList *vl = scope; vl; vl = vl->next) {
    Var *var = vl->var;
    if (strlen(var->name) == tok->len &&
        memcmp(var->name, tok->str, var->len) == 0)
      return var;
  }
  return NULL;
}

static Node *new_node(NodeKind kind, Token *tok) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->tok = tok;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok) {
  Node *node = new_node(kind, tok);
  node->lhs = expr;
  return node;
}

Node *new_num(int val, Token *tok) {
  Node *node = new_node(ND_NUM, tok);
  node->val = val;
  return node;
}

static Node *new_var_node(Var *var, Token *tok) {
  Node *node = new_node(ND_VAR, tok);
  node->var = var;
  return node;
}

static Var *new_var(char *name, Type *ty, bool is_local) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->len = strlen(name);
  var->ty = ty;
  var->is_local = is_local;

  // ローカル変数とグローバル変数が scope に追加される
  VarList *sc = calloc(1, sizeof(VarList));
  sc->var = var;
  sc->next = scope;
  scope = sc;
  return var;
}

static Var *new_lvar(char *name, Type *ty) {
  Var *var = new_var(name, ty, true);

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}

static Var *new_gvar(char *name, Type *ty) {
  Var *var = new_var(name, ty, false);

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = globals;
  globals = vl;
  return var;
}

static char *new_label(void) {
  static int cnt = 0;
  char buf[20];
  sprintf(buf, ".L.data.%d", cnt++);
  return strndup(buf, 20);
}

static Type *basetype(void);
static void *global_var(void);
static Function *function(void);
static Node *stmt(void);
static Node *stmt2(void);
static Node *expr(void);
static Node *assign(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *suffix(void);
static Node *primary(void);

static bool is_function(void) {
  Token *tok = token;
  basetype();
  bool ret = consume_ident() && consume("(");
  token = tok;  // consume したトークンを戻す
  return ret;
}

// program    = (global_var | function)*
Program *program(void) {
  Function head = {};
  Function *cur = &head;
  globals = NULL;

  while (!at_eof()) {
    if (is_function()) {
      cur->next = function();
      cur = cur->next;
    } else {
      global_var();
    }
  }

  Program *prog = calloc(1, sizeof(Program));
  prog->fns = head.next;
  prog->globals = globals;
  return prog;
}

// basetype = ("char" | "int") "*"*
static Type *basetype(void) {
  Type *ty;
  if (consume("char")) {
    ty = char_type;
  } else if (consume("int")) {
    ty = int_type;
  }

  while (consume("*")) {
    ty = pointer_to(ty);  // ポインタの場合は ptr_to にアドレスが入る
  }
  return ty;
}

static Type *read_type_suffix(Type *base) {
  // 配列定義を処理
  if (!consume("[")) return base;
  int sz = expect_number();
  expect("]");
  base = read_type_suffix(base);  // 再帰呼び出し (ex: array[N][M])
  return array_of(base, sz);
}

static VarList *read_func_arg(void) {
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = new_lvar(name, ty);
  return vl;
}

static VarList *read_func_args(void) {
  if (consume(")")) return NULL;  // 引数なし

  // 引数のリスト作成
  VarList *head = read_func_arg();
  VarList *cur = head;

  while (consume(",")) {
    // locals に引数を追加する
    cur->next = read_func_arg();
    cur = cur->next;
  }

  expect(")");
  return head;
}

static void *global_var(void) {
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  expect(";");
  new_gvar(name, ty);
}

// function = basetype ident "(" read-func-args? ")" "{" stmt* "}"
static Function *function(void) {
  locals = NULL;

  basetype();
  Function *fn = calloc(1, sizeof(Function));
  fn->name = expect_ident();
  expect("(");

  VarList *sc = scope;
  fn->args = read_func_args();  // 引数のリスト
  expect("{");

  Node head = {};
  Node *cur = &head;
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  scope = sc;  // 関数を抜けたら scope の参照先を関数呼び出し前に戻す

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// declartion = basetype ident (read_type_suffix)* ("=" expr) ";"
static Node *declaration(void) {
  Token *tok = token;
  Type *ty = basetype();
  char *name = expect_ident();
  ty = read_type_suffix(ty);
  Var *var = new_lvar(name, ty);

  if (tok = consume(";"))
    return new_node(ND_NULL, tok);  // 初期化しない場合は ND_NULL

  // lhs = rhs ;
  expect("=");
  Node *lhs = new_node(ND_VAR, tok);
  lhs->var = var;
  Node *rhs = expr();
  Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
  expect(";");
  return new_unary(ND_EXPR_STMT, node, tok);
}

static Node *read_expr_stmt(void) {
  Token *tok = token;
  return new_unary(ND_EXPR_STMT, expr(), tok);
}

static Node *stmt(void) {
  Node *node = stmt2();
  add_type(node);
  return node;
}

static bool is_typename(void) { return peek("char") || peek("int"); }

// stmt    = expr ";"
//         | "return" expr ";"
//         | "if" "(" expr ")" stmt ("else" stmt)?
//         | "while" "(" expr ")" stmt
//         | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//         | "{" stmt* "}"
//         | declaration
static Node *stmt2(void) {
  Node *node;
  Token *tok;

  if (tok = consume("return")) {
    node = new_unary(ND_RETURN, expr(), tok);
    expect(";");
    return node;
  } else if (tok = consume("if")) {
    node = new_node(ND_IF, tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  } else if (tok = consume("while")) {
    node = new_node(ND_WHILE, tok);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  } else if (tok = consume("for")) {
    node = new_node(ND_FOR, tok);
    expect("(");
    if (!consume(";")) {
      node->init = read_expr_stmt();
      expect(";");
    }
    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if (!consume(")")) {
      node->step = read_expr_stmt();
      expect(")");
    }
    node->then = stmt();
    return node;
  } else if (tok = consume("{")) {
    Node head = {};
    Node *cur = &head;

    VarList *sc = scope;
    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }
    scope = sc;  // block 抜けたら scope の参照先を戻す

    node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    return node;
  } else if (is_typename()) {
    node = declaration();
    return node;
  }
  tok = token;
  node = read_expr_stmt();
  expect(";");
  return node;
}

// expr       = assign
static Node *expr(void) { return assign(); }

// assign     = equality ("=" assign)?
static Node *assign(void) {
  Node *node = equality();
  Token *tok;

  if (tok = consume("=")) node = new_binary(ND_ASSIGN, node, assign(), tok);
  return node;
}

// equality   = relational ("==" relational | "!=" relational)*
static Node *equality(void) {
  Node *node = relational();
  Token *tok;

  for (;;) {
    if (tok = consume("=="))
      node = new_binary(ND_EQ, node, relational(), tok);
    else if (tok = consume("!="))
      node = new_binary(ND_NE, node, relational(), tok);
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(void) {
  Node *node = add();
  Token *tok;

  for (;;) {
    if (tok = consume("<"))
      node = new_binary(ND_LT, node, add(), tok);
    else if (tok = consume("<="))
      node = new_binary(ND_LE, node, add(), tok);
    else if (tok = consume(">"))
      node = new_binary(ND_LT, add(), node, tok);
    else if (tok = consume(">="))
      node = new_binary(ND_LE, add(), node, tok);
    else
      return node;
  }
}

static Node *new_add(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_ADD, lhs, rhs, tok);  // ptr + num
  else if (lhs->ty->ptr_to && is_integer(rhs->ty))
    return new_binary(ND_PTR_ADD, lhs, rhs, tok);  // ptr + num
  else if (is_integer(lhs->ty) && rhs->ty->ptr_to)
    return new_binary(ND_PTR_ADD, rhs, lhs, tok);  // ptr + num
  error_tok(tok, "定義されてない演算子です");
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok) {
  add_type(lhs);
  add_type(rhs);

  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_binary(ND_SUB, lhs, rhs, tok);  // num - num
  else if (lhs->ty->ptr_to && is_integer(rhs->ty))
    return new_binary(ND_PTR_SUB, lhs, rhs, tok);  // ptr - num
  else if (lhs->ty->ptr_to && rhs->ty->ptr_to)
    return new_binary(ND_PTR_DIFF, lhs, rhs, tok);  // ptr - ptr
  error_tok(tok, "定義されてない演算子です");
}

// add        = mul ("+" mul | "-" mul)*
static Node *add(void) {
  Node *node = mul();
  Token *tok;
  for (;;) {
    if (tok = consume("+"))
      node = new_add(node, mul(), tok);
    else if (tok = consume("-"))
      node = new_sub(node, mul(), tok);
    else
      return node;
  }
}

// mul     = unary ("*" unary | "/" unary)*
static Node *mul(void) {
  Node *node = unary();
  Token *tok;

  for (;;) {
    if (tok = consume("*"))
      node = new_binary(ND_MUL, node, unary(), tok);
    else if (tok = consume("/"))
      node = new_binary(ND_DIV, node, unary(), tok);
    else
      return node;
  }
}

// unary   = ("+" | "-" | "&" | "*")? unary
//         | "sizeof" unary
//         | suffix
static Node *unary(void) {
  Token *tok;
  if (tok = consume("+"))
    return unary();
  else if (tok = consume("-"))
    return new_binary(ND_SUB, new_num(0, tok), unary(), tok);
  else if (tok = consume("&"))
    return new_unary(ND_ADDR, unary(), tok);
  else if (tok = consume("*"))
    return new_unary(ND_DEREF, unary(), tok);
  else if (tok = consume("sizeof")) {
    Node *node = unary();
    add_type(node);
    return new_num(node->ty->size, tok);
  }
  return suffix();
}

// suffix = primary ("[" expr "]")*
static Node *suffix(void) {
  Node *node = primary();
  Token *tok;
  while (tok = consume("[")) {
    // x[y] = *(x + y)
    Node *exp = new_add(node, expr(), tok);
    expect("]");
    node = new_unary(ND_DEREF, exp, tok);
  }
  return node;
}

// stmt-expr = "(" "{" stmt stmt* "}" ")"
//
// Statement expression is a GNU C extension.
static Node *stmt_expr(Token *tok) {
  VarList *sc = scope;

  Node *node = new_node(ND_STMT_EXPR, tok);
  node->body = stmt();
  Node *cur = node->body;

  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }
  expect(")");

  scope = sc;  // ({}) を抜けたら scope の参照先を戻す

  if (cur->kind != ND_EXPR_STMT)
    error_tok(cur->tok, "stmt expr returning void is not supported");
  memcpy(cur, cur->lhs, sizeof(Node));
  return node;
}

// func-args = "(" (expr ("," expr)*)? ")"
static Node *func_args(void) {
  if (consume(")")) return NULL;  // 引数なし
  // 引数を引数のリスト作成
  Node *head = expr();
  Node *cur = head;
  while (consume(",")) {
    cur->next = assign();
    cur = cur->next;
  }
  expect(")");
  return head;
}

// primary = "(" "{" stmt-expr-tail
//         | "(" expr ")"
//         | ident func-args?
//         | str
//         | num
static Node *primary(void) {
  Node *node;
  Token *tok;
  if (tok = consume("(")) {
    if (consume("{")) return stmt_expr(tok);
    node = expr();
    expect(")");
    return node;
  } else if (tok = consume_ident()) {
    if (consume("(")) {
      // 関数の場合
      node = new_node(ND_FUNCALL, tok);
      node->funcname = strndup(tok->str, tok->len);
      node->args = func_args();
      return node;
    }
    // 変数の場合
    Var *var = find_var(tok);
    if (!var) error_tok(tok, "定義されてない識別子です");
    node = new_var_node(var, tok);
    return node;
  } else if (consume("(")) {
    node = expr();
    expect(")");
    return node;
  }

  tok = token;
  if (tok->kind == TK_STR) {
    token = token->next;
    Type *ty = array_of(char_type, tok->cont_len);
    Var *var = new_gvar(new_label(), ty);
    var->contents = tok->contents;
    var->cont_len = tok->cont_len;
    return new_var_node(var, tok);
  }

  if (tok->kind != TK_NUM) error_tok(tok, "expected expression");
  return new_num(expect_number(), tok);
}
