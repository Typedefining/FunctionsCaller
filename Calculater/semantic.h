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

class FCFunctionFrame {
public:
  void reset() { m_nextSlot = 0; }

  int allocateSlot() { return m_nextSlot++; }
  int frameSize() const { return m_nextSlot; }

private:
  int m_nextSlot = 0;
};

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

  const std::unordered_map<std::string, VarDeclPtr>& currentScopeDeclarations() const;

  int currentFunctionFrameSize() const;
  int functionFrameSize(const std::string &functionName) const;

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
  std::set<std::string> m_functionSet;
  std::string m_currentFunctionName;
  FCFunctionFrame m_frame;
  std::unordered_map<std::string, int> m_functionFrameSizes;
};

} // namespace FCMarks
