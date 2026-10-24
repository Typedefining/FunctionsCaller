# 第 3 段 · 语义分析：名字如何变成槽位

> 与第 1、2 段同一套走法：**先跑 → 读懂输出 → 按调用顺序读代码 → 补这一段需要的概念 → 动手改一行**。
>
> 本段验收标准（读完应能不查资料回答）：
> 1. 语义分析的输出里，`slot`、`depth`、`frameIndex`、`bindings` 各回答什么问题；
> 2. 为什么 `sum` 的参数 `n` 是 `depth=0`，局部变量 `s` 是 `depth=3`，循环变量 `i` 是 `depth=4`；
> 3. 符号表（按名字索引）与绑定表（按 AST 节点索引）为什么必须同时存在，遮蔽时它们各自表现如何。

---

## 第 1 步：先跑，把语义分析结果打印出来

语义阶段的产物是每个函数帧的布局，同样看不见。项目里新增了 `FC_TRACE=semantic` 阶段，把编译结果打印到 stderr：

```powershell
$env:FC_TRACE = "semantic"
```

打印位置在 `Calculater/scanner.cc:42-45`：语义分析成功之后、返回 AST 之前。打印器是 `Calculater/semantic_dump.h` 与 `Calculater/semantic_dump.cc`。

**改动一**（只为换成本段的例子）：`Calculater/main.cc` 第 6 行。

```cpp
scanner.analysis("var a:int = 1 + 2 * 3; a");
```

构建并运行：

```powershell
cmake --build build-strict --config Debug --target FunctionsCalculater
.\build-strict\Calculater\Debug\FunctionsCalculater.exe
```

实测输出（原样抄录）：

```text
CompiledProgram
  frame <global> (global) frameSize=0 maxTempSlots=0
    a:int slot=0 depth=0 frameIndex=-1
  bindings=2
```

换成带函数与循环的程序（`scanner.analysis("def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } sum(4)");`）：

```text
CompiledProgram
  frame <global> (global) frameSize=0 maxTempSlots=0
  frame sum (function) frameSize=3 maxTempSlots=3 params=[0]
    n:int slot=0 depth=0 frameIndex=0
    s:int slot=1 depth=3 frameIndex=0
    i:int slot=2 depth=4 frameIndex=0
  bindings=8
```

再看一个同名遮蔽的程序（`scanner.analysis("def f() { var x:int = 1; { var x:int = 2; x } x } f()")`）：

```text
CompiledProgram
  frame <global> (global) frameSize=0 maxTempSlots=0
  frame f (function) frameSize=2 maxTempSlots=2
    x:int slot=0 depth=3 frameIndex=0
  bindings=4
```

注意最后这个输出：`frameSize=2` 说明帧里确实有两个变量，但符号表只列出**一个** `x`。这不是打印器漏了，而是符号表按名字索引，同名只保留一个。区分两个 `x` 的是绑定表（`bindings=4`），第 2 步解释。

看完复原（同时关掉开关）：

```powershell
git checkout -- Calculater/main.cc
Remove-Item Env:\FC_TRACE
```

---

## 第 2 步：读懂这份输出

| 字段 | 回答的问题 | 谁写它 |
|---|---|---|
| `frame <name> (global\|function)` | 这是哪一个栈帧 | `reset()` 建立 `<global>`（`semantic.cc:17-34`）；`registerFunction()` 移交函数帧（`semantic.cc:174-207`） |
| `frameSize=N` | 这个帧需要多少个值槽位 | `FrameLayout::allocateSlot()` 每次声明加一（`semantic.h:43-62`）；函数帧在分析结束时回填（`semantic.cc:481-485`） |
| `maxTempSlots=M` | 分析期间该帧同时占用的槽位峰值 | 同上，`FrameLayout` 记录峰值 |
| `params=[0, 1]` | 形参在帧里的槽位，按声明顺序 | `registerFunction()`（`semantic.cc:197-204`） |
| `slot=S` | 该变量在所属帧中的下标 | `declareVariable()` 分配（`semantic.cc:153`） |
| `depth=D` | 声明时作用域栈的深度 | `Scope::depth`（`semantic.h:36-39`） |
| `frameIndex=F` | 运行期用第几个栈帧取值：`-1` 表示全局帧，`0` 表示当前函数帧 | `declareVariable()`（`semantic.cc:157`） |
| `bindings=K` | 绑定表里记录了多少条“AST 节点 → 符号” | `bindVariable()`（`semantic.cc:292-298`） |

