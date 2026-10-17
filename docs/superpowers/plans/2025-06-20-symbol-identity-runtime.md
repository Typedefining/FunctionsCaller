# Symbol-Identity Runtime + VariableStorage Flatten — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成工作区中半途而废的 `refactor/var-symbol-binding` 改动：把 `VariableStorage`（kind+slot）扁平化为纯 `int slot`，删除 `CompiledProgram::allSymbols` 中心化符号表，让解释器与 codegen 都通过符号身份（symbol identity）寻址，并让 `VariableRuntimeInfo.frameIndex` 真正支撑闭包捕获变量的跨帧存活。

**Architecture:** 四层递进：(1) 核心模型定型（token.h / runtime.h）→ (2) 语义期按新模型分配槽位与注册全局符号（semantic.h/.cc）→ (3) 解释器改为符号身份寻址 + 帧索引（evaluator.h/.cc）→ (4) codegen 去掉对 `allSymbols` 的依赖（codegen.cc）→ (5) 单元测试全量迁移并补捕获变量用例（unit_test.cc）。每一步都可独立编译、独立跑测试。

**Tech Stack:** C++17, LLVM 17 (仅 FunctionsCodegen 目标), CMake, Windows + MSVC（仓库已有 build-debug 配置）。

**Spec:** 本计划即设计文档；语言语义见 `README` 第 195–314 行（函数/变量/条件/循环规则），项目结构见 458–491 行。

## Global Constraints

- `FunctionsCore` 目标不得引入 LLVM 依赖（`CMakeLists.txt:23-28`）；LLVM 只由 `FunctionsCodegen` 引入（`CMakeLists.txt:44-49`）。
- 语义层 `FCSemanticContext` 的公开 API（`lookupVariableShared` / `declareVariable` / `bindVariable` / `boundSymbol` / `scopedScope` / `scopedFunction`）签名不变，只改内部实现。
- 解释执行与 codegen 的可见行为必须与重构前一致（README 第 277–291 行的全局/局部遮蔽规则、第 315 行起的条件/循环语义）。
- 行覆盖率门槛 95%（`tools/coverage.ps1`，排除 `unit_test.cc` / `call_ast_test.cc` / `main.cc`）。
- 不新增第三方依赖。

## Review Focus

1. **全局变量被误判为局部**：`FCSemanticContext::declareVariable` 用 `isGlobalScope()`（`scopeStack.size() == 1`）判断，但顶层块 `{ var g:int = 1; }` 会让 `size()==2`，从而把全局变量注册进函数帧布局并导致 `evaluateVariable` 读不到值。`README:291` 明确要求顶层任意位置的 `var` 都是全局。→ 由 Task 2 用 `!m_currentState.isActive` 替代，Task 5 加回归用例。
2. **捕获变量在帧弹出后丢失**：旧模型值存在 `Frame::locals[slot]`，帧弹出即失效。新模型必须让 `localSymbols` 按符号持有 `shared_ptr<FCValue>`，且 `popFrame` 不清除 `isCaptured` 符号。→ Task 3 实现，Task 5 用嵌套函数闭包用例验证。
3. **`bindNode` 按名字绑定的死路**：`evaluator.h:38` 的 `bindNode(const void*, const string&)` 依赖已删除的 `allSymbols`，且单元测试用它手工构造错误路径。→ Task 1 改为按符号绑定，Task 5 重写测试。
4. **codegen 顶层全局发现路径断裂**：`codegen.cc:759,970` 通过 `allSymbols.lookupShared(decl->name)` + `storage.kind == Global` 决定是否为顶层全局声明；`allSymbols` 删除后该路径必须改为遍历 `globalSymbols`。→ Task 4。
5. **`parametersIdx` 再次变成死字段**：它目前在全库零引用。必须让语义期按参数声明顺序填充，否则 `CompiledFunction` 丢失形参槽位布局。→ Task 2 填充，Task 5 断言。

---

## Task 1: 核心模型定型（token.h / runtime.h）

