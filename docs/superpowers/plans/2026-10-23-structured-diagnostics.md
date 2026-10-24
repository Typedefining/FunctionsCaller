# 结构化诊断通道（FCDiagnostic）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use subagent-driven-development (recommended) or executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 lexer/parser/semantic/evaluator/codegen 五阶段的 `fprintf(stderr)` 错误输出替换为一个结构化诊断通道（`FCDiagnostic` 列表），使调用方能程序化读取错误的阶段、位置与消息，且 parser 错误携带源位置。

**Architecture:** 新增 header-only 的 `Calculater/diagnostic.h`（诊断类型 + 收集器 + 格式化函数）。各阶段把错误写入注入的 `FCDiagnostics` 收集器，控制流（`nullptr`/`false`/`Dangle` 返回值）保持不变——诊断是错误载荷，不替代错误信号。打印 stderr 的职责上移到 CLI 边界（main.cc），库代码不再直接打印。

**Tech Stack:** C++20（根 CMakeLists.txt:7 现状）、无新第三方依赖、MSVC/Windows。

**Spec:** 2026-10-22 设计审查结论第 1 条（错误处理无结构化通道）：所有阶段用 fprintf 报错；Token 已携带 line/column 但 logError 丢弃位置；调用方无法程序化消费诊断。

## Global Constraints

- 无新第三方依赖；`diagnostic.h` 为 header-only，加入 FunctionsCore 目标。
- 一切错误消息文本与现状保持一致（除 codegen.cc:725 的 "defiened" 拼写错误随本行修改为 "defined"）。
- 各阶段失败时的返回值行为不变：parser/semantic 返回 `nullptr`/`false`，evaluator 返回 `Dangle`，codegen 返回 `nullptr`。
- 命名沿用仓库 `FC` 前缀约定（`FCDiagnostic` / `FCDiagnostics`）。
- 术语规范：诊断（diagnostic）、错误恢复（error recovery）等使用标准术语。
- 不做 parser 错误恢复与多错误收集（首错即停的现状保留）——ponytail：诊断通道天然支持多条目，未来加同步点（synchronization point）即可启用。
- 语义期诊断不携带源位置（AST 节点无位置字段）——ponytail：位置为 `FCSourceLocation{}`（0:0，含义"未知"），未来 AST 加位置字段后回填。

## Review Focus

1. **scanner 复用残留**：同一 `FCScanner` 连续两次 `analysis()`，第二次成功后 `diagnostics()` 必须为空（若入口不清理，旧诊断泄漏）。→ Task 1 测试 `scannerDiagnosticsResetOnReuse`。
2. **EOF 位置错误**：错误发生在文件尾（如 `if 1 then 2` 后缺 `else`）时，诊断位置须是 EOF token 的位置而非垃圾值。→ Task 1 测试 `parserDiagnosticAtEofHasPosition`。
3. **词法错误穿透**：未闭合字符串产生 `tok_error`，parser 转发时必须保留 lexer 记录的 token 起始位置。→ Task 1 测试 `lexerErrorSurfacesWithPosition`。
4. **行为不变式**：诊断加入后 `Dangle`/`nullptr` 返回路径逐一保持（错误值与错误报告并存，不互相吞并）。→ Task 3/4 各测试同时断言返回值与诊断。
5. **静默失败空窗**：`declareVariable`/`registerFunction` 改为静默返回失败后，所有调用方必须补上报告，否则错误消失。→ Task 2 编译器逐一暴露调用方，测试覆盖每个调用方路径。

---

### Task 1: 诊断类型 + Parser/Scanner 接入

**Files:**
- Create: `Calculater/diagnostic.h`
- Modify: `Calculater/parser.h:15`、`Calculater/parser.cc:80-89`（logError/logErrorP）
- Modify: `Calculater/scanner.h`、`Calculater/scanner.cc:12-14,17-18,50-55`
- Modify: `Calculater/CMakeLists.txt:29-40`（FunctionsCore 源列表加 `diagnostic.h`）
- Test: `Calculater/unit_test.cc`（新增 Scanner/Parser 诊断用例）

**Interfaces:**
- Produces（后续任务全部依赖）:

