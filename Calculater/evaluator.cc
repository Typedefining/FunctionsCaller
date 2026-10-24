#include "evaluator.h"
#include "runtime.h"
#include "trace.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

using namespace FCExprClass;
using namespace FCMarks;

FCValue evaluateExpression(const FCExprAST *expression, FCEvaluationContext &context);

namespace {
// Void：无值（空块/空序列/定义语句）；FCValue{} 默认构造即 Void
FCValue makeVoidValue() {
  return FCValue{};
}

// Error：求值失败哨兵（Task 1 过渡态无诊断；Task 2 收口为带诊断重载）
FCValue makeErrorValue() {
  return FCValue{FCErrorValue{}};
}

// 诊断输出用的值文本形式；与单元测试期望的格式保持一致。
std::string formatValue(const FCValue &value) {
  switch (value.category()) {
  case FCValueCategory::Integer:
    return std::to_string(*value.asInteger());
  case FCValueCategory::Floating: {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", *value.asFloating());
    return std::string(buffer);
  }
  case FCValueCategory::String:
    return "\"" + *value.asString() + "\"";
  case FCValueCategory::Void:
    return "Void";
  case FCValueCategory::Error:
    break;
  }
  return "Error";
}

FCValue evaluateNumber(const FCNumberExprAST *node) {
  if (node->isFloating())
    return FCValue{node->m_doubleVal};
  return FCValue{node->m_intVal};
}

FCValue evaluateVariable(const FCVariableExprAST *node, FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> symbol = context.boundSymbol(node);
  if (symbol == nullptr)
    return makeErrorValue();

  Frame *frame = context.frameForSymbol(*symbol);
  if (frame == nullptr || symbol->slot < 0 ||
      symbol->slot >= static_cast<int>(frame->locals.size()))
    return makeErrorValue();

  return frame->locals[symbol->slot];
}

FCValue evaluateIf(const FCIfExprAST *node, FCEvaluationContext &context) {
  const auto condition = evaluateExpression(node->getCondition(), context);
  bool truthy = false;
  if (condition.category() == FCValueCategory::Integer)
    truthy = *condition.asInteger() != 0;
  else if (condition.category() == FCValueCategory::Floating)
    truthy = *condition.asFloating() != 0.0;
  else if (condition.isError())
    return condition;  // 错误传播；条件必须数值（Void/类别非法 → Error）
  else
    return makeErrorValue();
  return evaluateExpression(truthy ? node->getThen() : node->getElse(),
                            context);
}

FCValue evaluateFor(const FCForExprAST *node, FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> forSymbol = context.boundSymbol(node);
  if (node->getDecl() == nullptr || forSymbol == nullptr || context.callStack.empty())
    return makeErrorValue();
  // for 循环变量应为局部变量，不应落在全局帧（frameIndex == -1）上
  if (forSymbol->runtime.frameIndex == -1)
    return makeErrorValue();
  const auto start = evaluateExpression(node->getStart(), context);
  FCValue step = node->getStep() == nullptr
                     ? FCValue{1}
                     : evaluateExpression(node->getStep(), context);

  if (start.category() != FCValueCategory::Integer ||
      step.category() != FCValueCategory::Integer)
    return makeErrorValue();
  const int stepValue = *step.asInteger();

  const int slot = forSymbol->slot;
  size_t frameIdx = context.callStack.size() - 1;
  if (slot < 0 || slot >= static_cast<int>(context.callStack[frameIdx].locals.size()))
    return makeErrorValue();

  context.callStack[frameIdx].locals[slot] = start;

  FCValue result = start;
  while (true) {
    const auto end = evaluateExpression(node->getEnd(), context);
    bool truthy;
    if (const int *e = end.asInteger())
      truthy = *e != 0;
    else if (const double *e = end.asFloating())
      truthy = *e != 0.0;
    else if (end.isError())
      return end;  // 错误穿透循环（终止条件必须数值）
    else {
      // 终止条件必须数值（与 if 条件同一规则）；旧实现读 union 位模式
      context.diagnostics.add("evaluator", {},
                              "for end condition must be numeric");
      return makeErrorValue();
    }
    if (!truthy)
      break;
    result = evaluateExpression(node->getBody(), context);
    if (result.isError())
      return result;  // 仅错误中止；Void 流经继续循环
    auto &counter = context.callStack[frameIdx].locals[slot];
    const int *current = counter.asInteger();
    if (current == nullptr) {
      // 循环体可能把循环变量赋成非 int；旧实现此处做指针位模式整数加法
      context.diagnostics.add("evaluator", {},
                              "for loop variable must stay integer");
      return makeErrorValue();
    }
    counter = FCValue{*current + stepValue};
  }
  return result;  // 体全 Void 时返回 Void
}

FCValue evaluateBlock(const FCBlockExprAST *node,
                      FCEvaluationContext &context) {
  FCValue result = makeVoidValue();

  for (const auto &expr : node->getExpressions()) {
    result = evaluateExpression(expr.get(), context);
    if (result.isError())
      return result;
  }

  return result;
}

FCValue evaluateSequence(const FCSeqExprAST *node,
                         FCEvaluationContext &context) {
  FCValue result;
  for (const auto &expr : node->getExpressions()) {
    result = evaluateExpression(expr.get(), context);
    if (result.isError())
      return result;
  }
  return result;
}

FCValue evaluateDeclaration(const FCVarDeclExprAST *node,
                            FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> symbol = context.boundSymbol(node);
  if (symbol == nullptr || node->decl == nullptr || node->initExpr == nullptr)
    return makeErrorValue();

  const auto value = evaluateExpression(node->initExpr.get(), context);
  if (value.isError())
    return value;
  Frame *frame = context.frameForSymbol(*symbol);
  if (frame == nullptr || symbol->slot < 0 ||
      symbol->slot >= static_cast<int>(frame->locals.size()))
    return makeErrorValue();
  frame->locals[symbol->slot] = value;
  return value;
}

FCValue evaluateProgram(const FCProgramAST *node,
                        FCEvaluationContext &context) {
  context.functions.index(const_cast<FCProgramAST *>(node));
  FCValue result = makeVoidValue();
  for (const auto &statement : node->getStatements()) {
    if (dynamic_cast<const FCFunctionAST *>(statement.get()) != nullptr)
      continue;
    result = evaluateExpression(statement.get(), context);
  }
  return result;
}

} // namespace

