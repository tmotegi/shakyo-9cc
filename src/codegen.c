#include "9cc.h"

// ラベル用のカウンタ
static int label_counter = 0;
static char *funcname;

// レジスタ
// https://www.sigbus.info/compilerbook#%E6%95%B4%E6%95%B0%E3%83%AC%E3%82%B8%E3%82%B9%E3%82%BF%E3%81%AE%E4%B8%80%E8%A6%A7
static char *argreg1[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argreg2[] = {"di", "si", "dx", "cx", "r8w", "r9w"};
static char *argreg4[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
static char *argreg8[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static void gen(Node *node);

// 変数のアドレスをスタックにプッシュする
void gen_addr(Node *node) {
  switch (node->kind) {
    case ND_VAR:
      if (node->var->is_local) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->var->offset);
        printf("  push rax\n");
      } else {
        printf("  push offset %s\n", node->var->name);
      }
      return;
    case ND_DEREF:
      gen(node->lhs);
      return;
    case ND_MEMBER:
      gen_addr(node->lhs);  // x.y の x の offset を計算
      printf("  pop rax\n");
      printf("  add rax, %d\n",
             node->member->offset);  // x.y の y の offset を計算
      printf("  push rax\n");
      return;
  }

  error_tok(node->tok, "代入の左辺値が変数ではありません");
}

static void gen_lval(Node *node) {
  if (node->ty->kind == TY_ARRAY)
    error_tok(node->tok,
              "not an lvalue");  // 配列のアドレスはスタックに積まない
  gen_addr(node);
}

static void load(Type *ty) {
  printf("  pop rax\n");
  if (ty->size == 1)
    printf("  movsx rax, byte ptr [rax]\n");  // 64 <- 8 bit
  else if (ty->size == 2)
    printf("  movsx rax, word ptr [rax]\n");  // 64 <- 16 bit
  else if (ty->size == 4)
    printf("  movsx rax, dword ptr [rax]\n");  // 64 <- 32 bit
  else
    printf("  mov rax, [rax]\n");
  printf("  push rax\n");
}

static void store(Type *ty) {
  printf("  pop rdi\n");
  printf("  pop rax\n");
  if (ty->kind == TY_BOOL) {
    printf("  cmp rdi, 0\n");  // 0かどうか
    printf("  setne dil\n");  // 0 でない場合は dil レジスタに1をセットする
    printf("  movzb rdi, dil\n");  // ゼロ拡張でストア
  }

  if (ty->size == 1)
    printf("  mov [rax], dil\n");  // rdi の 下位8ビット
  else if (ty->size == 2)
    printf("  mov [rax], di\n");  // rdi の 下位16ビット
  else if (ty->size == 4)
    printf("  mov [rax], edi\n");  // rdi の 下位32ビット
  else
    printf("  mov [rax], rdi\n");  // 左辺値に右辺値をストア

  printf("  push rdi\n");
}

static void truncate(Type *ty) {
  printf("  pop rax\n");

  if (ty->kind == TY_BOOL) {
    printf("  cmp rax, 0\n");
    printf("  setne al\n");
  }

  if (ty->size == 1) {
    printf("  movsx rax, al\n");
  } else if (ty->size == 2) {
    printf("  movsx rax, ax\n");
  } else if (ty->size == 4) {
    printf("  movsxd rax, eax\n");
  }
  printf("  push rax\n");
}

static void gen(Node *node) {
  switch (node->kind) {
    case ND_NULL:
      return;
    case ND_NUM:
      if (node->val == (int)node->val)
        printf("  push %ld\n", node->val);
      else {
        printf("  movabs rax, %ld\n", node->val);
        printf("  push rax\n");
      }
      return;
    case ND_EXPR_STMT:
      gen(node->lhs);
      printf("  add rsp, 8\n");
      return;
    case ND_VAR:  // 変数の値をスタックにプッシュする
    case ND_MEMBER:
      gen_addr(node);
      if (node->ty->kind != TY_ARRAY) load(node->ty);
      return;
    case ND_ASSIGN:
      gen_lval(node->lhs);  // 左辺値のアドレスをスタックにプッシュ
      gen(node->rhs);       // 右辺値の値をスタックにプッシュ
      store(node->ty);
      return;
    case ND_RETURN:  // returnの返り値の式を評価して，スタックトップをRAXに設定して関数から戻る
      gen(node->lhs);
      printf("  pop rax\n");
      printf("  jmp .L.return.%s\n", funcname);
      return;
    case ND_IF: {
      int label = label_counter++;
      if (node->els) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lelse%d\n", label);
        gen(node->then);
        printf("  jmp .Lend%d\n", label);
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
    case ND_BLOCK:
    case ND_STMT_EXPR:
      for (Node *n = node->body; n; n = n->next) gen(n);
      return;
    case ND_FUNCALL: {
      int reg_counter = 0;
      for (Node *arg = node->args; arg; arg = arg->next, reg_counter++)
        gen(arg);

      for (int i = reg_counter - 1; i >= 0; i--)
        printf("  pop %s\n", argreg8[i]);

      // We need to align RSP to a 16 byte boundary before
      // calling a function because it is an ABI requirement.
      // RAX is set to 0 for variadic function.
      int seq = label_counter++;
      printf("  mov rax, rsp\n");
      printf("  and rax, 15\n");
      printf("  jnz .L.call.%d\n", seq);
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->funcname);
      printf("  jmp .L.end.%d\n", seq);
      printf(".L.call.%d:\n", seq);
      printf("  sub rsp, 8\n");
      printf("  mov rax, 0\n");
      printf("  call %s\n", node->funcname);
      printf("  add rsp, 8\n");
      printf(".L.end.%d:\n", seq);
      printf("  push rax\n");
      return;
    }
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_DEREF:
      gen(node->lhs);
      if (node->ty->kind != TY_ARRAY) load(node->ty);
      return;
    case ND_CAST:
      gen(node->lhs);
      truncate(node->ty);
      return;
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
    case ND_PTR_ADD:
      printf("  imul rdi, %ld\n", node->ty->ptr_to->size);
      printf("  add rax, rdi\n");
      break;
    case ND_PTR_SUB:
      printf("  imul rdi, %ld\n", node->ty->ptr_to->size);
      printf("  sub rax, rdi\n");
      break;
    case ND_PTR_DIFF:
      printf("  sub rax, rdi\n");
      printf("  cqo\n");
      printf("  mov rdi, %ld\n",
             node->lhs->ty->ptr_to->size);  // node->ty->ptr_to は null
                                            // lhs のサイズに合わせる
      printf("  idiv rdi\n");
      break;
  }

  printf("  push rax\n");
}

void codegen(Program *prog) {
  printf(".intel_syntax noprefix\n");
  printf(".data\n");
  for (VarList *vl = prog->globals; vl; vl = vl->next) {
    printf("%s:\n", vl->var->name);
    if (!vl->var->contents)
      printf("  .zero %ld\n", vl->var->ty->size);
    else
      for (int i = 0; i < vl->var->cont_len; i++)
        printf("  .byte %d\n", vl->var->contents[i]);
  }
  printf(".text\n");
  for (Function *fn = prog->fns; fn; fn = fn->next) {
    // アセンブリの前半部分を出力
    printf(".global %s\n", fn->name);
    printf("%s:\n", fn->name);
    funcname = fn->name;

    // プロローグ
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  sub rsp, %d\n", fn->stack_size);

    // 引数の値をローカル変数の領域に書き込む
    int i = 0;
    for (VarList *vl = fn->args; vl; vl = vl->next)
      if (vl->var->ty->size == 1)
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg1[i++]);
      else if (vl->var->ty->size == 2)
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg2[i++]);
      else if (vl->var->ty->size == 4)
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg4[i++]);
      else
        printf("  mov [rbp-%d], %s\n", vl->var->offset, argreg8[i++]);

    // 先頭の式から順にコード生成
    for (Node *node = fn->node; node; node = node->next) gen(node);

    // エピローグ
    // 最後の式の結果がRAXに残っているのでそれが返り値になる
    printf(".L.return.%s:\n", fn->name);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
  }
}