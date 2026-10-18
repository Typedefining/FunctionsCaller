# FunctionsCaller 架构设计与模块设计分析

> 基于分支 `refactor/var-symbol-binding` 的重构完成版本  
> 状态：所有模块已实现，597 测试通过，覆盖率 95.24%  
> 主要变更：扁平化变量存储模型，基于 `shared_ptr<VariableSymbol>` 符号身份寻址

---

## 1. 项目概述

**FunctionsCaller** 是一个玩具级编译器/解释器工具链，包含词法分析（Scanner）、语义分析（Semantic）、中间代码生成（Codegen，LLVM IR）和运行时求值（Evaluator）四个阶段。支持整数、浮点、字符串类型，支持变量声明与赋值、函数定义与调用、if/for 控制流、块级作用域，以及全局/局部变量的符号绑定与寻址。

本次重构的核心目标是：**扁平化变量存储模型** —— 删除 `VariableStorage(kind+slot)` 结构，改为 `VariableSymbol` 的纯 `int slot`（所属域内下标）+ `VariableRuntimeInfo.frameIndex`（运行时定位，-1=全局，>=0=帧下标）；删除 `CompiledProgram::allSymbols` 中心化符号表，改为 `std::vector<shared_ptr<VariableSymbol>> globalSymbols`；解释器与 codegen 按 `shared_ptr<VariableSymbol>` 符号身份寻址（AST 节点指针绑定）。

---

## 2. 整体架构设计

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────────┐
│  用户入口 / 测试驱动                                          │
│  main.cc / unit_test.cc                                       │
├─────────────────────────────────────────────────────────────┤
│  前端阶段 (Frontend)                                          │
│  scanner.h/cc  —— 词法分析 + 递归下降语法分析                 │
│  token.h       —— AST 节点定义 + VariableSymbol 数据结构      │
├─────────────────────────────────────────────────────────────┤
│  语义分析阶段 (Semantic)                                      │
│  semantic.h/cc —— 作用域管理、符号绑定、类型推断、frameIndex  │
├─────────────────────────────────────────────────────────────┤
│  运行时求值阶段 (Evaluator)                                   │
│  evaluator.h/cc —— AST 树遍历求值、变量读写、函数调用栈       │
├─────────────────────────────────────────────────────────────┤
│  代码生成阶段 (Codegen)                                       │
│  codegen.h/cc  —— LLVM IR 生成、全局/局部变量分配、函数编译   │
├─────────────────────────────────────────────────────────────┤
│  运行时基础 (Runtime)                                         │
│  runtime.h     —— FCValue、Frame、VariableRuntimeInfo 等基础类型│
└─────────────────────────────────────────────────────────────┘
           ▲                              ▲
           │                              │
  SemanticBinding (AST节点↔符号)    LLVM 18.1.6 (后端目标)
```

### 2.2 模块依赖关系

```
scanner.cc  →  token.h, semantic.h, runtime.h
semantic.cc →  token.h, semantic.h, runtime.h
evaluator.cc→  evaluator.h, token.h, semantic.h, runtime.h
codegen.cc  →  codegen.h, token.h, semantic.h, runtime.h, LLVM IR
unit_test.cc→  所有上述头文件
```

### 2.3 数据流向

```
源代码字符串
    │
    ▼
FCScanner::analysis() ─────▶ FCExprAST (抽象语法树)
    │                          │
    │                          ├─▶ semantic.analyze() ──▶ CompiledProgram
    │                          │                          (globalSymbols, functions, m_binding)
    │                          │
    │                          ├─▶ evaluator.evaluate() ──▶ FCValue (运行时结果)
    │                          │      通过 compiledProgram.m_binding 查找符号
    │                          │      通过 frameForSymbol() 定位帧
    │                          │
    │                          └─▶ codegen.codegen() ──▶ LLVM Module (IR)
    │                                 通过 compiledProgram.globalSymbols 声明全局变量
    │                                 通过 namedValues/globalValues 分配存储
    │
    ▼
