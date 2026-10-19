# FunctionsCaller 项目架构信息

> 基准分支：`refactor/var-symbol-binding`（HEAD `b8ce4b1`）  
> 文档日期：2026-10-19  
> 当前状态：词法/语法/语义三阶段解耦完成，全量 694/694 测试通过，生产代码行覆盖率 95.11%  
> 语言标准：C++20（CMake 设定），LLVM 18.1.6（vcpkg），Windows + MSVC  
> 关联文档：`docs/superpowers/plans/2025-06-20-lexer-parser-semantic-decoupling.md`（解耦方案）、`ARCHITECTURE_DESIGN.md`（解耦前架构分析）

---

## 1. 项目概述

**FunctionsCaller** 是一个玩具级函数式表达式语言（FCLang）的完整前端工具链，覆盖从源代码字符串到 **树遍历解释执行** 与 **LLVM IR 生成** 两条后端路径。

语言特性：

| 特性 | 说明 |
|------|------|
| 类型 | `int` / `double` / `string`，值缺失用 `Dangle` 哨兵表示 |
| 变量 | `var name:type = init`（强制初始化），顶层任意位置（含块/分支内）声明一律为全局（README:291 规则） |
| 函数 | `def name(a:type, b:type) { body }`，函数体为块，末表达式即返回值 |
| 控制流 | `if c then e else e`（else 必选）、`for i = start, end[, step] in body` |
| 运算符 | `=`(5) `<`(9) `+ -`(10) `* /`(20)，无逻辑运算符与一元负号 |
| 作用域 | 块级嵌套作用域 + 同名遮蔽（shadowing），先声明后使用 |
| 错误处理 | 各阶段均返回错误哨兵（null AST / Dangle 值），不中断进程 |

工程特性：

- **三阶段独立可测**：Lexer、Parser、SemanticAnalyzer 各有专属单元测试套件，互不依赖
- **符号身份寻址**：变量经 `shared_ptr<VariableSymbol>` 身份绑定，后端不按名称查表
- **扁平化帧模型**：全局帧与调用帧统一为 `Frame::locals[slot]`，`runtime.frameIndex` 定位帧
- **双后端**：树遍历解释器与 LLVM codegen 消费同一份语义输出，行为一致

---

## 2. 分层架构

```
┌────────────────────────────────────────────────────────────────┐
│  可执行目标                                                      │
│  main.cc（最小入口）  unit_test.cc（694 断言测试）  shadow_repro  │
├────────────────────────────────────────────────────────────────┤
│  Facade 层                                                      │
│  scanner.h/cc (30/29 行) — FCScanner                            │
│  三阶段流水线编排：Lexer→Parser→SemanticAnalyzer                 │
│  对外仅暴露 analysis(string) 与 semanticContext()                │
├──────────────────────┬─────────────────────┬───────────────────┤
│  前端三阶段（独立）    │  语义核心            │  后端（双实现）      │
│                      │                     │                   │
│  lexer.h/cc (37/171) │  semantic.h/cc      │  evaluator.h/cc   │
│  FC::Lexer           │  (174/379)          │  (50/362)         │
│  字符流→Token流        │  FCSemanticContext  │  FCEvaluationCtx  │
│                      │  SemanticAnalyzer   │  树遍历求值         │
│  parser.h/cc (45/372)│  作用域/槽位/绑定     │                   │
│  FC::Parser          │                     │  codegen.h/cc     │
│  Token流→纯语法AST    │                     │  (43/847)         │
│  零语义依赖           │                     │  LLVM IR 生成      │
├──────────────────────┴─────────────────────┴───────────────────┤
│  数据契约层                                                      │
│  token.h (323)  AST 节点族 + VariableSymbol + CompiledProgram   │
│                FCBinaryOp 枚举 + SemanticBinding + SymbolTable  │
│  runtime.h (37) FCValue / Frame / VariableRuntimeInfo           │
└────────────────────────────────────────────────────────────────┘
```

**依赖方向（严格单向，无循环）**：