```cpp
// diagnostic.h, namespace FC
struct FCSourceLocation { int line = 0; int column = 0; };   // 0 = 未知

struct FCDiagnostic {
    std::string stage;            // "parser" / "semantic" / "evaluator" / "codegen"
    FCSourceLocation location;
    std::string message;
};

class FCDiagnostics {
public:
    void add(std::string stage, FCSourceLocation location, std::string message);
    bool empty() const;
    void clear();
    const std::vector<FCDiagnostic>& items() const;
private:
    std::vector<FCDiagnostic> m_items;
};

// 确切输出格式（测试以此固定）：
// 有位置：  "error[parser] 1:5: <message>"
// 无位置：  "error[parser]: <message>"
std::string formatDiagnostic(const FCDiagnostic& diagnostic);
```

- Produces: `Parser::Parser(Lexer& lexer, FCDiagnostics& diagnostics)`（构造函数新增第二参数）。
- Produces: `FCScanner::diagnostics()` 返回 `const FCDiagnostics&`；`analysis()` 入口处 `m_diagnostics.clear()`。

- [x] **Step 1: 写失败测试（unit_test.cc，runScannerTests 区域附近新增）**

```cpp
void runDiagnosticTests() {
    // 1. 解析错误产生带位置的诊断
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def 123() { 1 }");
        expect(ast == nullptr, "diag: parse error returns null");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty(), "diag: parse error recorded");
        if (!diags.empty()) {
            expect(diags[0].stage == "parser", "diag: stage is parser");
            expect(diags[0].location.line == 1 && diags[0].location.column == 5,
                   "diag: location points at offending token");
            expect(diags[0].message.find("Expected function name") != std::string::npos,
                   "diag: message preserved");
        }
    }
    // 2. 成功时无诊断
    {
        FCScanner scanner;
        auto ast = scanner.analysis("1 + 2");
        expect(ast != nullptr && scanner.diagnostics().empty(),
               "diag: clean input yields no diagnostics");
    }
    // 3. 复用清理
    {
        FCScanner scanner;
        scanner.analysis("def 123() { 1 }");       // 失败
        auto ok = scanner.analysis("1 + 2");        // 成功
        expect(ok != nullptr && scanner.diagnostics().empty(),
               "diag: scannerDiagnosticsResetOnReuse");
    }
    // 4. EOF 位置
    {
        FCScanner scanner;   // "if 1 then 2" 缺 else，错误在 EOF 处
        auto ast = scanner.analysis("if 1 then 2");
        expect(ast == nullptr, "diag: missing else fails");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty() && diags[0].location.line >= 1,
               "diag: parserDiagnosticAtEofHasPosition");
    }
    // 5. 词法错误穿透（未闭合字符串）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("\"abc");
        expect(ast == nullptr, "diag: unterminated string fails");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty()
               && diags[0].message.find("unterminated string literal") != std::string::npos
               && diags[0].location.line == 1 && diags[0].location.column == 1,
               "diag: lexerErrorSurfacesWithPosition");
    }
    // 6. formatDiagnostic 精确格式
    {
        FCDiagnostic d{"parser", {1, 5}, "msg"};
        expect(FC::formatDiagnostic(d) == "error[parser] 1:5: msg",
               "diag: formatDiagnostic with location");
        FCDiagnostic d2{"semantic", {}, "msg"};
        expect(FC::formatDiagnostic(d2) == "error[semantic]: msg",
               "diag: formatDiagnostic without location");
    }
}
```

并在 `runAllTests()` 中调用。

- [x] **Step 2: 运行确认失败**

Run: `cmake --build build-debug --config Debug --target FunctionsUnitTest --parallel 16 && .\build-debug\Calculater\Debug\FunctionsUnitTest.exe`
Expected: 编译失败（`scanner.diagnostics()` 不存在），或运行后新用例 `[FAIL]`。

- [x] **Step 3: 实现**

- 新建 `Calculater/diagnostic.h`，内容即 Interfaces 块中的类型与 `formatDiagnostic`（用 `std::to_string` 拼接，无未知分支）。
- `parser.h`：成员加 `FCDiagnostics& m_diagnostics;`（`#include "diagnostic.h"`），构造签名改 `explicit Parser(Lexer& lexer, FCDiagnostics& diagnostics);`。
- `parser.cc` logError 改为（logErrorP 不变，仍委托）:

