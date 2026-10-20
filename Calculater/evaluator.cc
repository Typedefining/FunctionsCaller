#include "evaluator.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace FCExprClass;
using namespace FCMarks;

FCValue evaluateExpression(const FCExprAST *expression, FCEvaluationContext &context);

namespace {
FCValue makeDangleValue() {
  FCValue value;
  value.type = FCValueCategory::Dangle;
  value.evaluteVal.danglingVal = nullptr;
  return value;
}

FCValue makeStringValue(const std::string &text) {
  FCValue value;
  value.type = FCValueCategory::String;
  value.evaluteVal.charVal = new FCStringValue{text};
  return value;
}

FCValue evaluateNumber(const FCNumberExprAST *node) {
  FCValue result;
  if (node->isFloating()) {
    result.type = FCValueCategory::Floating;
    result.evaluteVal.doubleVal = node->m_doubleVal;
  } else {
    result.type = FCValueCategory::Integer;
    result.evaluteVal.intVal = node->m_intVal;
  }
  return result;
}

FCValue evaluateVariable(const FCVariableExprAST *node, FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> symbol = context.boundSymbol(node);
  if (symbol == nullptr)
    return makeDangleValue();

  Frame *frame = context.frameForSymbol(*symbol);
  if (frame == nullptr || symbol->slot < 0 ||
      symbol->slot >= static_cast<int>(frame->locals.size()))
    return makeDangleValue();

  return frame->locals[symbol->slot];
}

FCValue evaluateIf(const FCIfExprAST *node, FCEvaluationContext &context) {
  const auto condition = evaluateExpression(node->getCondition(), context);
  bool truthy = false;
  if (condition.type == FCValueCategory::Integer)
    truthy = condition.evaluteVal.intVal != 0;
  else if (condition.type == FCValueCategory::Floating)
    truthy = condition.evaluteVal.doubleVal != 0.0;
  else
    return makeDangleValue();
  return evaluateExpression(truthy ? node->getThen() : node->getElse(),
                            context);
}

FCValue evaluateFor(const FCForExprAST *node, FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> forSymbol = context.boundSymbol(node);
  if (node->getDecl() == nullptr || forSymbol == nullptr || context.callStack.empty())
    return makeDangleValue();
  // for 循环变量应为局部变量，不应落在全局帧（frameIndex == -1）上
  if (forSymbol->runtime.frameIndex == -1)
    return makeDangleValue();
  const auto start = evaluateExpression(node->getStart(), context);
  FCValue step;
  if (node->getStep() == nullptr) {
    step.type = FCValueCategory::Integer;
    step.evaluteVal.intVal = 1;
  } else {
    step = evaluateExpression(node->getStep(), context);
  }

  if (start.type != FCValueCategory::Integer ||
      step.type != FCValueCategory::Integer)
    return makeDangleValue();

  const int slot = forSymbol->slot;
  size_t frameIdx = context.callStack.size() - 1;
  if (slot < 0 || slot >= static_cast<int>(context.callStack[frameIdx].locals.size()))
    return makeDangleValue();

  context.callStack[frameIdx].locals[slot] = start;

  FCValue result = start;
  while (evaluateExpression(node->getEnd(), context).evaluteVal.intVal != 0) {
    result = evaluateExpression(node->getBody(), context);
    if (result.type == FCValueCategory::Dangle)
      return result;
    context.callStack[frameIdx].locals[slot].evaluteVal.intVal += step.evaluteVal.intVal;
  }
  return result;
}

FCValue evaluateBlock(const FCBlockExprAST *node,
                      FCEvaluationContext &context) {
  FCValue result = makeDangleValue();

  for (const auto &expr : node->getExpressions()) {
    result = evaluateExpression(expr.get(), context);
    if (result.type == FCValueCategory::Dangle)
      return result;
  }

  return result;
}

FCValue evaluateSequence(const FCSeqExprAST *node,
                         FCEvaluationContext &context) {
  FCValue result;
  for (const auto &expr : node->getExpressions()) {
    result = evaluateExpression(expr.get(), context);
    if (result.type == FCValueCategory::Dangle)
      return makeDangleValue();
  }
  return result;
}

FCValue evaluateDeclaration(const FCVarDeclExprAST *node,
                            FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> symbol = context.boundSymbol(node);
  if (symbol == nullptr || node->decl == nullptr || node->initExpr == nullptr)
    return makeDangleValue();

  const auto value = evaluateExpression(node->initExpr.get(), context);
  if (value.type == FCValueCategory::Dangle)
    return value;
  Frame *frame = context.frameForSymbol(*symbol);
  if (frame == nullptr || symbol->slot < 0 ||
      symbol->slot >= static_cast<int>(frame->locals.size()))
    return makeDangleValue();
  frame->locals[symbol->slot] = value;
  return value;
}

FCValue evaluateProgram(const FCProgramAST *node,
                        FCEvaluationContext &context) {
  context.functions.index(const_cast<FCProgramAST *>(node));
  FCValue result = makeDangleValue();
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
      std::fprintf(stderr, "LogError: LHS of assignment must be a variable\n");
      return makeDangleValue();
    }

