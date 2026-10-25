# 第 4 段 · 求值器：一次调用如何占据一个栈帧

前三段把源码变成了 `CompiledProgram`（AST + 符号表 + 每个函数的帧大小）。
这一段让 AST 真正跑起来：输入是 AST 与 `CompiledProgram`，输出是一个 `FCValue`
（整数、浮点数、字符串或 `Dangle`）。

求值器不查名字表。它需要的三样东西——变量在哪一帧、在第几个槽位、
函数帧要分配多大——全部来自语义期写进符号和 `CompiledFunction` 的字段。
所以这一段的重点是**这些字段如何被消费**，而不是再次解析源码。

## 第 1 步：先跑，把调用过程打印出来

先构建入口程序（它现在能直接求值）：

```
cmd /c "C:\PROGRA~1\CMake\bin\cmake.exe --build build-strict --config Debug --target FunctionsCalculater --parallel 16"
```

跑一个两参数调用：

```
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def add(a:int, b:int) { a + b } add(2, 3)'
```

stdout：

```
[result] 5
```

打开求值器阶段的诊断输出，再跑一次（`FC_TRACE` 的值是阶段名，逗号分隔）：

```
$env:FC_TRACE = "evaluator"
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def add(a:int, b:int) { a + b } add(2, 3)'
```

stdout 不变，stderr 多两行：

```
[evaluator] call add(2, 3) depth=2
[evaluator] return add = 5
```

递归能看出调用栈的形状。`fib(4)` 的 stderr（stdout 只有 `[result] 3`）：

```
[evaluator] call fib(4) depth=2
[evaluator] call fib(3) depth=3
[evaluator] call fib(2) depth=4
[evaluator] call fib(1) depth=5
[evaluator] return fib = 1
[evaluator] call fib(0) depth=5
[evaluator] return fib = 0
[evaluator] return fib = 1
[evaluator] call fib(1) depth=4
[evaluator] return fib = 1
[evaluator] return fib = 2
[evaluator] call fib(2) depth=3
[evaluator] call fib(1) depth=4
[evaluator] return fib = 1
[evaluator] call fib(0) depth=4
[evaluator] return fib = 0
[evaluator] return fib = 1
[evaluator] return fib = 3
```

全局变量与函数调用混在一起时，变量读写落在栈底帧上，而不是当前调用帧：

```
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'var g:int = 0; def inc() { g = g + 1 } inc() inc() g'
```

```
[result] 2
[evaluator] call inc() depth=2
[evaluator] return inc = 1
[evaluator] call inc() depth=2
[evaluator] return inc = 2
```

失败路径也会留下痕迹。函数不存在时：

```
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def bad() { missing() } bad()'
```

```
[result] Dangle
Function not found: missing
[evaluator] return bad = Dangle
```

（第一行来自 stdout，后两行来自 stderr。）

注意实验做完后清空变量，否则后续章节的 stderr 会混进不需要的行：

```
$env:FC_TRACE = ""
```

## 第 2 步：读懂这份输出

两行日志各回答一个问题。

| 行 | 时机 | 读什么 |
|---|---|---|
| `call name(...) depth=N` | 实参求值完、写入形参槽之后 | 括号里是**实参的值**，`depth` 是 `callStack.size()` |
| `return name = V` | 函数体求值完、`popFrame()` 之前 | `V` 是函数体的求值结果 |

`depth` 从 2 开始，不是从 1 开始：`FCEvaluationContext` 构造时就压入了一个
`<global>` 帧（`evaluator.cc:447-454`），它是栈底，承载所有全局变量。
所以第一层函数调用发生时栈里已经有 1 个帧，调用帧就是第 2 个。

`call` 行里是 `add(2, 3)` 而不是 `add(a, b)`：打印发生在实参已经求值、
并且已经把值写进形参槽之后（`evaluator.cc:329-342`）。它显示的是运行期的值。

`fib` 的 trace 是深度优先的：`call fib(4)` 之后立刻 `call fib(3)`，
一直压到 `fib(1)` 才出现第一个 `return`，然后逐层弹回。
同一个 `depth` 上出现多次 `call`（例如 `depth=5` 的 `fib(1)` 与 `fib(0)`），
说明它们是同一个调用帧先后求出的两个实参，不是两次嵌套。

`return` 行的值允许是 `Dangle`：函数体出错时求值器不抛异常，而是把 `Dangle`
沿调用链返回，`call` 行已经打印过的调用照样有一条 `return` 记录。

## 第 3 步：按调用顺序读代码（一条路径，不要跳读）

