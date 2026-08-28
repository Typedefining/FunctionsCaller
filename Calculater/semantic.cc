#include <cassert>
#include <cstdio>

#include "token.h"
#include "semantic.h"
#include <algorithm>

namespace FCMarks {

FCSemanticContext::FCSemanticContext() { reset(); }
FCSemanticContext::~FCSemanticContext() { m_scopeStack.pop_back(); } // Ensure the global scope is cleaned up

void FCSemanticContext::reset() {
  m_binopPrecedence = {{'=', 5},  {'<', 9},  {'+', 10},
                       {'-', 10}, {'*', 20}, {'/', 20}};
  m_scopeStack.clear();
  m_functionSet.clear();
  m_frame.reset();

  m_scopeStack.emplace_back(); // Global scope
  m_currentFuncScopeIdx = 0; // Reset the current function scope index
}

int FCSemanticContext::getOperatorPrecedence(char op) const {
  const auto it = m_binopPrecedence.find(op);
  return it == m_binopPrecedence.end() ? 0 : it->second;
}

void FCSemanticContext::pushScope() {
  m_scopeStack.emplace_back();
}

void FCSemanticContext::popScope() {
    assert(m_scopeStack.size() > 1);
    m_scopeStack.pop_back();
}

void FCSemanticContext::pushFunctionScope() {
  assert(m_currentFuncScopeIdx == 0);
  assert(m_scopeStack.size() == 1);

  m_currentFuncScopeIdx = m_scopeStack.size();
  m_scopeStack.emplace_back();

  m_frame.reset();
}

void FCSemanticContext::popFunctionScope() {
    assert(m_currentFuncScopeIdx != 0);
    assert(m_scopeStack.size() >= m_currentFuncScopeIdx);

    m_scopeStack.resize(m_currentFuncScopeIdx);
    m_currentFuncScopeIdx = 0;

    m_frame.reset();
}

VarDeclPtr
FCSemanticContext::lookupVariableDecl(const std::string &name) const {

  if (m_scopeStack.empty())
      return nullptr;

  const size_t totalScopes = m_scopeStack.size();
  const size_t searchCount = (m_currentFuncScopeIdx <= totalScopes - 1)
                                 ? (totalScopes - m_currentFuncScopeIdx)
                                 : totalScopes;

  auto ritBegin = m_scopeStack.rbegin();
  auto ritEnd = ritBegin + searchCount;

  auto found = std::find_if(ritBegin, ritEnd, [&](const auto &scope) {
    return scope.variables.find(name) != scope.variables.end();
  });

  if (found != ritEnd) {
    auto varIt = found->variables.find(name);
    return varIt != found->variables.end() ? varIt->second : nullptr;
  }

  //回退到全局作用域查找
  auto it = m_scopeStack[0].variables.find(name);
  return it != m_scopeStack[0].variables.end() ? it->second : nullptr;
}

VarDeclPtr
FCSemanticContext::lookupVariableInCurrentScope(const std::string &name) const {

  if (m_scopeStack.empty()) {
    return nullptr;
  }

  const auto &currentScope = m_scopeStack.back();

  auto varIt = currentScope.variables.find(name);
  return varIt != currentScope.variables.end() ? varIt->second : nullptr;
}

VarDeclPtr
FCSemanticContext::lookupGlobalVariable(const std::string &name) const {
    auto it = m_scopeStack.begin()->variables.find(name);
  return it != m_scopeStack.begin()->variables.end() ? it->second : nullptr;
}

void FCSemanticContext::insertVariableInCurrentScope(const std::string &name, VarDeclPtr declaration) {
  assert(declaration != nullptr);

  if (m_scopeStack.empty()) {
    fprintf(stderr, "Error: no active scope for variable '%s'\n", name.c_str());
    return;
  }

  auto &currentScope = m_scopeStack.back();

  if (currentScope.variables.find(name) != currentScope.variables.end()) {
    fprintf(stderr, "Error: variable '%s' already declared in current scope\n",
            name.c_str());
    return;
  }

  declaration->slot = m_frame.allocateSlot();
  currentScope.variables[name] = declaration;
}

void FCSemanticContext::insertGlobalVariable(const std::string &name,
                                             VarDeclPtr declaration) {
  assert(declaration != nullptr);

  if (m_scopeStack.begin()->variables.find(name) != m_scopeStack.begin()->variables.end()) {
    fprintf(stderr, "Error: global variable '%s' already declared\n",
            name.c_str());
    return;
  }

  declaration->slot = static_cast<int>(m_scopeStack[0].variables.size());
  m_scopeStack[0].variables[name] = declaration;
}

bool FCSemanticContext::hasFunction(const std::string &functionName) const {
  return m_functionSet.find(functionName) != m_functionSet.end();
}

bool FCSemanticContext::registerFunction(const std::string &functionName) {
  if (hasFunction(functionName)) {
    fprintf(stderr, "Error: function '%s' already defined\n",
            functionName.c_str());
    return false;
  }
  m_functionSet.insert(functionName);
  return true;
}

const std::unordered_map<std::string, VarDeclPtr> &
FCSemanticContext::currentScopeDeclarations() const {
  return m_scopeStack.back().variables;
}

int FCSemanticContext::currentFunctionFrameSize() const {
  return m_frame.frameSize();
}

int FCSemanticContext::functionFrameSize(const std::string &functionName) const {
  const auto it = m_functionFrameSizes.find(functionName);
  return it == m_functionFrameSizes.end() ? 0 : it->second;
}

void FCSemanticContext::dumpScopes() const {
  printf("=== Scope Debug Info ===\n");
  printf("Scope depth: %zu\n", m_scopeStack.size());
  printf("Current function frame size: %d\n", m_frame.frameSize());

  for (size_t i = 0; i < m_scopeStack.size(); ++i) {
    const auto &scope = m_scopeStack[i];

    printf(": %zu variables\n", scope.variables.size());
    for (const auto &varPair : scope.variables) {
      const auto &varDecl = varPair.second;
      printf("  - %s: type=%s, slot=%d\n", varDecl->name.c_str(),
             varDecl->typeName.c_str(), varDecl->slot);
    }
  }
}

} // namespace FCMarks
