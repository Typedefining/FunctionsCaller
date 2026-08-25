#include <cassert>
#include <cstdio>

#include "token.h"
#include "semantic.h"

namespace FCMarks {

FCSemanticContext::FCSemanticContext() { reset(); }

void FCSemanticContext::reset() {
  m_binopPrecedence = {{'=', 5},  {'<', 9},  {'+', 10},
                       {'-', 10}, {'*', 20}, {'/', 20}};
  m_scopeStack.clear();
  m_funcDeclList.clear();
  m_functionSet.clear();
  m_currentFunctionName.clear();

}

int FCSemanticContext::getOperatorPrecedence(char op) const {
  const auto it = m_binopPrecedence.find(op);
  return it == m_binopPrecedence.end() ? 0 : it->second;
}

void FCSemanticContext::pushScope() {
  m_scopeStack.emplace_back(Scope(m_currentFunctionName));
}

void FCSemanticContext::popScope() {
  if (m_scopeStack.size() > 1) {
    m_scopeStack.pop_back();
  }
}

void FCSemanticContext::pushFunctionScope(const std::string &functionName) {
  m_scopeStack.emplace_back(Scope(functionName));
  m_currentFunctionName = functionName;
}

void FCSemanticContext::popFunctionScope(const std::string &functionName) {
  if (m_scopeStack.size() > 1 &&
      m_scopeStack.back().functionName == functionName) {
    m_scopeStack.pop_back();
  }

  if (m_currentFunctionName == functionName) {
    m_currentFunctionName.clear();
  }
}

VarDeclPtr
FCSemanticContext::lookupVariableDecl(const std::string &functionName,
                                      const std::string &name) const {

  for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
    if (!functionName.empty() && it->functionName != functionName) {
      continue;
    }

    auto varIt = it->variables.find(name);
    if (varIt != it->variables.end()) {
      return varIt->second;
    }
  }

  return nullptr;
}

VarDeclPtr
FCSemanticContext::lookupVariableInCurrentScope(const std::string &functionName,
                                                const std::string &name) const {

  if (m_scopeStack.empty()) {
    return nullptr;
  }

  const auto &currentScope = m_scopeStack.back();

  if (!functionName.empty() && currentScope.functionName != functionName) {
    return nullptr;
  }

  auto varIt = currentScope.variables.find(name);
  if (varIt != currentScope.variables.end()) {
    return varIt->second;
  }

  return nullptr;
}

VarDeclPtr
FCSemanticContext::lookupGlobalVariable(const std::string &name) const {
  //Global Scope is always the first scope in the stack,
  //so we can directly look it up in m_scopeStack.begin() 

  auto it = m_scopeStack.begin()->variables.find(name);
  return it == m_scopeStack.begin()->variables.end() ? nullptr : it->second;
}

void FCSemanticContext::insertVariableInCurrentScope(
    const std::string &functionName, const std::string &name,
    VarDeclPtr declaration) {

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

  currentScope.variables[name] = declaration;

  if (!functionName.empty()) {
    m_funcDeclList[functionName].push_back(declaration);
  }

  declaration->scopeLevel = static_cast<int>(m_scopeStack.size()) - 1;
}

void FCSemanticContext::insertGlobalVariable(const std::string &name,
                                             VarDeclPtr declaration) {
  assert(declaration != nullptr);

  if (m_scopeStack.begin()->variables.find(name) != m_scopeStack.begin()->variables.end()) {
    fprintf(stderr, "Error: global variable '%s' already declared\n",
            name.c_str());
    return;
  }

  declaration->scopeLevel = 0;
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

std::vector<VarDeclPtr> &
FCSemanticContext::functionDeclarations(const std::string &functionName) {
  return m_funcDeclList[functionName];
}

const std::vector<VarDeclPtr> &
FCSemanticContext::functionDeclarations(const std::string &functionName) const {
  static const std::vector<VarDeclPtr> empty;
  const auto it = m_funcDeclList.find(functionName);
  return it == m_funcDeclList.end() ? empty : it->second;
}

void FCSemanticContext::dumpScopes() const {
  printf("=== Scope Debug Info ===\n");
  printf("Scope depth: %zu\n", m_scopeStack.size());
  printf("Active function: %s\n", m_currentFunctionName.c_str());

  for (size_t i = 0; i < m_scopeStack.size(); ++i) {
    const auto &scope = m_scopeStack[i];

    printf("scope.functionName =  (%s)", scope.functionName.c_str());
    printf(": %zu variables\n", scope.variables.size());
  }
}

} // namespace FCMarks