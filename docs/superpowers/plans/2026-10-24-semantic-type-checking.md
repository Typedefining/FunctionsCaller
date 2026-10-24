# 语义阶段类型检查实施计划（修复审查项 #3：类型系统缺位）

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 语义阶段实现静态类型检查：类型错误在 `analysis()` 即被拒绝并输出结构化诊断；删除 codegen 中与之重复的 `inferExprType` 类型推断家族，函数返回类型以语义产物 `CompiledFunction::returnType` 为唯一数据源。

**Architecture:** 三阶段语义分析。Phase 1 = 现有名字解析/函数注册/绑定（行为不变）。Phase 2 = 返回类型不动点迭代：`FCTypeId` 域（乐观初值 Int，固定 4 轮，无收敛判定分支，残余不一致由 Phase 3 兜底）。Phase 3 = 全树类型检查走查，产出 stage="semantic" 诊断。evaluator 零修改（运行期类别检查保留为手工构造 AST 的安全网，有效程序不再触达分歧路径）；codegen 改读语义产物。类型策略：**严格同型，无隐式转换**——int+double 为错误（首要审核点，见 Review Focus 1）。

**Tech Stack:** C++20、MSVC、CMake ≥ 3.20、LLVM 18（vcpkg）、PowerShell。仅标准库，无新依赖。

**Spec:** 2026-10-22 审查结论 #3（对话记录）原文四条：类型错误由两个后端各自实现（evaluator 动态类别检查 + codegen inferExprType/castValue）且行为分歧；for 终止条件 union 成员未判别即读 `intVal`；赋值左侧"必须是变量"属静态可判定错误却推迟到运行期；类型名字符串比较散布三处（parser.cc:276/417、codegen.cc:238-245、VarDecl 构造点）。

## Global Constraints

- 每任务收尾三连必须满足：`cmake --build build-debug --config Debug --parallel 16` 零错误；`ctest --test-dir build-debug -C Debug --output-on-failure` 全绿；`pwsh -NoProfile -File tools/coverage.ps1 -BuildDir build-debug -Configuration Debug` ≥ 95%。
- 行为基线：现有断言的通过/失败结论不得翻转；**仅** 各任务显式列出的 re-baseline 项除外。
- 严格同型策略：无隐式数值转换（int↔double 不互转、不互赋、不混算）；`if`/`for` 终止条件允许 int 或 double（沿用"非 0 即真"）。
- 空函数体契约不变：`def f() { }` analysis 成功（ast 非空），codegen 失败——经 `returnType=Error` 传导，语义阶段不报错。
- 前向引用保持合法：`def a() { b() } def b() { 1 } a()` 现可执行，语义检查不得回退该能力。
- 嵌套函数开关（build-nested 配置）下 analysis 行为不收紧。
- 提交粒度：每任务一次提交，`feat: <任务名>` 或 `refactor: <任务名>`。
- 术语规范：标准软件工程术语（~/.dsh/AGENTS.md）。
- 与《2026-10-24-structure-responsibility-refactor.md》协调：本计划 Task 5 删除 `definitions` 映射即该计划 Task 4 的后半项，若那侧先执行到 Task 4，本任务跳过该步；`FCTypeId` 与 `FCTypeDescribe`（那侧 Task 6 删除）互不相干；若那侧 Task 5（dispatchAST）先合入，Phase 3 走查的分派优先改用 dispatchAST。

## Review Focus

1. **混合类型算术裁决**：现有基线自相矛盾——evaluator 错误用例（unit_test.cc:3342-3343 `1 + 2.0` 判 Dangle）vs semantic/codegen 成功用例（:688、:1250 mixed globals 判成功）。本计划裁决为**错误**（严格同型）。若审核要求 C 式加宽（int→double 提升），Phase 3 二元规则、Task 3 re-baseline 清单与 evaluator 零修改前提全部重写。→ Task 3 测试。
2. **递归函数返回类型**：现 codegen 以"环上谎报 Int"工作（codegen.cc:41-42 visiting 集合），加宽路径掩盖谎报；严格同型下谎报暴露（double 自递归将错判）。Phase 2 不动点替代之。测试必须包含 double 自递归。→ Task 2 测试。
3. **前向引用**：现由 evaluator `index()` 与 codegen 先声明全部函数支持；Phase 2/3 在注册完成后运行，天然支持，测试钉住。→ Task 4 测试。
4. **手工构造 AST 的 codegen 路径**（无语义记录）：declareFunction 改读记录后，手工函数节点报错。Task 5 审计现有手工用例（已知 :1466 期望失败、:1469-1472 nullBody2 构造后未使用）；若存在期望成功的手工函数用例，改为经 FCScanner 产出 AST。→ Task 5 步骤 3。
5. **空块类型 = Error（毒化）**：`var x:int = { }` 报错；`def f() { }` 不报错（空函数体契约）。Error 参与任何规则均不匹配，但函数返回位置静默记录。→ Task 2/3 测试。

