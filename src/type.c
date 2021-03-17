#include "9cc.h"

Type *int_type = &(Type){TY_INT};

bool is_integer(Type *ty) { return ty->kind == TY_INT; }

Type *pointer_to(Type *ptr_to) {
  Type *ty = calloc(1, sizeof(Type));
  ty->kind = TY_PTR;
  ty->ptr_to = ptr_to;
  return ty;
}