# 第 1 段 · 词法分析：`1 + 2 * 3` 如何变成词法单元序列

> 这是样章。每一段都按同样的五步走：**先跑 → 读懂输出 → 按调用顺序读代码 → 补这一段需要的概念 → 动手改一行**。
>
> 本段验收标准（读完应能不查资料回答）：
> 1. `1 + 2 * 3` 产生的每个词法单元里，哪些字段分别被后续哪个阶段消费；
> 2. 为什么 `+` 的词法单元里既有 `kind` 又有 `binop`，以及优先级表读的是哪一个；
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
[lexer] kind=1  lexeme=''  binop=6 @1:1
[lexer] kind=4  lexeme='1' binop=6 @1:1
[lexer] kind=21 lexeme='+' binop=2 @1:3
[lexer] kind=4  lexeme='2' binop=6 @1:5
[lexer] kind=23 lexeme='*' binop=4 @1:7
[lexer] kind=4  lexeme='3' binop=6 @1:9
[lexer] kind=1  lexeme=''  binop=6 @1:10
```

看完复原（同时关掉开关）：

```powershell
git checkout -- Calculater/main.cc
Remove-Item Env:\FC_TRACE
```

---

## 第 2 步：读懂这 7 行

先建立编号对照。`kind` 是 `FCToken` 的枚举值（`Calculater/token.h:20-46`），`binop` 是 `FCBinaryOp` 的枚举值（`token.h:48-56`）：

| kind | 枚举名 | | binop | 枚举名 |
|---|---|---|---|---|
| 1 | `tok_eof` | | 2 | `FCBinaryOp::Add` |
| 4 | `tok_number` | | 4 | `FCBinaryOp::Mul` |
| 17 | `tok_assign` | | 6 | `FCBinaryOp::Unknown` |
| 19 | `tok_semicolon` | | | |
| 20 | `tok_less` | | | |
| 21 | `tok_plus` | | | |
| 22 | `tok_minus` | | | |
| 23 | `tok_star` | | | |
| 24 | `tok_slash` | | | |
| 25 | `tok_error` | | | |

于是那 7 行可以翻译成：

| 行 | kind | binop | lexeme | 位置 | 说明 |
|---|---|---|---|---|---|
| 1 | `tok_eof` | Unknown | 空 | 1:1 | **与输入无关**：`FCScanner` 构造时先构造 `Parser`，`Parser` 构造函数就调用了一次 `advance()`，此时 Lexer 里还是空串（`scanner.cc:6-8`、`parser.cc:10-13`） |
| 2 | `tok_number` | Unknown | `1` | 1:1 | 数字字面量 |
| 3 | `tok_plus` | Add | `+` | 1:3 | 运算符：`kind` 与 `binop` 同时给出 |
| 4 | `tok_number` | Unknown | `2` | 1:5 | |
| 5 | `tok_star` | Mul | `*` | 1:7 | |
| 6 | `tok_number` | Unknown | `3` | 1:9 | |
| 7 | `tok_eof` | Unknown | 空 | 1:10 | 这次是真的输入结束，位置在最后一个字符之后 |

三个必须在第一次阅读时就注意到的现象：

1. **第一个 `tok_eof` 是构造期的副产品**。`Lexer` 的构造函数（`lexer.cc:11-20`）把当前词法单元初始化成 `tok_eof`；`Parser` 构造函数又会立刻取一个单元。所以任何一次 `analysis()` 都会先看到它，与输入内容无关。
2. **位置是词法单元第一个字符的位置，列号从 1 开始**。`1` 报 `@1:1`，`+` 报 `@1:3`，EOF 报 `@1:10`（最后一个字符之后）。这依赖 `readChar()` 在**推进行列之前**记录每个字符自身的位置（`lexer.cc:44-46`）。
3. **运算符有两条独立的信息**：`kind` 回答"这是什么类别的单元"，`binop` 回答"它是哪个二元运算符"。两者都在词法阶段确定，但消费方不同——见第 3 步末尾的表格。

---

## 第 3 步：按调用顺序读代码（只有一条路径，不要跳读）

```text
FCScanner::analysis            scanner.cc:11
  └─ m_lexer.reset(inputStr)   scanner.cc:15   → lexer.cc:22
  └─ m_parser.reset()          scanner.cc:16   → parser.cc:15
  └─ m_parser.parseProgram()   scanner.cc:19   → parser.cc:76
  └─ SemanticAnalyzer.analyze  scanner.cc:25
