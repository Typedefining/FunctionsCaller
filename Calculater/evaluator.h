#pragma once

#include "diagnostic.h"
#include "runtime.h"
#include "ast.h"
#include "symbol.h"
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
  // 运行期诊断通道：求值错误写这里（编译期错误在 scanner 持有的通道里）
  FC::FCDiagnostics diagnostics;

  // 按符号身份解析变量：符号既携带槽位，也携带声明时的作用域深度。
  // 全局变量在顶层帧（callStack[0]）中，局部变量在当前调用帧中。
  std::shared_ptr<VariableSymbol> boundSymbol(const void *astNode) const {
    if (auto symbol = compiledProgram.m_binding->find(astNode))
      return symbol;
    return localBinding.find(astNode);
  }

  // 按符号身份把 AST 节点绑定到符号；不再通过名称在中心化表中查找。
  bool bindNode(const void *astNode, std::shared_ptr<VariableSymbol> symbol) {
    return localBinding.bind(astNode, std::move(symbol));
  }

  // 按符号的运行时定位决定它落在哪一帧上：frameIndex == -1 为全局（顶层帧），
  // 其余为当前调用帧（闭包捕获将在后续任务中按 runtime.frameIndex 定位）。
  Frame *frameForSymbol(const VariableSymbol &symbol) {
    if (symbol.runtime.frameIndex == -1) {
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

// RAII 守卫：构造 pushFrame，析构 popFrame。evaluateCall 的所有错误路径
// 与正常返回路径都通过作用域结束自动弹帧，消除手工 popFrame 的遗漏风险。
struct FrameGuard {
  FCEvaluationContext& ctx;
  FrameGuard(FCEvaluationContext& c, const std::string& name)
    : ctx(c) { ctx.pushFrame(name); }
  ~FrameGuard() { ctx.popFrame(); }
  FrameGuard(const FrameGuard&) = delete;
  FrameGuard& operator=(const FrameGuard&) = delete;
};

FCValue evaluate(FCExprAST *expression, struct FCEvaluationContext &context);
FCValue evaluate(const FCExprAST *expression, struct FCEvaluationContext &context);
} // namespace FCExprClass
