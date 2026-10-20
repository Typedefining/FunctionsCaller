# FunctionsCaller 项目架构信息

> 基准分支：`refactor/var-symbol-binding`  
> 文档日期：2026-10-21  
> 当前状态：词法/语法/语义三阶段解耦完成；默认配置 782/782 断言通过，嵌套函数开关打开时 786/786；生产代码行覆盖率 95.66%  
> 语言标准：C++20（CMake 设定），LLVM 18.1.6（vcpkg），Windows + MSVC  
> 关联文档：`docs/superpowers/plans/2025-06-20-lexer-parser-semantic-decoupling.md`（解耦方案）、`docs/superpowers/specs/2026-10-20-nested-functions-opt-in-design.md`（嵌套函数开关设计）、`docs/postmortems/2026-10-20-frame-anchor-and-separator-bugs.md`（三处设计缺陷复盘）

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
│  main.cc（最小入口）  unit_test.cc（782 断言测试）  shadow_repro  │
├────────────────────────────────────────────────────────────────┤
│  Facade 层                                                      │
│  scanner.h/cc (35/49 行) — FCScanner                            │
│  三阶段流水线编排：Lexer→Parser→SemanticAnalyzer                 │
│  对外仅暴露 analysis(string) 与 semanticContext()                │
├──────────────────────┬─────────────────────┬───────────────────┤
│  前端三阶段（独立）    │  语义核心            │  后端（双实现）      │
│                      │                     │                   │
│  lexer.h/cc (52/186)  │  semantic.h/cc      │  evaluator.h/cc   │
│  FC::Lexer           │  (240/534)          │  (60/445)         │
│  字符流→Token流        │  FCSemanticContext  │  FCEvaluationCtx  │
│                      │  SemanticAnalyzer   │  树遍历求值         │
│  parser.h/cc (63/485)│  作用域/槽位/绑定     │                   │
│  FC::Parser          │                     │  codegen.h/cc     │
│  Token流→纯语法AST    │                     │  (51/983)         │
│  零语义依赖           │                     │                   │
│                      │                     │                   │
│  ast_dump.h/cc       │                     │                   │
│  (18/168)            │                     │                   │
│  纯语法树→缩进文本     │                     │                   │
├────────────────────────────────────────────────────────────────┤
│  trace.h/cc (16/56) — 按阶段启用的诊断输出（FC_TRACE 环境变量）   │
├────────────────────────────────────────────────────────────────┤
│  数据契约层                                                      │
│  token.h (387)  AST 节点族 + VariableSymbol + CompiledProgram   │
│                FCBinaryOp 枚举 + SemanticBinding + SymbolTable  │
│  runtime.h (44) FCValue / Frame / VariableRuntimeInfo            │
└────────────────────────────────────────────────────────────────┘
```

**依赖方向（严格单向，无循环）**：

```
runtime.h → token.h → lexer.h → parser.h
                ↘ semantic.h ↗       ↘ scanner.h(Facade)
        evaluator.h ─┘   codegen.h ─┘
