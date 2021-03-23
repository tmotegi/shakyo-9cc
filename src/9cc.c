#include "9cc.h"

// 指定されたファイルの内容を返す
char *read_file(char *path) {
  // ファイルを開く
  FILE *fp = fopen(path, "r");
  if (!fp) error("cannot open %s: %s", path, strerror(errno));

  int filemax = 10 * 1024 * 1024;
  // ファイル内容を読み込む
  char *buf = calloc(1, filemax);
  int size = fread(buf, 1, filemax - 2, fp);
  if (!feof(fp)) error("%s: file too large");

  // ファイルが必ず"\n\0"で終わっているようにする
  if (size == 0 || buf[size - 1] != '\n') buf[size++] = '\n';
  buf[size] = '\0';
  fclose(fp);
  return buf;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  // トークナイズしてパースする
  filename = argv[1];
  user_input = read_file(filename);
  token = tokenize();
  Program *prog = program();

  for (Function *fn = prog->fns; fn; fn = fn->next) {
    int offset = 0;
    for (VarList *vl = fn->locals; vl; vl = vl->next) {
      offset += vl->var->ty->size;
      vl->var->offset = offset;
    }
    fn->stack_size = offset;
  }

  codegen(prog);

  return 0;
}
