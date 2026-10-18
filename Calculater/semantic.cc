#include <cassert>
#include <cstdio>

#include "semantic.h"
#include "token.h"
#include <algorithm>

namespace FCMarks {

FCSemanticContext::FCSemanticContext() {
  reset();
  m_scopeStack.emplace_back(); // Global scope
}

FCSemanticContext::~FCSemanticContext() {
  if (!m_scopeStack.empty())
    m_scopeStack.pop_back();
} // Ensure the global scope is cleaned up

void FCSemanticContext::reset() {
  m_binopPrecedence = {{'=', 5},  {'<', 9},  {'+', 10},
                       {'-', 10}, {'*', 20}, {'/', 20}};
  m_scopeStack.clear();
  m_functionSet.clear();
  m_frameLayout.reset();
  m_globalLayout.reset();

  m_compiledProgram = std::make_shared<CompiledProgram>();
  m_currentState = CurrentCompilationState();

  m_currentState.isActive = false;
}

int FCSemanticContext::getOperatorPrecedence(char op) const {
  const auto it = m_binopPrecedence.find(op);
  return it == m_binopPrecedence.end() ? 0 : it->second;
}

void FCSemanticContext::pushScope() {
  m_scopeStack.emplace_back(Scope{
      .variables = {}, .depth = static_cast<int>(m_scopeStack.size() + 1)});
}

void FCSemanticContext::popScope() {
  assert(m_scopeStack.size() > 1);
  m_scopeStack.pop_back();
}

void FCSemanticContext::pushFunctionScope() {
  assert(m_scopeStack.size() == 1);

  m_scopeStack.emplace_back();
  m_frameLayout.reset();

  m_currentState.currentFunction = std::make_shared<CompiledFunction>();
  m_currentState.isActive = true;
}

void FCSemanticContext::popFunctionScope() {
  assert(m_scopeStack.size() > 1);

  m_scopeStack.resize(1);

  m_currentState.currentFunction = nullptr;
  m_currentState.isActive = false;

  m_frameLayout.reset();
}

bool FCSemanticContext::isInFunctionScope() const {
  return m_currentState.isActive;
}

bool FCSemanticContext::isGlobalScope() const {
  // 只要当前不在函数体内，顶层作用域（含顶层块、顶层分支）中的声明都是全局变量
  return !m_currentState.isActive;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupVariableShared(const std::string &name) const {
  for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
    auto found = it->variables.find(name);
    if (found != it->variables.end())
      return found->second;
  }
  return nullptr;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupVariableInCurrentScope(const std::string &name) const {
  if (m_scopeStack.empty())
    return nullptr;

  auto &scope = m_scopeStack.back();

  auto it = scope.variables.find(name);

  if (it == scope.variables.end())
    return nullptr;

  return it->second;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupGlobalVariable(const std::string &name) const {
  for (const auto &symbol : m_compiledProgram->globalSymbols) {
    if (symbol && symbol->declaration && symbol->declaration->name == name) {
      return symbol;
    }
  }
  return nullptr;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::declareVariable(VarDeclPtr declaration) {
  assert(declaration != nullptr);
  assert(!m_scopeStack.empty());

  // 变量始终记录在当前作用域（栈顶），保持块作用域 RAII 语义：
  // pushScope/ popScope 后，局部声明随块消失。
  // 全局变量（isGlobalScope() 为 true）同时已在 declareVariable 末尾
  // 加入 m_compiledProgram->globalSymbols，供 lookupGlobalVariable 查找。
  Scope &scope = m_scopeStack.back();

  if (scope.variables.find(declaration->name) != scope.variables.end()) {
    fprintf(stderr, "Error: variable '%s' already declared in current scope\n",
            declaration->name.c_str());
    return nullptr;
  }

  // 变量在声明时取得所属布局内的槽位：全局符号表的下标，或函数帧内的下标
  const int slot = isGlobalScope() ? m_globalLayout.allocateSlot()
                                   : m_frameLayout.allocateSlot();

  const std::string name = declaration->name;
  auto symbol = std::make_shared<VariableSymbol>(std::move(declaration), slot,
                                                 scope.depth);
  symbol->runtime.frameIndex = isGlobalScope() ? -1 : 0;
  scope.variables.emplace(name, symbol);

  if (m_currentState.isActive && m_currentState.currentFunction) {
    m_currentState.currentFunction->symbols.addSymbol(name, symbol);
  } else if (isGlobalScope()) {
    m_compiledProgram->globalSymbols.push_back(symbol);
  }

  return symbol;
}

bool FCSemanticContext::hasFunction(const std::string &functionName) const {
  return m_functionSet.find(functionName) != m_functionSet.end();
}

bool FCSemanticContext::registerFunction(const std::string &functionName,
                                         const FCFunctionAST *function) {
  if (hasFunction(functionName)) {
    fprintf(stderr, "Error: function '%s' already defined\n",
            functionName.c_str());
    return false;
  }

  auto compiledFunc = std::make_shared<CompiledFunction>();
  compiledFunc->ast = const_cast<FCFunctionAST *>(function);

  if (m_currentState.isActive && m_currentState.currentFunction) {
    compiledFunc->symbols = std::move(m_currentState.currentFunction->symbols);
    compiledFunc->frameSize = m_frameLayout.frameSize();
    compiledFunc->maxTempSlots =
        m_frameLayout.frameSize(); // Assuming maxTempSlots is the same as frame
                                   // size for now

    // 按原型声明顺序记录形参槽位，调用期据此把实参写入帧的固定位置
    for (const auto &varDecl : function->getProto()->getArgs()) {
      auto param = compiledFunc->symbols.lookupShared(varDecl->name);
      if (param) {
        compiledFunc->parametersIdx.push_back(param->slot);
      }
    }
  }

  m_compiledProgram->functions[functionName] = std::move(compiledFunc);

  if (m_currentState.isActive && m_currentState.currentFunction) {
    m_currentState.currentFunction = m_compiledProgram->functions[functionName];
  }

  m_functionSet.insert(functionName);
  return true;
}

const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> &
FCSemanticContext::currentScopeDeclarations() const {
  return m_scopeStack.back().variables;
}

int FCSemanticContext::currentFrameLayoutSize() const {
  return m_frameLayout.frameSize();
}

const CompiledProgram &FCSemanticContext::getCompiledProgram() const {
  return *m_compiledProgram;
}

const CompiledFunction *
FCSemanticContext::getCompiledFunction(const std::string &name) const {
  return m_compiledProgram->getFunction(name);
}

void FCSemanticContext::dumpScopes() const {
  printf("=== Scope Debug Info ===\n");
  printf("Scope depth: %zu\n", m_scopeStack.size());
  printf("Current function frame size: %d\n", m_frameLayout.frameSize());
  printf("Current global layout size: %d\n", m_globalLayout.globalSize());

  for (size_t i = 0; i < m_scopeStack.size(); ++i) {
    const auto &scope = m_scopeStack[i];

    printf(": %zu variables\n", scope.variables.size());
    for (const auto &pair : scope.variables) {
      const auto &symbol = *pair.second;

      if (!symbol.declaration)
        continue;

      const auto &decl = symbol.declaration;

      std::printf("  - %s: type=%s, slot=%d, depth=%d, mutable=%s\n",
                  decl->name.c_str(), decl->typeName.c_str(), symbol.slot,
                  symbol.scopeDepth, symbol.isMutable ? "yes" : "no");
    }
  }
}

bool SymbolTable::addSymbol(const std::string &name,
                            std::shared_ptr<VariableSymbol> symbol) {
  bool inserted = m_symbols.find(name) == m_symbols.end();
  if (!inserted) {
    // 不允许重定义同名符号
    return false;
  }
  m_symbols.insert_or_assign(name, std::move(symbol));
  return inserted;
}

std::shared_ptr<VariableSymbol>
SymbolTable::lookupShared(const std::string &name) const {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? it->second : nullptr;
}

bool SymbolTable::removeSymbol(const std::string &name) {
  return m_symbols.erase(name) > 0;
}

const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> &
SymbolTable::getAllSymbols() const {
  return m_symbols;
}

void SymbolTable::clear() { m_symbols.clear(); }

size_t SymbolTable::size() const { return m_symbols.size(); }

bool FCSemanticContext::bindVariable(const void *astNode,
                                     std::shared_ptr<VariableSymbol> symbol) {
  if (astNode == nullptr || symbol == nullptr)
    return false;

  return m_compiledProgram->m_binding->bind(astNode, std::move(symbol));
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::boundSymbol(const void *astNode) const {
  return m_compiledProgram->m_binding->find(astNode);
}

// 调试输出
void SymbolTable::dump() const {
  printf("Symbol Table (%zu symbols):\n", m_symbols.size());
  for (const auto &[name, symbol] : m_symbols) {
    const char *kind = symbol->slot < 0 ? "Unassigned"
                                        : "Local"; // 符号表仅容纳函数域符号

    printf("  %s: slot=%d, kind=%s, depth=%d, mutable=%s\n", name.c_str(),
           symbol->slot, kind, symbol->scopeDepth,
           symbol->isMutable ? "yes" : "no");
  }
}

// ==================== SemanticAnalyzer ====================

FCSemanticContext::SemanticAnalyzer::SemanticAnalyzer(FCSemanticContext &ctx)
    : m_ctx(ctx) {}

bool FCSemanticContext::SemanticAnalyzer::analyze(const FCExprClass::FCExprAST *root) {
  if (!root) {
    return false;
  }
  return analyzeExpr(root);
}

bool FCSemanticContext::SemanticAnalyzer::analyzeExpr(const FCExprClass::FCExprAST *expr) {
  if (!expr) {
    return true;
  }

  if (auto *n = dynamic_cast<const FCExprClass::FCProgramAST *>(expr))
    return analyzeProgram(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCBlockExprAST *>(expr))
    return analyzeBlock(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCSeqExprAST *>(expr))
    return analyzeSeq(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCVarDeclExprAST *>(expr))
    return analyzeVarDecl(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCVariableExprAST *>(expr))
    return analyzeVariable(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCForExprAST *>(expr))
    return analyzeFor(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCIfExprAST *>(expr))
    return analyzeIf(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCCallExprAST *>(expr))
    return analyzeCall(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCBinaryExprAST *>(expr))
    return analyzeBinary(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCFunctionAST *>(expr))
    return analyzeFunction(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCNumberExprAST *>(expr))
    return analyzeNumber(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCStringExprAST *>(expr))
    return analyzeString(n);

  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeProgram(const FCExprClass::FCProgramAST *node) {
  for (const auto &stmt : node->getStatements()) {
    if (!analyzeExpr(stmt.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeBlock(const FCExprClass::FCBlockExprAST *node) {
  FCSemanticContext::ScopeGuard guard(m_ctx);
  for (const auto &expr : node->getExpressions()) {
    if (!analyzeExpr(expr.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeSeq(const FCExprClass::FCSeqExprAST *node) {
  for (const auto &expr : node->getExpressions()) {
    if (!analyzeExpr(expr.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeVarDecl(const FCExprClass::FCVarDeclExprAST *node) {
  auto symbol = m_ctx.declareVariable(node->decl);
  if (!symbol) {
    return false;
  }
  if (node->initExpr) {
    return analyzeExpr(node->initExpr.get());
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeVariable(const FCExprClass::FCVariableExprAST *node) {
  auto symbol = resolveVariable(node->name);
  if (!symbol) {
    fprintf(stderr, "Error: variable '%s' is not declared\n", node->name.c_str());
    return false;
  }
  return m_ctx.bindVariable(node, symbol);
}

bool FCSemanticContext::SemanticAnalyzer::analyzeFor(const FCExprClass::FCForExprAST *node) {
  FCSemanticContext::ScopeGuard guard(m_ctx);

  auto symbol = m_ctx.declareVariable(node->getDecl());
  if (!symbol) {
    return false;
  }

  if (!analyzeExpr(node->getStart())) return false;
  if (!analyzeExpr(node->getEnd())) return false;
  if (!analyzeExpr(node->getStep())) return false;
  if (!analyzeExpr(node->getBody())) return false;
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeIf(const FCExprClass::FCIfExprAST *node) {
  if (!analyzeExpr(node->getCondition())) return false;
  if (!analyzeExpr(node->getThen())) return false;
  if (node->getElse()) {
    if (!analyzeExpr(node->getElse())) return false;
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeCall(const FCExprClass::FCCallExprAST *node) {
  for (const auto &arg : node->getArgs()) {
    if (!analyzeExpr(arg.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeBinary(const FCExprClass::FCBinaryExprAST *node) {
  if (!analyzeExpr(node->getLHS())) return false;
  if (!analyzeExpr(node->getRHS())) return false;
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeFunction(const FCExprClass::FCFunctionAST *node) {
  FCSemanticContext::FunctionScopeGuard guard(m_ctx);

  // 注册函数，以便后续阶段（evaluator/codegen）可按名称查找
  m_ctx.registerFunction(node->getProtoName(), node);

  for (const auto &arg : node->getProto()->getArgs()) {
    auto symbol = m_ctx.declareVariable(arg);
    if (!symbol) {
      return false;
    }
  }

  return analyzeExpr(node->getBody());
}

bool FCSemanticContext::SemanticAnalyzer::analyzeNumber(const FCExprClass::FCNumberExprAST *) {
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeString(const FCExprClass::FCStringExprAST *) {
  return true;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::SemanticAnalyzer::resolveVariable(const std::string &name) {
  return m_ctx.lookupVariableShared(name);
}

const SemanticBinding &
FCSemanticContext::semanticBinding() const {
  return *m_compiledProgram->m_binding;
}

} // namespace FCMarks