FCValue evaluateBinary(const FCBinaryExprAST *expression,
                       FCEvaluationContext &context) {
  const auto op = expression->getOperator();

  if (op == FCBinaryOp::Assign) {
    auto *variable =
        dynamic_cast<const FCVariableExprAST *>(expression->getLHS());
    if (variable == nullptr || variable->name.empty()) {
      context.diagnostics.add("evaluator", {},
                               "LHS of assignment must be a variable");
      return makeErrorValue();
    }

    const auto value = evaluateExpression(expression->getRHS(), context);
    if (value.isError())
      return value;

    std::shared_ptr<VariableSymbol> lhsSymbol = context.boundSymbol(variable);
    if (lhsSymbol == nullptr) {
      context.diagnostics.add("evaluator", {},
                               "LHS of assignment is not bound to a symbol");
      return makeErrorValue();
    }

    Frame *frame = context.frameForSymbol(*lhsSymbol);
    if (frame == nullptr || lhsSymbol->slot < 0 ||
        lhsSymbol->slot >= static_cast<int>(frame->locals.size())) {
      context.diagnostics.add("evaluator", {},
                               "Invalid slot for " + variable->name);
      return makeErrorValue();
    }

    frame->locals[lhsSymbol->slot] = value;
    return value;
  }

  const auto lhs = evaluateExpression(expression->getLHS(), context);
  const auto rhs = evaluateExpression(expression->getRHS(), context);
  if (lhs.category() == FCValueCategory::Integer &&
      rhs.category() == FCValueCategory::Integer) {
    switch (op) {
    case FCBinaryOp::Add:
      return FCValue{*lhs.asInteger() + *rhs.asInteger()};
    case FCBinaryOp::Sub:
      return FCValue{*lhs.asInteger() - *rhs.asInteger()};
    case FCBinaryOp::Mul:
      return FCValue{*lhs.asInteger() * *rhs.asInteger()};
    case FCBinaryOp::Div:
      return FCValue{*lhs.asInteger() / *rhs.asInteger()};
    case FCBinaryOp::Less:
      return FCValue{*lhs.asInteger() < *rhs.asInteger()};
    default:
      context.diagnostics.add("evaluator", {}, "unknown binary operator");
      return makeErrorValue();
    }
  }

  if (lhs.category() == FCValueCategory::Floating &&
      rhs.category() == FCValueCategory::Floating) {
    switch (op) {
    case FCBinaryOp::Add:
      return FCValue{*lhs.asFloating() + *rhs.asFloating()};
    case FCBinaryOp::Sub:
      return FCValue{*lhs.asFloating() - *rhs.asFloating()};
    case FCBinaryOp::Mul:
      return FCValue{*lhs.asFloating() * *rhs.asFloating()};
    case FCBinaryOp::Div:
      return FCValue{*lhs.asFloating() / *rhs.asFloating()};
    default:
      context.diagnostics.add("evaluator", {}, "unknown binary operator");
      return makeErrorValue();
    }
  }

  if (lhs.category() == FCValueCategory::String &&
      rhs.category() == FCValueCategory::String && op == FCBinaryOp::Add)
    return FCValue{*lhs.asString() + *rhs.asString()};

  return makeErrorValue();
}

