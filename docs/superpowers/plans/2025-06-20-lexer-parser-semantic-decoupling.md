# FunctionsCaller 词法/语法/语义解耦改动方案

> 基于分支 `refactor/var-symbol-binding` 的后续重构计划  
> 目标：将词法分析（Lexer）、语法分析（Parser）、语义分析（Semantic）三个阶段分隔为独立模块，各阶段均可独立测试、替换，依赖方向单向。  
> 状态：方案草案，尚未落地代码

---

## 1. 问题现状

`Calculater/scanner.h/cc` 中的 `FCScanner` 类同时承担：

| 职责 | 体现 |
|------|------|
| 词法分析 | `getNextToken()`、`getTok()`、`getTokPrecedence()`，状态 `m_curTok`、`m_lastChar`、`m_idx`、`m_identifierStr` 等 |
| 语法分析 | `parseExpression()`、`parsePrimary()`、`parseBinOpRHS()`、`parseDefinition()`、`ParseIfExpr()`、`ParseForExpr()`、`ParseVarExpr()`、`parseBlockExpr()` 等 |
| 语义分析 | 持有 `FCSemanticContext m_semanticContext`，在解析过程中直接调用 `declareVariable()`、`bindVariable()`、`lookupVariableShared()`、`scopedScope()`、`scopedFunction()` |

**耦合症状**：
1. 语法方法直接操作词法层私有状态（`m_curTok`、`getNextToken()`），无 Token 流抽象接口。
2. 语法阶段直接执行语义操作（`lookupVariableShared`、`bindVariable`），AST 节点携带语义对象（`FCVariableExprAST(varSym->declaration)`）。
3. 词法、语法、语义三者绑定在同一个类，无法独立单元测试，也无法替换任一阶段。

---

## 2. 目标架构

```
源代码字符串
     │
     ▼
┌─────────────┐
│   Lexer     │  字符流 → Token 流（独立可测）
│  lexer.h/cc │
└─────────────┘
     │ Token[]
     ▼
┌─────────────┐
│   Parser    │  Token 流 → 纯语法 AST（独立可测）
│  parser.h/cc │
└─────────────┘
     │ 纯 AST (FCExprAST)
     ▼
┌─────────────┐
│ Semantic    │  AST → 符号绑定 + frameIndex + CompiledProgram（独立可测）
│ Analyzer    │
│ semantic.h/cc│
└─────────────┘
     │ CompiledProgram + SemanticBinding
     ▼
┌─────────────┐
│ Evaluator / │  消费语义阶段输出，按 symbol 身份寻址（已完成，无需重构）
│   Codegen   │
└─────────────┘
```

依赖方向：Lexer ← Parser ← SemanticAnalyzer ← Evaluator/Codegen，单向，无循环。

---

## 3. 文件变更清单

| 文件 | 动作 | 内容 |
|------|------|------|
| `Calculater/lexer.h` | 新增 | `FC::Lexer` 类：`nextToken()`/`peekToken()`/`currentToken()`/`isAtEnd()`，搬移 `getTok()` 逻辑 |
| `Calculater/lexer.cc` | 新增 | 词法扫描实现（跳过空白、标识符/关键字、数字、字符串、注释、标点） |
| `Calculater/parser.h` | 新增 | `FC::Parser` 类：依赖注入 `Lexer&`，`parseProgram()` 及各 `parse*` 纯语法方法 |
| `Calculater/parser.cc` | 新增 | 语法分析实现，**不调用任何语义方法**，`FCVariableExprAST` 只存 `std::string name` |
| `Calculater/semantic.h` | 调整 | 新增 `FCMarks::SemanticAnalyzer` 类，`analyze(AST)` 入口，暴露 `binding()` / `program()` |
| `Calculater/semantic.cc` | 调整 | `SemanticAnalyzer` 遍历 AST，完成 `declareVariable`、`frameIndex` 赋值、`SemanticBinding` 绑定 |
| `Calculater/token.h` | 调整 | AST 节点纯化：`FCVariableExprAST` 去掉 `VarDecl declaration`，改为 `std::string name` |
| `Calculater/scanner.h` | 调整 | 退化为 Facade，内部组合 `Lexer+Parser+SemanticAnalyzer`，对外保持 `analysis(std::string)` |
| `Calculater/scanner.cc` | 调整 | Facade 实现，按顺序驱动三阶段，不再直接包含全部 parse/lex 逻辑 |
| `docs/superpowers/plans/` | 新增 | 本方案持久化为 `2025-06-20-lexer-parser-semantic-decoupling.md` |

