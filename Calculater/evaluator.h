#pragma once

#include "token.h"
#include "runtime.h"
#include "semantic.h"

namespace FCExprClass {
class FCFunctionRegistry {
public:
  bool registerFunction(FCFunctionAST *function);
  FCFunctionAST *findFunction(const std::string &name) const;
  bool index(FCExprAST *root);
  void clear();

private:
  std::unordered_map<std::string, FCFunctionAST *> m_functions;
};

struct FCEvaluationContext {
  FCFunctionRegistry functions;
  std::vector<Frame> callStack;
  CompiledProgram compiledProgram;
  FCMarks::SemanticBinding localBinding;

  // 按符号身份解析变量：符号既携带槽位，也携带声明时的作用域深度。
  // 全局变量在顶层帧（callStack[0]）中，局部变量在当前调用帧中。
  std::shared_ptr<VariableSymbol> boundSymbol(const void *astNode) const {
    if (compiledProgram.m_binding->size() != 0)
      return compiledProgram.m_binding->find(astNode);
    return localBinding.find(astNode);
  }

  // 按符号身份把 AST 节点绑定到符号；不再通过名称在中心化表中查找。
  bool bindNode(const void *astNode, std::shared_ptr<VariableSymbol> symbol) {
    return localBinding.bind(astNode, std::move(symbol));
  }

  // 根据符号的作用域深度决定它落在哪一帧上：深度 0 为顶层帧，
  // 其余为当前调用帧（闭包捕获将在后续任务中按 runtime.frameIndex 定位）。
  Frame *frameForSymbol(const VariableSymbol &symbol) {
    if (symbol.scopeDepth == 0) {
      if (callStack.empty())
        return nullptr;
      return &callStack[0];
    }
    if (callStack.empty())
      return nullptr;
    return &callStack.back();
  }

  FCEvaluationContext(const CompiledProgram &program);

  void pushFrame(const std::string &functionName);
  void popFrame();
  Frame &currentFrame();
};

FCValue evaluate(FCExprAST *expression, struct FCEvaluationContext &context);
FCValue evaluate(const FCExprAST *expression, struct FCEvaluationContext &context);
} // namespace FCExprClass
