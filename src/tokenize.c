#include "9cc.h"

// 現在着目しているトークン
Token *token;

// 入力プログラム
char *user_input;
char *filename;

char *strndup(char *s, size_t n) {
  char *t = calloc(n + 1, sizeof(char));
  memcpy(t, s, n);
  t[n] = '\0';
  return t;
}

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// Reports an error location and exit.
static void verror_at(char *loc, char *fmt, va_list ap) {
  // locが含まれている行の開始地点と終了地点を取得
  char *line = loc;
  while (user_input < line && line[-1] != '\n') line--;

  char *end = loc;
  while (*end != '\n') end++;

  // 見つかった行が全体の何行目なのかを調べる
  int line_num = 1;
  for (char *p = user_input; p < line; p++)
    if (*p == '\n') line_num++;

  // 見つかった行を、ファイル名と行番号と一緒に表示
  int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
  fprintf(stderr, "%.*s\n", (int)(end - line), line);

  // エラー箇所を"^"で指し示して、エラーメッセージを表示
  int pos = loc - line + indent;
  fprintf(stderr, "%*s", pos, "");  // pos個の空白を出力
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

// エラー箇所を報告する
void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(loc, fmt, ap);
}

// Reports an error location and exit.
void error_tok(Token *tok, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  verror_at(tok->str, fmt, ap);
}

Token *peek(char *s) {
  if (token->kind != TK_RESERVED || strlen(s) != token->len ||
      memcmp(token->str, s, token->len))
    return NULL;
  return token;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進めて
// 真を返す。それ以外の場合には偽を返す。
Token *consume(char *op) {
  if (!peek(op)) return false;
  Token *t = token;
  token = token->next;
  return t;
}

Token *consume_ident(void) {
  if (token->kind != TK_IDENT) return NULL;
  Token *tok = token;
  token = token->next;
  return tok;
}

// 次のトークンが期待している記号のときには、トークンを1つ読み進める。
// それ以外の場合にはエラーを報告する。
void expect(char *op) {
  if (!peek(op)) error_at(token->str, "'%s'ではありません", op);
  token = token->next;
}

// 次のトークンが数値の場合、トークンを1つ読み進めてその数値を返す。
// それ以外の場合にはエラーを報告する。
int expect_number(void) {
  if (token->kind != TK_NUM) error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

char *expect_ident(void) {
  if (token->kind != TK_IDENT) error_at(token->str, "識別子ではありません");
  char *name = strndup(token->str, token->len);
  token = token->next;
  return name;
}

bool at_eof() { return token->kind == TK_EOF; }

// 新しいトークンを作成してcurに繋げる
Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

static bool is_alpha(char c) { return isalpha(c) || c == '_'; };

static bool is_alnum(char c) { return isalnum(c) || c == '_'; }

static bool startswith(char *p, char *q) {
  return strncmp(p, q, strlen(q)) == 0;
}

static char *starts_with_reserved(char *p) {
  // Keyword
  static char *kw[] = {
      "return", "if",  "else", "while",  "for",     "char",
      "short",  "int", "long", "struct", "typedef", "sizeof",
  };

  for (int i = 0; i < sizeof(kw) / sizeof(*kw); i++) {
    int len = strlen(kw[i]);
    if (startswith(p, kw[i]) && !isalnum(p[len])) return kw[i];
  }

  // Multi-letter punctuator
  static char *ops[] = {"==", "!=", "<=", ">=", "->"};

  for (int i = 0; i < sizeof(ops) / sizeof(*ops); i++)
    if (startswith(p, ops[i])) return ops[i];

  return NULL;
}

static char get_escape_char(char c) {
  switch (c) {
    case 'a':
      return '\a';
    case 'b':
      return '\b';
    case 't':
      return '\t';
    case 'n':
      return '\n';
    case 'v':
      return '\v';
    case 'f':
      return '\f';
    case 'r':
      return '\r';
    case 'e':
      return 27;
    case '0':
      return 0;
    default:
      return c;
  }
}

static Token *read_string_literal(Token *cur, char *start) {
  char *p = start + 1;
  char buf[1024];
  int len = 0;

  for (;;) {
    if (len == sizeof(buf)) error_at(start, "string literal too large");
    if (*p == '\0') error_at(start, "unclosed string literal");
    if (*p == '"') break;

    if (*p == '\\') {
      p++;
      buf[len++] = get_escape_char(*p++);
    } else {
      buf[len++] = *p++;
    }
  }

  Token *tok = new_token(TK_STR, cur, start, p - start + 1);
  tok->contents = malloc(len + 1);
  memcpy(tok->contents, buf, len);
  tok->contents[len] = '\0';
  tok->cont_len = len + 1;
  return tok;
}

// 入力文字列pをトークナイズしてそれを返す
Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    // 行コメントをスキップ
    if (strncmp(p, "//", 2) == 0) {
      p += 2;
      while (*p != '\n') p++;
      continue;
    }

    // ブロックコメントをスキップ
    if (strncmp(p, "/*", 2) == 0) {
      char *q = strstr(p + 2, "*/");
      if (!q) error_at(p, "コメントが閉じられていません");
      p = q + 2;
      continue;
    }

    // Keywords or multi-letter punctuators
    char *kw = starts_with_reserved(p);
    if (kw) {
      int len = strlen(kw);
      cur = new_token(TK_RESERVED, cur, p, len);
      p += len;
      continue;
    }

    // 変数名を獲得する
    if (is_alpha(*p)) {
      char *q = p++;
      while (is_alnum(*p)) p++;
      cur = new_token(TK_IDENT, cur, q, p - q);
      continue;
    }

    // 文字列
    if (*p == '"') {
      cur = read_string_literal(cur, p);
      p += cur->len;
      continue;
    }

    if (ispunct(*p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}