---

## 非目标（本计划不做，防止范围蔓延）

- FCValue 裸 union / variant 化（审查项 #4）：evaluator 对**手工 AST** 的 union 误读保留现状。
- `std::stoi` 崩溃（审查项 #2）。
- 语义诊断携带源位置：AST 节点无位置字段，parser 改动超范围；semantic 诊断继续 `location={}`。
- evaluator 运行期检查的删除：保留为手工 AST 安全网；本计划对其零修改。
- `for` 表达式 codegen 返回常量 0（与求值器"返回最后一次循环体值"的值保真分歧，codegen.cc codegenFor 末行）：属值语义缺陷而非类型缺陷。
- 除零防护（evaluator 整数除法 UB、codegen SDiv）：运行期缺陷，非静态类型检查职责。
- 结构重构计划（2026-10-24）的 Task 2-7 各项。

---

### Task 1: `FCTypeId` 枚举与唯一映射点；codegen `getType(FCTypeId)`

**Files:**
- Modify: `Calculater/ast.h`（VarDecl）、`Calculater/ast.cc`（若构造函数在外实现则同步）、`Calculater/codegen.h`、`Calculater/codegen.cc:238-245,71,197,613,730`（getType 调用点）、`Calculater/unit_test.cc:1486-1490`（getType API 断言）
- Test: `Calculater/unit_test.cc`（新增 runTypeMappingTests，并入 runTokenApiTests 区块旁）

**Interfaces:**
- Produces（ast.h）:

```cpp
enum struct FCTypeId { Int, Double, String, Error };
struct VarDecl {
  std::string name;
  std::string typeName;      // 保留：dump/测试消费
  FCTypeId type;             // 新增：由 typeName 派生的唯一权威类型值
  VarDecl(const std::string &n, const std::string &t);  // "int"/"double"/"string"→枚举；未知→Error
};
```

- Produces（codegen.h）: `llvm::Type* getType(FCTypeId type);`——Int→i32、Double→double、String→ptr、**Error→断言失败或报诊断并返回 nullptr**（调用方已有空值传播）。字符串版 `getType(const std::string&)` 删除。
- Consumes: 无（首任务）。
- 测试构造 `VarDecl("a","int")` 的既有代码全部不变（构造签名保持字符串入参）。

- [ ] **Step 1: 写失败测试**——VarDecl 映射四例（int/double/string/未知→Error）与 `getType(FCTypeId::*)` 三例；把 unit_test.cc:1486-1490 的字符串版 getType 断言改写为枚举版（这五条同属本步，一次过编译期）。

```cpp
expect(VarDecl("a", "int").type == FCTypeId::Int, "type-map - int");
expect(VarDecl("a", "double").type == FCTypeId::Double, "type-map - double");
expect(VarDecl("a", "string").type == FCTypeId::String, "type-map - string");
expect(VarDecl("a", "bogus").type == FCTypeId::Error, "type-map - unknown maps to Error");
expect(cc.getType(FCTypeId::Double)->isDoubleTy(), "codegen-api - getType Double enum");
```

