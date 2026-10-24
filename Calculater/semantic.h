#pragma once

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "diagnostic.h"
#include "symbol.h"

namespace FCExprClass
{
  struct FCExprAST;
  struct FCFunctionAST;
  struct FCProgramAST;
  struct FCBlockExprAST;
  struct FCSeqExprAST;
  struct FCVarDeclExprAST;
  struct FCVariableExprAST;
  struct FCForExprAST;
  struct FCIfExprAST;
  struct FCCallExprAST;
  struct FCBinaryExprAST;
  struct FCNumberExprAST;
  struct FCStringExprAST;
}

namespace FCMarks {

struct VariableSymbol;
struct CompiledFunction;
struct CompiledProgram;
struct SymbolTable;
using VarDeclPtr = FCExprClass::VarDeclPtr;

struct Scope {
    std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> variables;
    int depth = 0;
};

struct SemanticBinding;

class SlotLayout {
public:
    int allocateSlot() { return m_nextSlot++; }
    int size() const { return m_nextSlot; }
    void reset() { m_nextSlot = 0; }
private:
    int m_nextSlot = 0;
};

class FCSemanticContext {
public:
  FCSemanticContext();
  ~FCSemanticContext();
  void reset();

  void pushScope();
  void popScope();

  void pushFunctionScope();
  void popFunctionScope();

  std::shared_ptr<VariableSymbol> lookupVariableShared(const std::string &name) const;
  std::shared_ptr<VariableSymbol> lookupVariableInCurrentScope(const std::string &name) const;
  std::shared_ptr<VariableSymbol> lookupGlobalVariable(const std::string &name) const;

  std::shared_ptr<VariableSymbol> declareVariable(VarDeclPtr declaration);

  bool hasFunction(const std::string &functionName) const;

  using FCFunctionAST = FCExprClass::FCFunctionAST;
  bool registerFunction(const std::string &functionName, const FCFunctionAST* function);

  const CompiledProgram& getCompiledProgram() const;
  const CompiledFunction* getCompiledFunction(const std::string& name) const;

  const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> & currentScopeDeclarations() const;

  bool bindVariable(const void* astNode, std::shared_ptr<VariableSymbol> symbol);
  // 按 AST 节点查询绑定符号；后端（evaluator/codegen）调用
  std::shared_ptr<VariableSymbol> boundSymbol(const void* astNode) const;
  const SemanticBinding& semanticBinding() const;

  // 帧布局查询：锚点帧大小 / <global> 帧大小（布局对象存于 vector，不外露引用）
  int currentFrameLayoutSize() const;
  int currentGlobalLayoutSize() const;

  // 当前位置能否开始一个函数帧：关闭嵌套定义时只允许在 <global> 帧上定义。
  // 判据用帧基址而非作用域栈深度——顶层块里的 def 帧基址仍为 0。
  bool mayDefineFunctionHere() const;

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

  // 按 AST 遍历的语义分析器入口（步骤3：新增）
  class SemanticAnalyzer {
  public:
    SemanticAnalyzer(FCSemanticContext &ctx, FC::FCDiagnostics &diagnostics);

    // 分析整个程序 AST，返回是否成功
    bool analyze(const FCExprClass::FCExprAST *root);

  private:
    FCSemanticContext &m_ctx;
    FC::FCDiagnostics &m_diagnostics;

    // 语义错误统一入口：stage 固定 "semantic"，位置固定未知 {}（AST 节点无位置字段）
    void reportError(const std::string &message);

    bool analyzeExpr(const FCExprClass::FCExprAST *expr);

    bool analyzeProgram(const FCExprClass::FCProgramAST *node);
    bool analyzeBlock(const FCExprClass::FCBlockExprAST *node);
    bool analyzeSeq(const FCExprClass::FCSeqExprAST *node);
    bool analyzeVarDecl(const FCExprClass::FCVarDeclExprAST *node);
    bool analyzeVariable(const FCExprClass::FCVariableExprAST *node);
    bool analyzeFor(const FCExprClass::FCForExprAST *node);
    bool analyzeIf(const FCExprClass::FCIfExprAST *node);
    bool analyzeCall(const FCExprClass::FCCallExprAST *node);
    bool analyzeBinary(const FCExprClass::FCBinaryExprAST *node);
    bool analyzeFunction(const FCExprClass::FCFunctionAST *node);
    bool analyzeNumber(const FCExprClass::FCNumberExprAST *node);
    bool analyzeString(const FCExprClass::FCStringExprAST *node);

