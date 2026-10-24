# Dangle 哨兵语义拆分（Void / Error）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `FCValueCategory::Dangle`（一词三义：错误哨兵 / 空块与定义语句的无值 / 默认初始化态）拆分为 `Void`（无值）与 `Error`（错误）两个独立类别，错误传播与无值传播规则分离，并补全"Error ⇒ 诊断非空"的耦合不变式。

**Architecture:** runtime.h 枚举拆分 + evaluator.cc 全部 Dangle 站点按归类表改写。传播规则变化集中在两处：Error 中止序列/块（保持现状），Void 流经（序列/块继续、可存入槽位）；`evaluateFor` 终止条件补类别检查，使 Error 可穿透循环（今日 union 误读把错误吞成常量 0）。静默错误站点的诊断补全独立成任务，用不变式测试收口。

**Tech Stack:** C++20（根 CMakeLists.txt:7 现值）、MSVC、Windows、LLVM 18（vcpkg）、PowerShell；仅标准库，无新依赖。

**Spec:** 2026-10-22 设计审查结论第 5 条（Dangle 一词三义）。相邻计划边界：结构化诊断（2026-10-23 计划）已合入工作区，本计划基于其后的行号；结构重构计划（2026-10-24）明确将本缺陷排除为"单独立项"，本计划即该立项。**不在范围**：FCValue 裸 union 与字符串所有权（缺陷 #4，含 `evaluteVal` 拼写）、语义期类型检查（#3）、stoi 崩溃（#2）。

## 与当前工作区的冲突与调整（已按实际状态修正）

**冲突 1（与 2026-10-24-fcvalue-variant-representation.md，计划 B）：** 计划 B 把 `FCValue` 裸 union 替换为 `std::variant<std::monostate, int, double, std::string>`，并声明“Dangle 类别语义原样保留”（monostate = Dangle）；本计划 A 把 `FCValueCategory::Dangle` 拆为 `Void` + `Error`，新增 `isVoid/isError/isValue`。两者同改 `runtime.h` 的值表示，不能并行独立执行。

**调整顺序：** 先执行计划 B（variant 表示，保留 Dangle 为 monostate 默认态），再在此基础上执行计划 A（把 Dangle 改名为 Void，追加 Error 类别与判定方法）。计划 A 的 `makeVoidValue`/`makeErrorValue` 替代计划 B 的 `makeDangleValue`；`context.diagnostics` 已存在（evaluator.h:27、scanner.h:30），无需新增通道，Task 2 的“删除无参 makeErrorValue”可直接落地。

**冲突 2（测试行号失效）：** 工作区已把 `unit_test.cc` 拆分为 `tests/` 目录下 9 个文件（`test_evaluator.cc`、`test_semantic.cc` 等）。计划中引用的 `unit_test.cc:593/606/617/863/912/939/960/997/1016/1048/1074/1081/2550/3360-3373/3379/3383/3388/3407/3417/3427/3438/3394/3399/3526` 均不再对应；`errorCases` 向量与 `3339` 条目需按内容在 `tests/test_evaluator.cc` 中重新定位。

**已完成（无需重复）：** `diagnostic.h` 与 `FCDiagnostics` 已合入；`FCEvaluationContext.diagnostics`（evaluator.h:27）、`FCScanner.diagnostics()`（scanner.h:30）、`SemanticAnalyzer(FCSemanticContext&, FC::FCDiagnostics&)`（semantic.h:135）均已存在，Task 1/2 假设的“结构化诊断通道已合入”成立。

- 行号基准：2026-10-24 工作区状态（`evaluator.cc` 仍为单文件，`FCValue` 仍为裸 union + Dangle，`unit_test.cc` 已拆分为 `tests/`）；结构重构计划 Task 0（测试拆分）已完成，测试改动按内容定位到 `tests/test_evaluator.cc`。
- 每任务收尾三连：`cmake --build build-debug --config Debug --parallel 16` 零错误；`ctest --test-dir build-debug -C Debug --output-on-failure` 全绿；`pwsh -NoProfile -File tools/coverage.ps1 -BuildDir build-debug -Configuration Debug` ≥ 95%。
- 行为基线：既有测试用例"通过"的结论不得翻转；断言目标值允许按归类表重指向（Dangle→Error 或 Void）。显式行为变化仅限传播规则表列出的三处（Void 流经、for-end 类别检查、if 条件 Error 传播），均以新测试钉住。
- 已有诊断站点的消息文本逐字保留（evaluator.cc:187/198/206/238/266/284/291/311/328/399）；新增诊断用本计划消息表的新文本。
- 术语规范（~/.dsh/AGENTS.md）：代码、注释、文档中 dangling/Dangle/悬空 全部清除，改用标准术语 Void（无值）/ Error（错误值）。
- 不动 codegen / semantic / lexer / parser（grep 证实它们不引用 FCValueCategory::Dangle）；`shadow_repro.cc` 只用 Integer/Floating 分支，不动。
- 历史文档（postmortems/、既有 plans/specs）不修改。

