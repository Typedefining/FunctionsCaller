# FCValue 值表示迁移（裸 union → std::variant）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用 `std::variant<std::monostate, int, double, std::string>` 替换 `FCValue` 的裸 union + `new FCStringValue` 表示，消灭内存泄漏与浅拷贝指针别名，并使所有运行时值读取类别安全。

**Architecture:** 判别式（variant 下标）成为值类别的唯一存放地，`category()` 从它推导 `FCValueCategory`（不再有 `type` 成员与 union 的两份事实）。字符串按值持有（`std::string` 成员），赋值/传参边界为深拷贝，所有权随值走，无手工 delete。类型不匹配的读取经 `asInteger()/asFloating()/asString()` 返回 `nullptr`，不再读非活跃 union 成员。

**Tech Stack:** C++20（根 CMake `CMAKE_CXX_STANDARD 20`）、std::variant/std::monostate/std::get_if（C++17 起可用）、MSVC /utf-8、CMake + ctest。

**Spec:** 本文档自含规格；缺陷证据见对话分析：`Calculater/runtime.h:18-35`（`new FCStringValue`，全仓库 grep 无 `delete`）、浅拷贝别名点 `evaluator.cc:71/161/208/301/333`、类别未检读 `evaluator.cc:115`（for 终止条件直接读 `.intVal`）。

## 与当前工作区的冲突与调整（已按实际状态修正）

**冲突 1（与 2026-10-24-dangle-sentinel-disambiguation.md，计划 A）：** 计划 A 要把 `FCValueCategory::Dangle` 拆为 `Void` + `Error`，追加 `isVoid/isError/isValue`；本计划 B 把裸 union 替换为 `std::variant<std::monostate, int, double, std::string>`，并声明“Dangle 类别语义原样保留”（monostate = Dangle）。两者同改 `runtime.h` 的值表示，不能并行独立执行。

**调整顺序：** 先执行本计划 B（variant 表示，保留 Dangle 为 monostate 默认态，`.type` 改为 `category()`，`.evaluteVal.xxx` 改为 `asInteger()/asFloating()/asString()`），再在此基础上执行计划 A（Dangle 改名为 Void，追加 Error）。计划 A 的 `makeVoidValue`/`makeErrorValue` 替代本计划保留的 `makeDangleValue`；`context.diagnostics` 已存在（evaluator.h:27、scanner.h:30），计划 B Step 3 中 `context.diagnostics.add(...)` 可直接使用（现已合入）。

**冲突 2（测试行号失效）：** 工作区已把 `unit_test.cc` 拆分为 `tests/` 目录下 9 个文件。计划中引用的 `unit_test.cc:177-190/2524/2532-2550/3547/3615-3616` 均失效；`valueToString`（test_harness.h:96-110，非 unit_test.cc:177）和 `FCValue{}`（测试按 content 定位）需在 `tests/test_harness.h` / 对应 `tests/test_*.cc` 中重新定位。

**已完成（无需重复）：** `diagnostic.h` 与 `FCDiagnostics` 已合入（diagnostic.h 全文；FCEvaluationContext.diagnostics evaluator.h:27）；`dispatchAST`（ast.h:210-226）已存在；`ast.h/ast.cc`、`symbol.h` 已拆分完成。token.h 已仅保留词法类别与二元运算符映射，`FCTypeDescribe`/`FCExprAST` 家族已移至 ast.h。

**冲突 3（接口已部分超前）：** `getType(const std::string&)`（codegen.h:53）仍是字符串版本，与计划 B"只换表示，FCValueCategory 枚举不变"一致；但 `FCValue`（runtime.h:31-35）仍为裸 union + Dangle，`makeStringValue`（evaluator.cc:24-29）仍用 `new FCStringValue`，`evaluator.cc:115` for 终止条件仍直接读 `.evaluteVal.intVal`——本计划的全部改动均可在当前代码上直接落地，无阻塞。

- Dangle 的语义重构（错误哨兵/空块结果/未定义值一词三义）——Dangle 类别语义原样保留；**与计划 A 的次序见“调整顺序”**。
- 整数除零防护（evaluator.cc:231 与 codegen `CreateSDiv`）——属类型检查缺陷，另行处理；
- visitor 模式替换 `dynamic_cast` 链、`isMutable`/`isCaptured` 等死字段清理、codegen 任何改动；
- `FCValueCategory` 枚举改名或重排——词汇表不动，只换表示。（后与计划 A 协调时，Dangle 可改名为 Void，新增 Error，enumeration 本身仍不重排，仅增项。）