FCValue evaluateCall(const FCCallExprAST *expression,
                     FCEvaluationContext &context) {
  // 单一解析路径：语义期记录是唯一数据源，原型与槽位同取自该记录；
  // 无记录（或记录无 ast）时回退 registry（手工构造 AST 模式）
  auto *record = context.compiledProgram.getFunction(expression->getName());
  FCFunctionAST *function = (record != nullptr && record->ast != nullptr)
                                ? record->ast
                                : context.functions.findFunction(
                                      expression->getName());
  if (function == nullptr) {
    context.diagnostics.add("evaluator", {},
                            "Function not found: " + expression->getName());
    return makeErrorValue();
  }

  const auto &prototype = function->getProto();
  if (expression->getArgs().size() != prototype->getArgs().size()) {
    context.diagnostics.add("evaluator", {},
                            "Argument count mismatch in " + expression->getName());
    return makeErrorValue();
  }

  std::vector<FCValue> argumentValues;
  argumentValues.reserve(expression->getArgs().size());
  for (const auto &argument : expression->getArgs()) {
    const auto value = evaluateExpression(argument.get(), context);
    if (value.isError())
      return value;
    argumentValues.push_back(value);
  }

  // RAII 守卫：析构时自动弹帧，所有返回路径（含错误分支）均受保护
  FrameGuard guard{ context, expression->getName() };
  Frame &frame = context.currentFrame();

  // 调用前先确认语义期记录存在（帧布局与参数符号都来自它）。
  // 无记录时 pushFrame 已按实参数量分配帧（手工构造 AST 模式的防御行为）
  if (record == nullptr) {
    context.diagnostics.add(
        "evaluator", {},
        "Error: no compiled record for '" + expression->getName() + "'");
    return makeErrorValue();
  }

  for (size_t i = 0; i < argumentValues.size(); ++i) {
    const auto &parameter = prototype->getArgs()[i];
    if (parameter == nullptr) {
      return makeErrorValue();
    }

    auto symbol = record->symbols.lookupShared(parameter->name);
    if (symbol == nullptr || symbol->slot < 0 ||
        symbol->slot >= static_cast<int>(frame.locals.size())) {
      context.diagnostics.add(
          "evaluator", {},
          "Error: unresolved parameter '" + expression->getName() + "'");
      return makeErrorValue();
    }
    frame.locals[symbol->slot] = argumentValues[i];
  }

  if (FC::traceEnabled("evaluator")) {
    std::string arguments;
    for (size_t i = 0; i < argumentValues.size(); ++i) {
      if (i > 0)
        arguments += ", ";
      arguments += formatValue(argumentValues[i]);
    }
    std::fprintf(stderr, "[evaluator] call %s(%s) depth=%zu\n",
                 expression->getName().c_str(), arguments.c_str(),
                 context.callStack.size());
  }

  const auto result = evaluateExpression(function->getBody(), context);

  if (FC::traceEnabled("evaluator"))
    std::fprintf(stderr, "[evaluator] return %s = %s\n",
                 expression->getName().c_str(), formatValue(result).c_str());

  return result;
}

FCValue evaluateExpression(const FCExprAST *expression, FCEvaluationContext &context);

struct EvaluatorVisitor {
  FCEvaluationContext& c;
  explicit EvaluatorVisitor(FCEvaluationContext& ctx) : c(ctx) {}