## 现状取证：Dangle 站点归类表（Task 1 的实现依据）

evaluator.cc 当前站点 → 新类别（传播 = 返回既有的 Error 值，不新造）：

| 函数（行号） | 语义 | 新类别 |
|---|---|---|
| makeDangleValue (16-21) | 哨兵构造 | 拆为 makeVoidValue / makeErrorValue |
| evaluateVariable 未绑定 (64-65) | 错误（静默） | Error |
| evaluateVariable 帧/槽无效 (69-70) | 错误（静默） | Error |
| evaluateIf 条件类别非法 (83) | 错误（静默） | Error |
| evaluateFor 未解析守卫 (91) | 错误（静默） | Error |
| evaluateFor 全局帧拒绝 (94) | 错误（静默） | Error |
| evaluateFor start/step 非 Integer (106) | 错误（静默） | Error |
| evaluateFor 槽越界 (111) | 错误（静默） | Error |
| evaluateFor 体传播 (118-119) | 错误传播 | 传播（仅 Error） |
| evaluateBlock 空初值 (127) | 空块无值 | **Void** |
| evaluateBlock 传播 (131-132) | 错误传播 | 传播（仅 Error） |
| evaluateSequence 默认初值 (140) | 空序列无值 | **Void**（默认构造即 Void） |
| evaluateSequence 传播 (143-144) | 错误传播 | 传播（仅 Error） |
| evaluateDeclaration 未解析 (152-153) | 错误（静默） | Error |
| evaluateDeclaration init 传播 (156-157) | 错误传播 | 传播（仅 Error；Void 存入槽位继续） |
| evaluateDeclaration 槽越界 (160-161) | 错误（静默） | Error |
| evaluateProgram 空初值 (169) | 仅含 def 的程序无值 | **Void** |
| evaluateBinary 赋值 LHS 非变量 (189) | 错误（有诊断） | Error |
| evaluateBinary 赋值 RHS 传播 (194) | 错误传播 | 传播（仅 Error；Void 存入继续） |
| evaluateBinary LHS 未绑定 (200) | 错误（有诊断） | Error |
| evaluateBinary 槽无效 (208) | 错误（有诊断） | Error |
| evaluateBinary 整型未知运算符 (238) | 错误（有诊断） | Error |
| evaluateBinary 浮点未知运算符 (266) | 错误（有诊断） | Error |
| evaluateBinary 类型失配兜底 (274) | 错误（静默） | Error |
| evaluateCall 函数不存在 (285) | 错误（有诊断） | Error |
| evaluateCall 实参数不符 (292) | 错误（有诊断） | Error |
| evaluateCall 实参传播 (299) | 错误传播 | 传播（仅 Error；Void 作实参传入） |
| evaluateCall 无编译记录 (311) | 错误（有诊断） | Error |
| evaluateCall 形参未解析 (328) | 错误（有诊断） | Error |
| evaluateCall 形参声明缺失 (331) | 错误（静默） | Error |
| 分派空表达式 (361) | 错误（静默） | Error |
| 分派帧内嵌套 def (399-401 前) | 错误（有诊断） | Error |
| 分派顶层 def 登记 (400-401) | 定义语句无值 | **Void** |
| 分派未知节点兜底 (405/408) | 错误（静默） | Error |

## 传播规则（新旧对照——唯一行为变化点）

| 载体 | 今日（遇 Dangle） | 新：遇 Error | 新：遇 Void |
|---|---|---|---|
| 序列 evaluateSequence | 中止，返回 Dangle | 中止，传播 Error | **继续**（末值取胜） |
| 块 evaluateBlock | 中止 | 中止，传播 | **继续** |
| 声明 init / 赋值 RHS | 返回 Dangle | 传播 Error | **存入槽位继续** |
| 调用实参 | 中止调用 | 中止调用 | **传入**（失配在体内暴露） |
| if 条件 | 类别非法→Dangle | 传播 Error | Error（条件必须数值） |
| for 终止条件 | **union 误读为 0，错误吞成正常退出**（116） | **传播 Error（新增类别检查）** | Error（同上） |
| for 体 | Dangle 中止 | 传播 Error | **继续**，循环结束返回 Void |
| evaluateProgram 逐语句 | 无检查，错误被后句覆盖（170-175） | **保持不变**（既有行为，无测试钉住，不属本缺陷） | 同左 |