    const auto value = evaluateExpression(expression->getRHS(), context);
    if (value.type == FCValueCategory::Dangle)
      return value;

    std::shared_ptr<VariableSymbol> lhsSymbol = context.boundSymbol(variable);
    if (lhsSymbol == nullptr) {
      std::fprintf(stderr, "LogError: LHS of assignment is not bound to a symbol\n");
      return makeDangleValue();
    }

    Frame *frame = context.frameForSymbol(*lhsSymbol);
    if (frame == nullptr || lhsSymbol->slot < 0 ||
        lhsSymbol->slot >= static_cast<int>(frame->locals.size())) {
      std::fprintf(stderr, "LogError: Invalid slot for %s\n",
                   variable->name.c_str());
      return makeDangleValue();
    }

    frame->locals[lhsSymbol->slot] = value;
    return value;
  }

  const auto lhs = evaluateExpression(expression->getLHS(), context);
  const auto rhs = evaluateExpression(expression->getRHS(), context);
  if (lhs.type == FCValueCategory::Integer &&
      rhs.type == FCValueCategory::Integer) {
    FCValue result;
    result.type = FCValueCategory::Integer;
    switch (op) {
    case FCBinaryOp::Add:
      result.evaluteVal.intVal = lhs.evaluteVal.intVal + rhs.evaluteVal.intVal;
      break;
    case FCBinaryOp::Sub:
      result.evaluteVal.intVal = lhs.evaluteVal.intVal - rhs.evaluteVal.intVal;
      break;
    case FCBinaryOp::Mul:
      result.evaluteVal.intVal = lhs.evaluteVal.intVal * rhs.evaluteVal.intVal;
      break;
    case FCBinaryOp::Div:
      result.evaluteVal.intVal = lhs.evaluteVal.intVal / rhs.evaluteVal.intVal;
      break;
    case FCBinaryOp::Less:
      result.evaluteVal.intVal = lhs.evaluteVal.intVal < rhs.evaluteVal.intVal;
      break;
    default:
      std::fprintf(stderr, "LogError: unknown binary operator\n");
      return makeDangleValue();
    }
    return result;
  }

  if (lhs.type == FCValueCategory::Floating &&
      rhs.type == FCValueCategory::Floating) {
    FCValue result;
    result.type = FCValueCategory::Floating;
    switch (op) {
    case FCBinaryOp::Add:
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal + rhs.evaluteVal.doubleVal;
      break;
    case FCBinaryOp::Sub:
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal - rhs.evaluteVal.doubleVal;
      break;
    case FCBinaryOp::Mul:
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal * rhs.evaluteVal.doubleVal;
      break;
    case FCBinaryOp::Div:
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal / rhs.evaluteVal.doubleVal;
      break;
    default:
      std::fprintf(stderr, "LogError: unknown binary operator\n");
      return makeDangleValue();
    }
    return result;
  }

  if (lhs.type == FCValueCategory::String &&
      rhs.type == FCValueCategory::String && op == FCBinaryOp::Add)
    return makeStringValue(lhs.evaluteVal.charVal->str +
                           rhs.evaluteVal.charVal->str);

  return makeDangleValue();
}

