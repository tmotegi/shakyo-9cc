#include "9cc.h"

LVar *locals;

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
LVar *find_lvar(Token *tok) {
  for (LVar *var = locals; var; var = var->next)
    if (var->len == tok->len && memcmp(var->name, tok->str, var->len) == 0)
      return var;
  return NULL;
}

static Node *stmt(void);
static Node *expr(void);
static Node *assign(void);
static Node *equality(void);
static Node *relational(void);
static Node *add(void);
static Node *mul(void);
static Node *unary(void);
static Node *primary(void);

// program    = stmt*
Function *program(void) {
  locals = NULL;

  Node head = {};
  Node *cur = &head;
  while (!at_eof()) {
    cur->next = stmt();
    cur = cur->next;
  }

  Function *prog = calloc(1, sizeof(Function));
  prog->node = head.next;
  prog->locals = locals;
  return prog;
}

// stmt    = expr ";"
//         | "if" "(" expr ")" stmt ("else" stmt)?
//         | "while" "(" expr ")" stmt
//         | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//         | "return" expr ";"
static Node *stmt(void) {
  Node *node;
  if (consume("if")) {
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
  } else if (consume("return")) {
    node = calloc(1, sizeof(Node));
    node->kind = ND_RETURN;
    node->lhs = expr();
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

// add        = mul ("+" mul | "-" mul)*
static Node *add(void) {
  Node *node = mul();

  for (;;) {
    if (consume("+"))
      node = new_node(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_node(ND_SUB, node, mul());
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

// - -10 をパースするケースを考えると expr -> mul -> unary -> (- | unary) ->
// (- | - | primary) = - - 10

// unary   = ("+" | "-")? unary | primary
static Node *unary(void) {
  if (consume("+"))
    return unary();
  else if (consume("-"))
    return new_node(ND_SUB, new_node_num(0), unary());
  else
    return primary();
}

// primary = num | ident | "(" expr ")"
static Node *primary(void) {
  Token *tok = consume_ident();
  if (tok) {
    LVar *lvar = find_lvar(tok);
    if (!lvar) {
      // 変数のリストの先頭に新しい変数を追加する
      lvar = calloc(1, sizeof(LVar));
      lvar->next = locals;
      lvar->name = tok->str;
      lvar->len = tok->len;
      locals = lvar;
    }
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->var = lvar;
    return node;
  } else if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }
  return new_node_num(expect_number());
}