```

- Lexer 不依赖 Parser/Semantic；Parser 不依赖 Semantic；SemanticAnalyzer 不依赖 Lexer/Parser 实现细节
- `trace.h` 只被各阶段的 `.cc` 包含，只向 stderr 输出诊断信息，不参与数据流
- 头文件只包含自身直接使用的组件（codegen.h 仅留 6 个直接使用的 LLVM 头，其余移入 .cc）

---

## 3. 模块职责与文件清单

| 模块 | 文件（行数） | CMake 目标 | 核心职责 |
|------|------------|-----------|---------|
| **Runtime** | `runtime.h` (37) | — | `FCValue`（Integer/Floating/String/Dangle 联合体）、`Frame`（funcName + locals 槽位向量）、`VariableRuntimeInfo::frameIndex`（-1=全局，≥0=帧下标） |
| **Token** | `token.h/cc` (387/153) | FunctionsCore | AST 节点族（13 种）；`FCToken`/`FCBinaryOp` 枚举；`binaryOpOf` 为 kind→运算符唯一映射点（替代已删除的 `charToBinaryOp` 与 `Token::binop`）；`VariableSymbol`（declaration/slot/scopeDepth/runtime）；`SemanticBinding`（AST节点→符号身份绑定表）；`SymbolTable`；`CompiledFunction`/`CompiledProgram` |
| **Lexer** | `lexer.h/cc` (52/186) | FunctionsCore | `FC::Lexer`：字符流→Token 流；Token 只携带 `kind/lexeme/line/column`（词法类别与位置；运算符种类由解析层推得）；读取位置与识别逻辑封装在可复制的 `Cursor` 中，`peekToken()` 在游标副本上识别、不推进主游标（无 `mutable`/缓存）；`nextToken/peekToken/currentToken/isAtEnd/reset`；标识符/关键字/数字/字符串/注释/标点全路径；词法错误返回 `tok_error`；**100% 行覆盖** |
| **Trace** | `trace.h/cc` (16/56) | FunctionsCore | `traceEnabled(stage)`：按环境变量 `FC_TRACE` 的逗号分隔阶段列表判断是否输出诊断；各阶段只在入口/出口调用，业务分支不含打印；**100% 行覆盖** |
| **Parser** | `parser.h/cc` (63/485) | FunctionsCore | `FC::Parser`：Token 流→纯语法 AST；递归下降 + 优先级爬升（`tokenPrecedence` 按 `FCBinaryOp`，非运算符返回 -1）；错误恢复返回 nullptr + `hadError()`；`tok_error` 的说明直接作为错误信息；构造不读取 token，`parseProgram()` 入口才进入解析阶段（`begin()`），`reset()` 复位阶段与错误标记，`started()` 可判定当前阶段；**零语义副作用** |
| **AST Dump** | `ast_dump.h/cc` (18/168) | FunctionsCore | `describeAst()`：纯语法树→缩进文本（13 种节点），供 `FC_TRACE=ast` 诊断与解析层断言；不参与语义与后端数据流；**100% 行覆盖** |
| **Semantic** | `semantic.h/cc` (240/534) | FunctionsSemantic | `FCSemanticContext`：作用域栈（RAII `ScopeGuard`/`FunctionScopeGuard`）、`declareVariable`（分配 slot + frameIndex）、`registerFunction`、`bindVariable/boundSymbol`；`SemanticAnalyzer`：AST 遍历分派（12 种节点），**帧布局唯一生产者**（CompiledFunction::frameSize/parametersIdx） |
| **Evaluator** | `evaluator.h/cc` (60/445) | FunctionsEvaluator | `FCEvaluationContext`：全局帧 + 调用栈；`frameForSymbol`（frameIndex==-1→全局帧，否则栈顶帧）；`pushFrame` 帧大小从 `CompiledFunction::frameSize`（语义期输出）读取；树遍历求值，按 `symbol->slot` 读写 `frame->locals` |
| **Codegen** | `codegen.h/cc` (51/983) | FunctionsCodegen | `FCCodegenContext`：`namedValues`（局部→Alloca）/`globalValues`（全局→GlobalVariable）按符号身份索引；`inferExprType` 类型推断；`codegenDeclaration` 按 `runtime.frameIndex==-1` 分派全局/局部分配；函数前向声明 + `__fc_main` 顶层包装 + verifyFunction/verifyModule |
| **Scanner(Facade)** | `scanner.h/cc` (35/49) | FunctionsFrontend | `analysis()` = lexer.reset → parser.reset → parseProgram → `FC_TRACE=ast` 输出 → SemanticAnalyzer.analyze；`hadParseError()/hadSemanticError()` 阶段错误标记；`resetState()` 复位 Lexer/Parser/语义上下文（含解析错误标记）；**100% 行覆盖** |

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
    ▼ ①  FC::Lexer::nextToken（内部 Cursor::getTok；peekToken 在游标副本上识别）
Token 流 {kind, lexeme, line, column}
    │      标点/关键字/运算符 → 统一 FCToken 枚举，Token 不携带运算符种类
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
Parser: binaryOpOf(FCToken kind)（唯一转换点）→ FCBinaryOp
Parser: tokenPrecedence(FCBinaryOp) → FCBinaryExprAST(FCBinaryOp)
Evaluator/Codegen: switch (FCBinaryOp) 分派
```

