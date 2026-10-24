#pragma once

// AST 节点定义：纯语法树结构与节点分派。
// 本头文件由原 token.h 拆分而来（2026-10 结构与职责重构 Task 1）：
//   token.h  —— 词法类别枚举与二元运算符映射
//   ast.h    —— AST 节点家族（本文件）
//   symbol.h —— 符号表 / 编译记录 / 绑定侧表
// 运行时值表示（FCValue/Frame）在 runtime.h，不依赖本文件。

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "token.h"

namespace FCMarks {
struct VariableSymbol;
}

namespace FCExprClass {
using VarDeclPtr = std::shared_ptr<struct VarDecl>;

// 参数/变量声明：名字 + 类型名（类型检查在语义期执行，此处只承载文本）
struct VarDecl {
  std::string name;
  std::string typeName;

  VarDecl(const std::string &n, const std::string &t) : name(n), typeName(t) {}
};
} // namespace FCExprClass

namespace FCMarks {
enum struct FCTypeDescribe {
  Expr,         // 抽象表达式
  NumberExpr,   // 数字字面量
  StringExpr,   // 字符串字面量
  VariableExpr, // 对象标识
  BinaryExpr,   // 二元表达式
  CallExpr,     // 可调用对象表达式
  Prototype,    // 函数原型表达式,弃用
  Function      // 函数定义
};
} // namespace FCMarks

namespace FCExprClass {
using namespace FCMarks;
struct FCExprAST {
public:
  FCExprAST() = default;
  virtual ~FCExprAST() = default;
  virtual void info() = 0;
  FCTypeDescribe type = FCTypeDescribe::Expr;
};
struct FCNumberExprAST : public FCExprAST {
  int m_intVal;
  double m_doubleVal;
  bool m_isFloating = false;

public:
  FCNumberExprAST(int val);
  FCNumberExprAST(double val);
  bool isFloating() const { return m_isFloating; }
  FCTypeDescribe type = FCMarks::FCTypeDescribe::NumberExpr;
  void info() override;
};

struct FCStringExprAST : public FCExprAST {
  std::string m_stringVal;

public:
  FCStringExprAST(std::string val);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::StringExpr;
  void info() override;
};

/// FCVariableExprAST - Expression struct for referencing a variable, like "a".
/// 纯语法节点：只保留变量名，语义绑定由 SemanticAnalyzer 通过 SemanticBinding 完成。
struct FCVariableExprAST : public FCExprAST {
  std::string name;

public:
  explicit FCVariableExprAST(std::string varName);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::VariableExpr;
  void info() override;
};

/// FCBinaryExprAST - Expression struct for a binary operator.
struct FCBinaryExprAST : public FCExprAST {
  FCBinaryOp m_Op;
  ::std::unique_ptr<FCExprAST> mup_LHS, mup_RHS;

public:
  FCBinaryExprAST(FCBinaryOp op, ::std::unique_ptr<FCExprAST> lhs,
                  ::std::unique_ptr<FCExprAST> rhs);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::BinaryExpr;
  void info() override;
  FCBinaryOp getOperator() const { return m_Op; }
  const FCExprAST *getLHS() const { return mup_LHS.get(); }
  const FCExprAST *getRHS() const { return mup_RHS.get(); }
};

/// FCCallExprAST - Expression struct for function calls.
struct FCCallExprAST : public FCExprAST {
  std::string m_callee;
  std::vector<std::unique_ptr<FCExprAST>> m_args;

public:
  FCCallExprAST(const std::string &callee,
                std::vector<std::unique_ptr<FCExprAST>> args);
  const ::std::string &getName() const;
  const std::vector<std::unique_ptr<FCExprAST>> &getArgs() const;
  FCTypeDescribe type = FCMarks::FCTypeDescribe::CallExpr;
  void info() override;
};

struct FCPrototypeAST : public FCExprAST {
  std::string m_funcName;
  std::vector<VarDeclPtr> m_funcArgsVar;

public:
  FCPrototypeAST(const std::string &name, std::vector<VarDeclPtr> args);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::Prototype;

  void info() override;
  ::std::string getProtoName() const;
  const std::vector<VarDeclPtr> &getArgs() const {
    return m_funcArgsVar;
  }
};

/// FCFunctionAST - 纯语法节点：原型 + 函数体。帧布局（参数/局部计数）
/// 是语义期产物，记录在 CompiledFunction::frameSize，不由 AST 携带。
struct FCFunctionAST : public FCExprAST {
  std::unique_ptr<FCPrototypeAST> mup_funcProto;
  std::unique_ptr<FCExprAST> mup_funcBody;

public:
  FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
                std::unique_ptr<FCExprAST> body);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::Function;
  ::std::string getProtoName() const;
  const FCExprAST *getBody() const;
  const std::unique_ptr<FCPrototypeAST> &getProto() const {
    return mup_funcProto;
  }
  void info() override;
};