```

本段只关心词法部分，也就是 `nextToken()` 被调用之后发生的事：

```text
Parser::advance        parser.cc:20   → m_curTok = m_lexer.nextToken()
Lexer::nextToken       lexer.cc:173   → 有预读就用预读，否则 getTok()
Lexer::getTok          lexer.cc:56    → 真正的识别逻辑
  └─ Lexer::readChar   lexer.cc:34    → 取一个字符，维护行号列号
```

逐个函数，只需要看懂这四件事：

### `readChar()`（`lexer.cc:34-54`）

- 从 `m_idx` 取一个字符，写入 `m_lastChar`；
- **先把该字符自身的位置写进 `m_lastCharLine` / `m_lastCharColumn`，再推进行列**（`lexer.cc:44-52`）：遇到换行把行号加一、列号归 1，否则列号加一；
- 到结尾返回 `EOF`，位置取"下一个字符本该出现的位置"（`lexer.cc:35-41`）。

关键点：`m_lastChar` 不是"当前字符"，而是"**已经读进缓冲、尚未被消费**的那个字符"。整个 `getTok()` 都是围绕这个缓冲字符写的——先看它属于哪一类，再用 `readChar()` 把下一个字符读进来。

### `getTok()`（`lexer.cc:56-171`）

分支顺序是固定的，**顺序本身就是规则**：

| 顺序 | 分支 | 位置 | 规则 |
|---|---|---|---|
| 1 | 跳过空白 | 57-60 | `isspace` 的所有字符（空格、制表、换行）直接丢弃 |
| 2 | 标识符 / 关键字 | 65-90 | 首字符 `isalpha`，后续 `isalnum`；先按标识符扫完，再与 7 个关键字逐个比较，命中就返回对应枚举值 |
| 3 | 数字 | 91-104 | 首字符 `isdigit` 或 `.`；后续接受数字与 `.` |
| 4 | 字符串 | 105-120 | 以 `"` 开始，读到下一个 `"` 为止；读不到返回 `tok_error` |
| 5 | 注释 | 121-129 | `#` 到行尾，然后**递归调用 `getTok()`** 继续 |
| 6 | 输入结束 | 130-134 | 返回 `tok_eof` |
| 7 | 标点与运算符 | 135-164 | 括号、花括号、逗号、等号、冒号、分号、`< + - * /` 各返回对应枚举值，同时填 `binop` |
| 8 | `default` | 165-169 | 返回 `tok_error`，`lexeme` 携带 `unexpected character 'x'` |

为什么标识符要"先扫完再查表"？因为 `define`、`ifx`、`forall` 这类词必须整体识别成标识符，不能见到 `if` 就切一刀。这就是**最长匹配**（maximal munch）：在合法前提下尽可能多地吃字符。

### `nextToken()` / `peekToken()` / `currentToken()`（`lexer.cc:173-204`）

- `nextToken()`：正常取下一个，把结果同时存进 `m_curToken`；随后若 `traceEnabled("lexer")` 为真，打印一行诊断信息（`lexer.cc:180-184`）——所有诊断输出都集中在各阶段的入口/出口，业务分支里不夹打印语句；
- `peekToken()`：需要看下一个但**不推进**。实现方式是把下一个单元预读进 `m_peekToken` 并置 `m_hasPeek`，下次 `nextToken()` 直接取缓存。注意它是 `const` 方法却要改成员，所以成员声明成 `mutable`，并用 `const_cast` 绕过 `const`（`lexer.cc:188-196`）——能工作，但不是好写法；
- `currentToken()`：返回已经消费的那个单元；
- `isAtEnd()`：判断当前单元是否为 `tok_eof`。

### 谁在使用这些字段

| 字段 | 消费者 | 用途 |
|---|---|---|
| `kind` | `Parser::parsePrimary`（`parser.cc:112`）、`parseProgram`（`parser.cc:76`） | 决定走哪条解析分支 |
| `lexeme` | `Parser::parseNumberExpr` / `parseIdentifierExpr` | 数值转换、变量名与函数名；`tok_error` 时直接作为错误信息 |
| `binop` | `Parser::tokenPrecedence`（`parser.cc:48`）、`FCBinaryExprAST` | 优先级判定与 AST 运算符 |
| `line` / `column` | 仅 `unit_test.cc` 的断言 | **没有参与任何错误报告**——报错信息里目前不带位置 |

