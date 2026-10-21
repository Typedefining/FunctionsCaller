# Codegen 直接生成 SSA IR —— 落地方案（路径 B）

文件：`docs/superpowers/specs/2026-10-21-codegen-ssA.md`
状态：设计待审，尚未改动产品代码
依赖：当前分支 `refactor/var-symbol-binding`，HEAD `55632ad`，126 commits，领先 origin 8（未 push）

## 1. 目标

让 `Calculater/codegen.cc` 直接发出 **SSA 形式的 LLVM IR**：
- 局部变量与函数参数不再通过 `alloca` + `load`/`store` 读写，而是以 **SSA 值**（`llvm::Value*`）在定义点与使用点之间直接传递。
- 控制流汇合点（`if` 的合并块、`for` 的循环头）插入 `llvm::PHINode`（`phi` 指令），按前驱块选择当前版本。
- 全局变量保留 `GlobalVariable`（本身就是跨函数单一定义，无需 phi）。
- 不引入外部 SSA 构造库；仅使用 LLVM Core 的 `IRBuilder`、`PHINode`、`BasicBlock`。

非目标（本版本不做）：对取地址变量的支持（本语言无取地址运算）、对 `mem2reg` Pass 的移除（`--emit-ir` 可选保留或去掉，两者并存）。

## 2. 现状与影响范围

当前模型（内存模型）：
- `Calculater/codegen.h:32` `namedValues`：`std::map<std::shared_ptr<VariableSymbol>, llvm::AllocaInst*>`，每个变量一个栈槽。
- `Calculater/codegen.cc:271` `codegenVariable`：从 `namedValues` 取 `AllocaInst*`，生成 `load`。
- `Calculater/codegen.cc:292` `codegenBinary` 的 `Assign` 分支：生成 `store` 到同一个槽。
- `Calculater/codegen.cc:461` `codegenFunction`：为每个参数 `createEntryBlockAlloca` + `store` 实参（`codegen.cc:494-496`）。
- `Calculater/codegen.cc:600` `codegenFor`：循环变量与循环体内变量均 `alloca`（`codegen.cc:611`）。
- `Calculater/codegen.cc:862` `codegenStandaloneTopLevel` + `985` `codegenTopLevel`：顶层包装入 `__fc_main`。

受改动的文件：
- `Calculater/codegen.h`：`FCCodegenContext` 增加 SSA 版本栈与查找/赋值方法；`namedValues` 值类型由 `AllocaInst*` 改为 `llvm::Value*`。
- `Calculater/codegen.cc`：重写 `codegenVariable`、`codegenBinary(Assign)`、`codegenVarDecl`、`codegenFunction`（参数绑定）、`codegenIf`（插 phi）、`codegenFor`（诱导变量与循环携带变量插 phi）、`codegenBlock/Sequence`（作用域快照恢复）、`castValue` 保持不变。
- `Calculater/unit_test.cc`：`runCodegenTests` 中“直接 codegen 字面量与变量”的手工绑定用例（约 `unit_test.cc:1308-1327`）需从绑定 `AllocaInst*` 改为绑定 SSA 值。
- `Calculater/main.cc`：无需改动（`--emit-ir` 走 `codegenTopLevel`，输出即为 SSA IR）。
- `docs/`：`ARCHITECTURE.md`、`README`、`walkthrough/05-codegen.md` 同步指标与 IR 示例。

## 3. 核心数据结构

在 `Calculater/codegen.h` 的 `FCCodegenContext` 内（或新增内部类 `SSAState`），保持：

```cpp
// 当前每个符号的 SSA 值栈：栈顶为该符号在当前控制流路径上的最新版本。
std::map<const VariableSymbol*, std::vector<llvm::Value*>> valueStack;
// 每个符号的下一个版本号（单调递增，用于命名 %x.1, %x.2 ...）。
std::map<const VariableSymbol*, unsigned> nextVersion;
```

`namedValues` 仍保留，但类型改为：

```cpp
std::map<std::shared_ptr<VariableSymbol>, llvm::Value*> namedValues;  // SSA 值，不再是 AllocaInst*
```

