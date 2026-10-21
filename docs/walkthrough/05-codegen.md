# 第 5 段 · 代码生成：同一棵树如何变成 LLVM IR

前四段走完，程序已经能正确求值。这一段换一条后端路线：把同一份 AST 交给 LLVM，
生成可读的中间表示（IR）。读这一段的产出不需要 LLVM 背景，只需要接受一件事：
**IR 是一份显式的、带基本块和类型标注的程序文本**。

## 第 1 步：先跑，把 IR 打印出来

入口程序新增了 `--emit-ir` 模式（`Calculater/main.cc:47-67`）：

```powershell
$exe = ".\build-strict\Calculater\Debug\FunctionsCalculater.exe"
& $exe --emit-ir 'def add(a:int, b:int) { a + b } add(2, 3)'
```

输出（原样抄录）：

```llvm
; ModuleID = 'fc_module'
source_filename = "fc_module"

define i32 @add(i32 %a, i32 %b) {
entry:
  %b2 = alloca i32, align 4
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  store i32 %b, ptr %b2, align 4
  %a3 = load i32, ptr %a1, align 4
  %b4 = load i32, ptr %b2, align 4
  %addtmp = add i32 %a3, %b4
  ret i32 %addtmp
}

define internal i32 @__fc_main() {
entry:
  %calltmp = call i32 @add(i32 2, i32 3)
  ret i32 %calltmp
}
```

再跑两个更有代表性的程序，后面逐步解释：

```powershell
& $exe --emit-ir 'var a:int = 1 + 2 * 3; a'
& $exe --emit-ir 'def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } sum(4)'
```

第一条只生成一个全局变量和入口函数：

```llvm
@a = internal global i32 0

define internal i32 @__fc_main() {
entry:
  store i32 7, ptr @a, align 4
  %a = load i32, ptr @a, align 4
  ret i32 %a
}
```

第二条出现了循环的四个基本块（只摘关键部分）：

```llvm
define i32 @sum(i32 %n) {
entry:
  %i = alloca i32, align 4
  %s = alloca i32, align 4
  %n1 = alloca i32, align 4
  store i32 %n, ptr %n1, align 4
  store i32 0, ptr %s, align 4
  store i32 0, ptr %i, align 4
  br label %loop

loop:
  %i2 = load i32, ptr %i, align 4
  %n3 = load i32, ptr %n1, align 4
  %cmptmp = icmp slt i32 %i2, %n3
  %booltmp = zext i1 %cmptmp to i32
  %ifcond = icmp ne i32 %booltmp, 0
  br i1 %ifcond, label %loopbody, label %afterloop

loopbody:
  %s4 = load i32, ptr %s, align 4
  %i5 = load i32, ptr %i, align 4
  %addtmp = add i32 %s4, %i5
  store i32 %addtmp, ptr %s, align 4
  %nextvar = add i32 %i5, 1
  store i32 %nextvar, ptr %i, align 4
  br label %loop

afterloop:
  %s7 = load i32, ptr %s, align 4
  ret i32 %s7
}
```

## 第 2 步：读懂这份输出

四个观察，按依赖顺序：

1. **函数头带类型**。`define i32 @add(i32 %a, i32 %b)` 表示返回 `i32`、两个 `i32` 参数；
   `@` 前缀是函数与全局变量的名字空间，`%` 前缀是局部值。
2. **局部变量被落到内存槽**。`a`、`b` 都先 `alloca` 出一块栈空间，再 `store` 参数进去，
   用到时 `load`。所以后面所有赋值都不需要 SSA 的 phi 节点——写内存即可。
   名字后面的数字（`%a1`、`%b2`）是 LLVM 用来区分同名的 `alloca` 与参数，不是源码里的名字。
3. **基本块以终结指令结尾**。`entry` 以 `br`/`ret` 结束，`loop` 与 `loopbody` 以 `br` 结束；
   没有终结指令的块会被 `verifyFunction` 判为非法。
4. **入口函数由编译器生成**。`__fc_main` 是 `internal i32`，函数体就是"把整个程序当表达式求值再返回"。
   注意 `1 + 2 * 3` 的 IR 里没有 `add`：两个操作数都是常量，LLVM 常量折叠后直接得到
   `ret i32 7`。

## 第 3 步：按调用顺序读代码（一条路径，不要跳读）

以 `def add(a:int, b:int) { a + b } add(2, 3)` 为例，从入口一路读下去：

1. `Calculater/main.cc:47-67`：解析 `--emit-ir`，调用 `scanner.analysis(source)`，
   再构造 `FCCodegenContext` 并调用 `codegenTopLevel`。
2. `Calculater/codegen.cc:985`（`FCExprClass::codegenTopLevel`）：程序/函数/原型走原有分支，
   其余顶层表达式统一交给 `codegenStandaloneTopLevel`。
3. `Calculater/codegen.cc:776`（`codegenProgram`）：先为 `def add` 调 `codegenFunction`，
   再调 `codegenStandaloneTopLevel` 生成 `__fc_main` 并调用 `add`。
4. `Calculater/codegen.cc:461-540`（`codegenFunction`）：创建函数与 `entry` 块；
   `494-507` 逐个参数 `alloca` 并把实参 `store` 进槽位，同时把参数符号登记进 `namedValues`；
   然后生成函数体，`523-531` 用 `castValue` 转成返回类型并 `CreateRet`。
5. `Calculater/codegen.cc:292`（`codegenBinary`）：先递归生成两侧，
   `330` 用 `castValue` 统一类型，`404` 起按 `FCBinaryOp` 选择 `CreateAdd`/`CreateSub`/…。
