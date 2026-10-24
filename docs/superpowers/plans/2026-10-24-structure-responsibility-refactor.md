# 结构与职责重构实施计划（#6-9 修正版）

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复代码审查中严重度组 #6-9 的结构与职责缺陷：AST 分派三处重复、绑定与注册表多副本、token.h 职责混杂、死字段与帧清理不一致。修正版砍掉越界任务（FCValue variant、类型校验单点、构建卫生、测试拆分改为执行前置项）。

**Architecture:** 先做纯机械的头文件拆分（#8，建立分层，Task 0 执行前置项除外），再收敛死字段与布局合并（#9a），修复绑定判定与调用解析（#7），最后收敛 AST 分派并删除 I/O 职责（#6）。每个任务独立构建、ctest 全绿、行覆盖率 ≥ 95%。

**Tech Stack:** C++20（CMake 现值）、MSVC、CMake ≥ 3.20、LLVM 18（vcpkg）、PowerShell。仅标准库，无新依赖。

**Spec:** 2026-10-22 会话审查结论（对话记录），缺陷分组 #6-9（中严重度：结构与职责）四条原文。诊断通道（#1）已在工作区实现，不在范围。

## Global Constraints

- CMAKE_CXX_STANDARD 20（保持现状）；平台 Windows + MSVC + vcpkg LLVM 18；构建一律带 `--config Debug`。
- 每任务收尾三连必须满足：`cmake --build build-debug --config Debug --parallel 16` 零错误；`ctest --test-dir build-debug -C Debug --output-on-failure` 全绿；`pwsh -NoProfile -File tools/coverage.ps1 -BuildDir build-debug -Configuration Debug` ≥ 95% 且退出码 0。
- 行为兼容基线：现有测试断言的通过/失败结论不得翻转。
- 提交粒度：每任务一次提交，`refactor: <任务名>`。
- 嵌套函数开关语义不变。
- 术语规范：标准软件工程术语（~/.dsh/AGENTS.md）。

## Review Focus

1. **混合绑定可见性**：`boundSymbol` 用 `m_binding->size() != 0` 选数据源（evaluator.h:30-34），语义绑定非空时手工 bindNode 节点被静默忽略。→ Task 3 测试。
2. **函数解析优先级**：`evaluateCall` 查 registry（原型）与 compiledProgram（槽位）两表（evaluator.cc:279/306），不一致时行为未定义。→ Task 4 测试。
3. **codegen definitions 副本**：codegen.h:35 的名称→AST 映射与 compiledProgram.functions 内容同源，属第三份副本。→ Task 4 收敛评估。
4. **新增 AST 节点漏接**：三处 dynamic_cast 链新增节点漏一处即静默 fallthrough。→ Task 5 dispatchAST 穷尽性（编译期保证）。
5. **const 契约破坏**：codegen.cc:979-983 非 const 重载委托 const 版本后 const_cast。→ Task 5 codegen 入口 const 化验证。

---

### Task 0: 执行前置项——unit_test.cc 拆分（非缺陷任务，执行策略项）

**动机**：3641 行单文件使后续子代理上下文与 diff 审查膨胀；拆分后每个缺陷任务只触碰一个测试文件。用户已确认作为执行前置项。

**Files:**
- Create: `Calculater/tests/test_harness.h`、`Calculater/tests/test_main.cc`、`Calculater/tests/test_lexer.cc`、`test_parser.cc`、`test_semantic.cc`、`test_evaluator.cc`、`test_codegen.cc`、`test_ast_dump.cc`、`test_diagnostics.cc`、`test_registry.cc`
- Modify: `Calculater/CMakeLists.txt:89-97`、`tools/coverage.ps1:45,68`
- Delete: `Calculater/unit_test.cc`（内容按 run* 家族逐个搬移）