## Review Focus

1. **Void 流经翻转**：序列/块遇空块从"中止"变"继续"。`(1; { }; 2)` 今日求值 Dangle，新方案 2。→ Task 1 测试 void-4。
2. **errorCases 3339 迁移**：`{"def f() { 1 }", "standalone function definition"}` 今日靠 isDangle 通过，新语义是 Void——留在 errorCases 会让 isError 检查失败。→ Task 1 Step 3 显式移除并迁入 void 用例。
3. **for-end 错误穿透**：`for i = 0, undefinedfn(), 1 in i` 今日返回 Integer 0（union 误读），新方案 Error。错误吞成错值是该缺陷最重的可执行后果。→ Task 1 测试 void-7。
4. **静默 Error 无诊断残留**：Task 1 过渡态有 13 处静默 Error；漏补一处则"Error ⇒ 诊断非空"破。→ Task 2 不变式测试逐路径断言。
5. **默认构造语义**：`FCValue` 默认从 Dangle 改 Void；`evaluateSequence` 的 `FCValue result;` 与 Frame 槽位未写读都落在默认态。核对该函数所有出口无"默认态被当错误"残留。→ Task 1 测试 void-2 + 既有槽位测试回归。

---

### Task 1: 枚举拆分 + 求值器全站点改写 + 既有断言重指向

**Files:**
- Modify: `Calculater/runtime.h`（枚举、FCValue 判定方法、union 成员名）
- Modify: `Calculater/evaluator.cc`（归类表全部站点 + evaluateFor 循环重构 + formatValue）
- Modify: `Calculater/main.cc`（printValue）
- Test: `Calculater/unit_test.cc`（断言重指向 + 新增 runEvaluatorVoidTests）

**Interfaces:**
- Produces（Task 2/3 依赖）:

```cpp
// runtime.h
enum struct FCValueCategory { Integer, Floating, String, Void, Error };
struct FCValue {
  FCValueCategory type = FCValueCategory::Void;   // 默认无值（原 Dangle 默认）
  FCValueUnion evaluteVal{};
  bool isVoid() const { return type == FCValueCategory::Void; }
  bool isError() const { return type == FCValueCategory::Error; }
  bool isValue() const;   // Integer/Floating/String 之一
};
// FCValueUnion::danglingVal 更名 voidPayload（Void/Error 载荷，恒 nullptr）
```

- Produces（evaluator.cc 匿名命名空间）:

```cpp
FCValue makeVoidValue();                                            // 无值
FCValue makeErrorValue();                                           // 错误（过渡态，Task 2 删除）
FCValue makeErrorValue(FCEvaluationContext&, std::string message);  // 错误 + 诊断
```

- 传播站点统一改写：`if (value.type == FCValueCategory::Dangle) return value;` → `if (value.isError()) return value;`（evaluator.cc:118、131、143、156、193、299 共 6 处）。
- evaluateFor 循环重构（算法体，签名不变）：

```cpp
while (true) {
  const auto end = evaluateExpression(node->getEnd(), context);
  if (end.isError()) return end;                       // 错误穿透循环
  if (end.type != FCValueCategory::Integer)
    return makeErrorValue();                           // Task 2 换带消息重载
  if (end.evaluteVal.intVal == 0) break;
  result = evaluateExpression(node->getBody(), context);
  if (result.isError()) return result;                  // 仅错误中止；Void 继续
  context.callStack[frameIdx].locals[slot].evaluteVal.intVal += step.evaluteVal.intVal;
}
return result;   // 体全 Void 时返回 Void
```

- Test: 新增 `runEvaluatorVoidTests()`，既有断言按下方重指向表改。

**断言重指向表（unit_test.cc 当前行号）:**