```
runtime.h → token.h → lexer.h → parser.h
                ↘ semantic.h ↗       ↘ scanner.h(Facade)
        evaluator.h ─┘   codegen.h ─┘
```

- Lexer 不依赖 Parser/Semantic；Parser 不依赖 Semantic；SemanticAnalyzer 不依赖 Lexer/Parser 实现细节
- 头文件只包含自身直接使用的组件（codegen.h 仅留 6 个直接使用的 LLVM 头，其余移入 .cc）

---

## 3. 模块职责与文件清单

| 模块 | 文件（行数） | CMake 目标 | 核心职责 |
|------|------------|-----------|---------|
| **Runtime** | `runtime.h` (37) | — | `FCValue`（Integer/Floating/String/Dangle 联合体）、`Frame`（funcName + locals 槽位向量）、`VariableRuntimeInfo::frameIndex`（-1=全局，≥0=帧下标） |
| **Token** | `token.h/cc` (323/142) | FunctionsCore | AST 节点族（13 种）；`FCToken`/`FCBinaryOp` 枚举；`charToBinaryOp` 唯一字符转换点；`VariableSymbol`（declaration/slot/scopeDepth/runtime）；`SemanticBinding`（AST节点→符号身份绑定表）；`SymbolTable`；`CompiledFunction`/`CompiledProgram` |
| **Lexer** | `lexer.h/cc` (37/171) | FunctionsCore | `FC::Lexer`：字符流→Token 流；Token 携带 `kind/lexeme/line/column/binop`；`nextToken/peekToken/currentToken/isAtEnd/reset`；标识符/关键字/数字/字符串/注释/标点全路径；**100% 行覆盖** |
| **Parser** | `parser.h/cc` (45/372) | FunctionsCore | `FC::Parser`：Token 流→纯语法 AST；递归下降 + 优先级爬升（`tokenPrecedence` 按 `FCBinaryOp`，非运算符返回 -1）；错误恢复返回 nullptr + `hadError()`；`reset()` 支持实例复用；**零语义副作用** |
| **Semantic** | `semantic.h/cc` (174/379) | FunctionsSemantic | `FCSemanticContext`：作用域栈（RAII `ScopeGuard`/`FunctionScopeGuard`）、`declareVariable`（分配 slot + frameIndex）、`registerFunction`、`bindVariable/boundSymbol`；`SemanticAnalyzer`：AST 遍历分派（12 种节点），**帧布局唯一生产者**（CompiledFunction::frameSize/parametersIdx） |
| **Evaluator** | `evaluator.h/cc` (50/362) | FunctionsEvaluator | `FCEvaluationContext`：全局帧 + 调用栈；`frameForSymbol`（frameIndex==-1→全局帧，否则栈顶帧）；`pushFrame` 帧大小从 `CompiledFunction::frameSize`（语义期输出）读取；树遍历求值，按 `symbol->slot` 读写 `frame->locals` |
| **Codegen** | `codegen.h/cc` (43/847) | FunctionsCodegen | `FCCodegenContext`：`namedValues`（局部→Alloca）/`globalValues`（全局→GlobalVariable）按符号身份索引；`inferExprType` 类型推断；`codegenDeclaration` 按 `runtime.frameIndex==-1` 分派全局/局部分配；函数前向声明 + `__fc_main` 顶层包装 + verifyFunction/verifyModule |
| **Scanner(Facade)** | `scanner.h/cc` (29/30) | FunctionsFrontend | `analysis()` = lexer.reset → parser.reset → parseProgram → SemanticAnalyzer.analyze；`hadParseError()/hadSemanticError()` 阶段错误标记；`resetState()` 复位；**100% 行覆盖** |

构建目标依赖（`Calculater/CMakeLists.txt`）：

```
FunctionsCore      ← token/lexer/parser（无 LLVM）
FunctionsSemantic  ← FunctionsCore
FunctionsEvaluator ← FunctionsCore
FunctionsCodegen   ← FunctionsCore + LLVM(core/support/analysis)
FunctionsFrontend  ← FunctionsCore + FunctionsSemantic（仅 scanner.h/cc）
FunctionsUnitTest  ← Frontend + Evaluator + Codegen
```