测试断言 / 覆盖率统计
```

---

## 3. 模块设计分析

### 3.1 Runtime 模块 (`runtime.h`)

**职责**：定义运行时基础数据类型，为解释器和代码生成提供统一的值表示与帧模型。

**核心数据结构**：

| 结构体 | 字段 | 说明 |
|--------|------|------|
| `FCValueCategory` | `Integer`, `Floating`, `String`, `Dangle` | 值类型枚举 |
| `FCValueUnion` | `intVal`, `doubleVal`, `charVal`, `danglingVal` | 值联合体 |
| `VariableRuntimeInfo` | `frameIndex = -1` | 运行时定位：-1=全局，>=0=调用栈帧下标 |
| `FCValue` | `type`, `evaluteVal` | 运行时值 |
| `Frame` | `funcName`, `locals` (vector<FCValue>) | 调用栈帧，按 slot 下标存放值 |

**关键设计决策**：
- `Frame.locals` 统一用于全局帧（`callStack[0]`）和局部帧（`callStack.back()`），按 `symbol->slot` 下标访问，无需区分全局/局部存储类型。

---

### 3.2 Token 模块 (`token.h`)

**职责**：定义所有 AST 节点类、Token 枚举、符号表、编译程序模型。

**核心数据结构**：

| 结构/类 | 职责 |
|---------|------|
| `VarDecl` | 变量声明：name, typeName |
| `VariableSymbol` | **重构核心**：`declaration`、`slot`（域内下标）、`scopeDepth`、`isMutable`、`isCaptured`、`runtime`（`frameIndex`） |
| `SymbolTable` | 函数局部符号表，按名称查找 `VariableSymbol` |
| `CompiledFunction` | `ast`、`parametersIdx`（形参槽位，按原型声明顺序）、`symbols`（局部符号表）、`frameSize`、`maxTempSlots` |
| `SemanticBinding` | AST 节点指针 → `shared_ptr<VariableSymbol>` 的映射（身份绑定） |
| `CompiledProgram` | `m_binding`（语义绑定表）、`functions`（函数表）、**`globalSymbols`**（全局符号向量，替代旧 `allSymbols`） |

**AST 节点层次**：
```
FCExprAST (抽象基类, virtual info())
├── FCNumberExprAST    (数字字面量)
├── FCStringExprAST    (字符串字面量)
├── FCVariableExprAST  (变量引用, 绑定到 VariableSymbol)
├── FCBinaryExprAST    (二元运算, 包含赋值 '=')
├── FCCallExprAST      (函数调用)
├── FCPrototypeAST     (函数原型)
├── FCFunctionAST      (函数定义, 包含 proto + body)
├── FCIfExprAST        (if-then-else)
├── FCForExprAST       (for 循环, 包含 decl/start/end/step/body)
├── FCSeqExprAST       (序列表达式)
├── FCVarDeclExprAST   (变量声明+初始化)
├── FCProgramAST       (顶层程序, 语句列表)
└── FCBlockExprAST     (块表达式 { ... })
```

**重构变更点**：
- `VariableSymbol`：删除 `kind` 字段，保留纯 `int slot`；新增 `VariableRuntimeInfo runtime`（`frameIndex`）。
- `CompiledProgram`：删除 `allSymbols`（中心化名称表），新增 `std::vector<shared_ptr<VariableSymbol>> globalSymbols`。
- `CompiledFunction`：`parameters` 重命名为 `parametersIdx`（`vector<int>`）。

---

### 3.3 Scanner 模块 (`scanner.h/cc`)

**职责**：词法分析 + 递归下降语法分析，将源代码字符串转换为 `FCExprAST` 抽象语法树，同时执行语义绑定（变量声明、作用域进入/退出）。

**核心流程**：
```
analysis(code)
  ├─ 词法扫描 (getNextToken / getTok)
  ├─ 语法分析 (parseExpression → parseBinOpRHS → parsePrimary → ...)
  │   ├─ parseDefinition()  → def 函数定义
  │   ├─ ParseVarExpr()     → var 变量声明 (调用 semantic.declareVariable)
  │   ├─ parseBlockExpr()   → { } 块 (调用 scopedScope())
  │   ├─ ParseForExpr()     → for 循环
  │   └─ ParseIfExpr()      → if 表达式
  └─ 返回 FCExprAST 根节点