供 `codegen` 内部使用的方法（声明于 `codegen.h`，实现于 `codegen.cc`）：

```cpp
// 为符号推入新版本（赋值/声明/参数绑定时调用），返回新值。
llvm::Value* pushSymbol(const VariableSymbol* sym, llvm::Value* val);
// 读取符号当前版本（取栈顶），未绑定返回 nullptr。
llvm::Value* peekSymbol(const VariableSymbol* sym) const;
// 为控制流合并点提前快照所有相关符号的当前值。
std::map<const VariableSymbol*, llvm::Value*> snapshotValues() const;
// 恢复快照（用于退出 if 分支后回到合并块前的状态，或作用域退出时恢复外层版本）。
void restoreValues(const std::map<const VariableSymbol*, llvm::Value*>& snap);
```

理由：本语言的 `declareVariable` 为每次声明创建独立的 `VariableSymbol` 对象（`semantic.cc` 中 `std::make_shared<VariableSymbol>(...)`），块内同名遮蔽即为不同对象，因此不需要按作用域名称回退；只需在控制流分支合并前后保存/恢复各符号的当前 SSA 值栈顶。

## 4. 各语法构造的代码生成改动

### 4.1 Variable 读取（`codegen.cc:271`）

```cpp
// 旧：从 AllocaInst 生成 load
// 新：直接返回当前 SSA 值
llvm::Value* codegenVariable(const FCVariableExprAST* expression,
    FCCodegenContext& context)
{
    auto sym = context.boundSymbol(expression);
    if (!sym)
        return context.logCodegenError("variable is not declared");
    llvm::Value* val = context.peekSymbol(sym.get());
    if (!val)
        return context.logCodegenError("variable used before definition");
    return val;
}
```

### 4.2 二元表达式中的赋值（`codegen.cc:292` 的 `Assign` 分支）

```cpp
case FCBinaryOp::Assign:
{
    llvm::Value* rhs = codegen(rhsExpr, context);
    if (!rhs) return nullptr;
    auto sym = context.boundSymbol(lhsExpr);  // 左侧必须是 Variable
    context.pushSymbol(sym.get(), rhs);
    return rhs;  // 赋值表达式的值为右值
}
```

### 4.3 变量声明（`codegen.cc:684` `codegenDeclaration`）

- 全局变量（`frameIndex == -1`）：保留 `declareGlobal` + `GlobalVariable`，不变。
- 局部变量：生成初始化表达式的 SSA 值，`pushSymbol(sym, initVal)`；不再 `createEntryBlockAlloca` + `store`。
- 若无初始化器：按类型生成零值常量（`ConstantInt::get(type, 0)` 或 `ConstantFP`）再 push。

### 4.4 函数入口与参数（`codegen.cc:461` `codegenFunction`）

- 创建函数与 `entry` 块（`codegen.cc:476`）不变。
- **删除** 为每个参数 `createEntryBlockAlloca` + `store` 实参（`codegen.cc:494-496`）。
- 改为：对每个形参符号，`pushSymbol(paramSym, &argument)`，将实参 `llvm::Value*` 直接作为该符号版本 0。
- 函数体生成 SSA 值，`return` 表达式的值经 `castValue` 转为返回类型后 `CreateRet`（`codegen.cc:523-531` 保持）。

### 4.5 If 表达式（`codegen.cc:547` `codegenIf`）

当前结构：`thenBlock` / `elseBlock` / `mergeBlock`，`mergeBlock` 处 `PHINode`。SSA 下改为：