- [ ] **Step 2: 构建，确认编译失败**（FCTypeId 未定义）。Run: `cmake --build build-debug --config Debug --parallel 16`。Expected: 编译错误。
- [ ] **Step 3: 实现**——ast.h 加枚举与 VarDecl.type 派生（构造函数体内一条 switch/比较链，字符串→枚举的**全仓库唯一映射点**）；codegen `getType(FCTypeId)` 实现并替换 5 处调用点（codegen.cc:71 经 `symbol->declaration->type`、:197、:613、:730 经 `decl->type`），删字符串重载。
- [ ] **Step 4: 三连验证**（既有 3641 行套件全绿 + 新测试绿 + 覆盖 ≥ 95%）。
- [ ] **Step 5: Commit** `git commit -m "feat: FCTypeId enum with single string-to-type mapping point"`

### Task 2: Phase 2——返回类型不动点；`CompiledFunction::returnType` 语义产物

**Files:**
- Modify: `Calculater/symbol.h`（CompiledFunction 加字段）、`Calculater/semantic.h`（SemanticAnalyzer 私有方法）、`Calculater/semantic.cc`（analyze 入口后接 Phase 2）、`Calculater/unit_test.cc`
- Test: `Calculater/unit_test.cc`（新增 runReturnTypeTests）

**Interfaces:**
- Produces（symbol.h）: `CompiledFunction` 增 `FCTypeId returnType = FCTypeId::Error;`（Phase 2 回填）。
- Produces（semantic.h，SemanticAnalyzer 私有）:

```cpp
// 全类型计算：读绑定符号与 callReturns；非法组合返回 Error（不报告——报告职责在 Phase 3）
FCTypeId typeOfExpr(const FCExprClass::FCExprAST *expr,
                    const std::unordered_map<std::string, FCTypeId> &callReturns);
// Phase 2：不动点迭代（乐观初值 Int，固定 4 轮），结果写入各 CompiledFunction::returnType
void computeReturnTypes();
```

- 规则表（typeOfExpr，全函数无副作用）：Number→isFloating?Double:Int；String 字面量→String；Variable→`boundSymbol` 的 `declaration->type`；VarDecl 节点→`decl->type`；Assign→LHS 符号类型；Binary 数值四则→两侧同型数值取该型，Less→同型数值→Int，String+String（仅 Add）→String，其余→Error；If→then/else 同型取该型，条件 Int/Double，否则 Error；For→body 类型（start/step/end 类型不在此查）；Block/Seq→末表达式，空→Error；Call→`callReturns[name]`（缺席→Error）；Function 节点→Int（定义无值，源程序语法不产生定义出现在值位置；与 codegen 旧谎报一致）。
- 迭代细节：`candidates[name]` 初值全 Int（仅遍历 `functions` 表中 `ast != nullptr` 的记录，`<global>` 的 ast 为空天然排除）；每轮对每个函数以当前 candidates 计算 `typeOfExpr(body, candidates)` 并回写；固定 4 轮后写入 `CompiledFunction::returnType`。**不做收敛判定**（`ponytail:` 上限 4 轮 = |FCTypeId|，残余不一致由 Phase 3 走查报告，死分支零覆盖风险由删除消除）。

- [ ] **Step 1: 写失败测试**（经 `scanner.semanticContext().getCompiledFunction(name)->returnType` 断言）：

```cpp
{"def f() { 1 }", "f", FCTypeId::Int},
{"def f() { 1.5 }", "f", FCTypeId::Double},
{"def f() { \"s\" }", "f", FCTypeId::String},
{"def f() { }", "f", FCTypeId::Error},                       // 空函数体契约：analysis 成功 + Error
{"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "fib", FCTypeId::Int},
{"def a() { b() } def b() { 2.0 }", "a", FCTypeId::Double},   // 前向引用
{"def d(n:double) { if n < 2.0 then n else d(n - 1.0) }", "d", FCTypeId::Double},  // 不动点必要性
```

  外加一条：`def f() { }` 同时断言 `ast != nullptr`（空函数体 analysis 不失败）。
- [ ] **Step 2: 构建 + ctest 确认红**（returnType 字段不存在 → 编译失败即红）。
- [ ] **Step 3: 实现** computeReturnTypes + typeOfExpr + 字段；`SemanticAnalyzer::analyze` 在 Phase 1 成功后调用 computeReturnTypes。注意：Phase 1 失败路径（现行首个名字错误即返回 false）不变。
- [ ] **Step 4: 三连验证**。
- [ ] **Step 5: Commit** `git commit -m "feat: return-type fixpoint as semantic product"`

