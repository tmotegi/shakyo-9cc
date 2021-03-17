#include "9cc.h"

static VarList *locals;

Node *new_node(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = ND_NUM;
  node->val = val;
  return node;
}

// 変数を名前で検索する。見つからなかった場合はNULLを返す。
static Var *find_var(Token *tok) {
  for (VarList *vl = locals; vl; vl = vl->next) {
    Var *var = vl->var;
    if (strlen(var->name) == tok->len &&
        memcmp(var->name, tok->str, var->len) == 0)
      return var;
  }
  return NULL;
}

static Var *new_lvar(char *name, Type *ty) {
  Var *var = calloc(1, sizeof(Var));
  var->name = name;
  var->len = strlen(name);
  var->ty = ty;

  VarList *vl = calloc(1, sizeof(VarList));
  vl->var = var;
  vl->next = locals;
  locals = vl;
  return var;
}

static Function *function(void);
static Node *stmt(void);
static Node *expr(void);
static Node *assign(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *primary(void);

// program    = function*
Function *program(void) {
  Function head = {};
  Function *cur = &head;

  while (!at_eof()) {
    cur->next = function();
    cur = cur->next;
  }

  return head.next;
}

static Type *basetype(void) {
  expect("int");
  Type *ty = int_type;
  while (consume("*")) {
    ty = pointer_to(ty);  // ポインタの場合は ptr_to にアドレスが入る
  }
  return ty;
}

static VarList *read_func_args(void) {
  if (consume(")")) return NULL;  // 引数なし
  // 引数を引数のリスト作成
  VarList *head = calloc(1, sizeof(VarList));
  head->var = new_lvar(expect_ident(), basetype());
  VarList *cur = head;

  while (consume(",")) {
    // locals に引数を追加する
    cur->next = calloc(1, sizeof(VarList));
    cur->next->var = new_lvar(expect_ident(), basetype());
    cur = cur->next;
  }
  expect(")");
  return head;
}

// function = basetype ident read-func-args "{" stmt* "}"
static Function *function(void) {
  locals = NULL;

  basetype();
  Function *fn = calloc(1, sizeof(Function));
  fn->name = expect_ident();
  expect("(");
  fn->args = read_func_args();  // 引数のリスト
  expect("{");

  Node head = {};
  Node *cur = &head;
  while (!consume("}")) {
    cur->next = stmt();
    cur = cur->next;
  }

  fn->node = head.next;
  fn->locals = locals;
  return fn;
}

// declartion = basetype ident ("=" expr) ";"
static Node *declaration(void) {
  Node *node;
  Type *ty = basetype();
  Var *var = new_lvar(expect_ident(), ty);

  if (consume(";")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_VAR;
    node->var = var;
    return node;
  }

  expect("=");
  // 左
  Node *lhs = calloc(1, sizeof(Node));
  lhs->kind = ND_VAR;
  lhs->var = var;
  // 右
  Node *rhs = expr();
  node = new_node(ND_ASSIGN, lhs, rhs);
  expect(";");
  return node;
}

// stmt    = expr ";"
//         | "return" expr ";"
//         | "if" "(" expr ")" stmt ("else" stmt)?
//         | "while" "(" expr ")" stmt
//         | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//         | "{" stmt* "}"
//         | declaration
static Node *stmt(void) {
  Node *node;
  if (consume("return")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
  } else if (consume("if")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_IF;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume("else")) {
      node->els = stmt();
    }
    return node;
  } else if (consume("while")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_WHILE;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
  } else if (consume("for")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_FOR;
    expect("(");
    if (!consume(";")) {
      node->init = expr();
      expect(";");
    }
    if (!consume(";")) {
      node->cond = expr();
      expect(";");
    }
    if (!consume(")")) {
      node->step = expr();
      expect(")");
    }
    node->then = stmt();
    return node;
  } else if (consume("{")) {
    Node head = {};
    Node *cur = &head;

    while (!consume("}")) {
      cur->next = stmt();
      cur = cur->next;
    }

    node = calloc(1, sizeof(Node));
    node->kind = ND_BLOCK;
    node->body = head.next;
    return node;
  } else if (peek("int")) {
    node = declaration();
    return node;
  } else {
    node = calloc(1, sizeof(Node));
    node->kind = ND_EXPR_STMT;
    node->lhs = expr();
  }
  expect(";");
  return node;
}

