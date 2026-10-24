#pragma once

// 符号表 / 编译记录 / 绑定侧表：语义期的数据模型。
// 本头文件由原 token.h 拆分而来（2026-10 结构与职责重构 Task 1）。
// 分层：token.h ← ast.h ← symbol.h（本文件）；semantic.h 在其上。

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.h"

namespace FCMarks {

struct VariableRuntimeInfo {
  int frameIndex = -1;
};

struct VariableSymbol {
  using VarDeclPtr = FCExprClass::VarDeclPtr;
  VarDeclPtr declaration;
  int slot;                    // 所属域内的槽位：全局域对应 globalSymbols 下标，函数域对应帧内下标
  int scopeDepth;              // 声明时的作用域深度
  VariableRuntimeInfo runtime; // 运行时定位：-1 为全局，>=0 为调用栈帧下标

  VariableSymbol()
    : declaration(nullptr)
    , slot{-1}
    , scopeDepth(0) {}

  VariableSymbol(VarDeclPtr decl, int st, int depth)
    : declaration(std::move(decl))
    , slot(st)
    , scopeDepth(depth) {}
};


class SymbolTable {
public:
  SymbolTable() = default;

  bool addSymbol(const std::string& name, std::shared_ptr<VariableSymbol> symbol) {
    bool inserted = m_symbols.find(name) == m_symbols.end();
    if (!inserted) {
      // 不允许重定义同名符号
      return false;
    }
    m_symbols.insert_or_assign(name, std::move(symbol));
    return inserted;
  }

  std::shared_ptr<VariableSymbol> lookupShared(const std::string& name) const {
    auto it = m_symbols.find(name);
    return it != m_symbols.end() ? it->second : nullptr;
  }

  bool removeSymbol(const std::string& name) {
    return m_symbols.erase(name) > 0;
  }

  const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>>& getAllSymbols() const {
    return m_symbols;
  }

  void clear() { m_symbols.clear(); }

  size_t size() const { return m_symbols.size(); }

  // 调试输出；定义在 semantic.cc（printf 输出，非 inline）
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

  CompiledFunction()
    : ast(nullptr)
    , frameSize(0) {}
};

struct SemanticBinding {
    std::unordered_map<const void*, std::shared_ptr<VariableSymbol>> variables;

    // 生命周期契约：键是 AST 节点地址。AST 必须存活到本侧表销毁为止；
    // AST 先销毁会使键悬挂（查无结果），但不得复用地址后再绑定。
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