1. 生成条件值（i1）。
2. 在 `entry` 处 `createCondition` + 条件分支到 `thenBlock` / `elseBlock`。
3. **快照**：在分支前记录所有可能被两个分支修改的符号的当前值（轻量方式：快照当前 `valueStack` 中所有栈顶，或仅快照体内预扫描出的被修改符号）。
4. 生成 `thenBlock` 内表达式，得到 `valThen`；记录该分支修改过的符号集 `modThen` 及其当前值。
5. 生成 `elseBlock`，得到 `valElse`；记录 `modElse`。
6. 在 `mergeBlock` 块头插入 `PHINode`：对 `modThen ∪ modElse` 中的每个符号，创建 `phi`，`addIncoming(valIfModified, itsBlock)`，未修改分支则传入快照值；将 `phi` 结果作为该符号在合并后的当前值（`pushSymbol` 覆盖，或设为栈顶）。
7. `if` 表达式自身的值：若两分支类型一致，返回 `mergeBlock` 处对应的 `phi` 或统一类型的值；类型不一致时 `castValue` 或报错。

### 4.6 For 表达式（`codegen.cc:600` `codegenFor`）

当前块划分：`entry` → `loop` → `loopbody` → `afterloop`，`loop` 为循环头，`loopbody` 回跳 `loop`。

SSA 下：
1. 诱导变量（loop var）：在 `entry` 生成初值 `initVal`；在 `loop` 头插入 `phi i32 [initVal, entry] [nextVal, loopbody]`；`nextVal` 在 `loopbody` 末尾生成（`i + step`），回跳时作为 `loopbody → loop` 的 incoming。
2. **循环携带变量**（在循环体内被赋值，且在循环条件、循环体后或后续循环迭代中读取的变量，如 `sum` 中的 `s`）：
   - 进行一次轻量前扫描：收集循环体内被 `Assign` 修改的符号集。
   - 对该集中在条件或 `afterloop` 中读取的符号，在 `loop` 头也插入 `phi`（`[initVal, entry] [curVal, loopbody]`）。
   - 其余仅在循环体内读写、循环外不可见的变量：不需 phi，版本栈在 `loopbody` 内维护即可。
3. 条件表达式使用循环头 `phi` 的结果；`afterloop` 使用循环结束后的当前值。

### 4.7 Block / Sequence（`codegen.cc:908`、`668`）

按顺序生成子表达式。块内声明的符号是独立 `VariableSymbol` 对象，不与外层同名符号冲突；块退出后外层符号的版本栈未受影响（因 key 不同）。无需额外 restore。

若某个块内对**外层符号**做了赋值，且该赋值应在块退出后对外层可见（本语言中块内赋值外层变量即属此种情况），则在块退出后该符号版本栈顶即为新值，无需额外处理；若语义要求块退出后回退到块前值，则在块入口 `snapshotValues`、出口 `restoreValues`。

## 5. Phi 插入的具体算法（手工版，不计算支配边界）

本语言控制流结构固定，不需要全局支配边界分析：

| 构造 | 汇合点 | Phi 插入位置 | Incoming 来源 |
|------|--------|-------------|--------------|
| `if c then t else e` | `mergeBlock` | `mergeBlock` 第一条指令前 | `[valT, thenBlock]`, `[valE, elseBlock]`；未修改分支传入分支前快照值 |
| `for i = a, b, step in body` | `loop` 头 | `loop` 第一条指令前 | `[initVal, entry]`, `[nextVal, loopbody]` |
| `for` 中循环携带变量 `s` | `loop` 头 | 诱导变量 phi 之后 | `[initS, entry]`, `[curS, loopbody]` |

`PHINode` 创建用 `llvm::PHINode::Create(type, numIncoming, name, insertPoint)`，随后 `addIncoming(value, block)`。`insertPoint` 可选 `mergeBlock` 或 `loop` 头的第一条非 phi 指令之前。

## 6. 测试迁移

`Calculater/unit_test.cc` 中 `runCodegenTests` 的手工绑定片段（约 1308-1327 行）：

```cpp
auto sym = std::make_shared<VariableSymbol>(decl, 0, 1);
auto* locAlloca = cc.createEntryBlockAlloca(fn, "loc", cc.getType("int"));
cc.namedValues[locSymbol] = locAlloca;        // 旧：绑定 alloca
auto* vv = codegen(var.get(), cc);            // 旧：codegenVariable 做 load
```

改为：