FCValue evaluateCall(const FCCallExprAST *expression,
                     FCEvaluationContext &context) {
  auto *function = context.functions.findFunction(expression->getName());
  if (function == nullptr) {
    std::fprintf(stderr, "Function not found: %s\n",
                 expression->getName().c_str());
    return makeDangleValue();
  }

  const auto &prototype = function->getProto();
  if (expression->getArgs().size() != prototype->getArgs().size()) {
    std::fprintf(stderr, "Argument count mismatch in %s\n",
                 expression->getName().c_str());
    return makeDangleValue();
  }

  std::vector<FCValue> argumentValues;
  argumentValues.reserve(expression->getArgs().size());
  for (const auto &argument : expression->getArgs()) {
    const auto value = evaluateExpression(argument.get(), context);
    if (value.type == FCValueCategory::Dangle)
      return value;
    argumentValues.push_back(value);
  }

  context.pushFrame(expression->getName());
  Frame &frame = context.currentFrame();

  // 调用前先确认语义期记录存在（帧布局与参数符号都来自它）
  auto func = context.compiledProgram.getFunction(expression->getName());
  if (func == nullptr) {
    std::fprintf(stderr, "Error: no compiled record for '%s'\n",
                 expression->getName().c_str());
    context.popFrame();
    return makeDangleValue();
  }

  for (size_t i = 0; i < argumentValues.size(); ++i) {
    const auto &parameter = prototype->getArgs()[i];
    if (parameter == nullptr) {
      context.popFrame();
      return makeDangleValue();
    }

    auto symbol = func->symbols.lookupShared(parameter->name);
    if (symbol == nullptr || symbol->slot < 0 ||
        symbol->slot >= static_cast<int>(frame.locals.size())) {
      fprintf(stderr, "Error: unresolved parameter '%s'\n",
              expression->getName().c_str());
      context.popFrame();
      return makeDangleValue();
    }
    frame.locals[symbol->slot] = argumentValues[i];
  }

  const auto result = evaluateExpression(function->getBody(), context);
  context.popFrame();
  return result;
}

FCValue evaluateExpression(const FCExprAST *expression,
                           FCEvaluationContext &context) {
  if (!expression)
    return makeDangleValue();

  if (auto *node = dynamic_cast<const FCNumberExprAST *>(expression))
    return evaluateNumber(node);

  if (auto *node = dynamic_cast<const FCStringExprAST *>(expression))
    return makeStringValue(node->m_stringVal);

  if (auto *node = dynamic_cast<const FCVariableExprAST *>(expression))
    return evaluateVariable(node, context);
  if (auto *node = dynamic_cast<const FCBinaryExprAST *>(expression))
    return evaluateBinary(node, context);

  if (auto *node = dynamic_cast<const FCCallExprAST *>(expression))
    return evaluateCall(node, context);

  if (auto *node = dynamic_cast<const FCIfExprAST *>(expression))
    return evaluateIf(node, context);

  if (auto *node = dynamic_cast<const FCForExprAST *>(expression))
    return evaluateFor(node, context);

  if (auto *node = dynamic_cast<const FCBlockExprAST *>(expression))
    return evaluateBlock(node, context);

  if (auto *node = dynamic_cast<const FCSeqExprAST *>(expression))
    return evaluateSequence(node, context);

  if (auto *node = dynamic_cast<const FCVarDeclExprAST *>(expression))
    return evaluateDeclaration(node, context);

  if (auto *node = dynamic_cast<const FCProgramAST *>(expression))
    return evaluateProgram(node, context);

  if (auto *node = dynamic_cast<const FCFunctionAST *>(expression)) {
    // callStack 栈底恒为 <global> 帧，故“多于一帧”才说明正处于函数调用中
    if (context.callStack.size() > 1) {
      fprintf(stderr,
              "Error: nested function definition '%s' is not supported by the "
              "evaluator\n",
              node->getProtoName().c_str());
      return makeDangleValue();
    }
    context.functions.registerFunction(const_cast<FCFunctionAST *>(node));
    return makeDangleValue();
  }

  return makeDangleValue();
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
