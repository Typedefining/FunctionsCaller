#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace FCMarks {

struct VarDecl;
using VarDeclPtr = std::shared_ptr<VarDecl>;

class FCSemanticContext {
public:
  FCSemanticContext();
  void reset();

  int getOperatorPrecedence(char op) const;

  void pushScope();
  void popScope();

  void pushFunctionScope(const std::string &functionName);
  void popFunctionScope(const std::string &functionName);

  VarDeclPtr lookupVariableDecl(const std::string &functionName,
                                const std::string &name) const;
  VarDeclPtr lookupVariableInCurrentScope(const std::string &functionName,
                                          const std::string &name) const;
  VarDeclPtr lookupGlobalVariable(const std::string &name) const;

  void insertVariableInCurrentScope(const std::string &functionName,
                                    const std::string &name,
                                    VarDeclPtr declaration);
  void insertGlobalVariable(const std::string &name, VarDeclPtr declaration);

  bool hasFunction(const std::string &functionName) const;
  bool registerFunction(const std::string &functionName);

  std::vector<VarDeclPtr> &
  functionDeclarations(const std::string &functionName);
  const std::vector<VarDeclPtr> &
  functionDeclarations(const std::string &functionName) const;

  const std::unordered_map<std::string, VarDeclPtr>& scopeDeclarations() const { return m_scopeStack.back().variables; }

  void dumpScopes() const;

private:
  struct Scope {
    std::string functionName;
    std::unordered_map<std::string, VarDeclPtr> variables;

    Scope(const std::string &fn = "")
        : functionName(fn) {}
  };

  std::unordered_map<char, int> m_binopPrecedence;
  std::vector<Scope> m_scopeStack;
  std::unordered_map<std::string, std::vector<VarDeclPtr>> m_funcDeclList;
  std::vector<VarDeclPtr> m_scopeDeclList;
  std::set<std::string> m_functionSet;
  std::string m_currentFunctionName;
};

} // namespace FCMarks