// expr       = assign
static Node *expr(void) { return assign(); }

// assign     = equality ("=" assign)?
static Node *assign(void) {
  Node *node = equality();
  if (consume("=")) node = new_node(ND_ASSIGN, node, assign());
  return node;
}

// equality   = relational ("==" relational | "!=" relational)*
static Node *equality(void) {
  Node *node = relational();

  for (;;) {
    if (consume("=="))
      node = new_node(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_node(ND_NE, node, relational());
    else
      return node;
  }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
static Node *relational(void) {
  Node *node = add();

  for (;;) {
    if (consume("<"))
      node = new_node(ND_LT, node, add());
    else if (consume("<="))
      node = new_node(ND_LE, node, add());
    else if (consume(">"))
      node = new_node(ND_LT, add(), node);
    else if (consume(">="))
      node = new_node(ND_LE, add(), node);
    else
      return node;
  }
}

static Node *new_add(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_node(ND_ADD, lhs, rhs);  // ptr + num
  else if (lhs->ty->ptr_to && is_integer(rhs->ty))
    return new_node(ND_PTR_ADD, lhs, rhs);  // ptr + num
  else if (is_integer(lhs->ty) && rhs->ty->ptr_to)
    return new_node(ND_PTR_ADD, rhs, lhs);  // ptr + num
  error_at(token->str, "定義されてない演算子です");
}

static Node *new_sub(Node *lhs, Node *rhs) {
  add_type(lhs);
  add_type(rhs);
  if (is_integer(lhs->ty) && is_integer(rhs->ty))
    return new_node(ND_SUB, lhs, rhs);  // num - num
  else if (lhs->ty->ptr_to && is_integer(rhs->ty))
    return new_node(ND_PTR_SUB, lhs, rhs);  // ptr - num
  else if (lhs->ty->ptr_to && rhs->ty->ptr_to)
    return new_node(ND_PTR_DIFF, lhs, rhs);  // ptr - ptr
  error_at(token->str, "定義されてない演算子です");
}

// add        = mul ("+" mul | "-" mul)*
static Node *add(void) {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_add(node, mul());
    else if (consume("-"))
      node = new_sub(node, mul());
    else
      return node;
  }
}

// mul     = unary ("*" unary | "/" unary)*
static Node *mul(void) {
  Node *node = unary();

  for (;;) {
    if (consume("*"))
      node = new_node(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_node(ND_DIV, node, unary());
    else
      return node;
  }
}

// unary   = "+"? unary
//         | "-"? unary
//         | "&" unary
//         | "*" unary
static Node *unary(void) {
  if (consume("+"))
    return unary();
  else if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), unary());
  else if (consume("&"))
    return new_node(ND_ADDR, unary(), NULL);
  else if (consume("*"))
    return new_node(ND_DEREF, unary(), NULL);
  else
    return primary();
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

// primary = num
//         | ident func-args?
//         | "(" expr ")"
static Node *primary(void) {
  Node *node;
  Token *tok = consume_ident();
  if (tok) {
    if (consume("(")) {
      // 関数の場合
      node = calloc(1, sizeof(Node));
      node->kind = ND_FUNCALL;
      node->funcname = calloc(tok->len + 1, sizeof(char));
      memcpy(node->funcname, tok->str, tok->len);
      node->funcname[tok->len] = '\0';
      node->args = func_args();
      return node;
    } else {
      // 変数の場合
      Var *var = find_var(tok);
      if (!var) error_at(token->str, "定義されてない識別子です");
      node = calloc(1, sizeof(Node));
      node->kind = ND_VAR;
      node->var = var;
      return node;
    }
  } else if (consume("(")) {
    node = expr();
    expect(")");
    return node;
  } else {
    node = new_node_num(expect_number());
  }
  return node;
}