### Task 3: Phase 3 走查 A——声明/赋值/二元/if/for 类型规则

**Files:**
- Modify: `Calculater/semantic.h`（`bool checkTypes(const FCExprClass::FCExprAST*)`）、`Calculater/semantic.cc`、`Calculater/unit_test.cc:688,694`（re-baseline 两例）与错误用例区
- Test: `Calculater/unit_test.cc`（新增 runSemanticTypeErrorTests；errorCases 列表驱动）

**Interfaces:**
- Produces: `checkTypes(root)`——与 analyzeExpr 同构的分派走查（12 节点），每规则先递归子节点再自检，首个违规 `reportError` 并返回 false（沿用 Phase 1 的 fail-fast 单错误风格）。`analyze()` 顺序：Phase 1 → Phase 2 → checkTypes。
- 规则与诊断文案（stage="semantic"，测试以子串断言）：
  - VarDecl：`typeOfExpr(init) == decl->type`，否则 `type mismatch: variable '<n>' expects <T>, initializer has <U>`（含 `var x:int = { }`）。
  - Assign：LHS 非 FCVariableExprAST → `left side of assignment must be a variable`；否则 RHS 类型须等于 LHS 符号类型，文案同上格式。
  - Binary：两侧类型查表——int/int（+ - * /）→int、double/double→double、`<` 同型数值→int、string/string 且仅 `+`→string；其余（含 **int+double 混合**、string 参与非加运算、含 Error 操作数）→ `type mismatch: '<op>' requires ...`。
  - If：条件 Int/Double，否则 `if condition must be numeric`；then/else 同型，否则 `if branch types do not match`。
  - For：start/end/step 均 Int，否则 `for <part> must be int`（part=start/end/step）。此规则在源程序层面消除 evaluator union 误读（审查项 #3 子条）。
- Re-baseline（本任务仅此两处翻转，其余全部不变）：
  - unit_test.cc:688 `{"var g1:int = 1; var g2:double = 2.5; def f() { g1 + g2 }", "mixed globals", true}` → 移入错误用例（期望 analysis 失败 + semantic 诊断）。
  - unit_test.cc:694 `{"var g:int = 1; var h:double = 2.0; var s:string = \"x\"; g + h", "three typed globals", true}` → 代码尾部 `g + h` 改为 `g`（保留三类型声明成功原意），另新增 `g + h` 混合顶层错误用例。

- [ ] **Step 1: 写失败测试**——错误列表（每例断言 `analysis()` 返回 nullptr 且 `scanner.diagnostics()` 首条 stage=="semantic"）：`var x:int = "s"`、`var x:int = 1.5`、`var x:double = 1`（严格同型）、`def f() { 1 + 2.0 }`、`def f() { 1.0 + 2 }`、`def f() { 1 = 2 }`、`def f() { "a" - "b" }`、`def f() { "a" * 2 }`、`def f(a:string,b:string) { a < b }`、`def f() { if "a" then 1 else 2 }`、`def f() { if 1 then 1 else 2.0 }`、`def f() { for i = 0.0, i < 3, 1 in i }`、`def f() { for i = 0, "a", 1 in i }`、`def f() { for i = 0, i < 3, "s" in i }`、`var x:int = { }`、:688/:1250 两例混合全局。成功钉子：`def concat(a:string,b:string) { a + b }`、`def f(n:double) { if n < 2.0 then n else n }`、`if 1.5 then 1 else 2`（条件 double 合法）。
- [ ] **Step 2: 构建 + ctest 确认红**。
- [ ] **Step 3: 实现 checkTypes** 走查与规则；接入 analyze() 第三步；执行两处 re-baseline。
- [ ] **Step 4: 三连验证**（注意 evaluator/codegen 错误环路的用例因 `if (ast)` 短路而保持绿，不需改断言；若个别用例断言了 `ast != nullptr` 则按其原意移入语义错误区）。
- [ ] **Step 5: Commit** `git commit -m "feat: semantic type checking for decl/assign/binary/if/for"`

### Task 4: Phase 3 走查 B——调用检查（存在性/元数/实参类型）

