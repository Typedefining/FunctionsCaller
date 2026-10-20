# 第 1 段 · 词法分析：`1 + 2 * 3` 如何变成词法单元序列

> 这是样章。每一段都按同样的五步走：**先跑 → 读懂输出 → 按调用顺序读代码 → 补这一段需要的概念 → 动手改一行**。
>
> 本段验收标准（读完应能不查资料回答）：
> 1. `1 + 2 * 3` 产生的每个词法单元里，哪些字段分别被后续哪个阶段消费；
> 2. 为什么 `+` 的 `kind` 已经是运算符枚举，而"它属于哪个二元运算符"要等解析层从 `kind` 推得；
> 3. 这一层曾经有哪四处实现缺陷，各自的症状是什么、修掉之后行为如何变化。

---

## 第 1 步：先跑，制造可观测的中间表示

词法分析的产物是一个序列，不打印就看不见。项目里已经内置了诊断输出开关，**不需要改代码**：

```powershell
$env:FC_TRACE = "lexer"      # 打开词法阶段诊断输出；"all" 打开全部阶段
```

开关的实现见 `Calculater/trace.h` 与 `Calculater/trace.cc`：读取环境变量 `FC_TRACE`，取值为阶段名（`lexer`、`parser`、`semantic`、`evaluator`、`codegen`），多个阶段用逗号分隔。它只影响诊断输出，不改变任何语义。

**改动一**（只为换成本段的例子）：`Calculater/main.cc` 第 6 行。

```cpp
scanner.analysis("1 + 2 * 3");
```

构建并运行：

```powershell
cmake --build build-strict --config Debug --target FunctionsCalculater
.\build-strict\Calculater\Debug\FunctionsCalculater.exe
```

实测输出（原样抄录，不是示意）：

```text
[lexer] kind=4 lexeme='1' @1:1
[lexer] kind=21 lexeme='+' @1:3
[lexer] kind=4 lexeme='2' @1:5
[lexer] kind=23 lexeme='*' @1:7
[lexer] kind=4 lexeme='3' @1:9
[lexer] kind=1 lexeme='' @1:10
```

看完复原（同时关掉开关）：

```powershell
git checkout -- Calculater/main.cc
Remove-Item Env:\FC_TRACE
```

---

## 第 2 步：读懂这 6 行

先建立编号对照。`kind` 是 `FCToken` 的枚举值（`Calculater/token.h:20-52`）：

| kind | 枚举名 | 含义 |
|---|---|---|
| 1 | `tok_eof` | 输入结束 |
| 4 | `tok_number` | 数字字面量 |
| 17 | `tok_assign` | `=` |
| 19 | `tok_semicolon` | `;` |
| 20 | `tok_less` | `<` |
| 21 | `tok_plus` | `+` |
| 22 | `tok_minus` | `-` |
| 23 | `tok_star` | `*` |
| 24 | `tok_slash` | `/` |
| 25 | `tok_error` | 词法错误（`lexeme` 携带说明） |

于是这 6 行可以翻译成：

| 行 | kind | lexeme | 位置 | 说明 |
|---|---|---|---|---|
| 1 | `tok_number` | `1` | 1:1 | 数字字面量 |
| 2 | `tok_plus` | `+` | 1:3 | 运算符：类别由 `kind` 表达 |
| 3 | `tok_number` | `2` | 1:5 | |
| 4 | `tok_star` | `*` | 1:7 | |
| 5 | `tok_number` | `3` | 1:9 | |
| 6 | `tok_eof` | 空 | 1:10 | 输入结束，位置在最后一个字符之后 |

三个必须在第一次阅读时就注意到的现象：