```

**与语义阶段的交互**：
- `FCScanner` 内部持有 `FCMarks::FCSemanticContext m_semanticContext`。
- 遇到 `var` 声明时调用 `m_semanticContext.declareVariable(decl)`，得到 `VariableSymbol`，再通过 `bindVariable(declExpr.get(), symbol)` 将 AST 节点与符号绑定。
- 块 `{` `}` 通过 `scopedScope()` RAII 管理作用域进入/退出。

**未覆盖行**：`scanner.cc:546` (`parseBlockExpr` 中的 `tok_brace_open` 检查分支) —— 需要通过空块 `{}` 测试覆盖。

---

### 3.4 Semantic 模块 (`semantic.h/cc`)

**职责**：作用域管理、符号声明与查找、全局/局部变量区分、`frameIndex` 赋值、编译程序模型构建。

**核心方法**：

| 方法 | 职责 |
|------|------|
| `declareVariable(VarDeclPtr)` | 声明变量，分配 slot，设置 `runtime.frameIndex`（-1=全局，0=局部），绑定到当前作用域 |
| `lookupVariableShared(name)` | 全链路查找（当前作用域 → 全局） |
| `lookupVariableInCurrentScope(name)` | 仅当前作用域查找 |
| `lookupGlobalVariable(name)` | 全局作用域查找 |
| `pushScope()` / `popScope()` | 块级作用域栈操作 |
| `pushFunctionScope()` / `popFunctionScope()` | 函数作用域操作 |
| `isGlobalScope()` | **重构修复**：由 `scopeStack.size()==1 && !isActive` 改为 `!isActive`，修复顶层块 `{ var g=1; }` 中 var 被误判为局部的 bug |
| `bindVariable(astNode, symbol)` | AST 节点 → 符号身份绑定 |
| `boundSymbol(astNode)` | 按 AST 节点查找绑定符号 |

**`isGlobalScope()` 修复（关键bug修复）**：
```cpp
// 旧：scopeStack.size()==1 && !isActive  —— 顶层块 { } 会使 size>1，导致顶层 var 误判为局部
// 新：!isActive                         —— 仅判断是否在函数内部
```

**`declareVariable` 中 `frameIndex` 赋值（关键修复）**：
```cpp
symbol->runtime.frameIndex = isGlobalScope() ? -1 : 0;
```

**未覆盖行**：`semantic.cc:92` (`lookupVariableInCurrentScope` 中 `m_scopeStack.empty()` 检查分支)。

---

### 3.5 Evaluator 模块 (`evaluator.h/cc`)

**职责**：运行时求值，按 AST 树结构递归计算表达式值，管理函数调用栈和变量读写。

**核心数据结构**：`FCEvaluationContext`
- `functions` (`FCFunctionRegistry`)：函数注册与查找
- `callStack` (`vector<Frame>`)：调用栈，`callStack[0]` 为全局帧
- `compiledProgram`：语义阶段输出的编译程序模型
- `localBinding`：本地符号绑定表（手工绑定场景）

**关键方法**：
- `boundSymbol(astNode)`：按 AST 节点指针从 `compiledProgram.m_binding` 或 `localBinding` 查找 `shared_ptr<VariableSymbol>`
- `frameForSymbol(symbol)`：按 `symbol.runtime.frameIndex == -1` 判断全局（返回 `callStack[0]`），否则返回 `callStack.back()`
- `evaluate(expression, context)`：入口，分发到具体的 `evaluateXxx`

**各 `evaluateXxx` 函数与未覆盖行对应关系**：

| 函数 | 未覆盖行 | 触发条件 |
|------|---------|---------|
| `evaluateAssignment` | 174-176 | `boundSymbol` 返回 nullptr（符号未绑定） |
| `evaluateAssignment` | 179-184 | `slot < 0 || slot >= frame->locals.size()`（slot 越界） |
| `evaluateBinary` | 213-215 | `default:` 分支（未知操作符） |
| `evaluateFor` | 88-89 | `slot < 0 || slot >= locals.size()`（for 变量 slot 越界） |
| `evaluateFor` | 66-72 | `frameIndex == -1`（顶层 for 变量，返回 Dangle） |

**重构变更点**：
- 变量读写按 `symbol->slot` 索引 `frame->locals`，不再通过 `storage.kind`/`storage.slot`。
- `evaluateCall` 中使用 `frame.locals[symbol->slot]`（修复了未定义变量 `slot` 的编译错误）。
- `evaluateFor` 新增 `frameIndex == -1` 早期返回，避免在全局帧操作循环变量。

---

### 3.6 Codegen 模块 (`codegen.h/cc`)

**职责**：将 `FCExprAST` 编译为 LLVM IR，生成可验证的 Module，支持全局变量声明、局部变量 alloca、函数定义与调用。

**核心数据结构**：`FCCodegenContext`
- `namedValues` (`map<shared_ptr<VariableSymbol>, AllocaInst*>`)：局部变量符号 → alloca 指令
- `globalValues` (`map<shared_ptr<VariableSymbol>, GlobalVariable*>`)：全局变量符号 → 全局变量
- `currentFunction`：当前正在编译的 LLVM Function
- `compiledProgram`：语义阶段输出

**关键代码生成函数**：

| 函数 | 职责 | 未覆盖行 |
|------|------|---------|
| `codegenProgram(FCProgramAST*)` | 顶层程序：声明全局变量、预声明函数、编译函数体、生成 `__fc_main` | 785, 819 |
| `codegenStandaloneTopLevel(FCExprAST*)` | 顶层独立表达式：声明全局、生成 main 函数 | — |
| `codegenFunction(FCFunctionAST*)` | 函数体：创建 entry block、分配参数 alloca、编译 body、verify | 458, 480-483, 494-497, 518-530 |
| `codegenDeclaration(FCVarDeclExprAST*)` | 变量声明：按 `frameIndex == -1` 决定全局/局部分配 | — |
| `inferExprType()` | 表达式类型推断（用于 codegen 类型检查） | 50, 115-125, 127 |
| `codegenVariable(FCVariableExprAST*)` | 变量加载：先查 `namedValues`，再查 `globalValues` | — |
| `codegenBinary('=')` | 赋值代码生成：查找地址后创建 Store | — |

**重构变更点（关键）**：
```cpp
// codegenDeclaration 中的分支修正（原分支颠倒）：
if (declSymbol->runtime.frameIndex == -1) {
    // 全局：globalValues + declareGlobal
} else {
    // 局部：namedValues + createEntryBlockAlloca
}