**Files:**
- Modify: `Calculater/token.h:59-143` (`VariableSymbol`, `CompiledFunction`, `CompiledProgram`, `SemanticBinding`)
- Modify: `Calculater/runtime.h:29-43` (`VariableRuntimeInfo`, `FCValue`, `Frame`)
- Test: `Calculater/unit_test.cc:1233-1262` (token-api 断言迁移)

**Interfaces:**
- Consumes: 无（这是底层模型层）
- Produces:
  ```cpp
  // runtime.h
  struct VariableRuntimeInfo {
      int frameIndex = -1;   // -1 = 全局; >=0 = callStack 中持有该值的帧下标
  };
  struct Frame {
      std::string funcName;
      std::vector<std::shared_ptr<VariableSymbol>> locals;  // 本帧引入的符号（不再直接存值）
  };

  // token.h
  struct VariableSymbol {
      VarDeclPtr declaration;
      int slot;                 // 所属域内的槽位（全局域 / 函数帧域）
      int scopeDepth;
      bool isMutable;
      bool isCaptured;
      VariableRuntimeInfo runtime;
      VariableSymbol(VarDeclPtr decl, int st, int depth);
  };
  struct CompiledFunction {
      FCFunctionAST* ast;
      std::vector<int> parametersIdx;   // 形参槽位，语义期按声明顺序填充
      SymbolTable symbols;
      int frameSize;
      int maxTempSlots;
  };
  struct CompiledProgram {
      std::shared_ptr<SemanticBinding> m_binding;
      std::unordered_map<std::string, std::shared_ptr<CompiledFunction>> functions;
      std::vector<std::shared_ptr<VariableSymbol>> globalSymbols;  // 槽位索引，替代 allSymbols
  };
  struct SemanticBinding {
      bool bind(const void* node, std::shared_ptr<VariableSymbol> symbol);
      std::shared_ptr<VariableSymbol> find(const void* node) const;
      // 强类型重载，消除 void* 侧表
      std::shared_ptr<VariableSymbol> find(const FCExprClass::FCVariableExprAST* node) const;
  };
  ```

- [ ] **Step 1: 写失败测试**
  在 `unit_test.cc:1233` 的 token-api 块中，把对 `VariableStorage` 的引用全部替换为新 API：
  ```cpp
  VariableSymbol sym;
  expect(sym.declaration == nullptr && sym.slot == -1 && sym.scopeDepth == 0
         && !sym.isMutable && !sym.isCaptured && sym.runtime.frameIndex == -1,
         "token-api - default symbol");
  auto decl = std::make_shared<VarDecl>("g", "int");
  VariableSymbol custom(decl, 4, 2);
  expect(custom.declaration == decl && custom.slot == 4 && custom.scopeDepth == 2,
         "token-api - custom symbol fields");
  ```
  并删除 `VariableStorage st;` 那一段。预期：**编译失败**（`VariableStorage` 已不存在）。

- [ ] **Step 2: 运行测试确认失败**
  `cmake --build build-debug --config Debug --target FunctionsUnitTest` → 预期因 `VariableStorage` 未定义而失败。

- [ ] **Step 3: 实施模型**
  `token.h`: 删除 `VariableStorage`；`VariableSymbol` 加 `runtime` 字段，构造器改为 `(decl, st, depth)`；`CompiledFunction::parameters` 改名为 `parametersIdx`（`vector<int>`）；`CompiledProgram` 删 `allSymbols`，加 `globalSymbols`；`SemanticBinding` 加 `find(const FCVariableExprAST*)` 强类型重载（内部 `static_cast<const void*>(node)` 后复用现有 map）。
  `runtime.h`: 保留 `VariableRuntimeInfo`；`Frame::locals` 改为 `vector<shared_ptr<VariableSymbol>>`。
  **关键约定**：`slot` 是"所属域内的下标"——全局符号的 `slot` 即 `globalSymbols` 下标，局部符号的 `slot` 即帧内下标。`frameIndex == -1` 当且仅当符号属于全局域。