| 行 | 今日 | 改为 |
|---|---|---|
| 593/606/617（diag 12-14） | `== Dangle` | `.isError()` |
| 863 | `type != Dangle` | `.isValue()` |
| 877 | 文本 "returned Dangle" | "returned Error" |
| 912/939/960/997 | `== Dangle` | `.isError()` |
| 1016/1048 | `== Dangle` | `.isError()` |
| 1074 顶层 def 登记 | `== Dangle` | `.isVoid()`（标签 "still registers" 保留） |
| 1081 帧内嵌套 def | `== Dangle` | `.isError()` |
| 2550 | `== Dangle` | `.isError()` |
| 3360-3373 errorCases 循环 | `isDangle = (type == Dangle)` | `isError = result.isError()`（catch 分支同改 true）；**从向量删除 3339 条目** |
| 3379/3383/3388/3407/3417/3427/3438 | `== Dangle` | `.isError()` |
| 3394 空序列 / 3399 空块 | `== Dangle` | `.isVoid()` |
| 3526 | `type != Dangle` | `.isValue()` |
| 585/1077 注释 | "Dangle" | "Error" |

- [ ] **Step 1: 写失败测试——runEvaluatorVoidTests + errorCases 删 3339**

```cpp
void runEvaluatorVoidTests() {
    // 1. 空块 → Void（今日断言 Dangle，3399 同步改）
    // 2. 空序列 → Void（今日 3394 同步改）
    // 3. 仅含 def 的程序 → Void（原 errorCases 3339 迁入）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { 1 }");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        expect(evaluate(ast.get(), ctx).isVoid(),
               "void - definitions-only program yields Void");
    }
    // 4. 序列遇空块继续：(1; { }; 2) → 2（今日 Dangle 中止——行为变化点）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("(1; { }; 2)");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        auto v = evaluate(ast.get(), ctx);
        expect(v.type == FCValueCategory::Integer && v.evaluteVal.intVal == 2,
               "void - sequence continues past empty block");
    }
    // 5. for 体为空块：循环走完，结果 Void（今日 Dangle 中止）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { for i = 0, i < 2 in { } } f()");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        expect(evaluate(ast.get(), ctx).isVoid(),
               "void - for with empty-block body yields Void");
    }
    // 6. for 终止条件为错误 → Error 传播（今日 union 误读返回 0——行为变化点）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { for i = 0, undefinedfn(), 1 in i } f()");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        expect(evaluate(ast.get(), ctx).isError(),
               "error - for end condition error propagates");
    }
    // 7. if 条件为 Void → Error（今日 Dangle）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("if { } then 1 else 2");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        expect(evaluate(ast.get(), ctx).isError(),
               "error - void if condition is an error");
    }
    // 8. Void 存入槽位继续：var x:int = { }; x → Void（今日 Dangle 中止）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { var x:int = { }; x } f()");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        expect(evaluate(ast.get(), ctx).isVoid(),
               "void - void initializer stores and flows");
    }
}
```

`runAllTests()` 中调用（runEvaluatorEdgeCaseTests 之后）。同时按重指向表修改既有断言（与本步同批，编译耦合强制）。

- [ ] **Step 2: 运行确认失败**

Run: `cmake --build build-debug --config Debug --target FunctionsUnitTest --parallel 16`
Expected: 编译失败（`FCValueCategory::Void` / `isError` 不存在）——编译错误即红。

- [ ] **Step 3: 实现**

- runtime.h：按 Interfaces 块改枚举与 FCValue；`danglingVal` → `voidPayload`（evaluator.cc:20 同步）。
- evaluator.cc：按归类表逐站点改写；传播 6 处按统一改写；evaluateFor 按算法体重构；`formatValue`（43-46）的 Dangle 分支拆为 `"Void"` / `"Error"` 两分支；静默站点 Task 1 期一律 `makeErrorValue()`（无诊断重载，Task 2 收口）。
- main.cc printValue（30-31）：Dangle 分支拆为 Void → `"[result] Void"`、Error → `"[result] Error"`（诊断已在其前打印，main.cc:72/82 现状）。
- unit_test.cc：重指向表全部落实 + errorCases 向量删 3339 + 新测试。

- [ ] **Step 4: 运行确认通过**

Run: `cmake --build build-debug --config Debug --parallel 16 && .\build-debug\Calculater\Debug\FunctionsUnitTest.exe`
Expected: 新用例与全部既有用例 `[PASS]`（重指向后结论不翻转）。

- [ ] **Step 5: 三连验证**

Run: 构建 + `ctest --test-dir build-debug -C Debug --output-on-failure` + coverage.ps1。
Expected: 全绿，覆盖率 ≥ 95%。

- [ ] **Step 6: Commit**

```bash
git add Calculater/runtime.h Calculater/evaluator.cc Calculater/main.cc Calculater/unit_test.cc
git commit -m "refactor: split Dangle sentinel into Void (no value) and Error (failure)"
```