---

## 4. 核心数据流

```
源代码字符串
    │
    ▼ ①  FC::Lexer::getTok
Token 流 {kind, lexeme, line, column, binop}
    │      标点/关键字 → FCToken 枚举；运算符 → FCBinaryOp 枚举
    ▼ ②  FC::Parser::parseProgram
纯语法 AST（FCProgramAST / FCExprAST 族）
    │      FCVariableExprAST 只含 name；FCBinaryExprAST 只含 FCBinaryOp
    │      FCFunctionAST 不含帧布局（纯语法）
    ▼ ③  FCSemanticContext::SemanticAnalyzer::analyze
语义输出 CompiledProgram
    ├── globalSymbols: vector<VariableSymbol>（按全局槽位序）
    ├── functions: name → CompiledFunction
    │       ├── ast（回指 AST）
    │       ├── parametersIdx（形参槽位，声明序）
    │       ├── symbols: SymbolTable（参数+局部，按 slot）
    │       └── frameSize / maxTempSlots（帧布局唯一权威）
    └── m_binding: SemanticBinding（AST节点指针 → shared_ptr<VariableSymbol>）
    │
    ├──────────────► ④ Evaluator
    │   boundSymbol(node) → frameForSymbol(frameIndex) → locals[slot]
    │   pushFrame(name) 按 CompiledFunction::frameSize 分配
    │   输出：FCValue
    │
    └──────────────► ⑤ Codegen
        boundSymbol(node) → namedValues/globalValues（符号身份索引）
        frameIndex==-1 → GlobalVariable；否则 → AllocaInst
        输出：LLVM Module（verifyModule 验证）
```

---

## 5. 关键设计决策

### 5.1 符号身份绑定（替代名称查表）

```cpp
// SemanticBinding: AST 节点指针 → shared_ptr<VariableSymbol>
struct SemanticBinding {
    std::unordered_map<const void*, std::shared_ptr<VariableSymbol>> variables;
    bool bind(const void* node, std::shared_ptr<VariableSymbol> symbol);
    std::shared_ptr<VariableSymbol> find(const void* node) const;
};
```

- 同名遮蔽天然正确：两个 `x` 绑定到不同 `VariableSymbol` 实例
- 后端（evaluator/codegen）用 `std::map<shared_ptr<VariableSymbol>, ...>` 按身份索引存储，无名称歧义
- AST 保持纯语法结构，语义数据（slot/frameIndex）全部在符号侧

### 5.2 扁平化变量寻址

```
读/写变量 = frame->locals[symbol->slot]
定位帧    = symbol->runtime.frameIndex == -1 ? callStack[0]（全局帧） : callStack.back()（当前调用帧）
```

- 全局/局部统一为"帧 + 槽位"，无双存储类型
- `slot` 语义：全局域=globalSymbols 下标；函数域=帧内下标
- `frameIndex` 由语义期赋值（`isGlobalScope() ? -1 : 0`），`isGlobalScope() == !m_currentState.isActive`（顶层块内 var 亦为全局）

### 5.3 运算符全管线枚举化（FCBinaryOp）

```
Lexer: charToBinaryOp（唯一转换点）→ Token.binop
Parser: tokenPrecedence(FCBinaryOp) → FCBinaryExprAST(FCBinaryOp)
Evaluator/Codegen: switch (FCBinaryOp) 分派
```

- ASCII 字符只存在于 Lexer 词法识别与 `charToBinaryOp` 一处
- 非运算符 token 优先级返回 -1，自然终止二元表达式解析
- 旧 `tok_assign` 枚举仅用于 Parser 分支判断，语义由 `binop` 承载

### 5.4 帧布局单一权威（AST 不携带语义数据）

- 旧设计：`FCFunctionAST::m_localCount` 由 Parser 填 0、语义期无法回填 → evaluator 误读
- 新设计：**SemanticAnalyzer 是帧大小唯一生产者** → `CompiledFunction::frameSize`；`FCFunctionAST` 仅含 proto+body
- evaluator `pushFrame` 只查 `CompiledProgram::functions`（语义输出），不查 AST