```cpp
std::unique_ptr<FCExprAST> Parser::logError(const char* msg) {
    m_hadError = true;
    m_diagnostics.add("parser", {m_curTok.line, m_curTok.column}, msg);
    return nullptr;
}
```

- `scanner.h`：成员 `FCDiagnostics m_diagnostics;` 声明在 `m_parser` **之前**（初始化顺序）；`#include "diagnostic.h"`；加 `const FCDiagnostics& diagnostics() const`。
- `scanner.cc`：构造初始化 `m_parser(m_lexer, m_diagnostics)`；`analysis()` 开头 `m_diagnostics.clear();`；`resetState()` 同样 clear。
- `CMakeLists.txt` FunctionsCore 源列表加 `diagnostic.h`。
- 修复所有 `Parser` 构造调用点（编译器报错逐一引导）：unit_test.cc 中直接构造 Parser 处补一个局部 `FCDiagnostics`。

- [x] **Step 4: 运行确认通过**

Run: 同 Step 2。
Expected: 新用例全部 `[PASS]`，既有用例无回归。

- [x] **Step 5: Commit**

```bash
git add Calculater/diagnostic.h Calculater/parser.h Calculater/parser.cc Calculater/scanner.h Calculater/scanner.cc Calculater/CMakeLists.txt Calculater/unit_test.cc
git commit -m "feat: introduce FCDiagnostic channel, parser and scanner report structured diagnostics"
```

### Task 2: SemanticAnalyzer 接入诊断通道

**Files:**
- Modify: `Calculater/semantic.h:167-193`（SemanticAnalyzer）、`Calculater/semantic.cc:136-167`（declareVariable）、`Calculater/semantic.cc:173-205`（registerFunction）、`Calculater/semantic.cc:388-512`（analyzer 各错误点）
- Modify: `Calculater/scanner.cc:36-40`（analyzer 构造）
- Test: `Calculater/unit_test.cc`

**Interfaces:**
- Consumes: Task 1 的 `FCDiagnostics`、`FCDiagnostic`、`FCSourceLocation{}`。
- Produces: `SemanticAnalyzer(FCSemanticContext &ctx, FCDiagnostics &diagnostics)`（构造函数新增第二参数）；私有辅助 `void reportError(const std::string &message);`（stage 固定 `"semantic"`，位置固定未知 `{}`）。
- 语义不变式：`declareVariable`/`registerFunction` 失败改为**静默**返回 `nullptr`/`false`，报告职责全部上移到调用方（SemanticAnalyzer）。

- [x] **Step 1: 写失败测试（runDiagnosticTests 追加）**

```cpp
    // 7. 语义错误：重复声明
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { var x:int = 1; var x:int = 2; x }");
        expect(ast == nullptr, "diag: duplicate var fails");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty() && diags[0].stage == "semantic"
               && diags[0].message.find("'x'") != std::string::npos,
               "diag: duplicate var recorded with name");
    }
    // 8. 语义错误：未声明变量
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { y }");
        expect(ast == nullptr, "diag: undeclared variable fails");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty() && diags[0].message.find("'y'") != std::string::npos,
               "diag: undeclared variable recorded");
    }
    // 9. 语义错误：重复函数
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { 1 } def f() { 2 }");
        expect(ast == nullptr, "diag: duplicate function fails");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty() && diags[0].message.find("'f'") != std::string::npos,
               "diag: duplicate function recorded");
    }
    // 10. 语义错误：嵌套 def（默认配置）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { def g() { 1 } g() }");
        expect(ast == nullptr, "diag: nested def rejected");
        const auto& diags = scanner.diagnostics();
        expect(!diags.empty()
               && diags[0].message.find("nested function definition") != std::string::npos,
               "diag: nested def recorded");
    }
    // 11. for 循环变量重复声明
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { var i:int = 0; for i = 0, i < 1 in i }");
        expect(ast == nullptr, "diag: duplicate for-var fails");
        expect(!scanner.diagnostics().empty()
               && scanner.diagnostics()[0].message.find("'i'") != std::string::npos,
               "diag: duplicate for-var recorded");
    }
```

- [x] **Step 2: 运行确认失败**