**Files:**
- Modify: `Calculater/semantic.cc`（checkTypes 的 Call 分支）、`Calculater/unit_test.cc:585-621`（diag #12/#13/#14 re-baseline）、错误用例区
- Test: `Calculater/unit_test.cc`

**Interfaces:**
- Call 规则：被调函数不在 `getCompiledProgram().functions`（`ast != nullptr` 判真）→ `function '<n>' is not defined`；实参数 ≠ 原型形参数 → `argument count mismatch in '<n>'`；逐实参 `typeOfExpr(arg)` ≠ 形参 `decl->type` → `argument type mismatch in '<n>'`。Phase 2/3 在注册完成后运行，**前向引用天然合法**，无需延迟队列。
- Re-baseline（diag 测试区三例，期望从"evaluator/codegen 阶段报错"前移到"semantic 阶段拒绝"）：
  - :585-598 `undefinedfn(1)`：改为断言 `analysis()` 返回 nullptr 且 `scanner.diagnostics()[0].stage == "semantic"`（原 evaluator 阶段断言删除）。
  - :599-610 `add(2, 3, 4)` 元数：同上改写。
  - :611-621 `def f() { 1 = 2 }` 赋值左侧：同上改写（Task 3 规则已报，此处仅换断言归属）。

- [ ] **Step 1: 写失败测试**——成功：`def a() { b() } def b() { 1 } a()`（前向引用钉子，期望 analysis 成功）；`def f(n:int) { if n < 1 then 0 else f(n - 1) }`（递归实参检查）。错误：`def f() { g(1) } g(1)`（被调未定义）、`def f(n:int) { f("s") }`（递归实参类型）、`def f(a:int) { a } f("s")`。
- [ ] **Step 2: 构建 + ctest 确认红**。
- [ ] **Step 3: 实现** Call 分支三查；执行 :585-621 三例 re-baseline。
- [ ] **Step 4: 三连验证**。
- [ ] **Step 5: Commit** `git commit -m "feat: semantic call checking with forward references"`

### Task 5: codegen 消费语义产物；删除 inferExprType 家族与 definitions 映射

**Files:**
- Modify: `Calculater/codegen.h:34`（删 definitions 成员）、`Calculater/codegen.cc`（删 :30-143 inferExprType/inferFunctionReturnType、:91-99 其 call 分支、:785 definitions 填充、:583-588 codegenIf 类型推断、declareFunction 改造）、`Calculater/unit_test.cc`（手工 AST 用例审计）
- Test: `Calculater/unit_test.cc`（既有 codegen 全套；无新增断言家族，审计修正属本任务）

**Interfaces:**
- `declareFunction`：`context.compiledProgram.getFunction(name)` 有记录且 `record->ast == function` → 返回类型 `getType(record->returnType)`；`returnType == Error`（空函数体）或无记录（手工 AST）→ `logCodegenError` 并返回 nullptr。**`inferFunctionReturnType`/`inferExprType` 全家族删除。**
- `codegenIf`：phi 类型 = `thenValue->getType()`（严格同型下两支同型；手工 AST 分支类型不齐时由 verifyFunction 兜底失败）。删除 :583-588 的 noFunctions 推断。
- `definitions` 映射及其唯一填充点（codegenProgram :785）删除——其唯一消费者 inferExprType 已删。（若结构重构计划 Task 4 已执行并删除，跳过此步并在提交信息注明。）
- `castValue`、codegenBinary 分支结构、`codegenFunction` 的 `castValue(body, returnType)` 调用**保留**（合法程序下为恒等；手工 AST 的 LLVM 层崩溃防护）。

- [ ] **Step 1: 基线三连**（记录绿与覆盖率基线）。
- [ ] **Step 2: 实现**——declareFunction 改造 + codegenIf phi 改造 + 删三个函数/成员。
- [ ] **Step 3: 手工 AST 审计**——grep unit_test.cc 中 `make_unique<FCFunctionAST>`、`make_unique<FCIfExprAST>`、`codegen(` 直驱用例：已知 :1466 期望 nullptr ✓、:1469-1472 nullBody2 未使用 ✓、:1454-1461 原型/空指针 API 用例不经过 declareFunction ✓。若发现"手工构造带函数体且期望 codegen 成功"的用例，改为经 FCScanner 产出 AST（保持期望值不变）；发现依赖 definitions 的断言一并删除。
- [ ] **Step 4: 三连验证**（重点：`def empty() {}` 仍"analysis 成功 + codegen 失败"；空指针/原型 API 用例不变）。
- [ ] **Step 5: Commit** `git commit -m "refactor: codegen consumes semantic return types, drop inferExprType"`