### 5.5 三阶段错误契约

| 阶段 | 错误表示 | 恢复策略 |
|------|---------|---------|
| Lexer | 未闭合字符串→`tok_eof` | 终止扫描 |
| Parser | `nullptr` + `hadError()` 标志 | 跳过当前 token 继续尝试 |
| SemanticAnalyzer | `analyze()` 返回 false + stderr 诊断（如 `variable 'x' is not declared`） | Facade 标记 `hadSemanticError` |
| Evaluator | `Dangle` 值哨兵 | 逐表达式向上传播 |
| Codegen | `nullptr` + stderr 诊断 | 保留已生成 IR 片段 |

---

## 6. 三阶段独立测试体系（验收标准达成）

| 套件 | 测试组 | 隔离性 | 覆盖要点 |
|------|--------|--------|---------|
| `runLexerTests` | 37 断言 | 仅 `FC::Lexer`，零依赖 | 关键字/标识符/数字/字符串/未闭合串/注释跳过/行列追踪/标点枚举/binop 字段/peek-current/reset/空输入 |
| `runParserTests` | 17 断言 | 仅 Lexer→Parser，零语义 | 优先级子树（`x+(1*2)`）/纯语法变量节点/函数原型+块体/for 四元组/错误路径 hadError/顶层序列折叠 vs Program 包装 |
| `runSemanticAnalyzerTests` | 22 断言 | 手工构造 AST → analyze | 全局/顶层块/括号序列 var 均为 frameIndex=-1；函数局部 frameIndex≥0；shadowing 双符号；未声明变量拒绝；for 变量局部；if 分支声明 |
| `runScannerFacade`（scanner 组内） | 5 断言 | Facade 级 | hadParseError/hadSemanticError 互斥标记；resetState 复用 |

后端行为由 `runEvaluatorTests`（求值正确性）、`runCodegenTests`（IR 生成+verifyModule）、`runStressTests`（递归/大循环压力）与错误路径套件覆盖，全部经由 Facade 全链路驱动。

---

## 7. 覆盖率与质量指标

| 文件 | 行覆盖率 | 说明 |
|------|---------|------|
| lexer.cc | 100.00% | 独立套件全覆盖 |
| scanner.cc | 100.00% | Facade 全路径 |
| token.cc | 100.00% | 含 binaryOpName 全分支 |
| parser.cc | 98.36% | 剩余为不可达防御分支 |
| semantic.cc | 96.10% | SemanticAnalyzer + 上下文 |
| evaluator.cc | 96.22% | 错误分支经边界用例覆盖 |
| codegen.cc | 90.94% | LLVM 错误注入路径为主 |
| **Overall** | **95.35%** | **≥95% 门禁达标** |

统计口径：`tools/coverage.ps1`（Microsoft.CodeCoverage.Console），排除 `unit_test.cc/main.cc`。

测试总量：**694 断言，0 失败**（ctest 全绿）。

---

## 8. 语言限制与已知行为

- 不支持：`return`、`extern`、布尔字面量、除 `<` 外的比较/逻辑运算、一元负号、数组/结构体、字符串转义、下划线标识符、嵌套 `def`（闭包）
- 循环变量仅 `int`；浮点循环报错
- 字符串拼接 codegen 调用 libc（strlen/malloc/strcpy/strcat），不释放内存
- LLVM 顶层 `__fc_main` 将最终表达式 cast 为 int，失败回退常量 0
- `for` 循环变量为函数域局部；顶层 for（无函数帧）求值返回 Dangle（受保护路径）
- 同一 `FCEvaluationContext` 复用时，`popFrame` 不清除 `isCaptured` 符号值（闭包预留，当前语言不可触发）

---

## 9. 后续演进方向