// FCVarDeclExprAST 分发修正：
if (context.currentFunction != nullptr)
    return codegenDeclaration(declaration, context);  // 函数内局部
return codegenStandaloneTopLevel(declaration, context);  // 顶层全局
```

**`declareGlobalsInExpression` 简化**：移除了未使用的 `const FCExprAST* expression` 参数，直接遍历 `context.compiledProgram.globalSymbols`。

**未覆盖行**：`inferExprType` 中的空表达式、`number` 非浮点、`string`、`binary '='`、`call` 未找到定义、`conditional` 双分支非 double、`for`、`block` 空、`sequence` 空、`declaration` 无 decl、`function` 等分支；`codegenFunction` 中的参数缺失、符号缺失、body 为空、return 类型不匹配、verify 失败等错误路径；`codegenProgram` 中的 `mainFunction` 已非空、`runtimeStatements` 为空等分支。

---

## 4. 核心设计模式

### 4.1 符号身份绑定 (Symbol Identity Binding)

```cpp
// SemanticBinding: AST 节点指针 → shared_ptr<VariableSymbol>
struct SemanticBinding {
    std::unordered_map<const void*, std::shared_ptr<VariableSymbol>> variables;
    bool bind(const void* node, std::shared_ptr<VariableSymbol> symbol);
    std::shared_ptr<VariableSymbol> find(const void* node) const;
    std::shared_ptr<VariableSymbol> find(const FCVariableExprAST* node) const;
};
```

**设计意图**：变量不再通过名称在中心化表中查找，而是通过 AST 节点指针直接绑定到符号对象。这消除了名称查找的歧义，支持同名变量在不同作用域中绑定到不同的 `VariableSymbol` 实例。

### 4.2 扁平化变量寻址

```cpp
// VariableSymbol: 纯 int slot + runtime.frameIndex
struct VariableSymbol {
    int slot;                          // 所属域内下标
    VariableRuntimeInfo runtime;       // frameIndex: -1=全局, >=0=帧下标
};