- [ ] **Step 4: 编译确认**
  `cmake --build build-debug --config Debug --target FunctionsCore` → 通过。此时 evaluator/codegen 仍会因旧 API 报错，属预期。

- [ ] **Step 5: 提交**
  `git add Calculater/token.h Calculater/runtime.h Calculater/unit_test.cc`
  `git commit -m "refactor: flatten VariableStorage to int slot; symbol-identity runtime model"`

---

## Task 2: 语义期槽位分配与全局符号注册（semantic.h/.cc）

**Files:**
- Modify: `Calculater/semantic.h:179` (删 `m_variableBindings`)
- Modify: `Calculater/semantic.cc:118-150` (`declareVariable`), `156-183` (`registerFunction`), `203-229` (`dumpScopes`), `275-286` (`SymbolTable::dump`), `261-272` (`bindVariable`/`boundSymbol`)
- Test: `Calculater/unit_test.cc:1277-1320` (symbol-table), `1325-1440` (compiled), `1499-1564` (semantic-api)

**Interfaces:**
- Consumes: Task 1 的 `CompiledProgram::globalSymbols`, `VariableSymbol::slot`, `VariableSymbol::runtime`
- Produces: `declareVariable` 返回的符号 `slot` 已按域分配、全局符号已追加到 `compiledProgram->globalSymbols`；`registerFunction` 填充 `parametersIdx`；`boundSymbol(const FCVariableExprAST*)` 可直接调用

- [ ] **Step 1: 写失败测试**
  在 `unit_test.cc` 的 `runSymbolTableTests` / `runCompiledProgramTests` 中：
  - 把 `VariableSymbol s1(d1, {VariableStorage::Kind::Global, 0}, 0)` 改为 `VariableSymbol s1(d1, 0, 0)`，断言 `s1.slot == 0`。
  - `"compiled - only globals"` 用例：`expect(prog.globalSymbols.size() == 2, ...)`、`prog.globalSymbols[0]->slot == 0`、`prog.globalSymbols[1]->slot == 1`，删 `prog.allSymbols` 三处引用。
  - `"compiled - param a/b/t layout"` 用例：断言 `add->parametersIdx == vector<int>{0, 1}`（两形参槽位）。
  预期：编译失败（`storage.kind`, `allSymbols`, `parameters` 均不存在）。

- [ ] **Step 2: 运行确认失败**
  `cmake --build build-debug --config Debug --target FunctionsSemantic` → 预期失败。

- [ ] **Step 3: 实施**
  `semantic.h`: 删除 `m_variableBindings`（它与 `m_binding` 语义重复且从未被写入/读取）；`SemanticBinding` 声明强类型 `find(const FCVariableExprAST*)`。
  `semantic.cc`:
  ```cpp
  // declareVariable
  int slot;
  const bool isGlobal = !m_currentState.isActive;   // README:291: 顶层任意块内的 var 都是全局
  if (isGlobal) {
      slot = m_globalLayout.allocateSlot();
  } else {
      slot = m_frameLayout.allocateSlot();
  }
  auto symbol = std::make_shared<VariableSymbol>(std::move(declaration), slot, scope.depth);
  symbol->runtime.frameIndex = isGlobal ? -1 : 0;   // 0 为占位；帧 push 时由 evaluator 修正
  scope.variables.emplace(name, symbol);
  if (isGlobal) {
      m_compiledProgram->globalSymbols.push_back(symbol);   // slot == 下标
  } else if (m_currentState.currentFunction) {
      m_currentState.currentFunction->symbols.addSymbol(name, symbol);
  }
  ```
  注意：原代码 `else if (isGlobalScope())` 在函数作用域内声明局部变量时不会把全局加入 `allSymbols`，新逻辑等价（全局只在 `!isActive` 时注册）。
  ```cpp
  // registerFunction: 填充 parametersIdx
  auto compiledFunc = std::make_shared<CompiledFunction>();
  compiledFunc->ast = const_cast<FCFunctionAST*>(function);
  // 参数在当前函数作用域内已按声明顺序 declareVariable 过
  for (const auto& arg : function->getProto()->getArgs()) {
      auto psym = compiledFunc->symbols.lookupShared(arg->name);
      compiledFunc->parametersIdx.push_back(psym ? psym->slot : -1);
  }
  compiledFunc->frameSize = m_frameLayout.frameSize();
  compiledFunc->maxTempSlots = m_frameLayout.frameSize();
  ```
  `dumpScopes` / `SymbolTable::dump`: `storage.kind == Global ? "Global" : "Local"` 改为 `symbol->runtime.frameIndex == -1 ? "Global" : "Local"`。
  `bindVariable` / `boundSymbol` 不变（仍走 `m_binding`）；新增 `SemanticBinding::find(const FCVariableExprAST*)` 内联实现。