## Global Constraints

- 只修改 FCValue 表示及其直接消费点；`FCValueCategory` 枚举名与顺序不变。
- `FCValue{}` 必须默认可构造且等于 Dangle（unit_test.cc:2524 依赖）。
- 迁移不改变任何已有测试断言的期望值（已定义行为全部保持）。
- 每个任务收尾有可复现证据：构建输出、ctest 输出、grep 输出（AGENTS.md 证据规范）。
- 构建环境：现有 `build-debug` 目录，`cmake --build build-debug --config Debug --parallel 16` + `ctest --test-dir build-debug -C Debug --output-on-failure`。
- 提交信息与文档遵守 ~/.dsh/AGENTS.md 术语规范（标准术语，无自造意象词）。

## Review Focus（规格未显式钉住、迁移最可能咬人的输入，每条已配测试）

1. for 终止条件为非数值（String）或 Dangle → 现在：Dangle + 诊断；不再无限循环/吞错。→ Task 2 测试 A
2. 循环体内把循环变量赋成非 int（`for i = 0, i < 3, 1 in i = "s"`）→ 增量读取按类别安全处理，返回 Dangle，不崩溃、不读垃圾。→ Task 2 测试 B（旧代码此输入会读指针位模式，无法预写）
3. 字符串跨帧传递（函数返回字符串、参数传字符串）→ 值语义深拷贝，帧弹出后结果仍有效。→ Task 2 测试 C
4. 字符串赋值独立性（`var b:string = a` 后 `b = b + "!"` 不影响 `a`）→ 钉死值语义，防未来退回共享缓冲实现。→ Task 2 测试 D
5. 大量字符串求值泄漏 → 新表示构造上无 `new`（grep 证据 + 既有套件回归），而非运行期内存断言。

## 备选方案（已否决）

- `std::shared_ptr<std::string>`：仍每串一次堆分配 + 引用计数开销，API 改动量相同，收益为负。
- 字符串驻留池/arena：当前规模下过度设计（YAGNI）；出现性能证据再升级。

---

### Task 1: FCValue 表示替换 + 全消费点迁移（编译原子任务）

**Files:**
- Modify: `Calculater/runtime.h`（全文重写 FCValue 段，`Frame`/`FCValueCategory`/`VariableRuntimeInfo` 不动）
- Modify: `Calculater/evaluator.cc`（约 40 处读写点）
- Modify: `Calculater/main.cc:17-33`（printValue）
- Modify: `Calculater/shadow_repro.cc:26-28`
- Modify: `Calculater/unit_test.cc:177-190, 2524, 2532-2550, 3547, 3615-3616`

**Interfaces:**
- Produces（后续任务与钉住测试依赖的唯一 API）：

```cpp
struct FCValue
{
    std::variant<std::monostate, int, double, std::string> storage;

    FCValueCategory category() const;   // 下标→类别的唯一映射点
    const int*         asInteger()  const;  // 类别不匹配返回 nullptr，不抛异常
    const double*      asFloating() const;
    const std::string* asString()    const;
};
```

- 构造：`FCValue{}`（Dangle）、`FCValue{42}`、`FCValue{2.5}`、`FCValue{std::string("x")}`（聚合初始化经 variant 转换构造）。
- `.type` 成员不存在了：所有 `v.type == FCValueCategory::X` → `v.category() == FCValueCategory::X`；所有 `v.evaluteVal.intVal` → `*v.asInteger()`（读取前必须有类别检查或空指针检查）。

**说明（任务边界为何这样切）：** runtime.h 重定义后所有引用 `.evaluteVal` 的翻译单元立即编译失败，无法按文件分任务渐进。本任务必须一次性迁移全部消费点才可编译；风险由步骤顺序（先底层后上层）与完整回归套件兜住。

- [ ] **Step 1: 重写 runtime.h 的值表示段**

删除 `FCStringValue`、`FCValueUnion`、`FCValue::type`、`FCValue::evaluteVal`。新表示（完整代码，逐字使用）：