- ASCII 字符只存在于 Lexer 的词法识别分支（`charToBinaryOp` 与 `Token::binop` 已删除，运算符种类只由 `kind` 决定）
- 运算符优先级表只有 `Parser::tokenPrecedence` 一处；`FCSemanticContext` 曾有一份 `char→int` 的副本（`getOperatorPrecedence`/`m_binopPrecedence`），已于 2026-10-21 删除
- 非运算符 token 优先级返回 -1，自然终止二元表达式解析
- 运算符种类在解析层从 `kind` 推得一次（`parser.cc:151`、`163`），`Token` 不再保存第二份表示

### 5.4 帧布局单一权威（AST 不携带语义数据）

- 旧设计：`FCFunctionAST::m_localCount` 由 Parser 填 0、语义期无法回填 → evaluator 误读
- 新设计：**SemanticAnalyzer 是帧大小唯一生产者** → `CompiledFunction::frameSize`；`FCFunctionAST` 仅含 proto+body
- evaluator `pushFrame` 只查 `CompiledProgram::functions`（语义输出），不查 AST

### 5.5 三阶段错误契约

| 阶段 | 错误表示 | 恢复策略 |
|------|---------|---------|
| Lexer | 词法错误→`tok_error`（`lexeme` 携带可直接展示的说明，如 `unterminated string literal`） | 终止扫描，Parser 报错并置 `hadError()` |
| Parser | `nullptr` + `hadError()` 标志 | 跳过当前 token 继续尝试 |
| SemanticAnalyzer | `analyze()` 返回 false + stderr 诊断（如 `variable 'x' is not declared`） | Facade 标记 `hadSemanticError` |
| Evaluator | `Dangle` 值哨兵 | 逐表达式向上传播 |
| Codegen | `nullptr` + stderr 诊断 | 保留已生成 IR 片段 |

---

## 6. 三阶段独立测试体系（验收标准达成）

| 套件 | 测试组 | 隔离性 | 覆盖要点 |
| `runAstDumpTests` | 19 断言 | 仅 Facade + `describeAst`，不看语义 | 三类容器节点（Program/Block/Seq）/优先级树形/if 三支顺序/for 带与不带 step/call 实参/字符串与浮点字面量/未特化节点兜底/未知运算符 `?`/解析失败无树 |
| `runLexerTests` | 66 断言 | 仅 `FC::Lexer`，零依赖 | 关键字/标识符/数字/字符串/未闭合串与未识别字符的 `tok_error`/注释跳过/行列号（含列号边界）/标点与运算符枚举/`binaryOpOf` 映射/trace 开关匹配/peek 的游标副本（跨注释跨行、重复 peek 稳定）/reset/空输入 |
| `runParserTests` | 17 断言 | 仅 Lexer→Parser，零语义 | 优先级子树（`x+(1*2)`）/纯语法变量节点/函数原型+块体/for 四元组/错误路径 hadError/顶层序列折叠 vs Program 包装 |
| `runSemanticAnalyzerTests` | 22 断言 | 手工构造 AST → analyze | 全局/顶层块/括号序列 var 均为 frameIndex=-1；函数局部 frameIndex≥0；shadowing 双符号；未声明变量拒绝；for 变量局部；if 分支声明 |
| `runScannerFacade`（scanner 组内） | 7 断言 | Facade 级 | hadParseError/hadSemanticError 互斥标记；resetState 复用与解析错误标记复位 |

后端行为由 `runEvaluatorTests`（求值正确性）、`runCodegenTests`（IR 生成+verifyModule）、`runStressTests`（递归/大循环压力）与错误路径套件覆盖，全部经由 Facade 全链路驱动。

---

## 7. 覆盖率与质量指标

| 文件 | 行覆盖率 | 说明 |
|------|---------|------|
| lexer.cc | 100.00% | 独立套件全覆盖（42/42，游标切分后行数减少） |
| scanner.cc | 100.00% | Facade 全路径 |
| trace.cc | 100.00% | 阶段匹配全分支 |
| ast_dump.cc | 100.00% | 三类容器、if/for/call、字面量与兜底节点（120/120） |
| parser.cc | 97.90% | 剩余为不可达防御分支 |
| semantic.cc | 96.15% | SemanticAnalyzer + 上下文 |
| evaluator.cc | 97.32% | 错误分支经边界用例覆盖 |
| codegen.cc | 90.97% | LLVM 错误注入路径为主 |
| **Overall** | **95.66%** | **≥95% 门禁达标（默认配置）** |

