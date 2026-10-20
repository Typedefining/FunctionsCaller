# 第 2 段 · 语法分析：词法单元序列如何变成语法树

> 与第 1 段同一套走法：**先跑 → 读懂输出 → 按调用顺序读代码 → 补这一段需要的概念 → 动手改一行**。
>
> 本段验收标准（读完应能不查资料回答）：
> 1. `1 + 2 * 3` 为什么形成 `+` 在树根、`*` 在子树，而不是从左到右的链；`tokenPrecedence()` 与 `parseBinOpRHS()` 各决定其中哪一部分；
> 2. `parsePrimary()` 的 case 表与 `startsExpression()` 为什么必须保持同构，序列又在哪个 token 上终止；
> 3. `a; b` 为什么折叠成一个 `Seq`，而两条 `def` 却产生 `Program`。

---

## 第 1 步：先跑，把语法树打印出来

语法分析的产物是一棵树，不打印同样看不见。项目里新增了 `FC_TRACE=ast` 阶段，把纯语法树打印到 stderr：

```powershell
$env:FC_TRACE = "ast"
```

打印位置在 `Calculater/scanner.cc:29-32`：解析成功之后、语义分析之前。这是刻意选的位置——此时树里还没有任何符号表、作用域与 `frameIndex` 信息，看到的完全是语法层产物。

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
Seq (2 expressions)
  VarDecl a:int
    Binary +
      Number 1
      Binary *
        Number 2
        Number 3
  Variable a
```

把同一程序换成递归函数再看一次（`scanner.analysis("def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }");`）：

```text
Function fib(n:int)
  Block (1 expressions)
    If
      Binary <
        Variable n
        Number 2
      Variable n
      Binary +
        Call fib
          Binary -
            Variable n
            Number 1
        Call fib
          Binary -
            Variable n
            Number 2