```cpp
#pragma once

// 解释器运行时值表示。不依赖 token/ast/symbol（分层最底层）。
// FCValue 为值语义：判别式（std::variant 下标）是类别的唯一存放地，
// 字符串按值持有，无堆指针别名、无手工生命周期。

#include <string>
#include <variant>
#include <vector>

namespace FCMarks
{
	enum struct FCValueCategory
	{
		Integer,
		Floating,
		String,
		Dangle
	};

	// monostate = Dangle（未定义/错误哨兵，语义与原实现一致）
	struct FCValue
	{
		std::variant<std::monostate, int, double, std::string> storage;

		FCValueCategory category() const {
			if (std::holds_alternative<int>(storage))
				return FCValueCategory::Integer;
			if (std::holds_alternative<double>(storage))
				return FCValueCategory::Floating;
			if (std::holds_alternative<std::string>(storage))
				return FCValueCategory::String;
			return FCValueCategory::Dangle;
		}

		// 类型安全读取：类别不匹配返回 nullptr（不抛异常）
		const int* asInteger() const { return std::get_if<int>(&storage); }
		const double* asFloating() const { return std::get_if<double>(&storage); }
		const std::string* asString() const { return std::get_if<std::string>(&storage); }
	};

	struct Frame
	{
		std::string funcName;
		std::vector<FCValue> locals;  // 按槽位存放值；全局帧与调用帧统一按 slot 下标访问
	};
}
```

- [ ] **Step 2: 迁移 evaluator.cc 构造点（7 处）**

- `makeDangleValue()`（:16-21）改为 `return FCValue{};`（保留函数，25+ 调用点）。
- 删除 `makeStringValue()`（:23-28）；调用点改为直接构造。
- `evaluateNumber`（:49-59）→ `return FCValue{node->m_doubleVal};` / `return FCValue{node->m_intVal};`。
- for 默认步长（:96-98）→ `FCValue step = node->getStep() == nullptr ? FCValue{1} : evaluateExpression(node->getStep(), context);`。
- 整数/浮点二元运算各分支（:218-221, :245-247）→ 结果直接 `FCValue{<表达式>}`。
- 字符串拼接（:271-274）→ `return FCValue{*lhs.asString() + *rhs.asString()};`。
- 字符串字面量（:366-367）→ `return FCValue{node->m_stringVal};`。

- [ ] **Step 3: 迁移 evaluator.cc 读取点（机械替换 + evaluateFor 重写）**

机械替换（读取前已有类别检查的分支）：
- `formatValue`（:31-47）：`std::to_string(*value.asInteger())`；`snprintf %g *value.asFloating()`；`"\"" + *value.asString() + "\""`。原 `charVal ? ... : "\"\""` 空指针分支删除——String 类别必持有 string，该状态不可表示。
- `evaluateIf`（:77-80）、`evaluateBinary` 两个数值分支的操作数读取（:222-234, :249-262）、所有 `result.type == FCValueCategory::X` → `result.category() == FCValueCategory::X`（:77, :79, :103-104, :117, :130, :142, :155, :192, :216-217, :243-244, :271-272, :299）。

`evaluateFor`（:87-122）重写——这是本计划唯一的算法决定点，完整代码逐字使用（终止条件语义见"审核要点 1"；循环计数经槽位读改写，循环体可把循环变量赋成非 int，按空指针检查处理）：

```cpp
FCValue evaluateFor(const FCForExprAST *node, FCEvaluationContext &context) {
  std::shared_ptr<VariableSymbol> forSymbol = context.boundSymbol(node);
  if (node->getDecl() == nullptr || forSymbol == nullptr || context.callStack.empty())
    return makeDangleValue();
  // for 循环变量应为局部变量，不应落在全局帧（frameIndex == -1）上
  if (forSymbol->runtime.frameIndex == -1)
    return makeDangleValue();
  const auto start = evaluateExpression(node->getStart(), context);
  FCValue step = node->getStep() == nullptr
                     ? FCValue{1}
                     : evaluateExpression(node->getStep(), context);

  if (start.category() != FCValueCategory::Integer ||
      step.category() != FCValueCategory::Integer)
    return makeDangleValue();
  const int stepValue = *step.asInteger();

  const int slot = forSymbol->slot;
  size_t frameIdx = context.callStack.size() - 1;
  if (slot < 0 || slot >= static_cast<int>(context.callStack[frameIdx].locals.size()))
    return makeDangleValue();

  context.callStack[frameIdx].locals[slot] = start;

  FCValue result = start;
  while (true) {
    const auto end = evaluateExpression(node->getEnd(), context);
    bool truthy;
    if (const int *e = end.asInteger())
      truthy = *e != 0;
    else if (const double *e = end.asFloating())
      truthy = *e != 0.0;
    else {
      // 终止条件必须数值（与 if 条件同一规则）；旧实现读 union 位模式
      context.diagnostics.add("evaluator", {},
                              "for end condition must be numeric");
      return makeDangleValue();
    }
    if (!truthy)
      break;
    result = evaluateExpression(node->getBody(), context);
    if (result.category() == FCValueCategory::Dangle)
      return result;
    auto &counter = context.callStack[frameIdx].locals[slot];
    const int *current = counter.asInteger();
    if (current == nullptr) {
      // 循环体可能把循环变量赋成非 int；旧实现此处做指针位模式整数加法
      context.diagnostics.add("evaluator", {},
                              "for loop variable must stay integer");
      return makeDangleValue();
    }
    counter = FCValue{*current + stepValue};
  }
  return result;
}
```

