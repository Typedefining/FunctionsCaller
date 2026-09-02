// 最小复现：块级遮蔽行为观察
#include "scanner.h"
#include "evaluator.h"
#include <cstdio>

using namespace FCExprClass;
using FCMarks::FCValue;
using FCMarks::FCValueCategory;

static void run(const char* src) {
  std::printf("---- src: %s\n", src);
  FCScanner scanner;
  auto ast = scanner.analysis(src);
  if (ast == nullptr) { std::printf("parse failed\n"); return; }
  FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
  // 打印函数符号表槽位绑定
  for (const auto& [name, sym] : scanner.semanticContext().getCompiledProgram().functions) {
    if (sym && sym->ast) {
      std::printf("  func %s frameSize=%zu: ", name.c_str(), (size_t)sym->frameSize);
      for (const auto& [vn, vs] : sym->symbols.getAllSymbols())
        std::printf("%s(%s,slot=%d) ", vn.c_str(),
                    vs->storage.kind == FCMarks::VariableStorage::Kind::Global ? "G" : "L",
                    vs->storage.slot);
      std::putchar(10);
    }
  }
  FCValue v = evaluate(ast.get(), ctx);
  if (v.type == FCValueCategory::Integer) std::printf("result = %d\n", v.evaluteVal.intVal);
  else if (v.type == FCValueCategory::Floating) std::printf("result = %f\n", v.evaluteVal.doubleVal);
  else std::printf("result = <dangle/other>\n");
}

int main() {
  run("def f() { var x:int = 1; { var x:int = 2; x } }; f()");       // 期望 2
  run("def f() { var x:int = 1; { var x:int = 2; }; x }; f()");      // 期望 1
  run("def f() { var x:int = 1; x }; f()");                          // 期望 1（对照）
  run("def f() { var x:int = 1; { var y:int = 2; x } }; f()");       // 期望 1（对照：不同名）
  run("def f() { { var x:int = 2; x } }; f()");                      // 期望 2（对照：仅内层）
  return 0;
}