  FCValue visit(const FCNumberExprAST& n)   { return evaluateNumber(&n); }
  FCValue visit(const FCStringExprAST& n)   { return FCValue{n.m_stringVal}; }
  FCValue visit(const FCVariableExprAST& n) { return evaluateVariable(&n, c); }
  FCValue visit(const FCBinaryExprAST& n)   { return evaluateBinary(&n, c); }
  FCValue visit(const FCCallExprAST& n)     { return evaluateCall(&n, c); }
  FCValue visit(const FCIfExprAST& n)       { return evaluateIf(&n, c); }
  FCValue visit(const FCForExprAST& n)      { return evaluateFor(&n, c); }
  FCValue visit(const FCBlockExprAST& n)    { return evaluateBlock(&n, c); }
  FCValue visit(const FCSeqExprAST& n)      { return evaluateSequence(&n, c); }
  FCValue visit(const FCVarDeclExprAST& n)  { return evaluateDeclaration(&n, c); }
  FCValue visit(const FCProgramAST& n)      { return evaluateProgram(&n, c); }
  FCValue visit(const FCFunctionAST& n) {
    // callStack 栈底恒为 <global> 帧，故“多于一帧”才说明正处于函数调用中
    if (c.callStack.size() > 1) {
      c.diagnostics.add(
          "evaluator", {},
          "Error: nested function definition '" + n.getProtoName()
              + "' is not supported by the evaluator");
      return makeErrorValue();
    }
    c.functions.registerFunction(const_cast<FCFunctionAST*>(&n));
    return makeVoidValue();  // 定义语句无值
  }
  // FCPrototypeAST 不作为顶层语句出现；不可达防御分支，按“未知节点”归类为 Error。
  FCValue visit(const FCPrototypeAST& n) { return makeErrorValue(); }
};

FCValue evaluateExpression(const FCExprAST *expression,
                           FCEvaluationContext &context) {
  if (!expression)
    return makeErrorValue();

  EvaluatorVisitor v{ context };
  return FCExprClass::dispatchAST(*expression, v);
}

namespace FCExprClass {
bool FCFunctionRegistry::registerFunction(FCFunctionAST *function) {
  if (function == nullptr)
    return false;

  const auto name = function->getProtoName();
  auto it = m_functions.find(name);
  if (it != m_functions.end())
    return it->second == function;

  m_functions.emplace(name, function);
  return true;
}

FCFunctionAST *FCFunctionRegistry::findFunction(const std::string &name) const {
  auto it = m_functions.find(name);
  return it == m_functions.end() ? nullptr : it->second;
}

void FCFunctionRegistry::clear() { m_functions.clear(); }

bool FCFunctionRegistry::index(FCExprAST *root) {
  if (root == nullptr)
    return false;

  if (auto *function = dynamic_cast<FCFunctionAST *>(root))
    return registerFunction(function);

  if (auto *program = dynamic_cast<FCProgramAST *>(root)) {
    bool success = true;
    for (const auto &statement : program->getStatements()) {
      if (auto *function = dynamic_cast<FCFunctionAST *>(statement.get()))
        success = registerFunction(function) && success;
    }
    return success;
  }

  return false;
}

FCEvaluationContext::FCEvaluationContext(const CompiledProgram &program)
    : compiledProgram(program) {
  // 顶层帧承载全局变量，后续 pushFrame 为每次函数调用分配新的调用帧
  Frame globalFrame;
  globalFrame.funcName = "<global>";
  globalFrame.locals.resize(program.globalSymbols.size());
  callStack.push_back(std::move(globalFrame));
}

void FCEvaluationContext::pushFrame(const std::string &functionName) {
  Frame frame;
  frame.funcName = functionName;
  // 帧大小是语义期输出：从 CompiledFunction::frameSize 读取
  if (const auto *compiled = compiledProgram.getFunction(functionName)) {
    frame.locals.resize(compiled->frameSize);
  }
  callStack.push_back(std::move(frame));
}

void FCEvaluationContext::popFrame() {
  assert(!callStack.empty());
  callStack.pop_back();
}

Frame &FCEvaluationContext::currentFrame() {
  assert(!callStack.empty());
  return callStack.back();
}

FCValue evaluate(const FCExprAST *expression, FCEvaluationContext &context) {
  return evaluateExpression(expression, context);
}

FCValue evaluate(FCExprAST *expression, FCEvaluationContext &context) {
  return evaluateExpression(expression, context);
}
} // namespace FCExprClass