**Interfaces:**
- Produces: test_harness.h 导出 `expect(cond, name)`、`summary()`（返回进程退出码）、`makeEvalContext(scanner)`、`attachSemanticOutput(cc, scanner)`、`valueToString(value)`、`TestSuite` 类骨架。
- 拆分规则：runLexerTests→test_lexer.cc；runParserTests→test_parser.cc；runSemantic*/runSymbolTable*/runCompiledProgram*→test_semantic.cc；runEvaluator*/runRegistry*→test_evaluator.cc；runCodegen*→test_codegen.cc；runAstDump*/runAstInfo*/runTokenApi*→test_ast_dump.cc；runDiagnosticTests→test_diagnostics.cc。逐字搬移，不改断言。

- [ ] **Step 1: 提取 test_harness.h**（现 145-183 行辅助 + expect/计数）。
- [ ] **Step 2: 每搬一个家族构建+ctest 一次**（七次循环，任一失败即停）。
- [ ] **Step 3: coverage.ps1 正则更新**——`--include-files` 覆盖 `Calculater\*.cc` 与 `Calculater\tests\*.cc`；排除正则改 `(unit_test|call_ast_test|main|test_main)\.cc$` 加 `test_.*\.cc$`。运行确认 ≥ 95%，被排除清单逐文件核对不含生产代码。
- [ ] **Step 4: 三连验证** 全绿。
- [ ] **Step 5: Commit** `git commit -m "refactor: split unit_test.cc by module as execution precondition"`

### Task 1: 头文件拆分（token.h → token.h + ast.h + symbol.h）

**Files:**
- Create: `Calculater/ast.h`、`Calculater/symbol.h`
- Modify: `Calculater/token.h`（缩到词法枚举）、`runtime.h`（删 VariableRuntimeInfo，移 symbol.h）、`semantic.h`、`evaluator.h`、`codegen.h`、`parser.h`、`scanner.h`、`ast_dump.h/.cc`、`semantic_dump.cc`、`evaluator.cc`、`semantic.cc`、`codegen.cc`、`main.cc`、`shadow_repro.cc`、`Calculater/CMakeLists.txt`
- Rename: `token.cc` → `ast.cc`（内容全为 AST 节点构造/打印）

**Interfaces:**
- Produces:
  - `token.h`：FCToken、FCBinaryOp、binaryOpOf，仅此。
  - `ast.h`：`FCExprClass::VarDecl`（自 token.h 移入）、FCExprAST 家族、FCTypeDescribe（Task 6 才删）。包含 token.h。
  - `symbol.h`：VariableRuntimeInfo（自 runtime.h 移入）、VariableSymbol、SymbolTable（6 成员函数 inline）、CompiledFunction、SemanticBinding、CompiledProgram。包含 ast.h。
  - `SemanticBinding` 头部加生命周期契约注释：键为 AST 节点地址；AST 必须存活到绑定侧表销毁。
- `semantic.h:34` 的 `using VarDeclPtr = ...` 与 token.h:97 重复声明删除一处。

- [ ] **Step 1: 基线三连**（记录覆盖率基线值）。
- [ ] **Step 2: 创建 ast.h/symbol.h，token.h 缩减**（SymbolTable 从 semantic.cc 移入 symbol.h inline 化）。
- [ ] **Step 3: git mv token.cc ast.cc**；`grep -n '#include "token.h"'` 逐个判定改 ast.h 或 symbol.h。
- [ ] **Step 4: CMakeLists 源列表更新**（token.h/.cc → token.h、ast.h、ast.cc + symbol.h 入 FunctionsCore）。
- [ ] **Step 5: 三连验证**（覆盖率 ≥ 基线）。
- [ ] **Step 6: Commit** `git commit -m "refactor: split token.h into token/ast/symbol headers"`

### Task 2: 删除死字段，合并 FrameLayout/GlobalLayout 为 SlotLayout（#9a）

**Files:**
- Modify: `semantic.h:43-83`（布局类）、`semantic.cc:190-191,481`、`symbol.h`（VariableSymbol 字段，Task 1 后位置）、`semantic_dump.cc:42`、`unit_test→tests/test_semantic.cc:846,1067,2434,2625`（行号以拆分后为准，按内容定位）。