统计口径：`pwsh -NoProfile -File tools/coverage.ps1 -BuildDir build-strict`（Microsoft.CodeCoverage.Console），排除 `unit_test.cc/main.cc`。

测试总量：**782 断言，0 失败**（默认配置）；`-DFC_ENABLE_NESTED_FUNCTIONS=ON` 时 **786 断言，0 失败**。
两种配置由 `tools/verify-configurations.ps1` 统一验证。

---

## 8. 语言限制与已知行为

- 分隔符只有 `;`，且只能跟在**非块终结**的表达式之后：块（`}`）已终止语句，其后写 `;` 报 `';' must not follow a block terminator`，下一条语句直接开始（`def f() { 1 } f()` 合法）
- 不支持：`return`、`extern`、布尔字面量、除 `<` 外的比较/逻辑运算、一元负号、数组/结构体、字符串转义、下划线标识符、闭包
- 嵌套 `def` 是**前端编译期选项**（`FC_ENABLE_NESTED_FUNCTIONS`，默认 OFF）：关闭时语义期直接拒绝；打开时前端允许表示与登记，但求值器与 codegen 一律拒绝执行，内层函数引用外层局部一律按捕获拒绝（`semantic.cc` 的 `m_symbolOwnerFrame` 判定）
- 打开态下嵌套 `def` 仍不可调用：`FCFunctionRegistry::index` 只登记顶层函数，调用会落到 “unknown function” 失败路径
- 顶层块中的 `def` 不受该开关影响（帧基址仍为 0）
- 循环变量仅 `int`；浮点循环报错
- 字符串拼接 codegen 调用 libc（strlen/malloc/strcpy/strcat），不释放内存
- LLVM 顶层 `__fc_main` 将最终表达式 cast 为 int，失败回退常量 0
- `for` 循环变量为函数域局部；顶层 for（无函数帧）求值返回 Dangle（受保护路径）
- 同一 `FCEvaluationContext` 复用时，`popFrame` 不清除 `isCaptured` 符号值（闭包预留，当前语言不可触发）

---

## 9. 后续演进方向

1. **闭包捕获**：`isCaptured` 字段与帧基址模型已就绪；开关打开后前端已能表示嵌套 `def`，下一步需要 upvalue 链并同时修正四处 `frameIndex` 单值假设，才能让内层函数安全引用外层局部
2. **Codegen 覆盖率**：90.97% 的缺口集中在 LLVM 验证失败注入路径，可补充针对性错误构造
3. **位置信息接入错误报告**：`line/column` 目前只被单元测试断言，未被任何错误信息消费；接入点集中在 `Parser::logError` 与各 `parse*` 分支
4. **deprecated char 构造移除**：`FCBinaryExprAST(char,...)` 过渡重载待测试全部迁移后删除
5. **REPL/模块化**：`Parser::reset()` 已支持多段输入复用，为交互式入口铺路
6. **诊断输出扩展**：`traceEnabled` 已按阶段划分，parser/semantic/evaluator/codegen 可继续在各自入口接入中间表示输出

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

# 嵌套函数开关（默认 OFF）：打开只放开前端表示，后端仍拒绝执行
cmake -S . -B build-nested `
  -DLLVM_DIR="D:\Code\github\FunctionsCaller\vcpkg_installed\x64-windows\share\llvm" `
  -DFC_ENABLE_NESTED_FUNCTIONS=ON

# 双配置验证（构建 + 全量断言）
pwsh -NoProfile -File .\tools\verify-configurations.ps1

# 诊断输出（只影响 stderr，不改变语义）
$env:FC_TRACE = "lexer"          # 单个阶段
$env:FC_TRACE = "lexer,parser"   # 多个阶段
$env:FC_TRACE = "all"            # 全部阶段

# 覆盖率（≥95% 门禁）
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\coverage.ps1 `
  -BuildDir .\build-strict -Configuration Debug
```

主要产物：

```
build-debug/Calculater/Debug/FunctionsCalculater.exe   # 最小入口
build-debug/Calculater/Debug/FunctionsUnitTest.exe    # 782 断言测试套件（默认配置）
build-debug/Calculater/Debug/ShadowRepro.exe          # 遮蔽行为复现工具
```