- [ ] **Step 4: 运行测试**
  `cmake --build build-debug --config Debug --target FunctionsSemantic && ctest --test-dir build-debug -C Debug -R "symbol-table|compiled|semantic-api" --output-on-failure` → 全部通过。

- [ ] **Step 5: 提交**
  `git add Calculater/semantic.h Calculater/semantic.cc Calculater/unit_test.cc`
  `git commit -m "refactor: allocate slots per domain; register globals into CompiledProgram::globalSymbols"`

---

## Task 3: 解释器改为符号身份寻址（evaluator.h/.cc）

**Files:**
- Modify: `Calculater/evaluator.h:21-49` (`FCEvaluationContext`)
- Modify: `Calculater/evaluator.cc:13` (删 `frameForStorage` 声明), `42-53` (`evaluateVariable`), `68-100` (`evaluateFor`), `126-141` (`evaluateDeclaration`), `156-189` (`evaluateBinary` '='), `256-316` (`evaluateCall`), `364-390` (删 `frameForStorage` 定义), `432-454` (`pushFrame`/`popFrame`)
- Test: `Calculater/unit_test.cc:1770-1839` (evaluator-error), `1843-1948` (stress), `1951-1992` (`run` 打印)

**Interfaces:**
- Consumes: Task 1 的 `Frame::locals` (符号列表)、`VariableSymbol::runtime.frameIndex`；Task 2 的 `CompiledProgram::globalSymbols`
- Produces: `evaluateVariable/Declaration/Binary/Call/For` 全部通过 `symbol->runtime.frameIndex` 选择寻址域；`localSymbols` 成为局部变量值的唯一持有者

- [ ] **Step 1: 写失败测试**
  在 `unit_test.cc` 的 evaluator-error 块中，删除所有 `ctx.compiledProgram.allSymbols.addSymbol(...)` + `ctx.bindNode(name)` 的手工构造，改用真实扫描路径的错误用例：
  ```cpp
  // 局部变量但无调用帧
  FCScanner scanner;
  auto ast = scanner.analysis("def f() { var x:int = 1; x }");
  FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
  // 故意不 pushFrame 直接求值变量
  auto var = ... // 从 AST 取出 FCVariableExprAST
  expect(evaluate(var.get(), ctx).type == FCValueCategory::Dangle,
         "evaluator-error - local variable without frame");
  ```
  更直接：保留手工构造但改用 `ctx.localBinding.bind(var.get(), sym)`（`localBinding` 是公有成员），并把 `VariableSymbol localSym(localDecl, 0, 1)` 的 `runtime.frameIndex` 设为 0。
  同时新增**捕获变量存活**用例：
  ```cpp
  def outer() { var x:int = 1; def inner() { x }; x = 2; inner }
  ```
  预期：语言当前不支持嵌套 `def`（`parseExpression` 不处理 `tok_def`），所以改为一个**作用域内捕获**的等价验证：`var a:int = 1; def f() { a }` 返回 1，且 `a` 的 `runtime.frameIndex == -1`（全局）——先验证全局路径。真正的闭包用例留到 Task 5 在语言支持后补充（见下方假设）。
  预期当前：**编译失败**（`frameForStorage`, `storage.kind`, `globalFrame`, `allSymbols` 均不存在）。

