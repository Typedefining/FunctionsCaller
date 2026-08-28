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

struct Scope {
  std::unordered_map<std::string, VarDeclPtr> variables;
};

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
  ~FCSemanticContext();
  void reset();

  int getOperatorPrecedence(char op) const;

  void pushScope();
  void popScope();

  void pushFunctionScope();
  void popFunctionScope();

  VarDeclPtr lookupVariableDecl(const std::string &name) const;
  VarDeclPtr lookupVariableInCurrentScope(const std::string &name) const;
  VarDeclPtr lookupGlobalVariable(const std::string &name) const;

  void insertVariableInCurrentScope(const std::string &name, VarDeclPtr declaration);
  void insertGlobalVariable(const std::string &name, VarDeclPtr declaration);

  bool hasFunction(const std::string &functionName) const;
  bool registerFunction(const std::string &functionName);

  const std::unordered_map<std::string, VarDeclPtr>& currentScopeDeclarations() const;

  bool isInFunctionScope() const { return m_currentFuncScopeIdx != 0; }
  const FCFunctionFrame& currentFunctionFrame() const { return m_frame; }
  int currentFunctionFrameSize() const;
  int functionFrameSize(const std::string &functionName) const;

  void dumpScopes() const;

public:
    class ScopeGuard {
    public:
        explicit ScopeGuard(FCSemanticContext &ctx)
            : m_ctx(ctx) {
            m_ctx.pushScope();
        }

        ~ScopeGuard() {
            m_ctx.popScope();
        }

        ScopeGuard(const ScopeGuard &) = delete;
        ScopeGuard &operator=(const ScopeGuard &) = delete;

    private:
        FCSemanticContext &m_ctx;
    };
  class FunctionScopeGuard {
  public:
      FunctionScopeGuard(FCSemanticContext &ctx)
          : m_ctx(ctx) {
          m_ctx.pushFunctionScope();
      }

      ~FunctionScopeGuard() {
          m_ctx.popFunctionScope();
      }

  private:
      FCSemanticContext &m_ctx;
  };

  ScopeGuard scopedScope() { return ScopeGuard(*this); }
  FunctionScopeGuard scopedFunction() { return FunctionScopeGuard(*this); }

  private:
  std::unordered_map<char, int> m_binopPrecedence;
  std::vector<Scope> m_scopeStack;
  std::set<std::string> m_functionSet;
  size_t m_currentFuncScopeIdx = 0;
  FCFunctionFrame m_frame;
  std::unordered_map<std::string, int> m_functionFrameSizes;
};

} // namespace FCMarks