---

## 11. 项目结构索引

```
FunctionsCaller/
├── CMakeLists.txt              # 根配置（C++20, find_package LLVM）
├── vcpkg.json                  # 依赖清单（llvm）
├── README                      # 语言规范（语法/作用域/限制）
├── Calculater/
│   ├── runtime.h    (44)      # FCValue / Frame / VariableRuntimeInfo
│   ├── language_options.h (17)# 编译期语言开关（嵌套 def，默认关闭）
│   ├── token.h/cc   (387/153) # AST 族 + 符号模型 + FCBinaryOp
│   ├── lexer.h/cc   (52/186)  # FC::Lexer    [阶段1 · 独立可测]
│   ├── trace.h/cc   (16/56)   # 按阶段启用的诊断输出（FC_TRACE）
│   ├── parser.h/cc  (63/485)  # FC::Parser   [阶段2 · 纯语法]
│   ├── ast_dump.h/cc (18/168) # 纯语法树→缩进文本 [阶段2 诊断输出]
│   ├── semantic.h/cc(240/534) # FCSemanticContext + SemanticAnalyzer [阶段3]
│   ├── scanner.h/cc (35/49)   # FCScanner Facade [三阶段编排]
│   ├── evaluator.h/cc(60/445) # 树遍历解释器 [后端A]
│   ├── codegen.h/cc (51/983)  # LLVM IR 生成 [后端B]
│   ├── main.cc      (9)       # 最小入口
│   ├── unit_test.cc (3265)    # 综合断言套件
│   └── shadow_repro.cc (38)   # 遮蔽行为复现
├── tools/
│   ├── coverage.ps1            # 行覆盖率（95% 门禁）
│   └── verify-configurations.ps1 # 双配置构建 + 全量断言
├── triplets/x64-windows-debug.cmake
└── docs/
    ├── ARCHITECTURE.md
    ├── walkthrough/01-lexer.md                                    # 代码阅读第 1 段：词法分析
    ├── walkthrough/02-parser.md                                   # 代码阅读第 2 段：语法分析
    ├── postmortems/2026-10-20-frame-anchor-and-separator-bugs.md  # 三处设计缺陷复盘
    ├── superpowers/plans/
    │   ├── 2025-06-20-symbol-identity-runtime.md          # 符号身份重构方案
    │   ├── 2025-06-20-lexer-parser-semantic-decoupling.md  # 三阶段解耦方案（已落地）
    │   ├── 2026-10-20-global-scope-merge-and-separator-unification.md
    │   └── ARCHITECTURE_DESIGN.md                          # 解耦前架构分析
    └── superpowers/specs/2026-10-20-nested-functions-opt-in-design.md
```

提交历史：119 commits（本分支领先 origin 4），最近演进序列：

```
36261ee fix: 分隔符只能跟在非块终结的表达式之后，块结束后禁止再写分号
8c83b97 docs: 新增第 2 段代码阅读章节（语法分析），同步架构指标
0a14a1a feat: 语法层输出 AST 文本形式，接入 FC_TRACE=ast 诊断输出
fca733d docs: 同步优先级表去重后的指标与行号
76b1390 refactor: 删除语义层重复的运算符优先级表
6fab9b5 docs: 同步 Token 去 binop 后的章节与指标
dbb1e19 refactor: Token 不再携带 binop，运算符种类由 kind 在解析层推得
220845d docs: 同步词法游标切分后的行号与指标
4ee8513 refactor: 词法游标可复制，peekToken 在副本上识别而不推进主游标
54e6907 fix: 解析器只在解析阶段读取 token，resetState 复位 Parser
10dc9ab docs: 更新架构与质量指标，新增第 1 段词法分析代码阅读章节
bb9916a feat: 新增按阶段启用的诊断输出开关，修正词法层四处缺陷
```

---

> 文档生成：2026-10-21（Asia/Shanghai），基于 `refactor/var-symbol-binding@36261ee` 实测代码结构与质量指标  
> 验证口径：cmake 3.20+ / MSVC / LLVM 18.1.6 / 默认配置 782 断言、开关打开 786 断言 / coverage 95.66%