---

## 4. 核心接口设计

### 4.1 Lexer（独立可测试）

```cpp
// lexer.h
namespace FC {
struct Token { int kind; std::string lexeme; int line; int column; };

class Lexer {
public:
    explicit Lexer(std::string input);
    void reset(const std::string& input);
    Token nextToken();
    Token peekToken() const;
    Token currentToken() const;
    bool isAtEnd() const;
private:
    std::string m_input;
    std::string::iterator m_idx;
    int m_lastChar;
    int m_curLine, m_curColumn;
    Token m_curToken;
    mutable Token m_peekToken;
    mutable bool m_hasPeek = false;
    Token getTok();
    char readChar();
    void skipWhitespace();
};
}
```

独立测试示例：`Lexer("123 + abc;")` → `tok_number("123")`、`'+'("+")`、`tok_identifier("abc")`、`';'(";")`。

### 4.2 Parser（独立可测试，纯语法）

```cpp
// parser.h
namespace FC {
class Parser {
public:
    explicit Parser(Lexer& lexer);
    std::unique_ptr<FCExprClass::FCExprAST> parseProgram();
private:
    Lexer& m_lexer;
    Token m_curTok;
    void advance();
    // 纯语法方法，不触及符号表、作用域、frameIndex
    std::unique_ptr<FCExprAST> parseExpression();
    std::unique_ptr<FCExprAST> parsePrimary();
    std::unique_ptr<FCExprAST> parseIdentifierExpr(); // 只创建 FCVariableExprAST(name)
    std::unique_ptr<FCExprAST> parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprAST> LHS);
    std::unique_ptr<FCPrototypeAST> parsePrototype();
    std::unique_ptr<FCFunctionAST> parseDefinition();
    std::unique_ptr<FCExprAST> parseSeqExpr();
    std::unique_ptr<FCExprAST> parseIfExpr();
    std::unique_ptr<FCExprAST> parseForExpr();
    std::unique_ptr<FCExprAST> parseVarDeclExpr();   // 只创建 FCVarDeclExprAST(decl, init)
    std::unique_ptr<FCExprAST> parseBlockExpr();
};
}
```

独立测试示例：`Lexer("x + 1;")` → `Parser` → `FCBinaryExprAST('+', FCVariableExprAST("x"), FCNumberExprAST(1))`。

### 4.3 SemanticAnalyzer（独立可测试）

```cpp
// semantic.h 新增
namespace FCMarks {
class SemanticAnalyzer {
public:
    explicit SemanticAnalyzer(CompiledProgram& program);
    void analyze(FCExprClass::FCExprAST* root);
    const SemanticBinding& binding() const;
    CompiledProgram& program();
private:
    CompiledProgram& m_program;
    FCSemanticContext m_ctx;      // 复用现有作用域栈/符号表逻辑
    SemanticBinding m_binding;    // AST节点指针 -> shared_ptr<VariableSymbol>
    // 按节点类型分派遍历
    void visitProgram(FCProgramAST* node);
    void visitBlock(FCBlockExprAST* node);
    void visitVarDecl(FCVarDeclExprAST* node);
    void visitVariable(FCVariableExprAST* node);
    void visitFor(FCForExprAST* node);
    void visitFunction(FCFunctionAST* node);
    void visitBinary(FCBinaryExprAST* node);
    // ...
};
}
```

独立测试示例：手工构造 `var g:int = 1; { var g:int = 2; }` AST → `analyze()` → 验证外层 `g.frameIndex == -1`（全局），块内 `g.frameIndex == 0`（局部），两个 `g` 绑定不同 `VariableSymbol`。

### 4.4 Facade（保持向后兼容）