1. **token 流与输入一一对应，没有多余单元**。`6` 个单元正是 `1`、`+`、`2`、`*`、`3` 和结尾标记。这一点曾经不成立：`Parser` 的构造函数里调用过一次 `advance()`，而 `FCScanner` 构造 `Parser` 时 Lexer 还持有空串，于是每次 `analysis()` 的第一个单元都是一个与输入无关的 `tok_eof`（`@1:1`）。修复方式是让"读取第一个 token"只发生在解析阶段开始时：`Parser::begin()`（`parser.cc:21-23`）在 `parseProgram()` 入口被调用（`parser.cc:87-88`），构造函数只建立与 Lexer 的引用关系。
2. **位置是词法单元第一个字符的位置，列号从 1 开始**。`1` 报 `@1:1`，`+` 报 `@1:3`，EOF 报 `@1:10`（最后一个字符之后）。这依赖 `readChar()` 在**推进行列之前**记录每个字符自身的位置（`lexer.cc:35-36`）。
3. **`Token` 只记录词法类别**：`+` 的类别是 `tok_plus`，"它是加法还是减法"不属于词法层。运算符含义由解析层用 `binaryOpOf(kind)` 推得（`token.h:66-78`），AST 才因此拿到 `FCBinaryOp`。早期 `Token` 里还有一个 `binop` 字段，与 `kind` 一一对应，属于同一事实的两份副本（违反单一数据源），两者可能不一致，已删除。

---

## 第 3 步：按调用顺序读代码（只有一条路径，不要跳读）

```text
FCScanner::analysis            scanner.cc:11
  └─ m_lexer.reset(inputStr)   scanner.cc:15   → lexer.cc:16
  └─ m_parser.reset()          scanner.cc:16   → parser.cc:15
  └─ m_parser.parseProgram()   scanner.cc:19   → parser.cc:84
  └─ SemanticAnalyzer.analyze  scanner.cc:25
```

本段只关心词法部分，也就是 `nextToken()` 被调用之后发生的事：

```text
Parser::begin          parser.cc:21-24 → 进入解析阶段并 advance()
Parser::advance        parser.cc:26-29 → m_curTok = m_lexer.nextToken()
Lexer::nextToken            lexer.cc:162   → 在主游标上识别一个 Token
Lexer::Cursor::getTok       lexer.cc:46    → 真正的识别逻辑（只读写游标与输入）
  └─ Lexer::Cursor::readChar lexer.cc:22   → 按下标取一个字符，维护行列号
```

逐个函数，只需要看懂这四件事：

### `Cursor::readChar()`（`lexer.cc:22-44`）

- 从 `m_input[pos]` 按**下标**取一个字符（`lexer.cc:31-33`），写入 `lastChar`，`pos` 后移；实现不持有迭代器，因此复制游标就等于复制读取位置；
- **先把该字符自身的位置写进 `lastCharLine` / `lastCharColumn`，再推进行列**（`lexer.cc:34-42`）：遇到换行把行号加一、列号归 1，否则列号加一；
- 到结尾返回 `EOF`，位置取"下一个字符本该出现的位置"（`lexer.cc:23-29`）。

关键点：`lastChar` 不是"当前字符"，而是"**已经读进缓冲、尚未被消费**的那个字符"。整个 `getTok()` 都是围绕这个缓冲字符写的——先看它属于哪一类，再用 `readChar()` 把下一个字符读进来。

### `Cursor::getTok()`（`lexer.cc:46-160`）

先注意它为什么是 `Cursor` 的成员而不是 `Lexer` 的：识别一个 Token 只需要"输入 + 读取位置 + 行列号 + 缓冲字符"这四样，而它们全在 `Cursor` 里。`Lexer` 持有的只是 `m_input`、一个主游标 `m_cursor` 和最近产出的 `m_curToken`。这个切分是 `peekToken()` 能保持纯函数语义的前提（见下一节）。

分支顺序是固定的，**顺序本身就是规则**：

| 顺序 | 分支 | 位置 | 规则 |
|---|---|---|---|
| 1 | 跳过空白 | 47-50 | `isspace` 的所有字符（空格、制表、换行）直接丢弃 |
| 2 | 标识符 / 关键字 | 55-80 | 首字符 `isalpha`，后续 `isalnum`；先按标识符扫完，再与 7 个关键字逐个比较，命中就返回对应枚举值 |
| 3 | 数字 | 81-93 | 首字符 `isdigit` 或 `.`；后续接受数字与 `.` |
| 4 | 字符串 | 94-109 | 以 `"` 开始，读到下一个 `"` 为止；读不到返回 `tok_error` |
| 5 | 注释 | 110-118 | `#` 到行尾，然后**递归调用 `getTok()`** 继续 |
| 6 | 输入结束 | 119-123 | 返回 `tok_eof` |
| 7 | 标点与运算符 | 124-153 | 括号、花括号、逗号、等号、冒号、分号、`< + - * /` 各返回对应枚举值；`Token` 到此为止，判运算符种类是解析层的事 |
| 8 | `default` | 154-159 | 返回 `tok_error`，`lexeme` 携带 `unexpected character 'x'` |

