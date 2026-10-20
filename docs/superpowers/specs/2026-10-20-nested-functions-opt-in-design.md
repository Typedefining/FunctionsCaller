# 嵌套函数定义作为编译期选项 —— 设计方案（待审 v2）

> 状态：**待审核，未实现**。v2 变更：按你的要求，**求值器与 codegen 均拒绝嵌套定义**；开关因此只控制前端（语法/语义）能否表示嵌套 def，不控制任何执行能力。
> 基线：分支 `refactor/var-symbol-binding`，HEAD `732ea2d`，721 测试全绿，覆盖率 95.35%。

## 0. 先回答：两个卡点是否都由"允许函数嵌套定义"引入？

**结论：是，两者都只在"两个函数帧同时活跃"时可触发；但性质不同。**

| 卡点 | 触发条件 | 引入点 | 性质 |
|------|----------|--------|------|
| 记录串用（单指针 `currentFunction` 被内层覆盖） | 两个函数作用域同时活跃 | `53ef58f` 放置单指针（**当时正确**，`99336b3` 的 `assert(size()==1)` 保证唯一）；`d9203e8` 撤掉前提却未换成栈 | 前提被删除，数据结构未同步更换 |
| 帧锚点越界（`operator[]` 越界死循环） | 锚点 `= m_scopeStack.size()` 与 `m_frameLayouts/m_frameRecords` 的下标域不一致 | `d9203e8` 自身实现缺陷 | 只在"块作用域下压第二个函数帧"时成立（顶层 def 时两域恰好都等于 1） |

再补一条**仍存活**的第三个问题（本方案必须关掉它）：

`unit_test.cc:2465-2504` 断言"内层函数引用外层局部（作用域栈连续可见）"，但从未执行它。运行时 `semantic.cc:154` 只写 `frameIndex = isCurrentFrameGlobal() ? -1 : 0`，`evaluator.h:40-49` 只区分"全局帧 / 当前帧"。内层函数引用外层局部时，符号被标成 `0`，求值期落到**内层自己的帧**上的同名 slot：读到错值或越界。静默错编译，不报错。

## 1. 目标 / 非目标 / 反目标

**目标**

- 一个编译期开关，默认关闭；关闭时恢复"同时至多一个函数帧"的不变式。
- 关闭时嵌套 `def` 是**语义错误**（诊断 + `analysis()` 返回空），不是断言崩溃。
- 打开时**只放开前端**：语义期可表示并登记嵌套 def；**求值器与 codegen 一律拒绝执行/生成**，给准确诊断。
- 前端拒绝规则唯一、可测；不复制第二套 semantic 实现。

**非目标**

- 闭包/捕获（打开态也拒绝引用外层局部）。
- 运行期开关。
- 嵌套 def 的执行能力（求值器与 codegen 都不支持，也不打算在这次支持）。

**反目标**

- 用 `#if` 把 `semantic.cc` 劈成两套实现。
- 用 `assert` 当唯一防线（Release 下消失）。
- 默认打开。
- 让打开态的嵌套 def 在某个后端"半跑"——那正是当前静默错编译的成因。

## 2. 开关设计

开关的语义要说死：**它只管前端能否表示嵌套 def，不管能不能执行。**

```cmake
# Calculater/CMakeLists.txt
option(FC_ENABLE_NESTED_FUNCTIONS "前端允许在函数体内定义函数（实验特性）" OFF)

target_compile_definitions(FunctionsSemantic PUBLIC
    $<$<BOOL:${FC_ENABLE_NESTED_FUNCTIONS}>:FC_ENABLE_NESTED_FUNCTIONS=1>)
```

```cpp
// Calculater/language_options.h（新增）
#pragma once

namespace FCMarks {
// 语言特性开关：函数体内定义函数。默认关闭。
// 关闭：语义期拒绝嵌套 def，保持“同时至多一个函数帧”不变式。
// 打开：语义期允许并登记嵌套 def，但被定义函数不得捕获外层局部；
//       求值器与 codegen 一律拒绝，不提供执行能力。
constexpr bool kNestedFunctionDefinitions =
#if defined(FC_ENABLE_NESTED_FUNCTIONS)
    true;
#else
    false;
#endif
} // namespace FCMarks
```

`PUBLIC` 的理由：`FunctionsFrontend`、`FunctionsUnitTest` 都包含 `semantic.h`，必须看到同一个常量值。

**两种落地方式对比**