**Interfaces:**
- Produces: `struct SlotLayout { int allocateSlot(); int size() const; }`。
- Deletes: `VariableSymbol::isMutable`、`isCaptured`（symbol.h，恒 false 无写入方）；`CompiledFunction::maxTempSlots`（恒等于 frameSize）；`FCSemanticContext::currentFrameLayoutFrameSize` 别名。
- semantic_dump 输出去掉 `maxTempSlots=` 字段；字符串断言同步更新。

- [ ] **Step 1: 基线三连**。
- [ ] **Step 2: 删字段与别名，合并布局类**（m_frameLayouts 元素类型改 SlotLayout；frameSize()/globalSize() 调用点改 size()；测试中 maxTempSlots 直接写入与断言删除或改断 frameSize）。
- [ ] **Step 3: 三连验证**（死代码删除只提高覆盖）。
- [ ] **Step 4: Commit** `git commit -m "refactor: drop dead symbol fields, merge slot layouts"`

### Task 3: boundSymbol 判定修复（#7a：语义绑定优先、本地绑定兜底）

**Files:**
- Modify: `evaluator.h:30-34`
- Test: `tests/test_evaluator.cc`（Evaluator Edge Case 区）

**Interfaces:**
- Produces: `boundSymbol`——先查 `compiledProgram.m_binding->find(node)`，为空再查 `localBinding.find(node)`；删除 size()!=0 启发式。codegen.h:43-48 显式模式（semantic 指针非空即用语义通道）保持不变。

- [ ] **Step 1: 写失败测试**——scanner 产出语义绑定（非空）后 bindNode 手工节点，期望 evaluate 读到手工符号（当前 Dangle）。
- [ ] **Step 2: ctest 确认红**。
- [ ] **Step 3: 实现新判定**。
- [ ] **Step 4: 三连验证**。
- [ ] **Step 5: Commit** `git commit -m "refactor: deterministic binding precedence in evaluator"`

### Task 4: evaluateCall 单一路径 + codegen definitions 收敛（#7b）

**Files:**
- Prototype tasks: 3 (TCandidateObjects/descriptor requirement mismatch)
- Test: `tests/test_evaluator.cc`、`tests/test_codegen.cc`

**Interfaces:**
- Produces: `evaluateCall` 解析顺序——
  1. `compiledProgram.getFunction(name)` 有记录且 `record->ast != nullptr` ⇒ 原型与槽位全部取自该记录；
  2. 否则回退 `context.functions.findFunction(name)`（手工 AST 模式）。
  一次调用只查一条路径。
- codegen `definitions`（codegen.h:35）评估：`codegenProgram`（codegen.cc:785）填充后仅用于 `inferExprType` 的 `FCCallExprAST` 分支查返回类型。收敛方案——`inferFunctionReturnType` 改为 `context.compiledProgram.getFunction(name)->ast`，若语义记录缺失则回退 `module->getFunction(name)->getReturnType()`；`definitions` 成员与填充代码删除。**评估结论为可行则实施；若语义记录缺 ast（手工构造 AST 的 codegen 测试路径），保留回退链后删除成员**。
- [ ] **Step 1: 写失败测试**——同名函数在 registry 与 compiledProgram 且原型参数个数不同；期望按 compiledProgram 记录执行（当前原型取 registry、槽位取记录）。
- [ ] **Step 2: ctest 确认红**。
- [ ] **Step 3: 实现 evaluateCall 顺序**。ghost（仅 registry）与 hollow（记录存在但符号表被清空）用例必须保持通过。
- [ ] **Step 4: definitions 收敛**——按 Interfaces 评估结论实施；test_codegen.cc 中依赖 definitions 的用例同步改。
- [ ] **Step 5: 三连验证**。
- [ ] **Step 6: Commit** `git commit -m "refactor: single call-resolution path, drop codegen definitions copy"`

### Task 5: dispatchAST 单一分派模板 + codegen 入口 const 化（#6a）

**Files:**
- Create: `ast.h` 内新增 `dispatchAST`（同文件）
- Modify: `semantic.cc:330-361`、`evaluator.cc:354-405`、`codegen.cc:923-977`、`ast_dump.cc:58-158`、`codegen.h:20-22`（入口签名）
- Test: 不改断言（行为等价迁移）；穷尽性用编译期验证