Run: 同 Task 1 Step 2。
Expected: 用例 7-11 `[FAIL]`（诊断为空，消息仍走 stderr）。

- [x] **Step 3: 实现**

- `semantic.h`：SemanticAnalyzer 构造签名加 `FCDiagnostics &diagnostics`，成员 `FCDiagnostics &m_diagnostics;`，声明 `void reportError(const std::string &message);`。
- `semantic.cc` `reportError` 实现：`m_diagnostics.add("semantic", {}, message);`。
- `declareVariable`（semantic.cc:144-148）：删除 `fprintf`，保留 `return nullptr;`；同函数注释注明"失败静默，调用方负责报告"。
- `registerFunction`（semantic.cc:175-179）：同样删 fprintf。
- analyzer 各错误点改为 `reportError(...)`，消息文本保持原样：
  - `analyzeVarDecl`：`declareVariable` 返回 null → `"Error: variable '" + name + "' already declared in current scope"`（name 取 `node->decl->name`）。
  - `analyzeFor`：同上（`node->getDecl()->name`）。
  - `analyzeFunction` 参数声明失败：同上（`arg->name`）；`registerFunction` 返回 false → `"Error: function '" + name + "' already defined"`；嵌套拒绝（semantic.cc:452）原消息改为 reportError。
  - `analyzeVariable`（semantic.cc:403）：原消息改 reportError。
- `scanner.cc:36`：`SemanticAnalyzer analyzer(m_semanticContext, m_diagnostics);`。
- 修复 unit_test.cc 中直接构造 `SemanticAnalyzer` 的调用点（编译器引导）。

- [x] **Step 4: 运行确认通过**

Run: 同 Task 1 Step 2。
Expected: 用例 7-11 `[PASS]`；既有语义测试无回归（它们断言返回值，不断言 stderr）。

- [x] **Step 5: Commit**

```bash
git add Calculater/semantic.h Calculater/semantic.cc Calculater/scanner.cc Calculater/unit_test.cc
git commit -m "feat: semantic analyzer reports into FCDiagnostic channel"
```

### Task 3: 求值器接入诊断通道

**Files:**
- Modify: `Calculater/evaluator.h:19-23`（FCEvaluationContext）、`Calculater/evaluator.cc`（fprintf 站点：186、196、203、235、263、281、288、308、324、394-397）
- Test: `Calculater/unit_test.cc`

**Interfaces:**
- Consumes: `FCDiagnostics`。
- Produces: `FCEvaluationContext` 公有成员 `FCDiagnostics diagnostics;`（求值期错误与编译期错误是两个独立收集器：scanner 持编译期，context 持运行期）。

- [x] **Step 1: 写失败测试（runDiagnosticTests 追加）**

```cpp
    // 12. 求值错误：函数不存在（诊断与 Dangle 并存）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("undefinedfn(1)");
        expect(ast != nullptr, "diag: eval case parses");
        FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
        FCValue v = evaluate(ast.get(), context);
        expect(v.type == FCValueCategory::Dangle, "diag: eval error yields Dangle");
        expect(!context.diagnostics.empty()
               && context.diagnostics.items()[0].stage == "evaluator"
               && context.diagnostics.items()[0].message.find("undefinedfn") != std::string::npos,
               "diag: unknown function recorded");
    }
    // 13. 求值错误：实参数不匹配
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def add(a:int, b:int) { a + b } add(2, 3, 4)");
        expect(ast != nullptr, "diag: arity case parses");
        FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
        FCValue v = evaluate(ast.get(), context);
        expect(v.type == FCValueCategory::Dangle, "diag: arity error yields Dangle");
        expect(!context.diagnostics.empty()
               && context.diagnostics.items()[0].message.find("Argument count mismatch") != std::string::npos,
               "diag: arity mismatch recorded");
    }
    // 14. 求值错误：赋值左侧非变量
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { 1 = 2 } f()");
        FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
        FCValue v = evaluate(ast.get(), context);
        expect(v.type == FCValueCategory::Dangle, "diag: bad assign yields Dangle");
        expect(!context.diagnostics.empty()
               && context.diagnostics.items()[0].message.find("assignment") != std::string::npos,
               "diag: bad assignment recorded");
    }
```