6. `Calculater/codegen.cc:271`（`codegenVariable`）：从 `namedValues` 找到该符号的 `alloca`，
   生成 `load`；找不到就报 `assigned variable is not allocated`/`variable is not declared`。
7. `Calculater/codegen.cc:862-906`（`codegenStandaloneTopLevel`）：建/取 `__fc_main`，
   生成表达式，`896-899` 把结果转成 `i32` 后 `CreateRet`。
8. `Calculater/codegen.cc:145`（`castValue`）：整数/浮点互转与 `i1` 扩展，其余组合返回 `nullptr`。

读到这里应该能回答一个问题：**为什么求值器改了不影响 IR，IR 改了不影响求值结果？**
因为它们是同一份 AST 的两个独立后端，各自实现 AST 的语义，没有共享执行逻辑。

## 第 4 步：这一段需要的概念

- **IR（中间表示）**：编译器内部的语言，比源码显式（有类型、有基本块），比机器码抽象。
- **SSA 与内存槽**：SSA 要求每个值只被赋值一次；把可变变量放进 `alloca` 槽位、用 `store` 改写、
  用 `load` 读取，就绕开了 phi 节点。本项目所有变量都走内存槽。
- **基本块（basic block）**：一段顺序执行的指令，最后一条必须是 `ret`/`br` 这类终结指令。
- **全局变量**：`@a = internal global i32 0`，初值在模块里，程序启动后由入口函数内的 `store` 写入用户给定的初值。
- **`__fc_main` 约定**：程序是"最后一个表达式的值"，所以入口统一返回 `i32`，转换失败时退化为常量 0。

## 第 5 步：这一层曾经踩过的三个缺陷

1. **顶层裸表达式不生成入口函数**。修复前 `--emit-ir '1 + 2 * 3'` 只输出模块头，
   没有任何函数（退出码仍是 0）：`codegen` 只对 Block/Seq/VarDecl 做顶层包装。
   现在由 `codegenTopLevel`（`Calculater/codegen.cc:985`）统一兜住，规则变成"每个程序都有 `__fc_main`"。
2. **嵌套函数定义被当成普通表达式**。旧路径会把 `llvm::Function*` 当作表达式值交给外层，
   错误要等到类型转换时才暴露；现在 `Calculater/codegen.cc:942-948` 直接报
   `nested function definition is not supported by codegen`。
3. **字符串程序的入口返回值被静默转成 0**。`greet` 返回指针，`__fc_main` 要求 `i32`，
   `castValue` 没有指针到整数的转换，`codegenProgram` 里的回退逻辑于是写 `ret i32 0`。
   这不是崩溃，而是"返回值类型不支持"的既有约定；读 IR 时看到 `ret i32 0` 要能想到它。

## 第 6 步：动手改一行

三个实验都在 `Calculater/codegen.cc` 上做，改完记得恢复（用 `git checkout --`，
因为此时改动已提交，回滚是安全的）：

**实验 1：把加法改成减法。** 把 `codegen.cc:404` 的 `CreateAdd(lhs, rhs, "addtmp")`
改成 `CreateSub`，重新构建后：

- `--emit-ir '1 + 2 * 3'` 输出 `ret i32 -5`（常量折叠直接算出减法结果）；
- `--emit-ir 'var a:int = 1; var b:int = 2; a + b'` 输出 `%addtmp = sub i32 %a, %b`；
- 不带参数的默认模式仍然是 `[result] 7`——求值器没被改动。

**实验 2：删掉参数的初始化 `store`。** 删掉 `codegen.cc:496` 的
`context.builder.CreateStore(&argument, alloca);`，`add` 的 IR 里两个 `alloca` 仍在，
但 `store i32 %a, ptr %a1` 消失了：槽位从未被写入，`load` 读到的是未定义值。
IR 本身仍能通过校验，因为 LLVM 不检查未初始化内存——这类错误只有在执行时才暴露。

**实验 3：删掉入口函数的类型回退。** 把 `codegen.cc:897-898` 的两行
（`if (value == nullptr) value = llvm::ConstantInt::get(...)`）改成直接返回 `nullptr`，
再运行 `--emit-ir '"hi"'`：程序以退出码 2 结束，stdout 一行都没有——
`codegenTopLevel` 失败时入口程序不打印半个模块。

## 第 7 步：自测题

1. `%a1` 和 `%a` 有什么区别？
   `%a` 是参数；`%a1` 是为它分配的栈槽，数字是 LLVM 为避免重名自动加的。
2. 为什么循环里看不到 phi 节点？
   变量都在 `alloca` 槽位里，循环通过 `load`/`store` 读写，不需要 SSA 的值合并。
3. `entry` 块里没有 `ret` 会怎样？
   `verifyFunction` 报 `Basic Block in function ... does not have terminator!`，`codegenStandaloneTopLevel`
   返回 `nullptr`。
4. `--emit-ir '1 + 2 * 3'` 为什么只有 `ret i32 7`，没有 `add`？
   两个操作数都是常量，LLVM 在生成时就折叠成一个常量。
5. 字符串函数 `greet` 返回指针，为什么 `__fc_main` 返回 `i32 0`？
   `castValue` 不支持指针到整数的转换，`codegenProgram` 回退成常量 0；类型系统在这里做了静默降级。

## 下一段的接口

到这里，五个阶段（词法、语法、语义、求值、代码生成）各走过一遍。
下一段回到横向问题：**同一条错误在不同阶段分别长什么样**——词法错误、语法错误、
语义错误、求值错误、代码生成错误，以及它们各自会让程序停在哪一步。