| | A. 策略接缝（推荐） | B. 条件编译分离 |
|---|---|---|
| 做法 | 保留帧记录栈，只用常量在**一处**判定"这里能否定义函数" | `#if` 决定是否存在 `m_frameAnchorStack` / 恢复逻辑 |
| 分叉点 | 1 个谓词 | `semantic.h` + `semantic.cc` 约 6 处 |
| 风险 | 关闭时多走一个恒假分支 | 两套行为都要测、都要维护；覆盖率按配置漂移 |
| 与 `d9203e8` 的关系 | 保留"全局也是帧"的统一模型 | 关闭态要把全局退回独立布局 —— 等于回退 Commit A 的核心收益 |

**推荐 A**：Commit A 之后帧记录栈已是 `<global>` 与普通函数的统一模型，物理剥离会把全局帧一起剥掉。

## 3. 模块改动

### 3.1 `Calculater/language_options.h`（新增）

放常量。所有判定只 include 它，不直接 `#if`。

### 3.2 `Calculater/semantic.h` / `semantic.cc`

唯一判定接缝：

```cpp
// semantic.h（FCSemanticContext public）
// 当前位置能否开始一个函数帧：关闭嵌套时只允许在 <global> 帧上定义。
bool mayDefineFunctionHere() const;
```

```cpp
// semantic.cc
bool FCSemanticContext::mayDefineFunctionHere() const {
  return kNestedFunctionDefinitions || m_frameAnchor == 0;
}
```

判据用**帧锚点**（`m_frameAnchor`），不是作用域栈深度 —— 卡点二的教训：两个计数域不能混用。顶层块里的 `def`（`{ def f() {...} }`）锚点仍为 0，属于"全局函数写在块里"，不归本开关管，两种配置都允许。

`SemanticAnalyzer::analyzeFunction`（`semantic.cc:442`）开头加守卫，**在压帧与声明参数之前**，保证失败无副作用：

```cpp
bool FCSemanticContext::SemanticAnalyzer::analyzeFunction(const FCFunctionAST *node) {
  if (!m_ctx.mayDefineFunctionHere()) {
    fprintf(stderr,
            "Error: nested function definition '%s' requires "
            "FC_ENABLE_NESTED_FUNCTIONS\n",
            node->getProtoName().c_str());
    return false;
  }
  FCSemanticContext::FunctionScopeGuard guard(m_ctx);
  ...
}
```

诊断走 `fprintf(stderr)` + 返回 `false`，与 `analyzeVariable`（`semantic.cc:396`）一致；`scanner.cc:26-29` 已把失败翻成 `m_hadSemanticError`。

`pushFunctionScope()`（`semantic.cc:52`）在压栈前补断言，把旧前提显式写回：

```cpp
assert(kNestedFunctionDefinitions || m_frameAnchor == 0);
```

作用是让"关闭态有人绕过 analyzer 直接压第二个帧"在 Debug 立刻暴露。

### 3.3 捕获拒绝（打开态的语言规则）

打开态下 `lookupVariableShared`（`semantic.cc:100`）会穿过外层函数作用域找到外层局部，绑定后就是静默错编译。已定为语言规则：**拒绝**。

- `semantic.h` 新增 `std::unordered_map<const VariableSymbol *, size_t> m_symbolOwnerFrame;`
- `declareVariable`（`semantic.cc:149` 附近）登记 `m_symbolOwnerFrame[symbol.get()] = m_frameAnchor;`
- `SemanticAnalyzer::resolveVariable`（`semantic.cc:477`）命中后判定：

```cpp
std::shared_ptr<VariableSymbol>
FCSemanticContext::SemanticAnalyzer::resolveVariable(const std::string &name) {
  auto symbol = m_ctx.lookupVariableShared(name);
  if (!symbol) return nullptr;
  const auto it = m_ctx.m_symbolOwnerFrame.find(symbol.get());
  if (it != m_ctx.m_symbolOwnerFrame.end() &&
      it->second != 0 &&                     // 0 = <global> 帧，任何位置可见
      it->second != m_ctx.m_frameAnchor) {   // 不是本帧 → 属于外层函数
    fprintf(stderr,
            "Error: nested function cannot capture enclosing local '%s' "
            "(closures are not supported)\n",
            name.c_str());
    return nullptr;
  }
  return symbol;
}
```

接缝放 `resolveVariable` 而不是 `lookupVariableShared`：前者是"名字解析"的语言规则，后者是上下文查询 API，被 20 余处测试直接调用（`unit_test.cc:2125-2244`），改语义会连带破坏既有断言。

`reset()`（`semantic.cc:16-35`）一并补 `m_symbolOwnerFrame.clear();`。

### 3.4 `Calculater/evaluator.cc` —— 求值拒绝

现状（`evaluator.cc:353-356`）：作为表达式遇到的 `FCFunctionAST` 会登记进注册表并返回 `Dangle`。`Dangle` 会让所在序列/块提前中止，所以"函数体内的嵌套 def"今天是被**静默当失败处理**，没有诊断。

