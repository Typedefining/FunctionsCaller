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

  m_binding.clear();
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

std::shared_ptr<VariableSymbol> FCSemanticContext::lookupVariableShared(const std::string &name) const {
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
  if (m_scopeStack.empty())
    return nullptr;

  auto &globalScope = m_scopeStack.front();

  auto it = globalScope.variables.find(name);

  if (it == globalScope.variables.end())
    return nullptr;

  return it->second;
}

std::shared_ptr<VariableSymbol> FCSemanticContext::declareVariable(VarDeclPtr declaration) {
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
  auto symbol = std::make_shared<VariableSymbol>(
      std::move(declaration), storage, scope.depth);
  scope.variables.emplace(name, symbol);

  if (m_currentState.isActive && m_currentState.currentFunction) {
    m_currentState.currentFunction->symbols.addSymbol(name, symbol);
  } else if (isGlobalScope()) {
    m_compiledProgram->allSymbols.addSymbol(name, symbol);
  }

  return symbol;
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

bool SymbolTable::addSymbol(const std::string& name, std::shared_ptr<VariableSymbol> symbol) {
  bool inserted = m_symbols.find(name) == m_symbols.end();
  if (!inserted)
  {
    //不允许重定义同名符号
    return false;
  }
  m_symbols.insert_or_assign(name, std::move(symbol));
  return inserted;
}

std::shared_ptr<VariableSymbol> SymbolTable::lookupShared(const std::string& name) const {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? it->second : nullptr;
}

bool SymbolTable::removeSymbol(const std::string& name) {
  return m_symbols.erase(name) > 0;
}

const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>>& SymbolTable::getAllSymbols() const {
  return m_symbols;
}

void SymbolTable::clear() {
  m_symbols.clear();
}

size_t SymbolTable::size() const {
  return m_symbols.size();
}

bool FCSemanticContext::bindVariable(const void* astNode, std::shared_ptr<VariableSymbol> symbol) {
  if (astNode == nullptr || symbol == nullptr)
    return false;

  return m_binding.bind(astNode, std::move(symbol));
}

std::shared_ptr<VariableSymbol> FCSemanticContext::boundSymbol(const void* astNode) const {
  return m_binding.find(astNode);
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