最后一行是这段代码的真实状态，也是你后面读 `parser.cc` 时会反复遇到的现象：错误信息只有一句 `LogError: ...`，没有行列号。

---

## 第 4 步：这一段需要的概念

- **词法单元（token）与词素（lexeme）**：词素是源文本里的原始片段（`"1"`、`"+"`），词法单元是它的分类结果（`tok_number` + 词素 + 位置）。同一个词素在不同上下文可能属于不同类别，分类由词法规则决定。
- **最长匹配**：识别时总是取最长的合法前缀。`ifx` 是一条标识符而不是 `if` 加 `x`。
- **关键字不是词法单元类型，而是标识符的保留值**：实现上先扫成标识符，再查一张表（`lexer.cc:74-87`）把它改判为关键字。所以"关键字"在词法层面只是"某些特定的标识符"。
- **预读（lookahead）**：解析器经常需要"看下一个单元但不推进"。两种实现：词法分析器内部缓存（本项目的做法），或者解析器自己维护一个单元缓冲。
- **词法错误必须有自己的类别**：一个阶段发现错误后，要么产出表示"错误"的单元，要么向上抛异常；不能伪装成正常的输入结束。否则上层只能报出"表达式位置遇到输入结束"这类与真实原因无关的信息。
- **词法阶段不该知道的东西**：优先级、类型、作用域。本项目里 `binop` 只是把字符映射成枚举，优先级表在 `parser.cc:48`——这条边界保持得比较干净。

---

## 第 5 步：这一层曾经有四处缺陷，现在逐一修掉了

阅读别人的代码，先找"这里的写法是否有第二种解释"。下面四处在本仓库都曾真实存在，修复后行为发生了变化，你要能从代码上指出"为什么原来的写法错"。

**1. 列号偏移一（旧的 `getTok()` 起点记录）**

旧代码在跳过空白**之后**仍用 `m_curLine` / `m_curColumn` 记录起点，而 `readChar()` 每读一个字符就加一，于是 `1 + 2 * 3` 里的 `1` 被报成 `@1:2`。修复方式是把位置记录前移到 `readChar()` 内部、推进之前（`lexer.cc:44-46`）。这类缺陷不会让程序算错，只会让诊断信息误导人——而诊断信息是排障时唯一的线索。

**2. 死分支（旧的数字分支）**

```cpp
if (curDouble) {
    return { tok_number, numStr, startLine, startColumn };
} else {
    return { tok_number, numStr, startLine, startColumn };
}
```

两个分支返回完全相同的词法单元。`curDouble` 被计算、被判断，却从未影响结果——浮点数与整数的区分根本没有体现在词法单元上，而是在后面由 `lexeme` 是否存在 `.` 重新判断。修复：删掉 `curDouble` 与整个 `if/else`，直接返回；整数与浮点的区别由 `lexeme` 表达，具体类型在语义阶段依据声明类型确定（`lexer.cc:91-104`）。

**3. 未闭合字符串伪装成正常结束**

旧代码在缺少右引号时返回 `tok_eof`，解析器看到的不是"字符串未闭合"，而是"表达式位置突然遇到输入结束"，最终错误信息是 `LogError: unknown FCToken when expecting an expression`。修复：增加 `tok_error` 类别，词法错误原样带上说明传上去（`lexer.cc:113-117`），由 `parser.cc:114-118` 直接展示：

```text
LogError: unterminated string literal
```

**4. 运算符的两种编码并存**

旧代码里 `+` 的 `kind` 是 ASCII 码 43，而 `=` 的 `kind` 是枚举值 `tok_assign`。同一个概念在同一个结构体里有两种表示法，任何按 `kind` 做 `switch` 的代码都必须记得这份例外。修复：`token.h:38-46` 增加 `tok_less`、`tok_plus`、`tok_minus`、`tok_star`、`tok_slash`、`tok_error`，`lexer.cc:135-169` 全部返回枚举值；未识别字符则返回 `tok_error`。

---

## 第 6 步：动手改一行

每个实验都遵守同一流程：改 → 构建 → 运行 → 记录预期与实际 → `git checkout --` 复原。做实验时先打开 `$env:FC_TRACE = "lexer"`，能直接看到改动对词法单元的影响。

**实验 1：让 `+` 失去运算符身份**

把 `Calculater/token.h:63` 的 `case '+': return FCBinaryOp::Add;` 改成 `case '+': return FCBinaryOp::Unknown;`。