改为：只有在**没有活动调用帧**时才保持"登记"语义（顶层 AST 与手工构造的测试用 AST 依赖它），一旦处于函数调用中即显式拒绝：

```cpp
if (auto *node = dynamic_cast<const FCFunctionAST *>(expression)) {
  if (!context.callStack.empty()) {
    fprintf(stderr,
            "Error: nested function definition '%s' is not supported "
            "by the evaluator\n",
            node->getProtoName().c_str());
    return makeDangleValue();
  }
  context.functions.registerFunction(const_cast<FCFunctionAST *>(node));
  return makeDangleValue();
}
```

补充事实（据此写契约，不额外实现）：`FCFunctionRegistry::index`（`evaluator.cc:382-392`）只登记顶层语句里的函数，因此嵌套定义出来的函数**不可被调用** —— 调用会走 `evaluateCall` 的"unknown function"失败路径，属明确报错而非错值。

### 3.5 `Calculater/codegen.cc` —— 生成拒绝

现状核对（`codegen.cc:940-941`）：函数体内的 `def` 会落到 `codegenFunction`。它会保存/恢复插入点、`currentFunction`、`namedValues`（`codegen.cc:470-474`、`541-543`），不会直接污染外层 IR；但它返回 `llvm::Function*`，外层序列再把这条语句的值 `castValue` 到外层返回类型（`codegen.cc:523`）——表现为"类型不匹配"或更隐晦的错值。

改为在分派处显式拒绝：

```cpp
if (auto* function = dynamic_cast<FCFunctionAST*>(expression))
{
    // 嵌套定义只存在于前端模型；在函数体内生成会得到一个 Function* 值，
    // 外层再把它当表达式值使用，错误会推迟到类型转换才暴露
    if (context.currentFunction != nullptr)
        return logCodegenError(
            "nested function definition is not supported by codegen");
    return codegenFunction(function, context);
}
```

`logCodegenError` 是文件内既有诊断入口（`codegen.cc:704`、`725`）。

### 3.6 `Calculater/unit_test.cc`

测试矩阵（`if constexpr (kNestedFunctionDefinitions)` 分流）：

| 场景 | 关闭 | 打开 |
|------|------|------|
| 顶层 `def f() {...}; f()` | 通过 | 通过 |
| 顶层块内 `def`（`{ def f() {...} }`） | 语义通过 | 语义通过 |
| 函数体内 `def`，体内只用全局/自身参数局部 | **语义拒绝**（嵌套诊断） | 语义通过、记录已登记 |
| 函数体内 `def`，引用外层函数局部 | **语义拒绝**（嵌套诊断） | **语义拒绝**（捕获诊断） |
| 打开态：函数体内 `def` 的程序交给求值器 | — | 求值期报错（nested not supported） |
| 打开态：同上交给 codegen | — | 生成期报错（nested not supported） |

- 现有"帧锚点模型"用例（`2461-2504`）的 AST 引用了外层局部，因此在**两种配置下都必须失败**，只是诊断不同。用例改为各自的负向断言，并检查失败后无状态污染（`getCompiledFunction("inner") == nullptr`、`outer` 记录仍在且 `frameSize` 不含 inner 的部分）。
- 新增打开态**正向**用例：内层 def 只引用全局 → `analyze` 通过、`getCompiledFunction("inner")` 非空且与 `outer` 记录不同。
- 新增求值器/ codegen 负向用例（打开态编译时才有意义），断言错误发生且不产生 IR/不写帧。

### 3.7 验证与覆盖率

```powershell
# 严格（默认，覆盖率门禁跑这个）
cmd /c "cmake -S . -B build-strict -DCMAKE_BUILD_TYPE=Debug > log 2>&1"
cmd /c "cmake --build build-strict --config Debug --target FunctionsUnitTest --parallel 16 >> log 2>&1"
build-strict\Calculater\Debug\FunctionsUnitTest.exe

# 嵌套打开
cmd /c "cmake -S . -B build-nested -DCMAKE_BUILD_TYPE=Debug -DFC_ENABLE_NESTED_FUNCTIONS=ON > log 2>&1"
```

覆盖率门禁仍按**严格配置**（语言默认形态）。打开态只要求测试全绿；`if constexpr` 折叠掉的启用专用分支若被工具计为未覆盖，按"配置专属代码"排除，不造假调用凑数。

### 3.8 文档

`docs/ARCHITECTURE.md` 语言限制表加一行：嵌套函数定义 = 前端编译期选项，默认关闭；即使打开，求值与 codegen 也拒绝。`README` 构建说明加 `-DFC_ENABLE_NESTED_FUNCTIONS=ON`。