为什么标识符要"先扫完再查表"？因为 `define`、`ifx`、`forall` 这类词必须整体识别成标识符，不能见到 `if` 就切一刀。这就是**最长匹配**（maximal munch）：在合法前提下尽可能多地吃字符。

### `nextToken()` / `peekToken()` / `currentToken()`（`lexer.cc:162-185`）

- `nextToken()`：在主游标上识别下一个 Token，把结果同时存进 `m_curToken`（`lexer.cc:162-163`）；随后若 `traceEnabled("lexer")` 为真，打印一行诊断信息（`lexer.cc:164-169`）——所有诊断输出都集中在各阶段的入口/出口，业务分支里不夹打印语句；
- `peekToken()`：需要看下一个但**不推进**。实现是复制主游标得到 `lookahead`，在副本上调用 `Cursor::getTok()`（`lexer.cc:172-177`）。主游标的读取位置、行列号、缓冲字符都不变，所以 `peekToken()` 是 `const` 方法却不需要 `mutable`、`const_cast` 或预读缓存；代价是 peek 时把下一个 Token 完整识别一次（原本也要识别，只是结果被缓存起来）；
- `currentToken()`：返回已经消费的那个单元（`lexer.cc:179-181`）；
- `isAtEnd()`：判断当前单元是否为 `tok_eof`（`lexer.cc:183-185`）。

为什么不能"只判断下一个字符"来实现 peek？因为下一个 Token 可能隔着空白、注释甚至换行，长度也不确定；要得到它的 `kind` 与 `lexeme`，只能完整识别一次。把识别逻辑放进游标，才能既复用同一套规则，又不移动主游标。

### 谁在使用这些字段

| 字段 | 消费者 | 用途 |
|---|---|---|
| `kind` | `Parser::parsePrimary`（`parser.cc:122`）、`parseProgram`（`parser.cc:84`） | 决定走哪条解析分支 |
| `lexeme` | `Parser::parseNumberExpr` / `parseIdentifierExpr` | 数值转换、变量名与函数名；`tok_error` 时直接作为错误信息 |
| 运算符种类 | `Parser::parseBinOpRHS`（`parser.cc:151`）用 `binaryOpOf(kind)` 推得，再写入 `FCBinaryExprAST` | 优先级判定与 AST 运算符 |
| `line` / `column` | 仅 `unit_test.cc` 的断言 | **没有参与任何错误报告**——报错信息里目前不带位置 |

最后一行是这段代码的真实状态，也是你后面读 `parser.cc` 时会反复遇到的现象：错误信息只有一句 `LogError: ...`，没有行列号。

---

## 第 4 步：这一段需要的概念

- **词法单元（token）与词素（lexeme）**：词素是源文本里的原始片段（`"1"`、`"+"`），词法单元是它的分类结果（`tok_number` + 词素 + 位置）。同一个词素在不同上下文可能属于不同类别，分类由词法规则决定。
- **最长匹配**：识别时总是取最长的合法前缀。`ifx` 是一条标识符而不是 `if` 加 `x`。
- **关键字不是词法单元类型，而是标识符的保留值**：实现上先扫成标识符，再查一张表（`lexer.cc:64-77`）把它改判为关键字。所以"关键字"在词法层面只是"某些特定的标识符"。
- **预读（lookahead）**：解析器经常需要"看下一个单元但不推进"。常见实现有三种：词法分析器内部缓存结果；解析器自己维护一个单元缓冲；把读取位置做成可复制的值，peek 时在副本上识别（本项目 `lexer.cc:172-177` 的做法）。第三种不会让"已经读到哪里"变成隐藏状态，代价是重复识别一次。
- **词法错误必须有自己的类别**：一个阶段发现错误后，要么产出表示"错误"的单元，要么向上抛异常；不能伪装成正常的输入结束。否则上层只能报出"表达式位置遇到输入结束"这类与真实原因无关的信息。
- **词法阶段不该知道的东西**：优先级、类型、作用域。`Token` 只回答"这是哪类单元"，运算符种类与优先级都在解析层（`binaryOpOf` 与 `tokenPrecedence`）。这条边界是这次重构才划清的：`Token` 曾经自带 `binop`，等于把解析层的结论提前存了一份。

