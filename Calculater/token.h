#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace FCExprClass
{
  struct FCExprAST;
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
  tok_end,
};
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

struct VariableStorage {
  enum class Kind {
      Global,
      Local
  };
  Kind kind;
  int slot = -1;
};

struct VariableSymbol {
  using VarDeclPtr = std::shared_ptr<VarDecl>;
  VarDeclPtr declaration;      // 保留原始 AST 声明
  VariableStorage storage;     // 存储位置信息
  int scopeDepth;              // 声明时的作用域深度
  bool isMutable;              // 是否可变
  bool isCaptured;             // 是否被闭包捕获

  VariableSymbol()
    : declaration(nullptr)
    , storage{VariableStorage::Kind::Local, -1}
    , scopeDepth(0)
    , isMutable(false)
    , isCaptured(false) {}

  VariableSymbol(VarDeclPtr decl, VariableStorage st, int depth)
    : declaration(std::move(decl))
    , storage(st)
    , scopeDepth(depth)
    , isMutable(false)
    , isCaptured(false) {}
};


class SymbolTable {
public:
  SymbolTable() = default;

  // 添加符号（值版本：内部包装为共享持有后插入）
  bool addSymbol(const std::string& name, VariableSymbol symbol);
  // 添加符号（共享版本：与作用域/持久表共享同一 Symbol 对象）
  // 插入或覆盖：同名后声明覆盖先声明（内层遮蔽语义）
  bool addSymbol(const std::string& name, std::shared_ptr<VariableSymbol> symbol);
  // 查找符号
  const VariableSymbol* lookup(const std::string& name) const;
  VariableSymbol* lookup(const std::string& name);
  // 移除符号
  bool removeSymbol(const std::string& name);
  // 获取所有符号（符号对象为共享持有，拷贝 SymbolTable 不复制符号对象）
  const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>>& getAllSymbols() const;
  // 清空符号表
  void clear();
  // 获取符号数量
  size_t size() const;
  // 调试输出
  void dump() const;

private:
  std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> m_symbols;
};

struct CompiledFunction {
  using FCFunctionAST = FCExprClass::FCFunctionAST;
  FCFunctionAST* ast;
  std::vector<VariableStorage> parameters;
  SymbolTable symbols;
  int frameSize;
  int maxTempSlots;

  CompiledFunction()
    : ast(nullptr)
    , frameSize(0)
    , maxTempSlots(0) {}
};

struct CompiledProgram {
  SymbolTable allSymbols;
  std::unordered_map<std::string, std::shared_ptr<CompiledFunction>> functions;
  int globalFrameSize;

  CompiledProgram() : globalFrameSize(0) {}

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
struct FCVariableExprAST : public FCExprAST {
  VarDeclPtr decl;
  // 语义期绑定：指向唯一的 VariableSymbol（由 FCSemanticContext 共享持有）
  // 后端求值/生成直接读取 storage/slot，不再按名字查表
  const FCMarks::VariableSymbol* resolved = nullptr;

public:
  FCVariableExprAST(VarDeclPtr v);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::VariableExpr;
  void info() override;
};

/// FCBinaryExprAST - Expression struct for a binary operator.
struct FCBinaryExprAST : public FCExprAST {
  char m_Op;
  ::std::unique_ptr<FCExprAST> mup_LHS, mup_RHS;

public:
  FCBinaryExprAST(char op, ::std::unique_ptr<FCExprAST> lhs,
                  ::std::unique_ptr<FCExprAST> rhs);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::BinaryExpr;
  void info() override;
  char getOperator() const { return m_Op; }
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

/// FCFunctionAST - This struct represents a function definition itself.
struct FCFunctionAST : public FCExprAST {
  std::unique_ptr<FCPrototypeAST> mup_funcProto;
  std::unique_ptr<FCExprAST> mup_funcBody;
  int m_localCount = 0;

public:
  FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
                std::unique_ptr<FCExprAST> body, int localCount = 0);
  FCTypeDescribe type = FCMarks::FCTypeDescribe::Function;
  ::std::string getProtoName() const;
  const FCExprAST *getBody() const;
  const std::unique_ptr<FCPrototypeAST> &getProto() const {
    return mup_funcProto;
  }
  int getLocalCount() const { return m_localCount; }
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
  // 语义期绑定：循环变量对应的 VariableSymbol
  const FCMarks::VariableSymbol* resolved = nullptr;
  const FCMarks::VariableSymbol* getResolved() const { return resolved; }
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
  // 语义期绑定：声明对应的 VariableSymbol
  const FCMarks::VariableSymbol* resolved = nullptr;
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