### Task 2: 静默错误站点诊断补全 + Error/诊断耦合不变式

**Files:**
- Modify: `Calculater/evaluator.cc`（归类表"静默"标记的 13 处 + makeErrorValue 重载收敛）
- Test: `Calculater/unit_test.cc`（新增 runErrorDiagnosticsInvariantTests）

**Interfaces:**
- Consumes: Task 1 的 `isError`/`isVoid`/`makeErrorValue(ctx, msg)`。
- Produces: 删除无参 `makeErrorValue()`；全部错误站点经由带诊断重载。不变式：**求值错误必然伴随诊断**——单语句语义单元 `evaluate` 返回 Error ⇒ `context.diagnostics` 非空；返回 Void ⇒ 无诊断。

**消息表（全部新文本，站点见 Task 1 归类表）:**

| 站点 | 消息 |
|---|---|
| evaluateVariable 未绑定 | `variable '<name>' is not bound to a symbol` |
| evaluateVariable 帧/槽无效 | `variable '<name>' has no valid slot` |
| evaluateIf 条件非法 | `if condition must be numeric` |
| evaluateFor 未解析 | `for loop variable is not resolved` |
| evaluateFor 全局帧拒绝 | `for loop variable must be local to a function` |
| evaluateFor start/step | `for start and step must be integers` |
| evaluateFor 槽越界 | `for loop variable slot is out of range` |
| evaluateFor end 非 Integer | `for end condition must be integer` |
| evaluateDeclaration 未解析 | `declaration is not resolved to a symbol` |
| evaluateDeclaration 槽越界 | `declaration slot is out of range` |
| evaluateCall 形参缺失 | `parameter declaration is missing` |
| evaluateBinary 失配兜底 | `binary operator requires numeric or string operands` |
| 分派空表达式 | `cannot evaluate a null expression` |
| 分派未知节点 | `unknown AST node kind` |

- [ ] **Step 1: 写失败测试**

```cpp
void runErrorDiagnosticsInvariantTests() {
    // 源文本可达的静默路径（今日 Error 无诊断）
    const std::vector<std::string> sources = {
        "if { } then 1 else 2",                                  // if 条件
        "for i = 0, i < 1 in i",                                 // 顶层 for
        "def f() { for i = 0, undefinedfn(), 1 in i } f()",      // for end
        "1 + \"s\"",                                             // 二元失配兜底
    };
    for (const auto& src : sources) {
        FCScanner scanner;
        auto ast = scanner.analysis(src);
        FCEvaluationContext ctx = makeEvalContext(scanner);
        FCValue v = evaluate(ast.get(), ctx);
        expect(v.isError() && !ctx.diagnostics.empty(),
               "invariant - error carries diagnostic: " + src);
    }
    // 手工 AST 路径（沿用 runEvaluatorEdgeCaseTests 既有构造模式）
    {
        // 未绑定变量读取（908-912 同款构造）
        ... expect(v.isError() && !ctx.diagnostics.empty(),
               "invariant - unbound variable carries diagnostic");
    }
    {
        // 空表达式：evaluate(nullptr, ctx)
        ... expect(v.isError() && !ctx.diagnostics.empty(),
               "invariant - null expression carries diagnostic");
    }
    // 反向：合法无值不产生诊断
    {
        FCScanner scanner;
        auto ast = scanner.analysis("(1; { }; 2)");
        FCEvaluationContext ctx = makeEvalContext(scanner);
        FCValue v = evaluate(ast.get(), ctx);
        expect(!v.isError() && !v.isVoid() && ctx.diagnostics.empty(),
               "invariant - clean value has no diagnostics");
        // 空块单独求值
        ...
        expect(v.isVoid() && ctx.diagnostics.empty(),
               "invariant - void carries no diagnostic");
    }
}
```

`runAllTests()` 中调用（runEvaluatorVoidTests 之后）。（`...` 处沿用既有构造语句，不新造 API。）

- [ ] **Step 2: 运行确认失败**

Run: 同 Task 1 Step 4。
Expected: invariant 用例 `[FAIL]`（静默站点无诊断）。

- [ ] **Step 3: 实现**

- 归类表"静默"13 处：`return makeErrorValue();` → `return makeErrorValue(context, "<消息表文本>");`；Task 1 中 evaluateFor end 检查的无参调用同样替换。
- 删除无参 `makeErrorValue()` 重载（编译器暴露残留调用点，逐一替换）。
- 既有带诊断站点不动（消息逐字保留）。