```

看完复原（同时关掉开关）：

```powershell
git checkout -- Calculater/main.cc
Remove-Item Env:\FC_TRACE
```

---

## 第 2 步：读懂这两棵树

打印器在 `Calculater/ast_dump.h` 与 `Calculater/ast_dump.cc`：每层缩进 2 空格，节点名后跟该节点自有的信息（名字、类型、运算符），子节点按语法顺序排列。语法层只有三类容器，容易混淆，先分清：

| 输出 | 对应的节点 | 含义 | 出现条件 |
|---|---|---|---|
| `Program (N statements)` | `FCProgramAST` | 一段程序的多条**语句** | 顶层语句数 ≥ 2；只有 1 条时不额外包装 |
| `Seq (N expressions)` | `FCSeqExprAST` | 一条语句内由 `;` 连接的多个**表达式** | 同一语句里 `;` 分隔的表达式 ≥ 2 |
| `Block (N expressions)` | `FCBlockExprAST` | 花括号块体里的多个**表达式** | 每个 `{ ... }` |

`var a:int = 1 + 2 * 3; a` 的顶层是两条语句，但它们共享一个 `Seq`——原因见第 3 步最后的 `parseProgram()`：`parseSeqExpr()` 一次就能吃掉 `;` 连接的全部表达式，顶层因此只收到 **1 条**语句，`Program` 不出现。两条 `def` 之间没有 `;` 可供 `parseSeqExpr()` 续接，才会得到 2 条语句、被包成 `Program`。

再看优先级。`Binary +` 的两个孩子是 `Number 1` 和 `Binary *`，说明乘法先结合成子树，加法后结合——优先级高的运算符在树里更深。

`If` 有三个孩子，顺序固定为 **条件、then 分支、else 分支**；`For` 的孩子是 **初值、终止条件、步长（可省略）、循环体**；`Call` 的孩子就是实参，按书写顺序排列。

---

## 第 3 步：按调用顺序读代码（一条路径，不要跳读）

| 顺序 | 位置 | 作用 |
|---|---|---|
| 1 | `Calculater/scanner.cc:16-42` | `analysis()`：词法复位 → `m_parser.parseProgram()` → 打印 AST → 交给语义分析 |
| 2 | `Calculater/parser.cc:84-120` | `parseProgram()`：唯一入口，循环取语句 |
| 3 | `Calculater/parser.cc:122-146` | `parsePrimary()`：按 `kind` 分派到 9 种基本形式 |
| 4 | `Calculater/parser.cc:174-179` | `parseExpression()`：先取一个基本形式，再交给优先级处理 |
| 5 | `Calculater/parser.cc:158-181` | `parseBinOpRHS()`：优先级爬升，决定 `+`/`*` 谁先结合 |
| 6 | `Calculater/parser.cc:63-78` | `tokenPrecedence()`：优先级的**唯一**实现 |
| 7 | `Calculater/parser.cc:46-60` | `startsExpression()`：序列是否继续的唯一判据 |
| 8 | `Calculater/parser.cc:472-501` | `parseSeqExpr()`：`;` 连接的表达式序列 |
| 9 | `Calculater/parser.cc:434-470` | `parseBlockExpr()`：`{ ... }` 块体 |
| 10 | `Calculater/parser.cc:402-432` / `324-351` / `353-400` | 变量声明、`if`、`for` 三种带固定骨架的形式 |

### `parseProgram()`（`parser.cc:91-129`）

四个契约值得记住：

- `parseProgram()` 之前必须先 `reset()`：`assert(!m_started)`（`parser.cc:94`）。Parser 构造时不读 token，`begin()`（`parser.cc:22-26`）才推进到第一个词法单元。"已构造但未开始"这个状态因此不存在。
- `def` 是语句而不是表达式，单独走 `parseDefinition()`；定义体以 `}` 收尾，**该语句到此结束**。紧随的 `;` 会报 `';' must not follow a block terminator`（`parser.cc:103-106`），下一条语句直接开始：`def f() { 1 } f()` 合法。
- 顶层裸 `;` 报 `empty statement at top level`（`parser.cc:112-116`）——`;` 是分隔符，不是空语句。
- 任何一步失败都**立即返回** `nullptr`（`parser.cc:110`、`122`），不会带着同一个 token 重试。

### `parsePrimary()`（`parser.cc:122-146`）

`switch (m_curTok.kind)` 的 9 个 case 就是语法层允许出现在表达式位置的全部起始形式：标识符、数字、字符串、`(`、`{`、`if`、`for`、`var`，外加 `tok_error`（词法错误的说明文字直接作为解析错误展示，`parser.cc:126-128`），其余一律 `unknown FCToken when expecting an expression`。

### `parseBinOpRHS()`（`parser.cc:148-172`）

优先级爬升的全部逻辑只有 20 行，核心是两个判断：

```cpp
int TokPrec = tokenPrecedence(op);
if (TokPrec < ExprPrec) return LHS;        // 当前运算符不再比"我正在处理的层次"更紧，交还上层
...
int NextPrec = tokenPrecedence(nextOp);
if (TokPrec < NextPrec) {                  // 右边还有更紧的运算符，先把它结合成子树
    RHS = parseBinOpRHS(TokPrec + 1, std::move(RHS));
}
LHS = std::make_unique<FCBinaryExprAST>(op, std::move(LHS), std::move(RHS));
```

`TokPrec + 1` 而不是 `TokPrec`，决定**左结合**：同级运算符在下一层递归里被拒绝，于是由外层循环继续向左合并。把它改成 `TokPrec`，`1 - 2 - 3` 会变成 `1 - (2 - 3)`（实验 2 会验证）。

`tokenPrecedence()` 返回 `-1` 表示"当前 token 不是二元运算符"（`parser.cc:75-76`）。这个 `-1` 是必需的：如果返回 `0`，而调用方以 `0` 作为起始层次，`TokPrec < ExprPrec` 就变成 `0 < 0` 为假，`;` 与 EOF 会被当作运算符消费掉——第 5 步第 2 条会讲这个缺陷。

### `parseSeqExpr()`（`parser.cc:472-501`）与 `startsExpression()`（`parser.cc:46-60`）

`;` 是分隔符，不是语句终止符。`parseSeqExpr()` 先解析一个表达式，然后**只在**下一个 token 是 `;` 时继续；`;` 之后用 `startsExpression()` 判断是否还有下一条表达式。`startsExpression()` 的 case 表与 `parsePrimary()` 完全相同，这不是重复，而是**同一个集合的两种用途**：前者回答"序列该不该继续"，后者回答"这个起始符该走哪个分支"。一旦两者不同构，就会出现"序列认为还有表达式、`parsePrimary` 却认不出"的错位。

结构性 token——`}`、`)`、`else`、`in`、`def`、EOF、`;`——都不在起始符集合里，因此出现在序列位置时序列自然终止，把控制权交还外层结构消费。`parseBlockExpr()` 的终止符是 `}` 且由块自己消费（`parser.cc:434`、`467`）；块内空语句报 `empty statement in block`（`parser.cc:446-447`）。块内的规则与顶层一致：`;` 可以连接非块终结的表达式，但块语句以 `}` 结束后下一条语句直接开始（`parser.cc:463-464`），再写 `;` 报 `block terminator` 错误（`parser.cc:454-457`）。

---

## 第 4 步：这一段需要的概念

1. **词法单元（token）与语法类别**：词法层只回答"这是一个数字/标识符/运算符"，不回答"它是加法还是减法"——后者由解析层用 `binaryOpOf(kind)` 推得。
2. **递归下降（recursive descent）**：每个语法构造对应一个函数，函数之间互相调用，调用栈就是语法结构的嵌套。`if`、`for`、块、函数定义各有一个 `parse*` 函数。
3. **优先级爬升（precedence climbing）**：处理二元运算符的经典方法，用一个 `ExprPrec` 参数表示"当前允许结合的最低优先级"，遇到更松的运算符就返回，遇到更紧的就递归。它同时解决了优先级与左结合。
4. **终结符与非终结符**：`startsExpression()` 的集合是"能开始一个表达式的终结符集合"；`parsePrimary()` 是"遇到某个终结符时展开成哪个非终结符产生式"。
5. **抽象语法树（AST）与具体语法树（CST）**：本项目只保留 AST——括号不建节点，`;` 不建节点，`1 + 2 * 3` 的树里没有任何标点。括号的作用只体现在树的形状上（`(1 + 2) * 3` 会让 `+` 变深）。
6. **左结合与右结合**：同级运算符向哪边聚拢，由 `parseBinOpRHS()` 递归时的 `TokPrec + 1` 决定，不由优先级表决定。

---

## 第 5 步：这一层曾经踩过的三个坑（都已修掉）

1. **词法单元的类型域不统一**：早期 `kind` 对运算符返回 ASCII 码（`'+'` 即 43），而 `=` 等标点返回枚举值，解析层却在用 `static_cast<char>(kind)` 比较——两边永远不相等，所有含运算符的程序都在解析期失败。现在运算符也有独立枚举（`tok_less=20` … `tok_slash=24`），解析层只比较枚举。
2. **非运算符的优先级返回 0**：配合调用方以 `0` 为起始层次，`0 < 0` 为假，`;` 与 EOF 被当成二元运算符消费，随后 `parsePrimary()` 撞上 `}`、`def`、EOF，报出与实际原因无关的 `unknown FCToken`。现在非运算符返回 `-1`（`parser.cc:75-76`）。
3. **失败后不终止**：早期 `parseProgram()` 在 `parseSeqExpr()` 失败且已有错误标记时既不前进也不退出，同一个 token 反复进入序列解析，实测刷出 437 行重复 `LogError` 并触发 60 秒超时。现在失败路径只有一条：立即 `return nullptr`。
4. **Parser 在构造期读 token**：早期构造函数直接 `advance()`，而构造时 Lexer 还是空串，于是每次分析都先产生一个与输入无关的 `tok_eof`。现在 `begin()` 才读第一个 token（`parser.cc:21-24`），并加断言禁止未开始就读取位置。

---

## 第 6 步：动手改一行

每个实验都只改一处、改完复原，观察输出变化。

**实验 1：把乘除的优先级降到与加减同级**

在 `parser.cc:65-67` 把 `Mul`/`Div` 的返回值从 `20` 改成 `10`。重新构建并运行第 1 步的程序，`Binary +` 会变成 `Binary *` 在树根——`1 + 2 * 3` 被解析成 `(1 + 2) * 3`。这说明树的形状完全由优先级表决定。

**实验 2：破坏左结合**

结合方向由两处配合决定：`parser.cc:164` 的 `TokPrec < NextPrec` 与 `parser.cc:165` 递归时的 `TokPrec + 1`。同时把 `<` 改成 `<=`、把 `TokPrec + 1` 改成 `TokPrec`，再解析 `1 - 2 - 3`：

```text
Binary -
  Number 1
  Binary -
    Number 2
    Number 3