1. **闭包捕获**：`isCaptured` 字段与 `frameIndex` 机制已就绪，待嵌套 `def` 语法扩展后启用跨帧存活
2. **Codegen 覆盖率**：90.94% 的缺口集中在 LLVM 验证失败注入路径，可补充针对性错误构造
3. **Token 结构统一**：`FCToken` 与 `FCBinaryOp` 存在职责重叠（tok_assign），可在后续清理中归并为单一 token 域
4. **deprecated char 构造移除**：`FCBinaryExprAST(char,...)` 过渡重载待测试全部迁移后删除
5. **REPL/模块化**：`Parser::reset()` 已支持多段输入复用，为交互式入口铺路

---

## 10. 构建、测试与工具链

```powershell
# 依赖（vcpkg.json 声明 llvm）
vcpkg install --triplet x64-windows

# 配置
cmake -S . -B build-debug `
  -DLLVM_DIR="D:\Code\github\FunctionsCaller\vcpkg_installed\x64-windows\share\llvm"

# 构建与测试
cmake --build build-debug --config Debug --parallel 16
ctest --test-dir build-debug -C Debug --output-on-failure

# 覆盖率（≥95% 门禁）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\coverage.ps1 `
  -BuildDir .\build-debug -Configuration Debug
```

主要产物：

```
build-debug/Calculater/Debug/FunctionsCalculater.exe   # 最小入口
build-debug/Calculater/Debug/FunctionsUnitTest.exe    # 694 断言测试套件
build-debug/Calculater/Debug/ShadowRepro.exe          # 遮蔽行为复现工具
```

---

## 11. 项目结构索引

```
FunctionsCaller/
├── CMakeLists.txt              # 根配置（C++20, find_package LLVM）
├── vcpkg.json                  # 依赖清单（llvm）
├── README                      # 语言规范（语法/作用域/限制，487 行）
├── Calculater/
│   ├── runtime.h    (37)      # FCValue / Frame / VariableRuntimeInfo
│   ├── token.h/cc   (323/142) # AST 族 + 符号模型 + FCBinaryOp
│   ├── lexer.h/cc   (37/171)  # FC::Lexer    [阶段1 · 独立可测]
│   ├── parser.h/cc  (45/372)  # FC::Parser   [阶段2 · 纯语法]
│   ├── semantic.h/cc(174/379) # FCSemanticContext + SemanticAnalyzer [阶段3]
│   ├── scanner.h/cc (29/30)   # FCScanner Facade [三阶段编排]
│   ├── evaluator.h/cc(50/362) # 树遍历解释器 [后端A]
│   ├── codegen.h/cc (43/847)  # LLVM IR 生成 [后端B]
│   ├── main.cc      (7)       # 最小入口
│   ├── unit_test.cc (2357)    # 694 断言综合套件
│   └── shadow_repro.cc (35)   # 遮蔽行为复现
├── tools/coverage.ps1          # 行覆盖率（95% 门禁）
├── triplets/x64-windows-debug.cmake
└── docs/superpowers/plans/
    ├── 2025-06-20-symbol-identity-runtime.md          # 符号身份重构方案
    ├── 2025-06-20-lexer-parser-semantic-decoupling.md  # 三阶段解耦方案（已落地）
    └── ARCHITECTURE_DESIGN.md                          # 解耦前架构分析
```

提交历史：95 commits（本分支领先 origin 17），最近演进序列：

```
b8ce4b1 step6: 独立 Parser 单元测试（三阶段隔离达成）
74a2b96 step5: 头文件依赖清理 + 过渡 shim 移除
519f10c step4: Facade 流水线 + FCBinaryOp 统一 + AST/语义解耦
2edf139 step3: SemanticAnalyzer（AST 遍历语义分析）
8c11cde step2: 纯语法 Parser + AST 节点纯化
3ef6238 step1: Lexer 模块
```

---

> 文档生成：2026-10-19（Asia/Shanghai），基于 `refactor/var-symbol-binding@b8ce4b1` 实测代码结构与质量指标  
> 验证口径：cmake 3.20+ / MSVC / LLVM 18.1.6 / ctest 694 通过 / coverage 95.11%