- [ ] **Step 4: 运行确认通过**

Run: 同 Task 1 Step 4。
Expected: invariant 用例 `[PASS]`；既有用例无回归（消息新增不进断言）。

- [ ] **Step 5: 三连验证**（同 Task 1 Step 5，覆盖率 ≥ 95%——新诊断分支须被 invariant 用例覆盖）。

- [ ] **Step 6: Commit**

```bash
git add Calculater/evaluator.cc Calculater/unit_test.cc
git commit -m "refactor: every evaluation error carries a diagnostic (Error/diagnostic invariant)"
```

### Task 3: 活文档同步 + 双配置收尾

**Files:**
- Modify: `README`（19、25 行值/错误行；472 行 API 注释 `// result.type 为 ... Dangle` → Void/Error）
- Modify: `docs/ARCHITECTURE.md`（19、25、90、204、255 行——运行时值表示与错误处理行）
- Modify: `docs/walkthrough/04-evaluator.md`（5、89-91、123、143、174、225、250、259 行——按新传播规则重写 Dangle 段落与 Q&A 答案）
- 不改: `docs/postmortems/`、既有 plans/specs（历史文档）。

**Interfaces:**
- Consumes: Task 1/2 的最终语义（传播规则表 + 不变式）。
- Produces: 活文档中 `Dangle` 计数归零；文档语义与代码一致（传播规则、`[result] Void/Error` 输出）。

- [ ] **Step 1: 文档改写**——README 值表加 Void/Error 语义说明；ARCHITECTURE 错误处理行改"错误值 Error + 诊断"；walkthrough 04 按传播规则表重写第 174 行起的"Dangle 来源"段与第 250 行 Q&A。
- [ ] **Step 2: 验收 grep**

Run: `grep -rn "Dangle" README docs/walkthrough docs/ARCHITECTURE.md`
Expected: 0 命中（postmortems 与 superpowers 目录除外）。

- [ ] **Step 3: 三连 + 双配置**

```powershell
cmake --build build-debug --config Debug --parallel 16
ctest --test-dir build-debug -C Debug --output-on-failure
pwsh -NoProfile -File tools/coverage.ps1 -BuildDir .\build-debug -Configuration Debug
pwsh -NoProfile -File .\tools\verify-configurations.ps1
```

Expected: 默认与 `FC_ENABLE_NESTED_FUNCTIONS=ON` 两配置构建 + 全量断言通过（嵌套模式走"帧内嵌套 def → Error（有诊断）/ 顶层 def → Void"双路径）。

- [ ] **Step 4: Commit**

```bash
git add README docs/ARCHITECTURE.md docs/walkthrough/04-evaluator.md
git commit -m "docs: document Void/Error value semantics, remove Dangle terminology"
```

---

## Self-Review

1. **Spec 覆盖**：三义逐一拆解——错误哨兵→Error（归类表 26 站点）、空块/定义无值→Void（4 站点）、默认初始化→FCValue 默认 Void（runtime.h）✓；错误可识别性由 isVoid/isError 判定 + 传播规则承载 ✓；诊断耦合为 Task 2 不变式 ✓。
2. **步骤歧义**：归类表、消息表、重指向表、传播规则表即实现者不可自决的全部决策；`...` 构造引用既有测试模式，避免转写失同步。
3. **类型一致**：`makeErrorValue(ctx, msg)` Task 1 产、Task 2 消费；`isVoid/isError/isValue` 各测试与 evaluator 内部统一；`voidPayload` 两文件同步。
4. **Review Focus 落点**：1→Task 1 void-4；2→Task 1 Step 1 重指向表 + void-3；3→Task 1 void-6；4→Task 2 invariant；5→Task 1 void-2 + 3394/3399。
5. **比例**：三任务；代码块仅 evaluateFor 算法体与测试，无函数体转写；计划/代码比约 1:2。

## Plan Verification

- **Depth**: STANDARD
- **Scope**: 4 个源/测试文件路径 + 3 个文档路径；FCValueCategory/FCValue/makeVoidValue/makeErrorValue 等 8 个 API/类型名；归类表 35 行、重指向表 14 行、传播规则 8 行均已对照当前工作区逐条核实（evaluator.cc 488 行版、unit_test.cc 3641 行版，2026-10-24 读取）。
- **Escalation**: 未升级——无常驻状态机拓扑变更声明；传播规则与不变式全部以测试锚定，未经形式验证（如需可对值流转 Void/Value/Error 三态机另行建模送 logicprobe_verify）。