// FCEvaluationContext::frameForSymbol
Frame *frameForSymbol(const VariableSymbol &symbol) {
    if (symbol.runtime.frameIndex == -1) return &callStack[0];  // 全局帧
    return &callStack.back();                                     // 局部帧
}
```

**访问模式**：`frame->locals[symbol->slot]` —— 无论全局还是局部，都通过 `slot` 下标在对应帧的 `locals` 向量中读写。

### 4.3 作用域栈与 RAII 守卫

```cpp
class ScopeGuard {  // 构造 pushScope，析构 popScope
    explicit ScopeGuard(FCSemanticContext &ctx);
};
class FunctionScopeGuard {  // 构造 pushFunctionScope，析构 popFunctionScope
    explicit FunctionScopeGuard(FCSemanticContext &ctx);
};
```

**使用场景**：
- `parseBlockExpr()` 使用 `scopedScope()` 管理块作用域。
- 函数定义使用 `scopedFunction()` 管理函数作用域。

---

## 5. 执行流程详解

### 5.1 语义分析流程

```
源代码
  │
  ▼
FCScanner::analysis()
  │
  ├─▶ parseVarExpr() 遇到 "var x:int = 5;"
  │     │
  │     ├─▶ semantic.declareVariable(decl)
  │     │     ├─▶ 查找是否重复声明
  │     │     ├─▶ 分配 slot (FrameLayout.allocateSlot() 或 GlobalLayout.allocateSlot())
  │     │     ├─▶ 设置 symbol->runtime.frameIndex = isGlobalScope() ? -1 : 0
  │     │     └─▶ 插入到当前 Scope.variables 或 globalSymbols
  │     │
  │     └─▶ semantic.bindVariable(declExpr.get(), symbol)  // AST 节点绑定
  │
  ├─▶ parseBlockExpr() 遇到 "{"
  │     └─▶ scopedScope() → pushScope() ... popScope()
  │
  └─▶ 返回 FCExprAST 根节点
```

### 5.2 运行时求值流程

```
evaluate(ast, context)
  │
  ├─ evaluateVariable(node, context)
  │     ├─ symbol = boundSymbol(node)        // 按 AST 节点查符号
  │     ├─ frame = frameForSymbol(*symbol)   // 按 frameIndex 定位帧
  │     └─ return frame->locals[symbol->slot] // 按 slot 读取值
  │
  ├─ evaluateAssignment(expr, context)
  │     ├─ symbol = boundSymbol(variable)
  │     ├─ frame = frameForSymbol(*symbol)
  │     └─ frame->locals[symbol->slot] = value  // 按 slot 写入值
  │
  ├─ evaluateCall(expr, context)
  │     ├─ function = functions.findFunction(name)
  │     ├─ context.pushFrame(name)
  │     ├─ 绑定参数到 frame->locals[parametersIdx[i]]
  │     ├─ result = evaluate(body, context)
  │     └─ context.popFrame()
  │
  └─ evaluateFor(node, context)
        ├─ 若 forSymbol->runtime.frameIndex == -1 → return Dangle  // 顶层 for 变量保护
        ├─ frameIdx = callStack.size() - 1
        ├─ frame->locals[slot] = start
        └─ while (end != 0) { body; frame->locals[slot] += step; }
