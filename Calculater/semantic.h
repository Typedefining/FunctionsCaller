#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace FCExprClass
{
  struct FCExprAST;
  struct FCFunctionAST;
}

namespace FCMarks {

struct VarDecl;
struct VariableSymbol;
struct VariableStorage;
struct CompiledFunction;
struct CompiledProgram;
struct SymbolTable;
using VarDeclPtr = std::shared_ptr<VarDecl>;


struct Scope {
    // 共享持有：同一 Symbol 对象同时被作用域与持久符号表引用，
    // 作用域弹出后符号仍存活（CompiledProgram/持久表持有）
    std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> variables;
    int depth = 0;
};

class FrameLayout {
public:
    int allocateSlot()
    {
        return m_nextSlot++;
    }

    int frameSize() const
    {
        return m_nextSlot;
    }

    void reset()
    {
        m_nextSlot = 0;
    }

private:
    int m_nextSlot = 0;
};

class GlobalLayout {
public:
    int allocateSlot()
    {
        return m_nextSlot++;
    }

    int globalSize() const
    {
        return m_nextSlot;
    }

    void reset()
    {
        m_nextSlot = 0;
    }

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

  const VariableSymbol* lookupVariable(const std::string &name) const;
  const VariableSymbol* lookupVariableInCurrentScope(const std::string &name) const;
  const VariableSymbol* lookupGlobalVariable(const std::string &name) const;

  VariableSymbol* declareVariable(VarDeclPtr declaration);

  bool hasFunction(const std::string &functionName) const;

  using FCFunctionAST = FCExprClass::FCFunctionAST;
  bool registerFunction(const std::string &functionName, const FCFunctionAST* function);

  const CompiledProgram& getCompiledProgram() const;
  const CompiledFunction* getCompiledFunction(const std::string& name) const;

  const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> & currentScopeDeclarations() const;

  const FrameLayout& currentFrameLayout() const { return m_frameLayout; }
  int currentFrameLayoutSize() const;
  const GlobalLayout& currentGlobalLayout() const { return m_globalLayout; }

  void dumpScopes() const;
  void dumpSymbolTable() const;
  void dumpCompiledProgram() const;

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

      FunctionScopeGuard(const FunctionScopeGuard &) = delete;
      FunctionScopeGuard &operator=(const FunctionScopeGuard &) = delete;
  private:
      FCSemanticContext &m_ctx;
  };

  ScopeGuard scopedScope() { return ScopeGuard(*this); }
  FunctionScopeGuard scopedFunction() { return FunctionScopeGuard(*this); }

private:
  struct CurrentCompilationState {
    std::string functionName;
    std::shared_ptr<CompiledFunction> currentFunction;
    bool isActive;

    CurrentCompilationState()
      : currentFunction(nullptr)
      , isActive(false) {}
  };

  CurrentCompilationState m_currentState;

  bool isInFunctionScope() const;
  bool isGlobalScope() const;


private:
  std::unordered_map<char, int> m_binopPrecedence;
  std::vector<Scope> m_scopeStack;
  std::set<std::string> m_functionSet;
  FrameLayout m_frameLayout;
  GlobalLayout m_globalLayout;

  std::shared_ptr<SymbolTable> m_persistentSymbolTable = std::make_shared<SymbolTable>();
  std::shared_ptr<CompiledProgram> m_compiledProgram = std::make_shared<CompiledProgram>();
};

} // namespace FCMarks