## 4. 打开态的语义契约

| 场景 | 行为 |
|------|------|
| `def f() { def g() { 1 } }` | 语义允许；`f`、`g` 各一条 `CompiledFunction` 记录 |
| 调用 `g()` | **不可用**：注册表只索引顶层函数，调用走 "unknown function" 失败路径 |
| `g` 引用 `f` 的局部 | 语义拒绝（capture 诊断） |
| `g` 引用全局 / `g` 自己的参数与局部 | 语义允许 |
| 程序进入求值器且遇到 `g` 的定义 | 拒绝（evaluator 诊断），不登记 |
| 程序进入 codegen 且遇到 `g` 的定义 | 拒绝（codegen 诊断），不生成 IR |
| 顶层块内 `def` | 两种配置都允许（锚点为 0） |

**这个契约的代价要说清楚**：打开态下含嵌套 def 的程序"能过前端，不能执行"。开关的价值是让前端模型可建模、可测试、为将来闭包留位置，而不是交付可运行特性。如果你要的是"打开态可执行"，那需要 upvalue 环境链 + 改 `frameIndex` 二值模型（`semantic.cc:154`、`evaluator.h:41`、`codegen.cc:695`、`evaluator.cc:71`），是另一个特性。

## 5. 任务拆分（批准后按此顺序，每任务一个 commit）

| # | 任务 | 文件 | 验证 |
|---|------|------|------|
| 1 | 开关骨架：CMake option、`language_options.h`、PUBLIC 定义 | `CMakeLists.txt`、`Calculater/CMakeLists.txt`、`Calculater/language_options.h` | 两配置可构建；常量与配置一致 |
| 2 | 关闭态拒绝：`mayDefineFunctionHere()` + `analyzeFunction` 守卫 + `pushFunctionScope` 断言 | `semantic.h`、`semantic.cc` | 嵌套 def 被拒且无副作用；严格配置全绿 |
| 3 | 现有用例按开关分流 + 打开态正向用例 | `unit_test.cc` | 两配置全绿 |
| 4 | 捕获拒绝：`m_symbolOwnerFrame` + `resolveVariable` 守卫 | `semantic.h`、`semantic.cc` | 打开态捕获被拒；同函数块内局部仍可见 |
| 5 | 求值器拒绝嵌套 def | `evaluator.cc` | 打开态求值报错；顶层/手工 AST 路径行为不变 |
| 6 | codegen 拒绝嵌套 def | `codegen.cc` | 打开态生成报错，不产生 IR |
| 7 | 双配置验证脚本 + 文档 | `tools/`、`docs/ARCHITECTURE.md`、`README` | 两配置 ctest 全绿；严格配置覆盖率 ≥95% |

## 6. 风险

1. **`if constexpr` 与覆盖率**：严格配置里启用专用分支可能被计为未覆盖。先测量再决定是否排除，不靠造假调用凑数。
2. **现有测试基线**：`2461-2504` 是唯一直接断言嵌套可见性的用例；分流后它在两种配置下都是负向用例，容易把断言力度写弱。
3. **`frameIndex` 的二值假设**：`semantic.cc:154`、`evaluator.h:41`、`codegen.cc:695`、`evaluator.cc:71` 四处依赖"只有 -1 与 0"。本次不改；将来做闭包必须四处一起改。
4. **双配置漂移**：两个构建目录由脚本同时跑，避免只验证默认配置。
5. **"能过前端不能执行"的观感**：需要在诊断文案里说清是"未支持"而不是"语法错误"，避免用户以为是写错了。

## 7. 决策记录

| 决策 | 结论 | 来源 |
|------|------|------|
| 打开态下嵌套函数引用外层局部 | **拒绝**（语义错误），不实现闭包 | 你已拍板（2026-10-20） |
| 求值器遇到嵌套 def | **拒绝**（诊断 + `Dangle`），不提供执行 | 你已拍板（2026-10-21） |
| codegen 遇到嵌套 def | **显式拒绝**，不做"提升为顶层函数" | 你已拍板（2026-10-21） |
| 开关默认值 | **关闭** | 按方案推荐锁定 |

因此本方案不含 upvalue 环境链，也不改动 `frameIndex` 的 `-1 / 0` 二值模型。

另有两项判断已写进方案，审核时仍可推翻：

- 顶层块内 `def` 不归本开关管（判据用帧锚点，不用作用域深度）—— 卡点二的教训。
- 判定接缝放在 `SemanticAnalyzer::resolveVariable` 而非 `lookupVariableShared` —— 后者被 20 余处测试直接调用。