预期：`1 + 2 * 3` 分析失败，`stderr` 出现 `LogError: unknown FCToken when expecting an expression`。

推导链（读代码验证）：`binop=Unknown` → `tokenPrecedence` 返回 `-1`（`parser.cc:61`）→ `parseBinOpRHS` 认为 `+` 不是运算符、不消费它 → `parseSeqExpr` 返回 `1` → `parseProgram` 下一轮仍在 `+` 上 → `parseSeqExpr` → `parsePrimary` 的 `default` 分支报错（`parser.cc:112-115`）。

这个实验说明一件事：**语法分析能否把 `+` 当运算符，取决于词法阶段给的 `binop`，而不是 `kind`**。

**实验 2：数字不再接受前导小数点**

把 `lexer.cc:95` 的 `|| m_lastChar == '.'` 删掉，输入改成 `.5`。

预期：`.` 变成 `tok_error`，错误信息是 `LogError: unexpected character '.'`（不再是"两个单元"——因为 `.` 已经由 `default` 分支统一处理为词法错误）。

**实验 3：把词法错误降级回"未知单元"**

删除 `parser.cc:114-117` 中 `case tok_error:` 那三行。输入改成 `"abc`，观察错误信息退化为 `LogError: unknown FCToken when expecting an expression`。

预期：错误信息不再说明真实原因。恢复后确认它变回 `LogError: unterminated string literal`。这个实验说明错误类别不是装饰，它决定了排障时能不能一次定位。

**实验 4：把列号改回偏移一**

把 `lexer.cc:44-46` 的三行（`m_lastCharLine` / `m_lastCharColumn` 的赋值）移到 `lexer.cc:47-52` 的行列推进之后，重新构建并运行 `FunctionsUnitTest`。

预期：`runLexerTests` 里以 `column` 为条件的断言失败，`1 + 2 * 3` 的列号整体加一。改完复原，并确认 751 个断言重新全绿。

---

## 第 7 步：自测题

1. `1 + 2 * 3` 的 7 个单元里，为什么第一个是 `tok_eof`？如果我们把 `FCScanner` 改成延迟构造 `Parser`，这个单元还会出现吗？
2. `def` 为什么必须作为关键字处理，而不能在语法分析阶段当作普通标识符？如果不这样做，`parsePrimary` 会遇到什么歧义？
3. `peekToken()` 被声明为 `const`，却修改了 `m_peekToken` 与 `m_hasPeek`。除了 `mutable`，还有什么等价写法？各自的代价是什么？
4. 诊断输出为什么放在 `nextToken()` 而不是 `getTok()`？
5. 如果把 `line`/`column` 接进错误报告，最小改动点在哪几个函数？

答案：

1. 不会。它来自 `Parser` 构造函数里的第一次 `advance()`（`parser.cc:10-13`），而 `FCScanner` 的成员初始化列表（`scanner.cc:6-8`）在 `Lexer` 还是空串时就构造了 `Parser`。
2. 因为 `def` 后面跟的是"函数名 + 参数列表 + 函数体"，与"标识符后跟调用参数"在词法层面完全同形。若不在词法层区分，`parsePrimary` 见到 `def` 只能是标识符或函数调用，无法判断接下来该解析原型还是实参。
3. 方案一：去掉 `const`，让调用方持有非 `const` 引用（最简单，但丢失"不修改已消费状态"的语义）。方案二：把预读状态抽成独立的 `Tokenizer` 对象，由调用方持有可变副本（干净，但要改所有调用点）。
4. `nextToken()` 是"已消费单元"的唯一出口，所有调用方都经过它，因此一处打印即可覆盖全部消费路径；`getTok()` 还会被注释递归调用，且返回后可能被 `peekToken()` 缓存而不立即消费，放在那里会重复或提前打印。
5. `Parser::logError`（`parser.cc:65`）需要当前单元的位置；`Parser::parse...` 各处的错误信息需要带上 `m_curTok.line/column`；`FCScanner::analysis` 不会变。改造成本主要不在代码，而在于所有断言错误信息的测试都要跟着变。

---

## 下一段的接口

第 2 段（`var a:int = 1 + 2 * 3; a`）会用到本段的三个结论：

- 表达式起始符的判定在 `parser.cc:31` 的 `startsExpression`，它只认 8 种 `kind`；
- 优先级表吃的是 `binop`（`parser.cc:48`），不是 `kind`；
- 位置信息目前没有被消费，所以下一段看到的错误依然不带行列号。