几个容易误读的地方：

- **`<global>` 的 `frameSize=0`，但 `a` 的 `slot=0`。** 全局变量确实占槽位，只是全局帧记录不保存帧大小：求值阶段建全局帧时按 `globalSymbols.size()` 定长（第 4 段会看到）。这是本段的一个悬而未决处，先记住现象。
- **参数 `n` 的 `depth=0`，而函数体的块是 `depth=3`。** `pushFunctionScope()` 压入的是一个全新的空作用域（`semantic.cc:46-62`），参数声明在其中，深度为 0；函数体的 `{ ... }` 又经由 `pushScope()` 压入子作用域，栈里此时已有全局、函数两个作用域，于是深度为 3。`s` 与 `i` 落在函数体块与 `for` 作用域里，深度分别是 3、4。
- **`bindings` 数为 2 的程序是“一个声明 + 一次引用”。** `var a:int = ...` 的声明节点绑定一次，`a` 的引用节点绑定一次。`sum` 的 8 条则是声明（`s`、`i`）、引用（`n`、`s`、`i`、`s`、`i`、`s`）等节点各一条。
- **遮蔽时符号表只剩一个 `x`，绑定表仍有 4 条。** 符号表（`SymbolTable`）是“名字 → 符号”的字典，同名不重复登记（`semantic.cc:260-269`）；绑定表（`SemanticBinding`）是“AST 节点指针 → 符号”的映射（`token.h:153-171`）。解析一个引用的语义时，真正决定它指向哪个声明的是绑定表；符号表更像“某个帧里出现过哪些名字”的摘要。

---

## 第 3 步：按调用顺序读代码（一条路径，不要跳读）

| 顺序 | 位置 | 作用 |
|---|---|---|
| 1 | `Calculater/scanner.cc:35-45` | 把纯语法树交给 `SemanticAnalyzer::analyze()`，成功后打印 |
| 2 | `Calculater/semantic.cc:321-326` | `analyze()`：入口，复位后分析整棵树 |
| 3 | `Calculater/semantic.cc:17-34` | `reset()`：建立 `<global>` 作用域与帧记录 |
| 4 | `Calculater/semantic.cc:328-359` | `analyzeExpr()`：`dynamic_cast` 分派 12 种节点 |
| 5 | `Calculater/semantic.cc:361-378` | `analyzeProgram()` / `analyzeBlock()`：逐条语句分析 |
| 6 | `Calculater/semantic.cc:389-399` | `analyzeVarDecl()`：声明一个变量并绑定声明节点 |
| 7 | `Calculater/semantic.cc:137-168` | `declareVariable()`：分配槽位、登记符号表、写 `frameIndex` |
| 8 | `Calculater/semantic.cc:102-110` | `lookupVariableShared()`：自栈顶向下找名字 |
| 9 | `Calculater/semantic.cc:502-513` | `resolveVariable()`：自栈顶向下解析名字，内层函数可引用外层名字 |
| 10 | `Calculater/semantic.cc:450-492` | `analyzeFunction()`：声明参数 → 注册函数 → 分析函数体 → 回填帧大小 |
| 11 | `Calculater/semantic.cc:174-207` | `registerFunction()`：把当前帧记录移交给函数名 |
| 12 | `Calculater/semantic.cc:290-301` | `bindVariable()` / `boundSymbol()`：写、读绑定表 |

### `reset()`（`semantic.cc:17-34`）

每次分析开始都重建一个 `<global>` 帧：压入空作用域、重置帧布局、把帧记录登记到 `functions["<global>"]`。它同时把 `m_frameAnchor` 归零，`isCurrentFrameGlobal()` 因此为真（`semantic.cc:83-86`）——全局区的声明全部落在这一帧上。

### `declareVariable()`（`semantic.cc:137-168`）

声明一个变量的全部动作都在这里，顺序固定：

1. 在当前作用域登记符号（`SymbolTable::addSymbol()`，`semantic.cc:260-269`；同名不覆盖，直接返回已存在的符号）；
2. 在**当前帧**的布局里分配槽位 `slot`（`semantic.cc:152`）；
3. 写 `runtime.frameIndex`：当前帧是全局帧则 `-1`，否则 `0`（`semantic.cc:157`）；
4. 返回符号，调用方把它绑定到声明节点。

