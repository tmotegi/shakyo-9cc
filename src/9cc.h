#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Tokenizer
//
typedef struct Type Type;

// トークンの種類
typedef enum {
  TK_RESERVED,  // 記号
  TK_IDENT,     // 識別子
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

char *strndup(char *s, size_t n);
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
Token *peek(char *s);
Token *consume(char *op);
Token *consume_ident(void);
void expect(char *op);
int expect_number(void);
char *expect_ident(void);
bool at_eof(void);

Token *tokenize(void);

extern char *user_input;
extern Token *token;

//
// Parser
//
typedef struct Var Var;

// ローカル変数の型
struct Var {
  char *name;
  Type *ty;
  int len;
  int offset;
  bool is_local;
};

typedef struct VarList VarList;

struct VarList {
  VarList *next;
  Var *var;
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
  ND_VAR,        // ローカル変数
  ND_RETURN,     // Return
  ND_EXPR_STMT,  // Expression statement
  ND_IF,         // If
  ND_WHILE,      // While
  ND_FOR,        // For
  ND_BLOCK,      // Block
  ND_FUNCALL,    // Function call
  ND_ADDR,       // &
  ND_DEREF,      // *
  ND_PTR_ADD,    // ptr + num
  ND_PTR_SUB,    // ptr - num
  ND_PTR_DIFF,   // ptr - ptr
} NodeKind;

typedef struct Node Node;

// 抽象構文木のノードの型
struct Node {
  NodeKind kind;  // ノードの型
  Node *next;
  Token *tok;
  Type *ty;

  Node *lhs;  // 左辺
  Node *rhs;  // 右辺

  // "if" or "while" statement
  Node *cond;
  Node *then;
  Node *els;

  // "for" statement
  Node *init;
  Node *step;

  Node *body;  // "block" statement

  int val;   // kind==ND_NUM
  Var *var;  // kind==ND_LVAR

  char *funcname;  // kind==ND_FUNCALL
  Node *args;      // "func call" statement
};

typedef struct Function Function;
struct Function {
  Function *next;
  char *name;
  Node *node;
  VarList *locals;
  VarList *args;
  int stack_size;
};

typedef struct {
  VarList *globals;
  Function *fns;
} Program;

Program *program(void);

//
// type
//
typedef enum {
  TY_CHAR,
  TY_INT,
  TY_PTR,
  TY_ARRAY,
} TypeKind;

struct Type {
  TypeKind kind;
  size_t size;
  Type *ptr_to;
  size_t array_size;
};

extern Type *int_type;
extern Type *char_type;

bool is_integer(Type *ty);
void add_type(Node *node);
Type *pointer_to(Type *ptr_to);
Type *array_of(Type *ptr_to, int len);

//
// Code generator
//
void codegen(Program *prog);