- [ ] **Step 2: 运行确认失败**
  `cmake --build build-debug --config Debug --target FunctionsEvaluator` → 预期失败。

- [ ] **Step 3: 实施**
  `evaluator.h`: `bindNode` 改为按符号绑定：
  ```cpp
  bool bindNode(const void* astNode, std::shared_ptr<VariableSymbol> symbol) {
      return localBinding.bind(astNode, std::move(symbol));
  }
  ```
  删 `globalFrame`；保留 `localSymbols`（`unordered_map<shared_ptr<VariableSymbol>, shared_ptr<FCValue>>`）。
  `evaluator.cc`:
  ```cpp
  // 辅助：按符号读值
  static FCValue readSymbol(const std::shared_ptr<VariableSymbol>& sym,
                            FCEvaluationContext& ctx) {
      if (sym->runtime.frameIndex == -1) {   // 全局
          if (sym->slot < 0 || static_cast<size_t>(sym->slot) >= ctx.globalSymbols.size())
              return makeDangleValue();
          // 全局值存在 ctx.globalValues 里（见 pushFrame 下方的 globalValues 字段）
          return ctx.globalValues[sym->slot];
      }
      // 局部：按符号身份查值；捕获变量的值由 shared_ptr 持有，帧弹出后仍可达
      auto it = ctx.localSymbols.find(sym);
      if (it == ctx.localSymbols.end()) return makeDangleValue();
      return *it->second;
  }
  ```
  因此 `FCEvaluationContext` 需加 `std::vector<FCValue> globalValues;`（与 `globalSymbols` 平行，槽位索引），并在构造时按 `program.globalSymbols.size()` 初始化为 Dangle。

  **寻址统一化**：`evaluateVariable`、`evaluateDeclaration`、`evaluateBinary` 的 `=` 分支、`evaluateFor`（第 85/90/97 行）、`evaluateCall`（第 290-310 行）全部改用 `readSymbol` / `writeSymbol`：
  ```cpp
  static void writeSymbol(const std::shared_ptr<VariableSymbol>& sym,
                          const FCValue& val, FCEvaluationContext& ctx) {
      if (sym->runtime.frameIndex == -1) {
          if (sym->slot < 0 || static_cast<size_t>(sym->slot) >= ctx.globalValues.size())
              return;  // 或报错
          ctx.globalValues[sym->slot] = val;
      } else {
          ctx.localSymbols[sym] = std::make_shared<FCValue>(val);
      }
  }
  ```
  `evaluateCall`: 用 `parametersIdx` 直接取形参槽位（不再按名字查 `func->symbols`）：
  ```cpp
  auto func = context.compiledProgram.getFunction(expression->getName());
  for (size_t i = 0; i < argumentValues.size(); ++i) {
      int slot = (func && i < func->parametersIdx.size()) ? func->parametersIdx[i] : -1;
      if (slot < 0) { context.popFrame(); return makeDangleValue(); }
      // 参数符号 = func->symbols.getAllSymbols() 中 slot==slot[i] 的那个
      ...
  }
  ```
  更干净：`pushFrame` 时已为每个符号设好 `runtime.frameIndex`，所以 `evaluateCall` 只需找到形参符号（`func->symbols.lookupShared(paramName)`，这一步保留，因为需要符号身份来写入 `localSymbols`），然后 `writeSymbol(sym, arg, ctx)`。`parametersIdx` 作为布局记录存在，供测试断言，不强制用于求值路径——避免为了"有消费者"而伪造逻辑。

  `pushFrame` / `popFrame`:
  ```cpp
  void FCEvaluationContext::pushFrame(const std::string& functionName) {
      Frame frame;
      frame.funcName = functionName;
      if (const auto* function = functions.findFunction(functionName)) {
          frame.locals = function->symbols.getAllSymbols()  // 需 SymbolTable 导出 vector 或遍历 map
          for (auto& [name, sym] : function->symbols.getAllSymbols()) {
              sym->runtime.frameIndex = static_cast<int>(callStack.size());
          }
      }
      callStack.push_back(std::move(frame));
  }
  void FCEvaluationContext::popFrame() {
      assert(!callStack.empty());
      const Frame& frame = callStack.back();
      for (const auto& sym : frame.locals) {
          if (!sym->isCaptured) ctx.localSymbols.erase(sym);   // 捕获变量的值必须保留
      }
      callStack.pop_back();
  }
  ```
  **注意**：`getAllSymbols()` 返回 `const map&`，`sym` 是 `shared_ptr`，可修改其内容（`runtime.frameIndex`）。需确认 `SymbolTable::getAllSymbols` 的返回类型允许非 const 遍历——当前签名是 `const unordered_map&`，值类型是 `shared_ptr<VariableSymbol>`，修改 `->runtime` 合法（`shared_ptr` 指向的对象非 const）。

  删 `frameForStorage` 及其声明；`FCEvaluationContext` 构造函数按 `program.globalSymbols.size()` 初始化 `globalValues`。