### Task 6: 文档同步与双配置全量验证

**Files:**
- Modify: `README`（:283 校验阶段描述、:408-431 已知行为）、`docs/walkthrough/03-semantic.md`、`docs/walkthrough/05-codegen.md`（如引用 inferExprType）
- Test: 无新增；`tools/verify-configurations.ps1` 双配置

**Interfaces:** 无代码接口；文档事实与实现对齐。

- [ ] **Step 1: README 更新**——:283 "参数个数与类型在解释执行 / codegen 阶段校验"改为"在语义分析阶段校验（前向引用合法）"；已知行为区新增"混合数值运算（int 与 double）为类型错误，无隐式转换"；已知行为区保留"空函数体语法可解析、codegen 失败"。
- [ ] **Step 2: walkthrough 同步**——03-semantic.md 增补"类型检查（Phase 2/3）"小节（规则表一段文字 + 不动点一句）；05-codegen.md 删除 inferExprType 相关描述（如有）。
- [ ] **Step 3: 双配置验证**——`pwsh -NoProfile -File tools/verify-configurations.ps1`（默认 + build-nested 两种配置构建 + 全量断言）。若 nested 配置套件存在混合类型成功用例（语义收紧后翻转），按 Task 3 的 re-baseline 原则移入错误区并在此说明。
- [ ] **Step 4: 三连验证 + 覆盖率**（新增语义代码行均被 Task 2-4 测试覆盖）。
- [ ] **Step 5: Commit** `git commit -m "docs: semantic type checking and strict typing policy"`

---

## 已知事项（本计划不做，已记录归属）

- for 表达式 codegen 返回常量 0（codegenFor 末行）与求值器"最后一次循环体值"的**值**保真分歧：类型上一致（0 为 int），属值语义缺陷，超出类型系统范围。
- evaluator 对手工构造 AST 的 union 误读（for 终止条件读 `intVal` 不判类别）：源程序已由 Phase 3 规则（end 必须 int）消除；手工 AST 路径归 FCValue 表示缺陷（#4）。
- 语义诊断无源位置：AST 节点无位置字段，位置携带需 parser 改动，独立立项。
- 不动点 4 轮上限的残余不一致：由 Phase 3 走查以普通类型错误形式暴露；`ponytail:` 若未来引入函数类型或多态，改为按调用图拓扑序求解。

## Self-Review 记录

1. **Spec 覆盖**：双后端重复实现→Task 5 删除 codegen 侧（evaluator 侧定位为安全网，非重复实现：有效程序不再触达）；行为分歧→严格同型裁决 + Task 3 re-baseline；for union 误读→Task 3 for 规则（源程序消除）；赋值左侧静态化→Task 3 Assign 规则；字符串比较散布→Task 1 唯一映射点（parser 两处校验保留为错误报告门，VarDecl 构造为唯一映射，下游全枚举）。
2. **步骤扫描**：每步一个可判定产物；typeOfExpr 规则表与迭代上限已定死（4 轮）；无 TBD。
3. **类型一致性**：FCTypeId（Task 1 产，全链消费）；returnType（Task 2 产，Task 5 codegen 消费）；checkTypes（Task 3 产骨架、Task 4 扩 Call 分支）；callReturns 形参贯穿 typeOfExpr 两调用点。
4. **Review Focus 归属**：1→Task 3（审核点）；2→Task 2 测试 d 例；3→Task 4 成功用例；4→Task 5 Step 3；5→Task 2 Step 1 空体断言 + Task 3 `var x:int = {}`。
5. **比例**：6 任务对应约 250 行实现 + 40 行测试，计划未代写函数体（typeOfExpr 规则表是设计决策非代码转录）。
6. **协调**：结构重构计划 Task 4 definitions 半项被本计划 Task 5 覆盖，双向注明（Global Constraints 末条 + Task 5 Step 2）。