    // AST 分派访客：由 dispatchAST 驱动，12 个 visit 重载分别委托
    // 对应的 analyze* 方法；嵌套类可访问外层私有成员。
    struct DispatchVisitor {
      SemanticAnalyzer& a;
      explicit DispatchVisitor(SemanticAnalyzer& analyzer) : a(analyzer) {}

      bool visit(const FCExprClass::FCProgramAST& n)   { return a.analyzeProgram(&n); }
      bool visit(const FCExprClass::FCBlockExprAST& n) { return a.analyzeBlock(&n); }
      bool visit(const FCExprClass::FCSeqExprAST& n)   { return a.analyzeSeq(&n); }
      bool visit(const FCExprClass::FCVarDeclExprAST& n) { return a.analyzeVarDecl(&n); }
      bool visit(const FCExprClass::FCVariableExprAST& n) { return a.analyzeVariable(&n); }
      bool visit(const FCExprClass::FCForExprAST& n) { return a.analyzeFor(&n); }
      bool visit(const FCExprClass::FCIfExprAST& n) { return a.analyzeIf(&n); }
      bool visit(const FCExprClass::FCCallExprAST& n) { return a.analyzeCall(&n); }
      bool visit(const FCExprClass::FCBinaryExprAST& n) { return a.analyzeBinary(&n); }
      bool visit(const FCExprClass::FCFunctionAST& n) { return a.analyzeFunction(&n); }
      bool visit(const FCExprClass::FCNumberExprAST& n) { return a.analyzeNumber(&n); }
      bool visit(const FCExprClass::FCStringExprAST& n) { return a.analyzeString(&n); }
      // FCPrototypeAST 不作为顶层语句出现（解析器把它包进 FCFunctionAST）；
      // 手工构造的树遇到孤立原型时保持旧行为：视为已处理（no-op）。
      bool visit(const FCExprClass::FCPrototypeAST& n) { return true; }
    };

    std::shared_ptr<VariableSymbol> resolveVariable(const std::string &name);
  };

private:
  struct CurrentCompilationState {
    std::string functionName;
    bool isActive;

    CurrentCompilationState()
      : isActive(false) {}
  };

  CurrentCompilationState m_currentState;

  bool isInFunctionScope() const;
  bool isCurrentFrameGlobal() const;

  // 当前帧基址处的编译记录（<global> 记录或当前函数）
  CompiledFunction& currentFrameFunction() const;
  // 当前帧记录指针（注册时移交用）
  std::shared_ptr<CompiledFunction> currentFrameRecord() const;

  // 分析失败时撤销已登记的函数记录，避免半成品记录被后端当成可调用函数
  void discardFunction(const std::string &functionName);


private:
  std::vector<Scope> m_scopeStack;                                 // 可见性栈（块只动栈顶）
  size_t m_frameAnchor = 0;                                        // 当前函数帧基址（帧计数域）；0 = <global>
  // 外层帧基址与进入时作用域栈深（函数可在任意嵌套块位置定义；
  // 帧下标走帧计数域，与作用域栈深度解耦，避免 block scope 参与帧寻址）
  struct FrameAnchorEntry { size_t prevAnchor; size_t scopeDepthAtPush; };
  std::vector<FrameAnchorEntry> m_frameAnchorStack;
  std::vector<std::unique_ptr<SlotLayout>> m_frameLayouts;          // 每个函数帧一个布局
  std::vector<std::shared_ptr<CompiledFunction>> m_frameRecords;    // 每帧一个编译记录；注册时移交 functions 表
  std::set<std::string> m_functionSet;

  std::shared_ptr<CompiledProgram> m_compiledProgram = std::make_shared<CompiledProgram>();
};

} // namespace FCMarks