- [ ] **Step 4: 迁移 main.cc printValue（:17-33）**

switch 保留（按 `value.category()`），三个读取改为 `*value.asInteger()` / `*value.asFloating()` / `value.asString()->c_str()`（String case 中 asString() 必非空，可直接解引用）。

- [ ] **Step 5: 迁移 shadow_repro.cc（:26-28）**

同 Step 4 模式：`v.asInteger()` 空指针判断代替类别判断亦可（现文件用 if-else 链，改为 `if (const int* i = v.asInteger()) ... else if (const double* d = v.asFloating()) ...`，保持输出格式不变）。

- [ ] **Step 6: 迁移 unit_test.cc 读取点**

- `valueToString`（:177-190）：类别判断改 `value.category()`，读取改 `*value.asInteger()` / `*value.asFloating()` / `*value.asString()`。浮点的"去尾零"逻辑（:183-187）逐字保留。
- `:2524` `FCValue{}` 不变（仍默认构造为 Dangle）。
- `:2532` `*add.asInteger() == 5`；`:2536` 同；`:2541` `*floating.asFloating() == 2.5`。
- `:2545-2546` → `text.category() == FCValueCategory::String && *text.asString() == "xy"`（原 charVal 非空判断删除，String 类别即持有值）。
- `:3547` `*r2.asInteger() == 42`。
- `:3615-3616` 同 Step 5 模式。

- [ ] **Step 7: 构建**

Run: `cmake --build build-debug --config Debug --parallel 16`
Expected: 0 error。若 MSVC 报 variant 歧义构造，检查是否误写 `FCValue{"字面量"}`——字符串一律经 `std::string` 显式路径。

- [ ] **Step 8: 全量回归**

Run: `ctest --test-dir build-debug -C Debug --output-on-failure`
Expected: FunctionsUnitTest 全绿（含 for/循环/字符串/递归/压力全部既有断言，期望值无一改变）。

- [ ] **Step 9: 泄漏与残留证据（grep）**

Run: `pwsh -c "Get-ChildItem Calculater -Include *.cc,*.h -Recurse | Select-String -Pattern 'evaluteVal|FCValueUnion|FCStringValue|new FCString'"`
Expected: 0 代码命中（本计划文档与历史日期文档除外）。此即泄漏修复证据：新表示构造上不存在堆分配点。

- [ ] **Step 10: Commit**

```bash
git add Calculater/runtime.h Calculater/evaluator.cc Calculater/main.cc Calculater/shadow_repro.cc Calculater/unit_test.cc
git commit -m "refactor: replace FCValue bare union with std::variant value semantics

- FCValue holds monostate/int/double/string by value; discriminant is
  the single source of the value category (category() derives it)
- removes per-evaluation heap allocation of FCStringValue (leak) and
  shallow-copy pointer aliasing across slots/frames/args
- all runtime reads are category-checked via asInteger/asFloating/asString
- for-loop end condition now requires a numeric value (was: raw union
  member read); non-integer loop counter after body assignment yields
  Dangle instead of pointer-bit-pattern arithmetic"
```

### Task 2: 语义钉住测试（迁移后行为的回归防线）

**Files:**
- Modify: `Calculater/unit_test.cc`（evaluator 测试区，建议加在 runEvaluatorErrorTests / runEvaluatorTests 内与既有用例同构的位置）

**Interfaces:**
- Consumes: Task 1 的 `FCValue` API（`category()`、`asInteger()` 等）与既有 `evaluateOf` lambda（:2520-2524）或 FCEvaluationContext 直连模式（:3520-3526 风格）。

- [ ] **Step 1: 新增四个测试（复用文件内既有 expect/evaluateOf 风格）**

测试 A（终止条件非数值 → Dangle + 诊断）：
```cpp
// "for i = 0, \"a\", 1 in i" 类似源码走 evaluateOf，断言结果 Dangle，
// 且 context.diagnostics 中含 "for end condition must be numeric"
```
注意：终止条件含字符串常量会先走 evaluateBinary(String vs int Less) → Dangle，同样落进新检查——直接用字面量 `"a"` 作终止条件即可命中（求值结果 String，非数值）。