- [ ] **Step 4: 运行测试**
  `cmake --build build-debug --config Debug --target FunctionsEvaluator && ctest --test-dir build-debug -C Debug -R "evaluator|stress" --output-on-failure` → 全部通过。重点核对 `run()` 里的 4 个块级遮蔽用例（`unit_test.cc:1976-1979`）期望值不变。

- [ ] **Step 5: 提交**
  `git add Calculater/evaluator.h Calculater/evaluator.cc Calculater/unit_test.cc`
  `git commit -m "refactor: evaluator addresses variables by symbol identity + frameIndex"`

---

## Task 4: codegen 去掉对 `allSymbols` 的依赖（codegen.cc）

**Files:**
- Modify: `Calculater/codegen.cc:673-728` (`codegenDeclaration`), `752-784` (`declareGlobalsInExpression`), `968-974` (顶层 `var` 分发)
- Test: `Calculater/unit_test.cc:985-1007` (codegen-api local variable), `1905-1948` (codegen stress)

**Interfaces:**
- Consumes: Task 1 的 `CompiledProgram::globalSymbols`、`VariableSymbol::runtime.frameIndex`；Task 2 的全局符号注册顺序
- Produces: `declareGlobalsInExpression` 不再走 AST 递归 + `kind` 检查；`codegenDeclaration` 用 `frameIndex` 判局部/全局

- [ ] **Step 1: 写失败测试**
  `unit_test.cc:985-1007` 的 `codegen-api - local variable load` 用例中，`cc.compiledProgram.allSymbols.addSymbol("loc", ...)` 改为构造真实语义上下文或直接用 `cc.namedValues[locSymbol] = locAlloca` + `cc.semanticBinding().bind(var.get(), locSymbol)`；`local variable load` 与 `local assignment` 断言不变。新增：顶层全局变量 codegen 后 `globalValues` 映射存在。
  预期：**编译失败**（`allSymbols`, `storage.kind` 不存在）。

- [ ] **Step 2: 运行确认失败**
  `cmake --build build-debug --config Debug --target FunctionsCodegen` → 预期失败。

- [ ] **Step 3: 实施**
  `codegenDeclaration`（line 685）:
  ```cpp
  const bool isLocal = declSymbol->runtime.frameIndex != -1;
  ```
  `declareGlobalsInExpression`（line 752-784）: 整个 AST 递归 + `allSymbols.lookupShared` + `kind == Global` 的逻辑替换为直接遍历已注册的全局符号：
  ```cpp
  void declareGlobalsInExpression(const FCExprAST*, FCCodegenContext&) {
      // 不再需要：全局符号已在语义期全部注册到 compiledProgram.globalSymbols
  }
  // 新增
  void declareAllGlobals(FCCodegenContext& context) {
      for (const auto& symbol : context.compiledProgram.globalSymbols) {
          declareGlobal(symbol, context);
      }
  }
  ```
  并在 `codegenProgram`（line 786）的首循环调用 `declareAllGlobals(context)`，在 `codegenStandaloneTopLevel`（line 872）开头也调用。这样 `codegen.cc:759` 和 `970` 两处 `allSymbols` 引用自然消失，且顶层 `var` 分发（line 968-974）简化为：
  ```cpp
  if (auto* declaration = dynamic_cast<FCVarDeclExprAST*>(expression)) {
      if (context.currentFunction == nullptr) {
          declareAllGlobals(context);   // 确保该全局已声明
          return codegenDeclaration(declaration, context);
      }
      return codegenDeclaration(declaration, context);
  }
  ```
  `codegenVariable` / `codegenBinary` / `codegenFor`（line 263, 297, 592, 600）已按 `namedValues` / `globalValues`（符号 key）查找，无需改动。

