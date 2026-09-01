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

  m_persistentSymbolTable->clear();
  m_compiledProgram = std::make_shared<CompiledProgram>();
  m_currentState = CurrentCompilationState();

  m_currentState.isActive = false;
}

int FCSemanticContext::getOperatorPrecedence(char op) const {
  const auto it = m_binopPrecedence.find(op);
  return it == m_binopPrecedence.end() ? 0 : it->second;
}

void FCSemanticContext::pushScope() {
  m_scopeStack.emplace_back(
      Scope{.variables = {}, .depth = static_cast<int>(m_scopeStack.size() + 1)});
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
  return !m_currentState.isActive && m_scopeStack.size() == 1;
}

const VariableSymbol *
FCSemanticContext::lookupVariable(const std::string &name) const {
  for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {

    auto found = it->variables.find(name);

    // 返回共享持有符号的裸指针：作用域弹出后符号仍由持久表/编译产物持有
    if (found != it->variables.end())
      return found->second.get();
  }

  return nullptr;
}

const VariableSymbol *
FCSemanticContext::lookupVariableInCurrentScope(const std::string &name) const {
  if (m_scopeStack.empty())
    return nullptr;

  auto &scope = m_scopeStack.back();

  auto it = scope.variables.find(name);

  if (it == scope.variables.end())
    return nullptr;

  return it->second.get();
}

const VariableSymbol *
FCSemanticContext::lookupGlobalVariable(const std::string &name) const {
  if (m_scopeStack.empty())
    return nullptr;

  auto &globalScope = m_scopeStack.front();

  auto it = globalScope.variables.find(name);

  if (it == globalScope.variables.end())
    return nullptr;

  return it->second.get();
}

VariableSymbol* FCSemanticContext::declareVariable(VarDeclPtr declaration) {
  assert(declaration != nullptr);
  assert(!m_scopeStack.empty());

  Scope &scope = m_scopeStack.back();

  if (scope.variables.find(declaration->name) != scope.variables.end()) {
    fprintf(stderr, "Error: variable '%s' already declared in current scope\n",
            declaration->name.c_str());
    return nullptr;
  }

  VariableStorage storage;
  if (isGlobalScope()) {
    storage = {VariableStorage::Kind::Global, m_globalLayout.allocateSlot()};
  } else {
    storage = {VariableStorage::Kind::Local, m_frameLayout.allocateSlot()};
  }


  const std::string name = declaration->name;
  // 符号对象共享持有：作用域、持久符号表、编译产物引用同一对象，
  // AST 节点的 resolved 指针在其任一持有者存活期间均有效
  auto symbol = std::make_shared<VariableSymbol>(
      std::move(declaration), storage, scope.depth);
  scope.variables.emplace(name, symbol);


  // 添加到持久化符号表
  // 持久表覆盖式登记：内层同名声明更新持久视图，与作用域解析一致
  m_persistentSymbolTable->addSymbol(name, symbol);


  // 根据作用域添加到相应的编译结果
  if (m_currentState.isActive && m_currentState.currentFunction) {
    m_currentState.currentFunction->symbols.addSymbol(name, symbol);
  } else if (isGlobalScope()) {
    m_compiledProgram->allSymbols.addSymbol(name, symbol);
  }

  return symbol.get();
}

bool FCSemanticContext::hasFunction(const std::string &functionName) const {
  return m_functionSet.find(functionName) != m_functionSet.end();
}

bool FCSemanticContext::registerFunction(const std::string &functionName, const FCFunctionAST* function) {
  if (hasFunction(functionName)) {
    fprintf(stderr, "Error: function '%s' already defined\n",
            functionName.c_str());
    return false;
  }

  auto compiledFunc = std::make_shared<CompiledFunction>();
  compiledFunc->ast = const_cast<FCFunctionAST*>(function);

  if (m_currentState.isActive && m_currentState.currentFunction) {
    compiledFunc->symbols = std::move(m_currentState.currentFunction->symbols);
    compiledFunc->frameSize = m_frameLayout.frameSize();
    compiledFunc->maxTempSlots = m_frameLayout.frameSize(); // Assuming maxTempSlots is the same as frame size for now
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



const CompiledProgram& FCSemanticContext::getCompiledProgram() const {
    return *m_compiledProgram;
}

const CompiledFunction*
FCSemanticContext::getCompiledFunction(const std::string& name) const {
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
      const auto &storage = symbol.storage;

      const char *storageKind =
          storage.kind == VariableStorage::Kind::Global ? "Global" : "Local";

      std::printf("  - %s: type=%s, storage=%s, slot=%d\n", decl->name.c_str(),
                  decl->typeName.c_str(), storageKind, storage.slot);
    }
  }
}


  // 添加符号（值版本：包装为共享持有后插入）
bool SymbolTable::addSymbol(const std::string& name, VariableSymbol symbol) {
  return addSymbol(name, std::make_shared<VariableSymbol>(std::move(symbol)));
}

// 添加符号（共享版本：插入或覆盖）
// 覆盖语义：同名后声明覆盖先声明，与作用域栈的内层遮蔽解析结果一致
bool SymbolTable::addSymbol(const std::string& name, std::shared_ptr<VariableSymbol> symbol) {
  bool inserted = m_symbols.find(name) == m_symbols.end();
  m_symbols.insert_or_assign(name, std::move(symbol));
  return inserted;
}

// 查找符号
const VariableSymbol* SymbolTable::lookup(const std::string& name) const {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? it->second.get() : nullptr;
}

VariableSymbol* SymbolTable::lookup(const std::string& name) {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? it->second.get() : nullptr;
}

// 移除符号
bool SymbolTable::removeSymbol(const std::string& name) {
  return m_symbols.erase(name) > 0;
}

// 获取所有符号
const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>>& SymbolTable::getAllSymbols() const {
  return m_symbols;
}

// 清空符号表
void SymbolTable::clear() {
  m_symbols.clear();
}

// 获取符号数量
size_t SymbolTable::size() const {
  return m_symbols.size();
}

// 调试输出
void SymbolTable::dump() const {
  printf("Symbol Table (%zu symbols):\n", m_symbols.size());
  for (const auto& [name, symbol] : m_symbols) {
    const char* kind =
      symbol->storage.kind == VariableStorage::Kind::Global ? "Global" : "Local";

    printf("  %s: slot=%d, kind=%s, depth=%d, mutable=%s\n",
      name.c_str(),
      symbol->storage.slot,
      kind,
      symbol->scopeDepth,
      symbol->isMutable ? "yes" : "no");
  }
}

} // namespace FCMarks