测试 B（循环体内把循环变量赋成字符串 → Dangle 不崩溃）：
```cpp
// evaluateOf("def f() { for i = 0, i < 3, 1 in i = \"s\" } f()")
// 断言 category() == Dangle；旧实现此处读指针位模式做整数加法
```

测试 C（字符串跨帧值语义）：
```cpp
// evaluateOf("def make() { \"abc\" } def id(s:string) { s } id(make())")
// 断言 category() == String && *asString() == "abc"
// 钉死：返回值与参数在帧弹出后仍有效（深拷贝，无悬挂）
```

测试 D（赋值独立性）：
```cpp
// evaluateOf("var a:string = \"x\"; var b:string = a; b = b + \"!\"; a")
// 断言 category() == String && *asString() == "x"
// 钉死值语义：b 的重绑不影响 a（防退回共享缓冲实现）
```

- [ ] **Step 2: 构建 + 全量回归**

Run: `cmake --build build-debug --config Debug --parallel 16; ctest --test-dir build-debug -C Debug --output-on-failure`
Expected: 全绿，新增 4 条 PASS。若测试 B 挂起：说明终止条件仍被按旧语义读取，回到 Task 1 Step 3 的重写段检查。

- [ ] **Step 3: Commit**

```bash
git add Calculater/unit_test.cc
git commit -m "test: pin FCValue value semantics after variant migration

- non-numeric for-end yields Dangle + diagnostic (was union raw read)
- reassigning loop counter to string yields Dangle, no pointer-bit math
- strings survive frame pop (deep copy at call/return boundaries)
- assignment copies strings; rebinding does not alias the source slot"
```

### Task 3: 文档同步 + 双配置全量验证

**Files:**
- Modify: `docs/walkthrough/04-evaluator.md:206`（"实验 1"代码片段中的 `result.evaluteVal.intVal = ...` 行——更新为新 API 等价行，并核对该文件其余引用 FCValue 构造的段落）
- Modify: `docs/superpowers/plans/ARCHITECTURE_DESIGN.md:93-95`（数据结构表：删 `FCValueUnion` 行，`FCValue` 行改为 `storage(std::variant), category(), asInteger/asFloating/asString()`）
- 不动: `docs/superpowers/plans/2025-06-20-*.md` 等历史日期文档（历史记录不改写）。

**Interfaces:**
- Consumes: Task 1/2 的最终行为。

- [ ] **Step 1: 更新两处文档**

04-evaluator.md 实验片段改为迁移后的真实代码行（保持"改一行验证除法语义"的教学结构）。ARCHITECTURE_DESIGN.md 表格与新 runtime.h 对齐。

- [ ] **Step 2: 双配置全量验证（默认 + 嵌套函数开关）**

Run: `pwsh -NoProfile -File .\tools\verify-configurations.ps1`
Expected: 两种配置构建成功、全量断言通过（脚本自报结果）。

- [ ] **Step 3: 残留终检**

Run: `pwsh -c "Get-ChildItem Calculater,docs -Include *.cc,*.h,*.md -Recurse | Select-String -Pattern 'evaluteVal|FCValueUnion|FCStringValue'"`
Expected: 仅历史日期文档命中；活文档（walkthrough/ARCHITECTURE_DESIGN）0 命中。

- [ ] **Step 4: Commit**

```bash
git add docs/walkthrough/04-evaluator.md docs/superpowers/plans/ARCHITECTURE_DESIGN.md
git commit -m "docs: sync evaluator walkthrough and architecture tables to variant FCValue"
```

---

## Self-Review 记录

- 规格覆盖：泄漏（构造上无 new + grep 证据）、别名（值语义 + 测试 C/D）、类别安全读取（as* API + 测试 A/B）——三联缺陷各有任务与验证。
- 行为变更仅"审核要点 1/2"两处，均已显式列出待确认；其余全部为保形迁移，由既有 3389 行套件回归兜底。
- 类型一致性：`category()`/`asInteger()` 等签名在 Task 1 Interfaces 定义，Task 2 测试按同名使用；`FCValue{}` 默认构造语义与 unit_test.cc:2524 兼容。
- 比例：代码块集中于 runtime.h 新表示与 evaluateFor 重写两个"签名与测试定不了的算法/接口决定点"，其余为位点清单。