以 `add(2, 3)` 为例，执行顺序如下。

1. `Calculater/main.cc:32-43`：分析源码拿到 AST，用
   `scanner.semanticContext().getCompiledProgram()` 构造 `FCEvaluationContext`，
   然后调 `evaluate`。
2. `Calculater/evaluator.cc:476-478`：`evaluate()` 只是转发到
   `evaluateExpression()`。
3. `Calculater/evaluator.cc:354-405`：`evaluateExpression()` 用 `dynamic_cast`
   分派节点类型。顶层 `FCProgramAST` 走 `evaluateProgram()`（`165-175`）：
   先用 `context.functions.index(node)` 把程序里的所有 `def` 登记进注册表
   （`428-445`），然后跳过 `def` 语句、按顺序求其余语句，返回最后一个值。
   所以顶层 `def` 本身不产生值；如果 `def` 出现在表达式位置（函数体内的嵌套
   定义），则走 `391-402` 的分支。
4. `Calculater/evaluator.cc:277-352`：调用表达式走 `evaluateCall()`，顺序是
   - 先取函数 AST（`258-263`），取不到就报 `Function not found`；
   - 逐个求实参（`272-279`），任一实参是 `Dangle` 就整次调用短路；
   - `pushFrame()`（`302`）分配新帧；
   - 取该函数的编译记录（`302-312`），缺失就报错并弹帧；
   - 按形参名字查符号，把实参值写进符号的槽位（`314-329`）；
   - 打印 `call`（`331-342`），求函数体（`344`），打印 `return`（`346-348`），
     弹帧（`350`）。
5. `Calculater/evaluator.cc:456-464`：`pushFrame()` 只做一件事——
   用 `compiledProgram.getFunction(name)->frameSize` 决定 `locals` 的大小。
   帧大小不是运行期数出来的，是语义期回填的快照。
6. `Calculater/evaluator.cc:61-85`：`evaluateVariable()` 用绑定表拿到符号
   （`evaluator.h:27-31`），再用 `frameForSymbol()` 选帧
   （`evaluator.h:40-49`：`frameIndex == -1` 取 `callStack[0]`，否则取 `back()`），
   最后做槽位边界检查并读取 `frame.locals[symbol->slot]`。
7. `Calculater/evaluator.cc:178-275`：`evaluateBinary()` 负责算术；`Assign`
   分支（`187`）把结果写回左操作数符号的槽位，其余分支只返回值。
8. `Calculater/evaluator.cc:87-135`：`evaluateFor()` 不建立新帧，
   循环变量与循环体都在当前帧里读写（`91` 写初值、`98` 加步长）。

读完之后应该能回答：为什么求值器一行代码都不需要知道变量名？
因为所有名字到槽位的映射在 `CompiledProgram` 里已经完成。

## 第 4 步：这一段需要的概念

- **调用栈（call stack）**：当前活跃的函数调用序列，栈底是全局帧。
- **栈帧（frame）**：一次调用独占的 `Frame` 对象，含 `funcName` 与 `locals`。
- **槽位（slot）**：变量在该帧 `locals` 中的下标；同名变量在不同作用域可以是
  不同槽位，语义期分配一次，运行期只按下标读写。
- **帧大小（frameSize）**：该函数所有局部变量与临时值需要的槽位总数；
  `pushFrame()` 按它分配，槽位越界说明两侧不一致。
- **按值传参**：实参先求值成 `FCValue`，再复制进形参槽位；函数体看不到调用方
  的表达式。
- **Dangle**：求值失败的统一表示，本层有至少四条来源——函数未找到、编译记录
  缺失、形参槽越界、变量未绑定或槽越界。
- **块的值**：块按顺序求值，返回最后一条表达式的值；所以
  `def f() { var x:int = 1; { var x:int = 2; x } x } f()` 得到 `1`，
  内层块的值 `2` 没有被使用。

## 第 5 步：这一层曾经踩过的四个缺陷（都已修掉）

1. **形参槽位用了未定义的变量**。早期版本在写实参时直接引用了一个不存在的
   `slot`，编译都过不去。改成从符号取：`frame.locals[symbol->slot]`。
2. **用作用域深度判断全局**。`frameForSymbol()` 曾经用“`scopeDepth == 0` 就是
   全局”作判据，而顶层块里的变量作用域深度不是 0，于是被当成当前帧的变量，
   读取时找不到槽位。改成看符号的 `runtime.frameIndex`。回归用例是
   `{ var g:int = 42; g }`——今天它仍然在测试里。