注：用例 12 需先确认 `undefinedfn(1)` 可通过语义分析（当前语义期不校验被调用函数是否存在——semantic.cc 无此检查）；若被拒绝则改为手工构造 AST 的既有模式（参照 unit_test.cc 既有 evaluator 错误测试）。

- [x] **Step 2: 运行确认失败**

Run: 同 Task 1 Step 2。
Expected: 用例 12-14 `[FAIL]`。

- [x] **Step 3: 实现**

- `evaluator.h` FCEvaluationContext 加公有成员 `FCDiagnostics diagnostics;`（`#include "diagnostic.h"`）。
- evaluator.cc 全部 `fprintf(stderr, ...)` 错误站点改为 `context.diagnostics.add("evaluator", {}, <同文本>)`；消息文本逐字保留（含 "LogError:" 前缀的去掉前缀，仅保留正文——前缀由 formatDiagnostic 统一承担）。
- 函数名等动态部分用字符串拼接保留原信息量。

- [x] **Step 4: 运行确认通过**

Run: 同 Task 1 Step 2。
Expected: 用例 12-14 `[PASS]`；既有 evaluator 测试无回归。

- [x] **Step 5: Commit**

```bash
git add Calculater/evaluator.h Calculater/evaluator.cc Calculater/unit_test.cc
git commit -m "feat: evaluator reports into FCDiagnostic channel"
```

### Task 4: Codegen 接入诊断通道

**Files:**
- Modify: `Calculater/codegen.h:27-54`（FCCodegenContext）、`Calculater/codegen.cc`（logCodegenError:24-28；直接 fprintf 站点 427、432；725 行）
- Test: `Calculater/unit_test.cc`

**Interfaces:**
- Consumes: `FCDiagnostics`。
- Produces: `FCCodegenContext` 公有成员 `FCDiagnostics diagnostics;`。
- 附带修复：codegen.cc:725 错误消息 `"variable has defiened in function!"` → `"variable already defined in function"`（拼写错误随行修改）。

- [x] **Step 1: 写失败测试（runCodegenErrorTests 模式下追加；手工 AST 用既有构造方式）**

```cpp
    // 15. codegen 错误：赋值左侧非变量
    {
        // 沿用 runCodegenErrorTests 中既有的手工 AST 构造方式：
        // Binary(Assign, Number(1), Number(2))
        FCCodegenContext context("diagModule", CompiledProgram{});
        auto* code = ...;  // 既有错误测试同款 Binary(Assign, 1, 2) AST
        expect(codegenTopLevel(code, context) == nullptr, "diag: codegen assign fails");
        expect(!context.diagnostics.empty()
               && context.diagnostics.items()[0].stage == "codegen"
               && context.diagnostics.items()[0].message.find("assignment") != std::string::npos,
               "diag: codegen assign error recorded");
    }
    // 16. codegen 错误：调用不存在的函数
    {
        // Call("nosuchfn", []) 手工 AST
        ...
        expect(codegenTopLevel(...) == nullptr, "diag: codegen call fails");
        expect(!context.diagnostics.empty()
               && context.diagnostics.items()[0].message.find("nosuchfn") != std::string::npos,
               "diag: codegen unknown function recorded");
    }
    // 17. 空函数体 codegen 失败（README 已知行为）
    {
        FCScanner scanner;
        auto ast = scanner.analysis("def f() { }");
        FCCodegenContext context("diagModule", scanner.semanticContext().getCompiledProgram());
        context.semantic = &scanner.semanticContextMutable();
        expect(codegenTopLevel(ast.get(), context) == nullptr, "diag: empty body fails");
        expect(!context.diagnostics.empty(), "diag: empty body error recorded");
    }
```

（`...` 处沿用 runCodegenErrorTests 中已有的节点构造语句，不新造 API。）

- [x] **Step 2: 运行确认失败**

Run: 同 Task 1 Step 2。
Expected: 用例 15-17 `[FAIL]`。

- [x] **Step 3: 实现**

- `codegen.h` FCCodegenContext 加公有成员 `FCDiagnostics diagnostics;`（`#include "diagnostic.h"`）。
- `logCodegenError` 改为 `context.diagnostics.add("codegen", {}, message); return nullptr;`（签名不变，仍返回 `llvm::Value*`）；去掉 stderr 打印。
- codegen.cc:427/432 两处直接 fprintf 改为 `expression` 所在函数的 `context.diagnostics.add("codegen", {}, ...)`，消息文本逐字保留。
- 725 行消息拼写修正（见 Interfaces）。

