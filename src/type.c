#include "9cc.h"

Type *int_type = &(Type){TY_INT, 8};

bool is_integer(Type *ty) { return ty->kind == TY_INT; }

Type *pointer_to(Type *ptr_to) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->ptr_to = ptr_to;
  ty->size = 8;
  return ty;
}

Type *array_of(Type *ptr_to, int len) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_ARRAY;
  ty->ptr_to = ptr_to;
  ty->array_size = len;
  ty->size = ptr_to->size * len;
}

void add_type(Node *node) {
  if (!node || node->ty) return;

  add_type(node->lhs);
  add_type(node->rhs);
  add_type(node->cond);
  add_type(node->then);
  add_type(node->els);
  add_type(node->init);
  add_type(node->step);

  for (Node *n = node->body; n; n = n->next) add_type(n);
  for (Node *n = node->args; n; n = n->next) add_type(n);

  switch (node->kind) {
    case ND_ADD:
    case ND_SUB:
    case ND_PTR_DIFF:
    case ND_MUL:
    case ND_DIV:
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
    case ND_FUNCALL:
    case ND_NUM:
      node->ty = int_type;
      return;
    case ND_PTR_ADD:
    case ND_PTR_SUB:
    case ND_ASSIGN:
      node->ty = node->lhs->ty;
      return;
    case ND_ADDR:
      node->ty = pointer_to(node->lhs->ty);
      return;
    case ND_DEREF:
      if (node->lhs->ty->kind == TY_PTR)
        node->ty = node->lhs->ty->ptr_to;
      else
        node->ty = int_type;
      return;
    case ND_VAR:
      node->ty = node->var->ty;
      return;
  }
}