---

## 第 5 步：这一层曾经有四处缺陷，现在逐一修掉了

阅读别人的代码，先找"这里的写法是否有第二种解释"。下面四处在本仓库都曾真实存在，修复后行为发生了变化，你要能从代码上指出"为什么原来的写法错"。

**1. 列号偏移一（旧的 `getTok()` 起点记录）**

旧代码在跳过空白**之后**仍用 `m_curLine` / `m_curColumn` 记录起点，而 `readChar()` 每读一个字符就加一，于是 `1 + 2 * 3` 里的 `1` 被报成 `@1:2`。修复方式是把位置记录前移到 `readChar()` 内部、推进之前（`lexer.cc:35-36`）。这类缺陷不会让程序算错，只会让诊断信息误导人——而诊断信息是排障时唯一的线索。

**2. 死分支（旧的数字分支）**

```cpp
if (curDouble) {
    return { tok_number, numStr, startLine, startColumn };
} else {
    return { tok_number, numStr, startLine, startColumn };
}
```

两个分支返回完全相同的词法单元。`curDouble` 被计算、被判断，却从未影响结果——浮点数与整数的区分根本没有体现在词法单元上，而是在后面由 `lexeme` 是否存在 `.` 重新判断。修复：删掉 `curDouble` 与整个 `if/else`，直接返回；整数与浮点的区别由 `lexeme` 表达，具体类型在语义阶段依据声明类型确定（`lexer.cc:81-93`）。

**3. 未闭合字符串伪装成正常结束**

旧代码在缺少右引号时返回 `tok_eof`，解析器看到的不是"字符串未闭合"，而是"表达式位置突然遇到输入结束"，最终错误信息是 `LogError: unknown FCToken when expecting an expression`。修复：增加 `tok_error` 类别，词法错误原样带上说明传上去（`lexer.cc:104-105`），由 `parser.cc:126-130` 直接展示：

```text
LogError: unterminated string literal
```

**4. 运算符的两种编码并存**

旧代码里 `+` 的 `kind` 是 ASCII 码 43，而 `=` 的 `kind` 是枚举值 `tok_assign`。同一个概念在同一个结构体里有两种表示法，任何按 `kind` 做 `switch` 的代码都必须记得这份例外。修复：`token.h:46-52` 增加 `tok_less`、`tok_plus`、`tok_minus`、`tok_star`、`tok_slash`、`tok_error`，`lexer.cc:124-159` 全部返回枚举值；未识别字符则返回 `tok_error`。

---

## 第 6 步：动手改一行

每个实验都遵守同一流程：改 → 构建 → 运行 → 记录预期与实际 → `git checkout --` 复原。做实验时先打开 `$env:FC_TRACE = "lexer"`，能直接看到改动对词法单元的影响。

**实验 1：让 `+` 失去运算符身份**

把 `Calculater/token.h` 中 `binaryOpOf` 的 `case FCToken::tok_plus: return FCBinaryOp::Add;` 改成 `return FCBinaryOp::Unknown;`。

预期：`1 + 2 * 3` 分析失败，`stderr` 出现 `LogError: unknown FCToken when expecting an expression`。词法阶段不受影响——`[lexer] kind=21 lexeme='+' @1:3` 照常打印，说明 `Token` 仍正确表达了"这是个 `+`"。

推导链（读代码验证）：`binaryOpOf(tok_plus)=Unknown` → `tokenPrecedence` 返回 `-1`（`parser.cc:69`）→ `parseBinOpRHS` 认为 `+` 不是运算符、不消费它 → `parseSeqExpr` 返回 `1` → `parseProgram` 下一轮仍在 `+` 上 → `parseSeqExpr` → `parsePrimary` 的 `tok_error`/`default` 分支报错（`parser.cc:124-128`）。

这个实验说明一件事：**运算符含义只存在于解析层这一次映射里；词法层给出的 `kind` 是唯一的类别表示，不存在需要与它保持一致的第二份副本**。

**实验 2：数字不再接受前导小数点**

把 `lexer.cc:85` 的 `|| lastChar == '.'` 删掉，输入改成 `.5`。