**Interfaces:**
- Produces:

```cpp
// ast.h —— 唯一节点分派点。Visitor 提供 const 重载集（12 节点类型）。
// 新增 AST 节点而访客缺重载 ⇒ 编译失败（穷尽性由重载决议保证）。
template <class Visitor>
decltype(auto) dispatchAST(const FCExprAST& node, Visitor& v);
```

- **codegen 入口 const 化**（消灭 const_cast，codegen.cc:979-983）：`codegen(const FCExprAST*)` 作为唯一入口；codegen 内部需要写 pointer 值（`CreateGlobalStringPtr` 等接受常量）不需 mutable 节点——评估结论：LLVM API 不修改 AST，直接全链路 const 化，删除两个 `codegen(FCExprAST*)` mutable 重载与 const_cast 委托。evaluator.cc:400 的 `const_cast<FCFunctionAST*>`（registry 登记路径）与 :167 的 `index` 调用同步改 const 正确性（registry 存 `FCFunctionAST*`，语义期已注册函数时跳过登记——按 Task 4 后的路径，语义注册的函数不再走 registry 登记分支）。
- 每个消费方改“访客结构体 + 12 visit 重载”，重载体一行委托既有处理函数。null 检查保留在各入口。
- [ ] **Step 1: ast.h 写 dispatchAST**（内部一条 dynamic_cast 链，全仓库仅此一条）。
- [ ] **Step 2: 四消费方逐个迁移**（semantic → ast_dump → evaluator → codegen，每迁一个构建零错误）。
- [ ] **Step 3: 穷尽性验证**——ast.h 临时加空 struct 测试访客缺一重载，确认编译失败后还原。
- [ ] **Step 4: codegen const 化**——删 mutable 重载对与 const_cast；main.cc 与 tests 调 `codegen(const …)` 版本。
- [ ] **Step 5: 三连验证**（覆盖率 ≥ 95%）。
- [ ] **Step 6: Commit** `git commit -m "refactor: single dispatchAST authority, const-correct codegen entry"`

### Task 6: 删除 info() 虚函数、FCTypeDescribe、遮蔽 type 成员、独立 Prototype codegen（#6b）

**Files:**
- Modify: `ast.h`（删 info() 纯虚、各节点 type 成员、FCTypeDescribe）、`ast.cc`（删全部 info() 实现）、`ast_dump.cc`（新增 nodeKindName）、`codegen.cc`（删 codegenPrototype:454-459 与分派分支:938-939、codegenTopLevel Prototype 分支:992-995）、`codegen.h:26` 注释、`tests/test_ast_dump.cc`（runAstInfoTests 迁移）、`tests/test_codegen.cc`

**Interfaces:**
- Produces: `const char* nodeKindName(const FCExprAST&)`——经 dispatchAST 返回 "Number"/"String"/"Variable"/"Binary"/"Call"/"Prototype"/"Function"/"If"/"For"/"VarDecl"/"Seq"/"Block"/"Program"。测试 `->type == FCTypeDescribe::X` 改 `std::string_view(nodeKindName(*node)) == "X"`。
- Deletes: info() 纯虚 + 12 实现（I/O 已由 describeAst 承担）；派生类遮蔽基类的 type 成员；FCTypeDescribe 枚举及枚举值断言（test_ast_dump.cc:1632-1639 对应段）；codegenPrototype 路径（parseProgram 从不产出独立 Prototype 顶层节点；FCPrototypeAST 结构体保留为 FunctionAST 组成部分）。
- [ ] **Step 1: ast.h/ast.cc 删 info 与 type 成员；ast_dump 加 nodeKindName**。
- [ ] **Step 2: 测试迁移**——runAstInfoTests 改 describeAst/nodeKindName 断言或并入 runAstDumpTests；`->type ==` 断言按规则改写。
- [ ] **Step 3: codegenPrototype 删除**——test_codegen.cc 中直接 codegen(prototype) 或断言独立原型 codegen 的用例删除，其余保留。codegenTopLevel 类型判定缩为 Program/Function。
- [ ] **Step 4: 三连验证**。
- [ ] ** precondition: Task 5 已合并（dispatchAST 已存在）。**
- [ ] **Step 5: Commit** `git commit -m "refactor: remove info() IO, FCTypeDescribe, dead prototype codegen"`