```

### 5.3 代码生成流程

```
codegen(program, context)
  │
  ├─ declareGlobalsInExpression(context)
  │     └─ 遍历 globalSymbols, 调用 declareGlobal(symbol, context)
  │
  ├─ 遍历语句：
  │     ├─ FCFunctionAST → codegen(function, context)
  │     │     ├─ declareFunction() 创建 LLVM Function 声明
  │     │     ├─ 为每个参数创建 alloca, 存入 namedValues[paramSymbol]
  │     │     ├─ codegen(body) 生成函数体 IR
  │     │     └─ verifyFunction()
  │     │
  │     └─ 非函数语句 → 收集到 runtimeStatements
  │
  ├─ 生成 __fc_main 函数
  │     ├─ 遍历 runtimeStatements, codegen(statement, context)
  │     └─ CreateRet(lastValue)
  │
  └─ return mainFunction
```

---

## 6. 变量存储模型重构对比

| 维度 | 旧模型 (VariableStorage) | 新模型 (VariableSymbol + frameIndex) |
|------|------------------------|-------------------------------------|
| 变量标识 | `VariableStorage { kind, slot }` | `VariableSymbol { slot, runtime.frameIndex }` |
| 全局/局部判据 | `storage.kind == Global/Local` | `runtime.frameIndex == -1 / >= 0` |
| 符号表 | `CompiledProgram::allSymbols` (中心化名称表) | `CompiledProgram::globalSymbols` (向量) + 函数局部 `CompiledFunction::symbols` |
| 变量查找 | 按名称 `bindNode(name)` | 按 AST 节点指针 `bindNode(node, sym)` |
| 顶层块 `{ var g=1; }` | `isGlobalScope()` 由 `scopeStack.size()==1 && !isActive` 决定 → **误判为局部** | `isGlobalScope()` 由 `!isActive` 决定 → **正确为全局** |
| frameIndex | 不存在 | `declareVariable` 中赋值：全局 -1，局部 0 |
| 形参槽位 | `parameters` | `parametersIdx` (vector<int>) |

---

## 7. 测试覆盖率分析

### 7.1 当前覆盖率

| 文件 | 覆盖率 | 行数 | 未覆盖行 |
|------|--------|------|---------|
| token.cc | 100.00% | 94/94 | 无 |
| scanner.cc | 99.03% | 410/414 | 546 (parseBlockExpr tok_brace_open 分支) |
| semantic.cc | 96.67% | 174/180 | 92 (lookupVariableInCurrentScope empty 检查) |
| evaluator.cc | 96.22% | 280/291 | 89 (evaluateFor slot 越界), 175-176, 182, 184 (assignment 错误分支), 214-215 (binary 默认操作符) |
| codegen.cc | 90.92% | 541/595 | inferExprType 各分支, codegenFunction 错误路径, codegenProgram 边界条件 |
| **Overall** | **95.24%** | **1499/1574** | — |

### 7.2 未覆盖代码的测试建议

| 未覆盖行 | 建议测试用例 |
|---------|-------------|
| scanner.cc:546 | 空块 `{}` 作为表达式 |
| semantic.cc:92 | 在空作用域栈上调用 lookupVariableInCurrentScope |
| evaluator.cc:89 | 构造 for 表达式，symbol->slot 越界 |
| evaluator.cc:175-176 | 赋值给未绑定符号的变量 |
| evaluator.cc:182-184 | 赋值给 slot 越界的符号 |
| evaluator.cc:214-215 | 二元表达式使用未知操作符 (如 `?`) |
| codegen.cc:50, 115-125, 127 | 直接构造各类 AST 节点调用 inferExprType |
| codegen.cc:458-530 | 构造各种导致 codegenFunction 错误的函数定义 |
| codegen.cc:785, 819 | 构造触发 codegenProgram 边界条件的程序 |

---

## 8. 设计决策与权衡

### 8.1 为什么删除 `VariableStorage(kind+slot)`？

**原因**：`kind` 字段（Global/Local）与 `frameIndex` 信息冗余。`VariableRuntimeInfo.frameIndex` 已经可以区分全局（-1）和局部（>=0），无需额外的 `kind` 字段。扁平化后，变量只需要 `slot`（域内下标）+`frameIndex`（运行时帧定位）两个信息即可完成所有寻址。

### 8.2 为什么删除 `CompiledProgram::allSymbols`？

**原因**：中心化的名称→符号映射在存在作用域遮蔽时会产生歧义（同名变量在不同作用域应绑定不同符号）。改为 `globalSymbols` 向量（仅全局变量）+ 函数局部 `CompiledFunction::symbols` 符号表，使得全局变量有统一的线性布局，局部变量在函数内部独立管理。

### 8.3 为什么按 `shared_ptr<VariableSymbol>` 符号身份寻址？

**原因**：AST 节点指针是唯一的、不可变的标识符。通过 `SemanticBinding` 将 AST 节点与 `VariableSymbol` 绑定，解释器和 codegen 都可以直接按符号身份定位变量，避免了在运行时重新进行名称查找和作用域解析。

### 8.4 `isGlobalScope()` 修复的影响

**旧逻辑**：`scopeStack.size()==1 && !isActive` —— 当进入顶层块 `{` 时，`scopeStack.size()` 变为 2，导致块内的 `var` 被误判为局部变量，frameIndex 被设为 0 而非 -1，导致顶层块内的全局变量被错误地放入局部帧。

**新逻辑**：`!isActive` —— 仅判断是否在函数内部，顶层块内的 `var` 正确被识别为全局变量，frameIndex = -1，存入全局帧 `callStack[0]`。

---

## 9. 模块接口契约总结

### 9.1 Scanner → Semantic

```cpp
FCScanner::analysis(const string& code) → unique_ptr<FCExprAST>
// 内部：调用 semanticContext.declareVariable()、bindVariable()
```

### 9.2 Semantic → Evaluator/Codegen

```cpp
FCSemanticContext::getCompiledProgram() → const CompiledProgram&
// 包含：globalSymbols, functions, m_binding (AST节点→符号)

// Evaluator:
FCEvaluationContext(const CompiledProgram& program)
// 通过 compiledProgram.m_binding 查找符号

// Codegen:
FCCodegenContext(const string& moduleName, const CompiledProgram& program)
// 通过 compiledProgram.globalSymbols 声明全局变量
// 通过 compiledProgram.m_binding 查找符号
```

### 9.3 Evaluator 内部契约

```cpp
boundSymbol(astNode) → shared_ptr<VariableSymbol>  // 按 AST 节点查符号
frameForSymbol(symbol) → Frame*                    // 按 frameIndex 定位帧
frame->locals[symbol->slot]                        // 按 slot 读写值
```

---

## 10. 后续改进建议

1. **闭包捕获支持**：当前 `isCaptured` 字段已预留，但 `frameIndex >= 0` 的捕获变量定位尚未实现（TODO 注释已标注）。
2. **覆盖率提升**：codegen.cc 仍有约 9% 未覆盖，主要在错误路径和类型推断分支，可通过直接构造 AST 节点调用 `inferExprType` 和 `codegenFunction` 的错误分支来提升。
3. **scanner.cc:546**：添加空块 `{}` 测试用例可将 scanner 覆盖率提升至 100%。
4. **语义模块**：添加空作用域栈下的查找测试可将 semantic 覆盖率提升。

---

> 文档生成时间：2025-06-20  
> 基于分支：`refactor/var-symbol-binding` (HEAD: `94325cf`)  
> 测试状态：597/597 通过，覆盖率 95.24%