```cpp
// scanner.h 调整
class FCScanner {
public:
    FCScanner();
    void resetState();
    std::unique_ptr<FCExprClass::FCExprAST> analysis(const std::string& input);
    FC::Lexer& lexer();
    FC::Parser& parser();
    FCMarks::SemanticAnalyzer& semanticAnalyzer();
    const CompiledProgram& compiledProgram() const;
private:
    std::unique_ptr<FC::Lexer> m_lexer;
    std::unique_ptr<FC::Parser> m_parser;
    std::unique_ptr<FCMarks::SemanticAnalyzer> m_semantic;
    CompiledProgram m_program;
};
```

Evaluator/Codegen 无需改动，继续消费 `CompiledProgram` + `SemanticBinding`，按 `symbol->slot` 和 `symbol->runtime.frameIndex` 寻址。

---

## 5. 数据流

```
Facade::analysis(input)
  │
  ├─▶ Lexer::reset(input)          // 字符流 → Token 流
  │
  ├─▶ Parser(lexer).parseProgram() // Token 流 → 纯语法 AST
  │
  └─▶ SemanticAnalyzer(program).analyze(ast.root())
        ├─▶ 遍历 AST，遇到 var/for/def → m_ctx.declareVariable()
        ├─▶ 设置 symbol->runtime.frameIndex = isGlobalScope() ? -1 : 0
        ├─▶ m_binding.bind(astNodePtr, symbol)
        └─▶ 输出 CompiledProgram（globalSymbols + m_binding）
```

---

## 6. 迁移路径（分步执行，不破坏现有测试）

| 步骤 | 动作 | 验证 |
|------|------|------|
| 1 | 新建 `lexer.h/cc`，将 `scanner.cc:getTok()` 及其状态变量搬移 | 原有 `FCScanner` 继续编译通过 |
| 2 | 新建 `parser.h/cc`，将 `scanner.cc:parse*` 搬移，**去掉**对 `m_semanticContext` 的调用 | 单独编译 Parser，Mock 输入验证 AST 结构 |
| 3 | `semantic.h/cc` 新增 `SemanticAnalyzer`，实现 `analyze()` 遍历 AST 并完成符号绑定与 `frameIndex` 赋值 | 手工构造 AST，验证全局/局部 frameIndex 正确 |
| 4 | `scanner.h/cc` 改为 Facade，组合 Lexer+Parser+SemanticAnalyzer，对外保持 `analysis()` | 原有 `unit_test.cc` 597 测试全部通过，覆盖率 ≥ 95% |
| 5 | 删除 `scanner.h` 中对 `semantic.h` 的直接头文件依赖（facade 内部已组合） | 编译无循环依赖 |
| 6 | 为 Lexer、Parser、SemanticAnalyzer 各自编写独立单元测试 | 三阶段可独立运行测试 |

---

## 7. 验收标准

1. **词法独立测试**：可在不实例化 Parser/Semantic 的情况下，对 `Lexer` 的 `nextToken()`/`peekToken()` 进行单元测试。
2. **语法独立测试**：可注入 Mock `Lexer`，对 `Parser` 的 `parseProgram()` 及各 `parse*` 方法进行单元测试，验证 AST 结构。
3. **语义独立测试**：可手工构造纯语法 AST，对 `SemanticAnalyzer::analyze()` 进行单元测试，验证 `frameIndex` 和 `SemanticBinding`。
4. **回归测试**：原有 `unit_test.cc` 597 个测试全部通过，覆盖率 ≥ 95%（当前 95.24%）。
5. **依赖单向**：`Lexer` 不依赖 `Parser`/`Semantic`；`Parser` 不依赖 `Semantic`；`SemanticAnalyzer` 不依赖 `Lexer`/`Parser` 实现细节。

---

## 8. 预计工作量

- 新增代码：`lexer.h/cc` (~200 行)、`parser.h/cc` (~400 行)
- 调整代码：`semantic.h/cc` (~150 行新增 SemanticAnalyzer)、`scanner.h/cc` (~100 行 Facade)、`token.h` (~10 行 AST 纯化)
- 新增文档：本文件（持久化方案）
- 测试：各阶段独立单元测试（根据现有 `unit_test.cc` 风格补充）

---

> 方案作者：AI Agent（基于当前 `refactor/var-symbol-binding` 分支代码分析）  
> 关联提交：当前 HEAD 为覆盖率达标版本（597/597 通过，95.24% 覆盖率），本方案为后续解耦阶段