```

树从 `(1 - 2) - 3` 变成 `1 - (2 - 3)`，求值结果从 `-4` 变成 `2`。只改 `+ 1` 这一处时树**不会**变化——`+ 1` 只在"递归已经为了更紧的运算符进入、又遇到同级运算符"时起作用，单独改它观察不到差异（实测确认）。

**实验 3：让序列不终止**

在 `parser.cc:486` 把 `startsExpression(m_curTok.kind)` 改成恒真，再解析 `1;`。序列在 `;` 之后看到 EOF 仍认为"还有表达式"，于是 `parsePrimary()` 收到 EOF：

```text
LogError: unknown FCToken when expecting an expression
```

这说明"序列该不该继续"必须由起始符集合回答，不能靠猜测。

**实验 4：让 `;` 重新跟在 `}` 之后**

把 `parseSeqExpr()` 里 `lastTokenWasBlockClose()` 的守卫（`parser.cc:483-484`）和 `parseProgram()` 的 def 分支守卫（`parser.cc:103-106`）都去掉，再把 `Calculater/main.cc` 的输入换成 `def f() { 1 }; f()`：

```text
ast=ok parseError=0
```

`;` 一旦无人拦截，就被当成普通分隔符吞掉，规则随之消失，`def f() { 1 }; f()` 重新变合法。这正是守卫存在的原因：`}` 已经结束语句，再写 `;` 是多余的标点。

---

## 第 7 步：自测题

**Q1**：`tokenPrecedence()` 返回 `-1` 而不是 `0`，直接后果是什么？

<details><summary>答案</summary>

`parseBinOpRHS()` 的起始层次是 `0`，判断是 `TokPrec < ExprPrec`。返回 `0` 时 `0 < 0` 为假，非运算符的 token（`;`、EOF、`}`）会被当作二元运算符消费，解析器随后在 `parsePrimary()` 里失败，错误信息与真实原因无关。返回 `-1` 才能保证任何非运算符都立即让递归返回。

</details>

**Q2**：为什么 `a; b` 是一个 `Seq`，而两条 `def` 生成 `Program`？

<details><summary>答案</summary>

`parseSeqExpr()` 会持续吃 `;` 连接的表达式，所以 `a; b` 只给 `parseProgram()` 返回 1 条语句，`parseProgram()` 对单条语句直接返回该节点、不包 `Program`。两条 `def` 之间没有 `;` 可续接，各自成为一条语句，语句数 ≥ 2 时才包成 `FCProgramAST`。

</details>

**Q3**：把 `parseBinOpRHS()` 递归里的 `TokPrec + 1` 改成 `TokPrec`（只改这一处），`1 - 2 - 3` 的树会变吗？为什么？

<details><summary>答案</summary>

不会变。递归只有在遇到**更紧**的运算符时才进入（`TokPrec < NextPrec`），同级运算符不会触发递归；进入递归后，循环仍会继续向左合并同级运算符。因此 `+ 1` 只影响"递归内部再遇到同级运算符"的情形，`1 - 2 - 3` 观察不到差异。要让结合方向翻转，必须同时把 `parser.cc:164` 的 `<` 改成 `<=`（实验 2）。

</details>

**Q4**：`startsExpression()` 与 `parsePrimary()` 的 case 表为什么要保持同构？

<details><summary>答案</summary>

`startsExpression()` 决定"序列是否继续"，`parsePrimary()` 决定"这个起始符展开成哪个分支"。若前者包含后者不认识的形式，序列会把控制权留在自己手里，然后 `parsePrimary()` 报 `unknown FCToken`；若前者遗漏后者认识的形式，合法表达式会被提前截断。

</details>

---

## 下一段的接口

解析层交给语义层的是**纯语法树**：节点里没有符号表指针、没有作用域、没有 `frameIndex`，变量节点只记录名字（`Calculater/token.h` 中 `FCVariableExprAST` 的 `name` 字段）。下一段看语义分析如何给这些名字找到声明、分配槽位，并决定每个变量在运行期属于哪个栈帧。