- [x] **Step 4: 运行确认通过**

Run: 同 Task 1 Step 2。
Expected: 用例 15-17 `[PASS]`；既有 codegen 测试无回归。

- [x] **Step 5: Commit**

```bash
git add Calculater/codegen.h Calculater/codegen.cc Calculater/unit_test.cc
git commit -m "feat: codegen reports into FCDiagnostic channel; fix 'defiened' typo"
```

### Task 5: CLI 打印诊断 + 全量验证

**Files:**
- Modify: `Calculater/main.cc:54-70`
- Test: 手工 CLI 验证 + 全量回归

**Interfaces:**
- Consumes: `FCScanner::diagnostics()`、`FCEvaluationContext::diagnostics`、`FCCodegenContext::diagnostics`、`FC::formatDiagnostic`。
- Produces: 用户可见输出格式 `"error[stage] line:col: message"`（stderr），成功路径零输出变化（`[result] ...` 不变）。

- [x] **Step 1: 实现 main.cc**

- `analysis` 失败（`ast == nullptr`）：遍历 `scanner.diagnostics().items()`，`fprintf(stderr, "%s\n", FC::formatDiagnostic(d).c_str())`，返回 1。
- 求值路径：`evaluate` 返回后若 `context.diagnostics` 非空，逐条打印（当前错误也会体现在 Dangle 结果中）。
- `--emit-ir` 路径：`codegenTopLevel` 返回 null 时打印 `context.diagnostics` 后返回 2。

- [x] **Step 2: 手工 CLI 验证**

```powershell
& .\build-debug\Calculater\Debug\FunctionsCalculater.exe 'def 123() { 1 }'
# 期望 stderr: error[parser] 1:5: Expected function name in prototype
& .\build-debug\Calculater\Debug\FunctionsCalculater.exe 'def f() { var x:int = 1; var x:int = 2; x }'
# 期望 stderr: error[semantic]: Error: variable 'x' already declared in current scope
& .\build-debug\Calculater\Debug\FunctionsCalculater.exe 'def add(a:int, b:int) { a + b } add(2, 3)'
# 期望 stdout: [result] 5（成功路径无额外输出）
```

- [x] **Step 3: 全量回归**

```powershell
cmake --build build-debug --config Debug --parallel 16
ctest --test-dir build-debug -C Debug --output-on-failure
pwsh -NoProfile -File .\tools\coverage.ps1 -BuildDir .\build-debug -Configuration Debug
```

Expected: 全部测试通过；行覆盖率 ≥ 95%（新增 diagnostic 代码被测试覆盖）。

- [x] **Step 4: 双配置验证**

```powershell
pwsh -NoProfile -File .\tools\verify-configurations.ps1
```

Expected: 默认与 `FC_ENABLE_NESTED_FUNCTIONS=ON` 两配置构建 + 全量断言通过。

- [x] **Step 5: Commit**

```bash
git add Calculater/main.cc
git commit -m "feat: CLI prints structured diagnostics from all stages"
```

---

## 自查记录（执行前已核对）

1. **Spec 覆盖**：五阶段（lexer 经 parser 转发）全部接入 ✓；位置信息进入 parser 诊断 ✓；程序化访问 ✓；CLI 打印保留 ✓。错误恢复与多错误收集明确排除（Global Constraints）。
2. **步骤歧义**：Task 4 Step 1 中手工 AST 以"沿用既有构造方式"引用而非抄录——既有 runCodegenErrorTests 已定义该模式，抄录会与现状失同步。
3. **类型一致**：`FCDiagnostics::add(stage, location, message)` 三处消费一致；`Parser(Lexer&, FCDiagnostics&)` 与 `SemanticAnalyzer(ctx, diagnostics&)` 各自任务的 Interfaces 块一致。
4. **Review Focus 落点**：5 项分别钉在 Task 1（1/2/3/4/6 号测试）、Task 2（5 号）、Task 3/4（返回值并存断言）。
5. **比例**：计划约为其实现代码量的 1/2，无函数体转写。