```cpp
auto sym = std::make_shared<VariableSymbol>(decl, 0, 1);
llvm::Value* initVal = llvm::ConstantInt::get(cc.getType("int"), 0);
cc.namedValues[locSymbol] = initVal;           // 新：绑定 SSA 值
auto* vv = codegen(var.get(), cc);             // 新：直接返回该 SSA 值
```

并新增断言：
- 顶层表达式 `codegenTopLevel(FCNumberExprAST(7))` 生成 `__fc_main` 且入口块以 `ret i32 7` 终结（已在 `901048b` 中补）。
- `def add(a:int,b:int){a+b}` 的 IR 中参数直接作为 `i32` 值使用，无 `alloca` / `store` 到参数槽。
- `if` 三分支与 `for` 循环的 IR 中出现 `phi` 指令。

## 7. 验证标准（必须在合并前满足）

1. `build-strict`（`FC_ENABLE_NESTED_FUNCTIONS=OFF`）：`FunctionsUnitTest` **803/803 全绿**。
2. `build-nested`（`FC_ENABLE_NESTED_FUNCTIONS=ON`）：`FunctionsUnitTest` **807/807 全绿**。
3. `tools/coverage.ps1 -BuildDir build-strict`：Overall **≥95%**。
4. `--emit-ir '1 + 2 * 3'` 输出包含 `define internal i32 @__fc_main()` 与 `ret i32 7`，且无 `alloca`。
5. `--emit-ir 'def sum(n:int){...} sum(4)'` 的 `loop` 块出现 `phi i32`（诱导变量与累加器）。
6. `--emit-ir 'def f(x:int){ if x < 0 then -x else x }'` 的 `ifcont` 合并块出现 `phi`。
7. 现有 `runCodegenTests` 中的 API 级断言（字面量、二元运算、局部变量读写、函数验证）全部通过（经测试迁移后）。

## 8. 回退与已知限制

- 若某个符号需要取地址（本语言当前无此语法），或 SSA 版本栈在复杂嵌套控制流中出现不匹配，可对该符号回退到 `createEntryBlockAlloca` + `load/store`，并在 `namedValues` 中同时保留 `AllocaInst*` 的兼容映射，或直接报 `codegen error: SSA not supported for this variable`。
- 字符串拼接返回指针，`__fc_main` 要求 `i32`，现有 `castValue` 回退 `ConstantInt 0` 的行为保留（非 SSA 核心，不在本方案改动范围）。
- `codegenStandaloneTopLevel` 里的 `verifyFunction` 失败返回 `nullptr`，`--emit-ir` 对无效 IR 退出码 2，保持不变。

## 9. 提交计划（按惯例分提交）

| 提交 | 内容 | 预计验证 |
|------|------|----------|
| `ssA-infra` | `codegen.h` 增 SSA 版本栈字段与方法；`namedValues` 类型改为 `llvm::Value*` | 构建通过，旧测试因类型不匹配预期失败 |
| `ssA-expr` | 重写 `codegenVariable`、`codegenBinary(Assign)`、`codegenDeclaration`（局部） | 构建通过，字面量/二元运算/简单变量测试通过 |
| `ssA-function` | `codegenFunction` 参数直接绑定 SSA 值，删参数 alloca/store | 函数调用测试通过 |
| `ssA-control` | `codegenIf` 插 phi、`codegenFor` 插诱导变量与循环携带变量 phi | `if`/`for` 测试通过，IR 出现 phi |
| `ssA-tests` | 迁移 `unit_test.cc` 手工绑定；补 phi 断言 | 803/803 全绿 |
| `ssA-docs` | `ARCHITECTURE.md`、`README`、`walkthrough/05-codegen.md` 同步 | 文档与代码一致 |
| `ssA-verify` | 双配置全量测试 + 覆盖率 ≥95% | 803/803、807/807、覆盖率达标 |

预计改动行数：`codegen.h` +30 行，`codegen.cc` +150/−80 行，`unit_test.cc` +40/−30 行。

完成后即可删掉或保留 `--emit-ir` 流水线中的 `mem2reg` Pass（若加过），改为直接展示前端生成的 SSA IR。