3. **零参数函数绕过了编译记录检查**。检查原本写在参数循环内部，参数个数为 0
   时整个循环不执行，函数没有编译记录也能被调用。现在检查提到循环之前
   （`evaluator.cc:302-312`）。
4. **嵌套函数定义在求值期没有承载者**。函数体里的 `def` 既不产生值，也没有
   对应的寄存器位置；直接执行会让调用栈语义失真。现在只要栈里多于一个帧，
   就报 `nested function definition ... is not supported by the evaluator`
   （`evaluator.cc:391-402`）。

## 第 6 步：动手改一行

三个实验都在 `Calculater/evaluator.cc` 里改一行，改完重新构建 `FunctionsCalculater`
再跑。**每次实验后都要恢复文件并重建**，否则后面的构建会沿用改坏的实现。

**实验 1：整数除法改成减法**

把 `evaluator.cc:229-230` 的

```cpp
return FCValue{*lhs.asInteger() / *rhs.asInteger()};
```

改成减号，然后跑：

```
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def q(a:int, b:int) { a / b } q(7, 2)'
```

改动前 `[result] 3`，改动后 `[result] 5`。运算符实现只有这一处，
语义层和解析层都不参与，所以改一处就改了整个语言的除法语义。

**实验 2：把帧大小换成 0**

把 `evaluator.cc:461` 的 `frame.locals.resize(compiled->frameSize);`
改成 `frame.locals.resize(0);`，再跑 `add(2, 3)`：

```
Error: unresolved parameter 'add'
[result] Dangle
```

形参有槽位，但帧里一个槽也没有，写参数时越界。这说明帧大小是语义期快照，
不是运行期按参数个数临时算的。

**实验 3：永远写第一个实参**

把 `evaluator.cc:329` 的 `argumentValues[i]` 改成 `argumentValues[0]`：

```
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def add(a:int, b:int) { a + b } add(2, 3)'
& .\build-strict\Calculater\Debug\FunctionsCalculater.exe 'def three(a:int, b:int, c:int) { a + b + c } three(1, 2, 3)'
```

第一个从 `5` 变 `4`，第二个从 `6` 变 `3`。每个形参写哪个实参，
由语义期的 `parametersIdx` 和槽位决定；求值器只是按顺序写槽。

## 第 7 步：自测题

1. 为什么 `call` 行的 `depth` 从 2 开始？
2. `call add(2, 3)` 里为什么是值，不是形参名？
3. 如果把 `evaluateCall()` 末尾的 `popFrame()` 删掉，最先出问题的是什么？
4. `add` 的 `a`、`b` 分别落在槽 0 和槽 1，这个分配是谁做的？
5. 为什么 `for` 循环没有额外的帧，也没有 `call` 日志？
6. 求值器返回 `Dangle` 的路径至少有几条？各是什么？

<details>
<summary>参考答案</summary>

1. `FCEvaluationContext` 构造时压入了 `<global>` 帧作为栈底（`evaluator.cc:447-454`），
   第一层函数调用时栈里已有 1 个帧。
2. 打印发生在实参求值、写入形参槽之后（`evaluator.cc:329-342`），此时只有值。
3. 帧不弹，后续变量定位到错误的帧（`frameForSymbol()` 取 `back()`），
   表现为错值或 `Dangle`，并且 `depth` 只会增大不会回落。
4. 语义期 `declareVariable()` 分配槽位，参数槽位由 `registerFunction()` 写进
   `CompiledFunction::parametersIdx`；第 3 段的 `FC_TRACE=semantic` 输出里
   `params=[0, 1]` 就是这份结果。
5. 循环不是函数调用，`evaluateFor()` 直接在当前帧读写循环变量与循环体
   （`evaluator.cc:87-135`），没有 `pushFrame()`。
6. 四条：函数未找到、函数没有编译记录、形参槽位越界、变量未绑定或槽位越界。

</details>

## 下一段的接口

求值器消费的是 `CompiledProgram` 与 AST；下一段 codegen 消费同样的输入，
但输出换成 LLVM IR。它需要的信息与这一段完全相同：每个变量的槽位、
每个函数帧的大小、每个函数的参数表。读懂这一段之后，codegen 只是在
“按槽位读写内存”之外多了一层“把节点翻译成指令”。

codegen 目前还没有接入诊断输出（`Calculater/codegen.cc` 里没有 `traceEnabled` 调用）；它的可观察结果是生成的 LLVM IR 与 `verifyModule` 的校验结论。
