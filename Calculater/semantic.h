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

// ---------------------------------------------------------------------------
// 语义绑定侧表：AST 保持纯语法结构，符号绑定关系由本表按节点指针索引。
// 生命周期约定：AST 与持有本表的对象（FCSemanticContext）必须同时存活。
// ---------------------------------------------------------------------------
struct SemanticBinding {
    // AST 节点 -> 符号（共享持有，保证侧表本身也参与符号生命周期）
    std::unordered_map<const void*, std::shared_ptr<VariableSymbol>> variables;

    // 登记：一个 AST 节点对应一个符号（重复登记视为错误返回 false）
    bool bind(const void* node, std::shared_ptr<VariableSymbol> symbol) {
        return variables.emplace(node, std::move(symbol)).second;
    }

    // 查询：返回符号裸指针；未绑定返回 nullptr
    const VariableSymbol* find(const void* node) const {
        auto it = variables.find(node);
        return it != variables.end() ? it->second.get() : nullptr;
    }

    void clear() { variables.clear(); }
    size_t size() const { return variables.size(); }
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

  // ---- 语义绑定侧表（AST 纯净，绑定关系存于此）----
  // 将 AST 节点与符号绑定；scanner 解析期调用
  bool bindVariable(const void* astNode, const VariableSymbol* symbol);
  // 按 AST 节点查询绑定符号；后端（evaluator/codegen）调用
  const VariableSymbol* boundSymbol(const void* astNode) const;
  // 侧表整体（供后端 Context 引用）
  const SemanticBinding& semanticBinding() const { return m_binding; }

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
  SemanticBinding m_binding;
  std::shared_ptr<CompiledProgram> m_compiledProgram = std::make_shared<CompiledProgram>();
};

} // namespace FCMarks
