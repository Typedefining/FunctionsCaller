#include "evaluator.h"
#include "scanner.h"

#include <cstdio>

using namespace FCExprClass;
using FCMarks::FCValue;
using FCMarks::FCValueCategory;

namespace {
void printValue(const FCValue &value) {
  switch (value.type) {
  case FCValueCategory::Integer:
    std::printf("[result] %d\n", value.evaluteVal.intVal);
    break;
  case FCValueCategory::Floating:
    std::printf("[result] %g\n", value.evaluteVal.doubleVal);
    break;
  case FCValueCategory::String:
    std::printf("[result] \"%s\"\n",
                value.evaluteVal.charVal ? value.evaluteVal.charVal->str.c_str() : "");
    break;
  case FCValueCategory::Dangle:
    std::printf("[result] Dangle\n");
    break;
  }
}
} // namespace

// 最小入口：分析一段源码并求值，结果打印到 stdout。
// 源码由第一个命令行参数给出，省略时使用内置示例。
int main(int argc, char *argv[]) {
  const char *source =
      argc > 1 ? argv[1] : "def add(a:int, b:int) { a + b } add(2, 3)";

  FCScanner scanner;
  auto ast = scanner.analysis(source);
  if (ast == nullptr)
    return 1;

  FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
  printValue(evaluate(ast.get(), context));
  return 0;
}
