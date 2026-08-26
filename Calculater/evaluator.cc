#include "evaluator.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace FCExprClass;
using namespace FCMarks;

FCValue evaluateExpression(const FCExprAST *expression,
                           FCEvaluationContext &context);
Frame *frameForDeclaration(const VarDecl *declaration,
                           FCEvaluationContext &context);

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

FCValue evaluateVariable(const FCVariableExprAST *node,
                         FCEvaluationContext &context) {
  Frame *frame = frameForDeclaration(node->decl.get(), context);
  if (frame == nullptr)
    return makeDangleValue();

  return frame->locals[node->decl->slot];
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
  if (node->getDecl() == nullptr || context.callStack.empty())
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

  Frame &frame = context.currentFrame();
  const int slot = node->getDecl()->slot;
  if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
    return makeDangleValue();
  frame.locals[slot] = start;
  while (evaluateExpression(node->getEnd(), context).evaluteVal.intVal != 0) {
    evaluateExpression(node->getBody(), context);
    frame.locals[slot].evaluteVal.intVal += step.evaluteVal.intVal;
  }
  return makeDangleValue();
}

FCValue evaluateBlock(const FCBlockExprAST *node,
                      FCEvaluationContext &context) {
  bool hasFrame = !context.callStack.empty();
  size_t frameIndex = hasFrame ? context.callStack.size() - 1 : 0;
  size_t oldSize = hasFrame ? context.callStack[frameIndex].locals.size() : 0;

  FCValue result = makeDangleValue();
  for (const auto &expr : node->getExpressions()) {
    result = evaluateExpression(expr.get(), context);
  }

  if (hasFrame) {
    context.callStack[frameIndex].locals.resize(oldSize);
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
  if (node->decl == nullptr || node->initExpr == nullptr)
    return makeDangleValue();
  const auto value = evaluateExpression(node->initExpr.get(), context);
  if (value.type == FCValueCategory::Dangle)
    return value;
  Frame *frame = frameForDeclaration(node->decl.get(), context);
  if (frame == nullptr)
    return makeDangleValue();
  frame->locals[node->decl->slot] = value;
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

Frame *frameForDeclaration(const VarDecl *declaration,
                           FCEvaluationContext &context) {
  if (declaration == nullptr)
    return nullptr;
  if (declaration->scopeLevel == 0) {
    if (declaration->slot < 0)
      return nullptr;
    if (declaration->slot >=
        static_cast<int>(context.globalFrame.locals.size()))
      context.globalFrame.locals.resize(declaration->slot + 1);
    return &context.globalFrame;
  }
  if (context.callStack.empty() || declaration->slot < 0)
    return nullptr;
  Frame &frame = context.currentFrame();
  if (declaration->slot >= static_cast<int>(frame.locals.size()))
    return nullptr;
  return &frame;
}

FCValue evaluateBinary(const FCBinaryExprAST *expression,
                       FCEvaluationContext &context) {
  const auto op = expression->getOperator();

  if (op == '=') {
    auto *variable =
        dynamic_cast<const FCVariableExprAST *>(expression->getLHS());
    if (variable == nullptr || variable->decl == nullptr) {
      std::fprintf(stderr, "LogError: LHS of assignment must be a variable\n");
      return makeDangleValue();
    }

    const auto value = evaluateExpression(expression->getRHS(), context);
    if (value.type == FCValueCategory::Dangle)
      return value;

    Frame *frame = frameForDeclaration(variable->decl.get(), context);
    if (frame == nullptr) {
      std::fprintf(stderr, "LogError: Invalid slot for %s\n",
                   variable->decl->name.c_str());
      return makeDangleValue();
    }

    frame->locals[variable->decl->slot] = value;
    return value;
  }

  const auto lhs = evaluateExpression(expression->getLHS(), context);
  const auto rhs = evaluateExpression(expression->getRHS(), context);
  if (lhs.type == FCValueCategory::Integer &&
      rhs.type == FCValueCategory::Integer) {
    FCValue result;
    result.type = FCValueCategory::Integer;
    switch (op) {
    case '+':
      result.evaluteVal.intVal = lhs.evaluteVal.intVal + rhs.evaluteVal.intVal;
      break;
    case '-':
      result.evaluteVal.intVal = lhs.evaluteVal.intVal - rhs.evaluteVal.intVal;
      break;
    case '*':
      result.evaluteVal.intVal = lhs.evaluteVal.intVal * rhs.evaluteVal.intVal;
      break;
    case '/':
      result.evaluteVal.intVal = lhs.evaluteVal.intVal / rhs.evaluteVal.intVal;
      break;
    case '<':
      result.evaluteVal.intVal = lhs.evaluteVal.intVal < rhs.evaluteVal.intVal;
      break;
    default:
      std::fprintf(stderr, "LogError: %c\n", op);
      return makeDangleValue();
    }
    return result;
  }

  if (lhs.type == FCValueCategory::Floating &&
      rhs.type == FCValueCategory::Floating) {
    FCValue result;
    result.type = FCValueCategory::Floating;
    switch (op) {
    case '+':
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal + rhs.evaluteVal.doubleVal;
      break;
    case '-':
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal - rhs.evaluteVal.doubleVal;
      break;
    case '*':
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal * rhs.evaluteVal.doubleVal;
      break;
    case '/':
      result.evaluteVal.doubleVal =
          lhs.evaluteVal.doubleVal / rhs.evaluteVal.doubleVal;
      break;
    default:
      std::fprintf(stderr, "LogError: %c\n", op);
      return makeDangleValue();
    }
    return result;
  }

  if (lhs.type == FCValueCategory::String &&
      rhs.type == FCValueCategory::String && op == '+')
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
  for (size_t i = 0; i < argumentValues.size(); ++i) {
    const auto &parameter = prototype->getArgs()[i];
    if (parameter.decl == nullptr) {
      context.popFrame();
      return makeDangleValue();
    }

    const int slot = parameter.decl->slot;
    if (slot < 0 || slot >= static_cast<int>(frame.locals.size())) {
      std::fprintf(stderr, "Invalid parameter slot in %s\n",
                   expression->getName().c_str());
      context.popFrame();
      return makeDangleValue();
    }
    frame.locals[slot] = argumentValues[i];
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

void FCEvaluationContext::pushFrame(const std::string &functionName) {
  Frame frame;
  frame.funcName = functionName;
  if (const auto *function = functions.findFunction(functionName))
    frame.locals.resize(function->getLocalCount());
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
