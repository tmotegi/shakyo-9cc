#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Tokenizer
//

// トークンの種類
typedef enum {
  TK_RESERVED,  // 記号
  TK_IDENT,     // 識別子
  TK_RETURN,    // Return
  TK_NUM,       // 整数トークン
  TK_EOF,       // 入力の終わりを表すトークン
} TokenKind;

typedef struct Token Token;

// トークン型
struct Token {
  TokenKind kind;  // トークンの型
  Token *next;     // 次の入力トークン
  int val;         // kindがTK_NUMの場合、その数値
  char *str;       // トークン文字列
  int len;         // トークンの長さ
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
bool consume(char *op);
Token *consume_ident(void);
void expect(char *op);
int expect_number(void);
bool at_eof(void);

Token *tokenize();

extern char *user_input;
extern Token *token;

//
// Parser
//
typedef struct LVar LVar;

// ローカル変数の型
struct LVar {
  LVar *next;
  char *name;
  int len;
  int offset;
};

// 抽象構文木のノードの種類
typedef enum {
  ND_ADD,        // +
  ND_SUB,        // -
  ND_MUL,        // *
  ND_DIV,        // /
  ND_NUM,        // 整数
  ND_EQ,         // ==
  ND_NE,         // !=
  ND_LT,         // <
  ND_LE,         // <=
  ND_ASSIGN,     // =
  ND_LVAR,       // ローカル変数
  ND_RETURN,     // Return
  ND_EXPR_STMT,  // Expression statement
  ND_IF,         // If
  ND_WHILE,      // While
  ND_FOR,        // For
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
  NodeKind kind;  // ノードの型
  Node *next;
  Node *lhs;  // 左辺
  Node *rhs;  // 右辺
  int val;    // kindがND_NUMの場合のみ使う
  LVar *var;  // kind==ND_LVAR

  Node *cond;
  Node *then;
  Node *els;
};

typedef struct Function Function;
struct Function {
  Node *node;
  LVar *locals;
  int stack_size;
};

Function *program(void);

//
// Code generator
//
void codegen(Function *prog);
