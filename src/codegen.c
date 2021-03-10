#include "9cc.h"

// ラベル用のカウンタ
static int label_counter = 0;

// 変数のアドレスをスタックにプッシュする
void gen_lval(Node *node) {
  if (node->kind != ND_LVAR) error("代入の左辺値が変数ではありません");
  printf("  mov rax, rbp\n");
  printf("  sub rax, %d\n", node->var->offset);
  printf("  push rax\n");
}

void gen(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  push %d\n", node->val);
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  add rsp, 8\n");
      return;
    case ND_LVAR:  // 変数の値をスタックにプッシュする
      gen_lval(node);
      printf("  pop rax\n");
      printf("  mov rax, [rax]\n");
      printf("  push rax\n");
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);  // 左辺値のアドレスをスタックにプッシュ
      gen(node->rhs);       // 右辺値の値をスタックにプッシュ
      printf("  pop rdi\n");
      printf("  pop rax\n");
      printf("  mov [rax], rdi\n");  // 左辺値に右辺値をストア
      printf("  push rdi\n");
      return;
    case ND_RETURN:  // returnの返り値の式を評価して，スタックトップをRAXに設定して関数から戻る
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .L.return\n");
      return;
    case ND_IF: {
      int label = label_counter++;
      if (node->els) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lelse%d\n", label);
        gen(node->then);
        printf("  je .Lend%d\n", label);
        printf(".Lelse%d:\n", label);
        gen(node->els);
        printf(".Lend%d:\n", label);
      } else {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
        gen(node->then);
        printf(".Lend%d:\n", label);
      }
      return;
    }
    case ND_WHILE: {
      int label = label_counter++;
      printf(".Lbegin%d:\n", label);
      gen(node->cond);
      printf("  pop rax\n");
      printf("  cmp rax, 0\n");
      printf("  je .Lend%d\n", label);
      gen(node->then);
      printf("  jmp .Lbegin%d\n", label);
      printf(".Lend%d:\n", label);
      return;
    }
    case ND_FOR: {
      int label = label_counter++;
      if (node->init) gen(node->init);
      printf(".Lbegin%d:\n", label);
      if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
      }
      gen(node->then);
      if (node->step) gen(node->step);
      printf("  jmp .Lbegin%d\n", label);
      printf(".Lend%d:\n", label);
      return;
    }
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->kind) {
    case ND_ADD:
      printf("  add rax, rdi\n");
      break;
    case ND_SUB:
      printf("  sub rax, rdi\n");
      break;
    case ND_MUL:
      printf("  imul rax, rdi\n");
      break;
    case ND_DIV:
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case ND_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LT:
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case ND_LE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
  }

  printf("  push rax\n");
}

void codegen(Function *prog) {
  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // プロローグ
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", prog->stack_size);

  // 先頭の式から順にコード生成
  for (Node *node = prog->node; node; node = node->next) gen(node);

  // エピローグ
  // 最後の式の結果がRAXに残っているのでそれが返り値になる
  printf(".L.return:\n");
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");
}