预期：`.` 变成 `tok_error`，错误信息是 `LogError: unexpected character '.'`（不再是"两个单元"——因为 `.` 已经由 `default` 分支统一处理为词法错误）。

**实验 3：把词法错误降级回"未知单元"**

删除 `parser.cc:126-129` 中 `case tok_error:` 那几行。输入改成 `"abc`，观察错误信息退化为 `LogError: unknown FCToken when expecting an expression`。

预期：错误信息不再说明真实原因。恢复后确认它变回 `LogError: unterminated string literal`。这个实验说明错误类别不是装饰，它决定了排障时能不能一次定位。

**实验 4：把列号改回偏移一**

把 `lexer.cc:35-36` 的两行（`lastCharLine` / `lastCharColumn` 的赋值）移到 `lexer.cc:37-42` 的行列推进之后，重新构建并运行 `FunctionsUnitTest`。

预期：`runLexerTests` 里以 `column` 为条件的断言失败，`1 + 2 * 3` 的列号整体加一。改完复原，并确认 756 个断言重新全绿。

---

## 第 7 步：自测题

1. `1 + 2 * 3` 的 6 个单元里，第一个为什么是 `tok_number` 而不是别的？如果 `Parser` 构造函数仍保留 `advance()`，看到的现象会变成什么？
2. `def` 为什么必须作为关键字处理，而不能在语法分析阶段当作普通标识符？如果不这样做，`parsePrimary` 会遇到什么歧义？
3. `peekToken()` 是 `const` 方法且不推进读取位置。它现在复制游标得到副本、在副本上完整识别一次；如果改成"复制识别逻辑、只读下一个字符就返回"，会在哪些输入上给出错误结果？
4. 诊断输出为什么放在 `nextToken()` 而不是 `Cursor::getTok()`？
5. 如果把 `line`/`column` 接进错误报告，最小改动点在哪几个函数？

答案：

1. 因为词法单元的产出从"输入的第一个字符"开始：`Parser` 在 `parseProgram()` 入口才调用 `begin()` 读取第一个单元（`parser.cc:21-23`、`86-89`），此时 Lexer 已收到 `1 + 2 * 3`，所以第一个就是 `tok_number`。若构造函数保留 `advance()`，它会先读空串，多出一个与输入无关的 `tok_eof @1:1`，单元数变成 7；这个单元随后被 `FCScanner::analysis` 里的 `reset()` 丢弃，所以不影响结果，只污染诊断输出。
2. 因为 `def` 后面跟的是"函数名 + 参数列表 + 函数体"，与"标识符后跟调用参数"在词法层面完全同形。若不在词法层区分，`parsePrimary` 见到 `def` 只能是标识符或函数调用，无法判断接下来该解析原型还是实参。
3. 下一个单元可能隔着空白、注释甚至换行，长度也不确定：`"1"`、`"abc"`、`"  # c\n42"` 三种输入的下一个单元长度各不相同。只看一个字符无法判断它属于关键字、标识符、数字还是字符串（`if` 与 `ifx` 前两个字符相同），也无法给出正确的 `lexeme` 与结束位置。所以要么完整识别一次，要么把识别规则复制一份——后者会在两处规则上产生漂移，风险更大。
4. `nextToken()` 是"已消费单元"的唯一出口，所有调用方都经过它，因此一处打印即可覆盖全部消费路径；`Cursor::getTok()` 还会被注释递归调用，且 `peekToken()` 也会在游标副本上调用它，放在那里会重复打印、并且把"只看不消费"的预读也打印成消费。
5. `Parser::logError`（`parser.cc:73`）需要当前单元的位置；`Parser::parse...` 各处的错误信息需要带上 `m_curTok.line/column`；`FCScanner::analysis` 不会变。改造成本主要不在代码，而在于所有断言错误信息的测试都要跟着变。

---

## 下一段的接口

第 2 段（`var a:int = 1 + 2 * 3; a`）会用到本段的三个结论：

- 表达式起始符的判定在 `parser.cc:39` 的 `startsExpression`，它只认 8 种 `kind`；
- 优先级表吃的是 `binaryOpOf(kind)`（`parser.cc:151-152`），`Token` 里没有 `binop`；
- 位置信息目前没有被消费，所以下一段看到的错误依然不带行列号。



> 第 2 段已就绪：[02-parser.md](02-parser.md)（语法分析：Token 流如何变成语法树）。