第 3 步的 `-1/0` 是本段最值得注意的设计：它**不是真实的帧编号**，而是“全局帧 / 当前函数帧”的二值标记。求值阶段据此选择 `callStack[0]` 或 `callStack.back()`（第 4 段展开）。

### `resolveVariable()`（`semantic.cc:502-513`）

引用一个名字时只有一条查找路径：`lookupVariableShared()`（`semantic.cc:102-110`）自栈顶向下遍历整个作用域栈，全局作用域也在栈里，因此全局变量与局部变量共用同一次查找；找不到时报 `variable 'x' is not declared`（`semantic.cc:404`）。找到符号后直接返回：作用域栈连续，内层函数因此能引用外层函数的局部变量与全局变量；能否执行由求值器与 codegen 决定，二者都拒绝在帧内执行函数定义。

### `analyzeFunction()`（`semantic.cc:450-492`）

函数定义的分析顺序是固定的五段，任何一步都不能换序：

1. 嵌套守卫：默认配置下函数内不允许再定义函数（`semantic.cc:452-458`）；
2. `FunctionScopeGuard` 压入新帧（`semantic.cc:460`）；
3. 声明形参，于是形参先占槽位（`semantic.cc:463-468`）；
4. `registerFunction()` 把当前帧记录交给函数名（`semantic.cc:471`）；
5. 分析函数体（`semantic.cc:476`），结束后回填 `frameSize` 与 `maxTempSlots`（`semantic.cc:479-483`）。

顺序的含义：形参必须比函数体里的局部变量先分配槽位（`params=[0, 1]` 因此稳定为前缀），而函数记录必须在分析函数体之前就登记好，递归调用才能查到自己的签名。

---

## 第 4 步：这一段需要的概念

1. **名称解析（name resolution）**：把一个名字在某一位置解析到唯一的声明。编译器里也叫“名字绑定”，与“绑定表”是同一件事的两面。
2. **作用域（scope）与作用域栈**：每进入一个 `{}` 或一个函数就压入一层，离开时弹出。查找名字时自栈顶向下，先命中的声明优先——这就是遮蔽。
3. **符号（symbol）与符号表（symbol table）**：符号是声明的编译期表示（名字、类型、槽位、所属帧），符号表是按名字索引的容器。
4. **绑定（binding）与绑定表**：名字解析的产物是“这个 AST 节点指向那个符号”的映射。本项目把它放在 `SemanticBinding` 里，键是节点指针，值是符号。
5. **帧布局（frame layout）与槽位分配（slot allocation）**：为每个帧内的变量分配下标，运行期用“帧 + 下标”定位值。槽位在编译期固定，帧大小因此也是编译期常量。
6. **静态嵌套深度与运行时帧下标**：`depth` 是编译期的作用域深度，`frameIndex` 是运行期的帧选择。两者不是一回事：`depth` 可以很大，`frameIndex` 在本项目里只有 `-1` 与 `0` 两个值。
7. **声明与引用（declaration / use）**：同一个名字在程序里出现多次，只有声明分配槽位，引用共享同一个符号——这正是 `bindings` 计数大于符号数的原因。

---

## 第 5 步：这一层曾经踩过的四个坑（都已修掉）

1. **两个函数共享同一条帧记录**：早期用占位名登记帧，注册函数名时“改名 + 移动”逻辑错位，`isEven` 与 `isOdd` 一度指向同一个 `CompiledFunction`（`params=[0, 0]`、`frameSize=2`）。现在 `registerFunction()` 把当前帧记录**移交**给函数名（`semantic.cc:184-186`），每个函数独立持有自己的记录。
2. **两个深度共用一个计数器**：帧基址曾取 `m_scopeStack.size()`，而帧记录数组只按函数帧增长——函数体内再定义函数时，下标越界，Debug 下断言失败。现在帧基址用帧计数域（`m_frameRecords.size()`），同时保存进入时的作用域深度用于弹出（`semantic.cc:46-62`）。
3. **声明节点没有绑定**：`analyzeVarDecl()` 曾只声明不绑定，语义分析照样通过，求值阶段才因为取不到符号返回 `Error`。现在声明后立刻 `bindVariable()`（`semantic.cc:394`）。
4. **符号表同名不覆盖**：`SymbolTable::addSymbol()` 遇到同名直接返回已有符号，而 `declareVariable()` 不检查返回值——遮蔽时内层声明在符号表里“消失”，只有绑定表还保留两条不同的绑定。这个行为是刻意的（符号表用于摘要），但读代码时容易误判成丢符号。

