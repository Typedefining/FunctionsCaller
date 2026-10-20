#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "runtime.h"

namespace FCExprClass
{
  struct FCExprAST;
  struct FCVariableExprAST;
  struct FCFunctionAST;
}

namespace FCMarks {
enum struct FCToken {
  tok_begin = 0,
  tok_eof = 1,
  tok_def = 2,
  tok_identifier = 3,
  tok_number = 4,
  tok_string = 5,
  tok_if = 6,
  tok_then = 7,
  tok_else = 8,
  tok_for = 9,
  tok_in = 10,
  tok_var = 11,
  //(
  tok_parenthes_open = 12,
  //)
  tok_parenthes_close = 13,
  //{
  tok_brace_open = 14,
  //}
  tok_brace_close = 15,
  tok_comma = 16,
  tok_assign,
  tok_colon,
  tok_semicolon,
  // 运算符与字面量标点一样，以枚举值表示，不在 Token::kind 中携带 ASCII 码
  tok_less,
  tok_plus,
  tok_minus,
  tok_star,
  tok_slash,
  // 词法错误：lexeme 携带可直接展示给用户的错误说明
  tok_error,
};

// 二元运算符的统一表示：Parser 依据 FCToken 映射后写入 AST，后端全程使用，杜绝 ASCII 泄露
enum struct FCBinaryOp {
  Assign,   // =
  Less,     // <
  Add,      // +
  Sub,      // -
  Mul,      // *
  Div,      // /
  Unknown   // 非法运算符（错误路径）
};

// kind → 运算符的唯一映射点：Token 只表达词法类别，运算符含义由解析层从 kind 推得
inline FCBinaryOp binaryOpOf(FCToken kind) {
  switch (kind) {
  case FCToken::tok_assign: return FCBinaryOp::Assign;
  case FCToken::tok_less:   return FCBinaryOp::Less;
  case FCToken::tok_plus:   return FCBinaryOp::Add;
  case FCToken::tok_minus:  return FCBinaryOp::Sub;
  case FCToken::tok_star:   return FCBinaryOp::Mul;
  case FCToken::tok_slash:  return FCBinaryOp::Div;
  default:                  return FCBinaryOp::Unknown;
  }
}
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

struct VarDecl {
  std::string name;
  std::string typeName;

  VarDecl(const std::string &n, const std::string &t) : name(n), typeName(t) {}
};

struct VariableSymbol {
  using VarDeclPtr = std::shared_ptr<VarDecl>;
  VarDeclPtr declaration;
  int slot;                    // 所属域内的槽位：全局域对应 globalSymbols 下标，函数域对应帧内下标
  int scopeDepth;              // 声明时的作用域深度
  bool isMutable;              // 是否可变
  bool isCaptured;             // 是否被闭包捕获
  VariableRuntimeInfo runtime; // 运行时定位：-1 为全局，>=0 为调用栈帧下标

  VariableSymbol()
    : declaration(nullptr)
    , slot{-1}
    , scopeDepth(0)
    , isMutable(false)
    , isCaptured(false) {}

  VariableSymbol(VarDeclPtr decl, int st, int depth)
    : declaration(std::move(decl))
    , slot(st)
    , scopeDepth(depth)
    , isMutable(false)
    , isCaptured(false) {}
};


class SymbolTable {
public:
  SymbolTable() = default;

  bool addSymbol(const std::string& name, std::shared_ptr<VariableSymbol> symbol);

  std::shared_ptr<VariableSymbol> lookupShared(const std::string& name) const;
  bool removeSymbol(const std::string& name);

  const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>>& getAllSymbols() const;
  void clear();
  size_t size() const;
  void dump() const;

private:
  std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> m_symbols;
};

struct CompiledFunction {
  using FCFunctionAST = FCExprClass::FCFunctionAST;
  FCFunctionAST* ast;
  std::vector<int> parametersIdx;  // 形参槽位，按原型声明顺序，语义期填充
  SymbolTable symbols;
  int frameSize;
  int maxTempSlots;

  CompiledFunction()
    : ast(nullptr)
    , frameSize(0)
    , maxTempSlots(0) {}
};

struct SemanticBinding {
    std::unordered_map<const void*, std::shared_ptr<VariableSymbol>> variables;

    bool bind(const void* node, std::shared_ptr<VariableSymbol> symbol) {
        return variables.emplace(node, std::move(symbol)).second;
    }

    std::shared_ptr<VariableSymbol> find(const void* node) const {
        auto it = variables.find(node);
        return it != variables.end() ? it->second : nullptr;
    }

    std::shared_ptr<VariableSymbol> find(const FCExprClass::FCVariableExprAST* node) const {
        return find(static_cast<const void*>(node));
    }

    void clear() { variables.clear(); }
    size_t size() const { return variables.size(); }
};

struct CompiledProgram {
  std::shared_ptr<SemanticBinding> m_binding = std::make_shared<SemanticBinding>();
  std::unordered_map<std::string, std::shared_ptr<CompiledFunction>> functions;
  std::vector<std::shared_ptr<VariableSymbol>> globalSymbols;  // 按全局槽位顺序，替代中心化名称表

  CompiledProgram() = default;

  const CompiledFunction* getFunction(const std::string& name) const {
      auto it = functions.find(name);
      return it != functions.end() ? it->second.get() : nullptr;
  }
};

} // namespace FCMarks

namespace FCExprClass {
using namespace FCMarks;
using VarDeclPtr = std::shared_ptr<VarDecl>;
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
} // namespace FCExprClass