- [ ] **Step 4: 运行测试**
  `cmake --build build-debug --config Debug --target FunctionsCodegen && ctest --test-dir build-debug -C Debug -R "codegen|stress" --output-on-failure` → 全部通过，含 `llvm::verifyModule` 断言。

- [ ] **Step 5: 提交**
  `git add Calculater/codegen.cc Calculater/unit_test.cc`
  `git commit -m "refactor: codegen discovers globals from CompiledProgram::globalSymbols"`

---

## Task 5: 全量测试迁移 + 捕获变量回归（unit_test.cc）

**Files:**
- Modify: `Calculater/unit_test.cc` 全部残留引用（约 28 处 `storage.kind` / `allSymbols` / `parameters` / `globalFrame` / `bindNode(name)`）
- 新增: `Calculater/unit_test.cc` 中的 `runCaptureTests()`（捕获变量生命周期）

**Interfaces:**
- Consumes: Task 1–4 的所有新 API
- Produces: 全量绿测 + 覆盖 95% 行覆盖率

- [ ] **Step 1: 全局 grep 清理**
  运行 `grep -rn "storage\.kind\|frameForStorage\|allSymbols\|parameters\.empty\|globalFrame\|VariableStorage" Calculater/`，确认除 `unit_test.cc` 外无残留。在 `unit_test.cc` 中逐处替换：
  - `sym.storage.kind == VariableStorage::Kind::Global` → `sym.runtime.frameIndex == -1`
  - `sym.storage.slot` → `sym.slot`
  - `prog.allSymbols` → `prog.globalSymbols`
  - `cf.parameters` → `cf.parametersIdx`
  - `ctx.globalFrame.locals` → `ctx.globalValues`
  - `ctx.bindNode(node, "name")` → `ctx.localBinding.bind(node, sym)`（从 scanner 取符号）

- [ ] **Step 2: 新增顶层块全局回归用例**
  ```cpp
  {
      FCScanner scanner;
      auto ast = scanner.analysis("{ var g:int = 42; g }");
      const auto& prog = scanner.semanticContext().getCompiledProgram();
      expect(prog.globalSymbols.size() == 1, "compiled - global declared inside top-level block");
      expect(prog.globalSymbols[0]->runtime.frameIndex == -1, "compiled - top-level-block var is global");
      FCEvaluationContext ctx(prog);
      expect(evaluate(ast.get(), ctx).evaluteVal.intVal == 42,
             "evaluator - top-level-block global reads correctly");
  }
  ```
  这验证 Task 2 对 `README:291` 的修复。