### Task 7: evaluator FrameGuard RAII——消除手工 popFrame（#9b 补遗）

**Files:**
- Create: `evaluator.h` 内 `struct FrameGuard`（仿 semantic.h ScopeGuard 模式）
- Modify: `evaluator.cc:302-352`（evaluateCall 帧生命周期）、`evaluator.cc` popFrame 声明保留（公共 API）
- Test: `tests/test_evaluator.cc`

**Interfaces:**
- Produces: `FrameGuard`——构造 `pushFrame(name)`，析构 `popFrame()`；evaluateCall 以栈对象持有帧生命周期，5 处错误路径手工 `context.popFrame()` 全部删除。异常安全：求值器抛异常时帧栈回滚（当前手工版异常路径泄漏帧）。
- [ ] **Step 1: 写失败测试**——深递归 + 语义期诊断路径触发异常前帧栈状态？**修正**：求值器不抛异常（返回 Dangle 协议），测试改为——构造调用错误（参数个数不符）后**继续**evaluate 后续表达式，断言帧栈深度恢复 1（当前错误路径 popFrame 各自为政，但单次错误也已 pop；此测试钉住 RAII 重构不引入回归）。红阶段：直接写实现前先断言现状可过（这是回归保护测试，非缺陷暴露测试，跳过红步骤，标注原因）。
- [ ] **Step 2: 实现 FrameGuard**；evaluateCall 内 5 处手工 popFrame 替换为 Guard 作用域。
- [ ] **Step 3: 三连验证**。
- [ ] **Step 4: Commit** `git commit -m "refactor: FrameGuard RAII for evaluator frames"`

---

## 已知事项（本计划不做）

- **NDEBUG assert**（semantic.cc:48 帧栈不变式）：正常路径已有 mayDefineFunctionHere 前置检查，assert 仅防绕过 analyzer 的 API 误用；接受。
- **Dangle 哨兵三元合一 / FCValue variant**（高严重度 #4）、**语义期类型检查器**（#3）、**stoi 崩溃**（#2）：超出 #6-9，单独立项。
- **构建卫生**（双 project、vcpkg.json 名称、.gitignore、/W4、残留日志）：低严重度组，单独立项。
- **scopeDepth 除 dump 无消费者**（#9 原文列出但语义期作用域深度是绑定判定的调试数据且 dump 测试消费它）：保留，理由——删除会破坏 dump 输出契约与 shadow_repro 复现能力；如需删，随 dump 格式修订一起评估。

## Self-Review 记录

范围核对（对照 #6-9 原文四条）：
- #6 分派三链 → Task 5；成员遮蔽/死字段 type → Task 6；info() I/O → Task 6；Prototype 弃用路径 → Task 6；**const_cast → Task 5 Step 4（修正版新增）**。
- #7 SemanticBinding 生命周期契约 → Task 1 注释；三份映射 → Task 4（evaluator 双表 + codegen definitions，修正版新增后者）；boundSymbol 启发式 → Task 3。
- #8 token.h 大杂烩 → Task 1。
- #9 死字段 → Task 2；assert → 已知事项（决策）；帧清理不一致 → **Task 7（修正版新增）**。
越界项移除：FCValue variant（#4）、类型校验单点（#3）、构建卫生（低危）、测试拆分降级为 Task 0 执行前置项。
类型一致性：SlotLayout.size（Task 2 产、自消费）；dispatchAST（Task 5 产、Task 6 nodeKindName 消费）；FrameGuard（Task 7 产、evaluator.cc 消费）。
Review Focus 五项归属：1→Task 3；2→Task 4；3→Task 4；4→Task 5 Step 3；5→Task 5 Step 4。
Task 7 红步骤缺失的补偿：回归保护测试 + 既有 3641 行测试全套照跑（三连约束）。