class FCIfExprAST : public FCExprAST {
  std::unique_ptr<FCExprAST> Cond, Then, Else;

public:
  FCIfExprAST(std::unique_ptr<FCExprAST> Cond, std::unique_ptr<FCExprAST> Then,
              std::unique_ptr<FCExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  void info() override;
  const FCExprAST *getCondition() const { return Cond.get(); }
  const FCExprAST *getThen() const { return Then.get(); }
  const FCExprAST *getElse() const { return Else.get(); }
};

class FCForExprAST : public FCExprAST {
  VarDeclPtr decl;
  std::unique_ptr<FCExprAST> Start, End, Step, Body;

public:
  FCForExprAST(VarDeclPtr decl, std::unique_ptr<FCExprAST> start,
               std::unique_ptr<FCExprAST> end, std::unique_ptr<FCExprAST> step,
               std::unique_ptr<FCExprAST> body)
      : decl(std::move(decl)), Start(std::move(start)), End(std::move(end)),
        Step(std::move(step)), Body(std::move(body)) {}

  void info() override;
  const VarDeclPtr &getDecl() const { return decl; }
  const FCExprAST *getStart() const { return Start.get(); }
  const FCExprAST *getEnd() const { return End.get(); }
  const FCExprAST *getStep() const { return Step.get(); }
  const FCExprAST *getBody() const { return Body.get(); }
};

struct FCSeqExprAST : public FCExprAST {
  std::vector<std::unique_ptr<FCExprAST>> exprs;
  FCSeqExprAST(std::vector<std::unique_ptr<FCExprAST>> e)
      : exprs(std::move(e)) {}

  void info() override {
    for (auto &i : exprs) {
      i->info();
    }
  }
  const std::vector<std::unique_ptr<FCExprAST>> &getExpressions() const {
    return exprs;
  }
};

struct FCVarDeclExprAST : public FCExprAST {
public:
  VarDeclPtr decl;
  std::unique_ptr<FCExprAST> initExpr; // 初始化表达式
  FCVarDeclExprAST(VarDeclPtr d, std::unique_ptr<FCExprAST> init)
      : decl(std::move(d)), initExpr(std::move(init)) {}
  void info() override;
};

class FCProgramAST : public FCExprAST {
private:
  std::vector<std::unique_ptr<FCExprAST>> m_statements;

public:
  FCProgramAST(std::vector<std::unique_ptr<FCExprAST>> statements)
      : m_statements(std::move(statements)) {}

  void info() override {
    std::cout << "FCProgramAST with " << m_statements.size()
              << " statements:\n";
    for (auto &stmt : m_statements) {
      stmt->info();
    }
  }

  const std::vector<std::unique_ptr<FCExprAST>> &getStatements() const {
    return m_statements;
  }
};

class FCBlockExprAST : public FCExprAST {
  std::vector<std::unique_ptr<FCExprAST>> m_expressions;

public:
  explicit FCBlockExprAST(std::vector<std::unique_ptr<FCExprAST>> exprs)
      : m_expressions(std::move(exprs)) {}

  const std::vector<std::unique_ptr<FCExprAST>> &getExpressions() const {
    return m_expressions;
  }

  void info() override {
    std::cout << "FCBlockExprAST with " << m_expressions.size()
              << " expressions" << std::endl;
  }
};

/// 唯一节点分派点：Visitor 需提供 12 个 const 成员重载
/// visit(const FCProgramAST&)、visit(const FCBlockExprAST&)、visit(const FCSeqExprAST&)、
/// visit(const FCVarDeclExprAST&)、visit(const FCVariableExprAST&)、visit(const FCForExprAST&)、
/// visit(const FCIfExprAST&)、visit(const FCCallExprAST&)、visit(const FCBinaryExprAST&)、
/// visit(const FCFunctionAST&)、visit(const FCNumberExprAST&)、visit(const FCStringExprAST&)。
/// 新增 AST 节点而访客缺重载 ⇒ 编译失败（穷尽性由重载决议保证）。
/// FCPrototypeAST 不走此分派（独立顶层节点由消费方自行处理）。
template <class Visitor>
decltype(auto) dispatchAST(const FCExprAST& node, Visitor& v) {
  if (auto* n = dynamic_cast<const FCProgramAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCBlockExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCSeqExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCPrototypeAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCVarDeclExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCVariableExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCForExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCIfExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCCallExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCBinaryExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCFunctionAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCNumberExprAST*>(&node)) return v.visit(*n);
  if (auto* n = dynamic_cast<const FCStringExprAST*>(&node)) return v.visit(*n);
  // 兜底：节点类型未登记进分派（新增节点类型时漏改本模板即命中此处）
  throw std::runtime_error("dispatchAST: unhandled node type");
}
} // namespace FCExprClass