- [ ] **Step 3: 新增捕获变量生命周期用例**
  由于当前语法不支持嵌套 `def`，用**同一符号跨帧复用**的等价场景验证 `localSymbols` 持有值：
  ```cpp
  def f() { var x:int = 1; def g() { x }; g }   // 当前语法不支撑，标记为 TODO
  ```
  改用可解析的等价验证：手动构造一个 `isCaptured=true` 的符号，在 pushFrame 写入值、popFrame 后再次读取，断言值仍在：
  ```cpp
  auto decl = std::make_shared<VarDecl>("c", "int");
  auto sym = std::make_shared<VariableSymbol>(decl, 0, 1);
  sym->isCaptured = true;
  sym->runtime.frameIndex = 0;
  FCEvaluationContext ctx({});
  ctx.pushFrame("captured");
  ctx.localBinding.bind(/*某个 FCVariableExprAST*/, sym);
  // 写入值
  FCValue v; v.type = FCValueCategory::Integer; v.evaluteVal.intVal = 7;
  // 通过 evaluateAssignment 或直接 localSymbols[sym]=...
  ctx.popFrame();
  expect(ctx.localSymbols.find(sym) != ctx.localSymbols.end(),
         "capture - value survives frame pop");
  ```
  同时断言非捕获符号在 pop 后被清除。

- [ ] **Step 4: 全量跑测**
  `cmake --build build-debug --config Debug --parallel 16 && ctest --test-dir build-debug -C Debug --output-on-failure` → 全部通过。
  `powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\coverage.ps1 -BuildDir .\build-debug -Configuration Debug` → ≥95%。

- [ ] **Step 5: 提交**
  `git add Calculater/unit_test.cc`
  `git commit -m "test: migrate all assertions to symbol-identity API; add capture-survival regression"`

---

## Task 6: 收尾与验证

**Files:** 无新增；可能修改 `README`（若决定记录 `frameIndex` / 捕获语义）。

- [ ] **Step 1: 全量构建 + 测试 + 覆盖率**（同 Task 5 Step 4，作为门控）。
- [ ] **Step 2: 死代码检查**：`grep -rn "VariableRuntimeInfo\|parametersIdx\|localSymbols\|globalSymbols" Calculater/`，确认每个新字段至少被一处生产代码（非测试）读写。若 `parametersIdx` 在生产路径确实无消费者，在 `CompiledFunction` 旁加注释说明其用途（布局记录 / 测试用），或删除它——**不要留无主字段**。
- [ ] **Step 3: 检查 shadow_repro.cc**（`Calculater/shadow_repro.cc:22` 仍用 `vs->storage.kind`）并迁移。
- [ ] **Step 4: 最终提交**
  `git add Calculater/` + `git commit -m "refactor: symbol-identity variable runtime (var-symbol-binding complete)"`

---

## 假设与已知取舍

1. **闭包语法不存在于当前语言**。`parseExpression` 不处理 `tok_def`，`scopedFunction()` 把嵌套定义的作用域重置到顶层，因此 `def outer() { def inner() {} }` 无法解析。`isCaptured` 与 `frameIndex` 在本计划中被实现为**就绪的寻址机制**（捕获值跨帧存活），但无法被用户程序触发。真正的闭包需要语法层扩展（嵌套函数定义 + 返回闭包值），那是后续功能，不在本计划范围。
2. **`slot` 在解释器中不再用于寻址**（改用 `localSymbols` / `globalValues`），仅作为布局索引保留，供 codegen 的 `namedValues`/`globalValues` 符号 key 之外的调试与测试断言使用。这是有意的设计取舍：code 层仍按符号身份查 map，`slot` 是语义期的产物。
3. **`popFrame` 不清除 `isCaptured` 符号的值**。这意味着长期运行的 `FCEvaluationContext` 若反复创建被捕获符号会泄漏；但上下文本就是按程序创建的短期对象（`unit_test.cc:1897` 的跨程序复用是显式 `compiledProgram` 替换），可接受。
4. **`isGlobalScope()` → `!m_currentState.isActive`** 是一个行为修正（修复 `README:291` 顶层块 `var` 被误标为局部的 bug）。若评审认为应严格保持旧行为，改回 `isGlobalScope()` 即可，但那样顶层块内的 `var` 会求值为 Dangle。
5. **`m_variableBindings`（semantic.h:179）被删除而非迁移**。它与 `m_binding` 键类型不同（`FCVariableExprAST*` vs `void*`）但指向同一份绑定关系，新增后从未被写入或读取，属于重复状态。任何需要强类型绑定的地方改用 `SemanticBinding::find(const FCVariableExprAST*)`。