---

## 第 6 步：动手改一行

每个实验都只改一处、改完复原，观察输出变化。

**实验 1：让全局变量也标成当前帧**

在 `semantic.cc:157` 把

```cpp
symbol->runtime.frameIndex = isCurrentFrameGlobal() ? -1 : 0;
```

改成 `symbol->runtime.frameIndex = 0;`。重新运行第 1 步的第一个程序，实测：

```text
  frame <global> (global) frameSize=0 maxTempSlots=0
    a:int slot=0 depth=0 frameIndex=0
```

全局变量从 `-1` 变成 `0`。注意**语义分析仍然成功**：`frameIndex` 只是一个标注，写错了不会被语义检查拦住，错误要等到运行期读错帧才暴露——这也是它必须被打印出来的原因。

**实验 2：删掉声明节点的绑定**

把 `analyzeVarDecl()` 里的 `m_ctx.bindVariable(node, symbol);`（`semantic.cc:394`）删掉，再跑第 1 步的程序，实测 `bindings` 从 2 降到 1；`sum` 从 8 降到 6（少了 `s`、`i` 两个声明），遮蔽用例从 4 降到 2。语义分析依旧通过，因为没有“每个声明都必须有绑定”的检查。

**实验 3：让名字查找只看当前作用域**

把 `lookupVariableShared()`（`semantic.cc:102-110`）改成只查 `m_scopeStack.back()`，再跑第 1 步的程序，实测：

```text
def add(a:int, b:int) { a + b } add(2, 3)   → rejected，Error: variable 'a' is not declared
def fib(n:int) { ... } fib(5)               → rejected，Error: variable 'n' is not declared
var g:int = 0; def inc() { g = g + 1 } ...  → rejected，Error: variable 'g' is not declared
def outer() { var x:int = 1; { x } }        → rejected，Error: variable 'x' is not declared
```

四种失败各自对应一层作用域边界：函数体块看不到函数作用域的形参（`a`、`n`），函数体看不到全局（`g`），内层块看不到外层块的局部（`x`）。这条实验说明“自栈顶向下遍历整个作用域栈”不是可选的优化，而是名称解析的定义。

---

## 第 7 步：自测题

**Q1**：`frameIndex=-1` 与 `frameIndex=0` 分别表示什么？它是帧编号吗？

<details><summary>答案</summary>

不是帧编号，是二值标记：`-1` 表示全局帧，`0` 表示当前函数帧。求值阶段据此选择 `callStack[0]` 或 `callStack.back()`。名字解析允许内层函数引用外层名字，但执行嵌套函数本身被求值器与 codegen 拒绝，因此不需要真实的帧编号。

</details>

**Q2**：`sum` 的参数 `n` 是 `depth=0`，局部 `s` 是 `depth=3`，这两个数字为什么不同？

<details><summary>答案</summary>

`depth` 是声明时作用域栈的深度。形参在 `pushFunctionScope()` 压入的新作用域里声明，此时栈里只有全局与函数两个作用域，深度按实现记为 0；函数体的 `{ ... }` 再压一层，`s` 在其中，深度为 3；`for` 再压一层，`i` 深度为 4。

</details>

**Q3**：遮蔽用例里 `frameSize=2` 而符号表只有一个 `x`，两处矛盾吗？

<details><summary>答案</summary>

不矛盾。`frameSize` 来自帧布局，两个 `x` 各占一个槽位，所以是 2；符号表按名字索引，同名不重复登记，所以只列出外层那个 `x`。区分两个 `x` 的是绑定表：声明节点与引用节点各自绑定到不同的符号，`bindings=4` 记录了这些条目。

</details>

**Q4**：`analyzeFunction()` 为什么必须先声明形参、再注册函数、最后分析函数体？

<details><summary>答案</summary>

先声明形参才能让形参占据帧槽位的前缀（`params=[0, 1]`）；先注册函数才能让函数体内的递归调用查到自己的签名；帧大小只能在函数体分析完之后回填，因为局部变量是在分析过程中才逐个分配槽位的。

</details>

---

## 下一段的接口

语义层交给求值层的是三样东西：每个变量的 `slot` 与 `frameIndex`、每个函数帧的 `frameSize`、以及“AST 节点 → 符号”的绑定表。下一段看求值器如何用一张调用栈装载这些帧、如何用 `frameIndex` 选择帧、如何在递归调用时压栈与弹栈。
