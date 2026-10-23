#include "codegen.h"
#include "diagnostic.h"
#include "evaluator.h"
#include "scanner.h"

#include "llvm/Support/raw_ostream.h"

#include <cstdio>
#include <cstring>

using namespace FCExprClass;
using FCMarks::FCValue;
using FCMarks::FCValueCategory;

namespace {
const char *kDefaultSource = "def add(a:int, b:int) { a + b } add(2, 3)";

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

// 把语义分析结果接到代码生成上下文：编译记录与“AST 节点 → 符号”侧表。
void attachSemanticOutput(FCCodegenContext &context, const FCScanner &scanner) {
  context.compiledProgram = scanner.semanticContext().getCompiledProgram();
  context.semantic = &scanner.semanticContext();
}

// 结构化诊断统一出口：stderr 逐条打印。
void printDiagnostics(const FC::FCDiagnostics &diagnostics) {
  for (const auto &diagnostic : diagnostics.items())
    std::fprintf(stderr, "%s\n", FC::formatDiagnostic(diagnostic).c_str());
}
} // namespace

// 最小入口：
//   默认   分析源码并求值，结果打印为 [result] ...
//   --emit-ir 分析源码并生成 LLVM IR，模块文本打印到 stdout
// 源码取自最后一个命令行参数，省略时使用内置示例。
int main(int argc, char *argv[]) {
  const bool emitIr = argc > 1 && std::strcmp(argv[1], "--emit-ir") == 0;
  const char *source = nullptr;
  if (emitIr)
    source = argc > 2 ? argv[2] : kDefaultSource;
  else
    source = argc > 1 ? argv[1] : kDefaultSource;

  FCScanner scanner;
  auto ast = scanner.analysis(source);
  if (ast == nullptr) {
    printDiagnostics(scanner.diagnostics());
    return 1;
  }

  if (emitIr) {
    FCCodegenContext context("fc_module", scanner.semanticContext().getCompiledProgram());
    attachSemanticOutput(context, scanner);
    if (codegenTopLevel(ast.get(), context) == nullptr) {
      printDiagnostics(context.diagnostics);
      return 2;
    }
    context.module->print(llvm::outs(), nullptr);
    return 0;
  }

  FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
  const FCValue result = evaluate(ast.get(), context);
  printValue(result);
  printDiagnostics(context.diagnostics);
  return 0;
}
