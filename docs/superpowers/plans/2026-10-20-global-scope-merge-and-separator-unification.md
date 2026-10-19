# Global/Function 作用域合并 + 分隔符统一化 — 落地方案

> 基准：`refactor/var-symbol-binding`（HEAD `f3a4dba`），2026-10-20 制定  
> 目标：A) 全局作用域与函数作用域归一（删除全局特权逻辑）；B) 语句/作用域分隔约束精确统一（删除黑名单与死枚举）  
> 约束：全程不动 evaluator/codegen/lexer；每步独立可验证；最终 694+ 测试全绿、覆盖率 ≥95%

---

## 一、问题定位（为何要改）

### A. 全局作用域的三重特殊地位

| 位置 | 特殊逻辑 |
|------|---------|
| `semantic.cc:12` | 构造时单独 `emplace_back()` 全局 scope，`popScope` 用 `assert(size()>1)` 保护栈底 |
| `semantic.cc:74-77` | `isGlobalScope()` = `!isActive`，否定式谓词反推 |
| `semantic.cc:132-145` | `declareVariable` 双路分派：globalLayout/frameLayout 双布局、frameIndex -1/0、登记 currentFunction/globalSymbols 双表 |
| `semantic.cc:50` | `pushFunctionScope` 强制 `assert(size()==1)`——函数定义被禁止出现在任何嵌套块内 |
| `semantic.cc:105-112` | `lookupGlobalVariable` 不走作用域栈，侧挂遍历 globalSymbols 向量 |

根源：全局不是作用域栈的普通成员，而是"栈底永驻特权 scope + 侧挂向量"的复合体。

### B. 分隔符约束三处各自为政

| 位置 | 现行规则 | 问题 |
|------|---------|------|
| `parseProgram`(63-73) | 顶层 `;` 当噪声跳过 | 与其他层级语义不同 |
| `parseBlockExpr`(408-420) | 表达式后必须 `;` 或 `}` | 唯一强制处；`;` 后跟 `}` 合法 |
| `parseSeqExpr`(434-441) | `;` 为连接符，遇 eof/def/else/`}`/end 停止 | **5-token 黑名单启发式**：`}`、`else` 是结构性 token，被 seq 猜测边界是语法知识泄露 |
| token.h:42 | `tok_end` | **死枚举**：Lexer 的 `end` 不是关键字，永不产生；黑名单中的它从未生效 |

---

## 二、方案 A：Global 作为 `<global>` 函数帧

### A.1 核心思想

```
旧: scopeStack = [GlobalScope(特权)] + 块 scope... + [函数 scope(约束:紧贴栈底)]
新: scopeStack  = 普通 scope 栈（<global> 是栈底那个普通帧）
    m_frameAnchor = 当前函数帧在栈中的锚点下标（0 = <global>）
    m_frameLayouts = 每个函数帧一个 FrameLayout
```

全局 = 名为 `<global>` 的常驻函数帧：有自己的 FrameLayout、自己的 CompiledFunction 记录；顶层 var 就是 `<global>` 帧的普通局部，运行时恰好是 `callStack[0]`。

**块作用域不再需要独立 layout**：块只是变量可见性边界，不是帧边界（同一函数帧共享布局）。

### A.2 数据结构变更（semantic.h）

```cpp
// FCSemanticContext 私有成员替换：
std::vector<Scope> m_scopeStack;              // 保留（可见性栈）
size_t m_frameAnchor = 0;                     // 新增：当前函数帧锚点
std::vector<std::unique_ptr<FrameLayout>> m_frameLayouts;  // 新增：每帧一布局
// 删除：m_frameLayout、m_globalLayout、m_currentState.currentFunction
```

### A.3 关键接口重写（semantic.cc）

```cpp
// 构造：压入 <global> 帧（不再 assert 保护）
FCSemanticContext::FCSemanticContext() {
  reset();
  m_scopeStack.emplace_back();           // <global> scope（普通成员）
  m_frameLayouts.push_back(std::make_unique<FrameLayout>());
  registerFunction("<global>", nullptr); // <global> 的 CompiledFunction 记录
}

// pushFunctionScope：无 assert，任意嵌套位置合法（为闭包铺路）
void FCSemanticContext::pushFunctionScope() {
  m_frameAnchor = m_scopeStack.size();
  m_scopeStack.emplace_back();
  m_frameLayouts.push_back(std::make_unique<FrameLayout>());
  m_currentState.isActive = true;
}

// popFunctionScope：弹回旧锚点（不再硬编码 resize(1)）
void FCSemanticContext::popFunctionScope() {
  m_scopeStack.resize(m_frameAnchor);
  m_frameLayouts.resize(m_frameAnchor + 1);   // 保留锚点帧布局（下个函数复用外层帧栈语义）
  m_currentState.isActive = (m_frameAnchor != 0);
}

// isGlobalScope → isCurrentFrameGlobal（肯定式谓词）
bool FCSemanticContext::isCurrentFrameGlobal() const {
  return m_frameAnchor == 0;
}

// declareVariable：删除双路分派，唯一分配/登记路径
std::shared_ptr<VariableSymbol>
FCSemanticContext::declareVariable(VarDeclPtr declaration) {
  Scope &scope = m_scopeStack.back();
  if (scope.variables.count(declaration->name)) { /* 重定义报错 */ return nullptr; }
  const int slot = m_frameLayouts[m_frameAnchor]->allocateSlot();
  auto symbol = std::make_shared<VariableSymbol>(
      std::move(declaration), slot, static_cast<int>(m_scopeStack.size()));
  symbol->runtime.frameIndex = isCurrentFrameGlobal() ? -1 : 0;
  scope.variables.emplace(declaration->name, symbol);
  // 唯一登记：进当前帧的 CompiledFunction（<global> 记录也存 functions 表）
  auto *fn = currentFrameFunction();
  fn->symbols.addSymbol(declaration->name, symbol);
  if (isCurrentFrameGlobal())
    m_compiledProgram->globalSymbols.push_back(symbol);  // 兼容视图，单点填充
  return symbol;
}

// registerFunction：删除 if (isActive) 双分支，无分支操作锚点帧
// lookupGlobalVariable：改查 functions["<global>"]->symbols（单一查找路径）
```

### A.4 行为保真点（回归防线）

- 顶层块 `{ var g = 1; }` 中 g 仍为全局：锚点=0 时无论压多少块 scope，分配都走 `m_frameLayouts[0]` → frameIndex=-1 ✓（README:291 不变）
- `scopeDepth` 语义照旧（栈深度），现有断言不变
- evaluator/codegen **零改动**：`CompiledProgram` 数据形状不变，`frameForSymbol` 的 -1 特判保留

### A.5 删除/替代清单

| 删除项 | 替代 |
|--------|------|
| `m_globalLayout` | `m_frameLayouts[0]` |
| `isGlobalScope()` 否定谓词 | `isCurrentFrameGlobal()` |
| `pushFunctionScope` 的 `assert(size()==1)` | 无约束 |
| `popFunctionScope` 的 `resize(1)` | `resize(m_frameAnchor)` |
| `lookupGlobalVariable` 遍历向量 | `functions["<global>"]->symbols.lookupShared` |
| `registerFunction` 的 isActive 双分支 | 无分支操作锚点帧 |
| `m_currentState.currentFunction` | `functions[当前帧函数名]` 或锚点记录 |

---

## 三、方案 B：分隔符约束统一

### B.1 三条统一规则

1. **`;` 是唯一表达式分隔符**，只出现在序列上下文（顶层/块/括号）。表示"这一条完了，后面还有"。**裸 `;`（空语句）一律非法**——顶层不再静默跳过。
2. **每个序列上下文有明确终止条件**，自己声明，禁止黑名单：顶层终止于 `eof`/`tok_def`；块终止于 `}`（块消费）；括号终止于 `)`（括号消费）。
3. **结构性 token 归属制**：`else` 属 if、`in` 属 for、`}` 属块、`)` 属括号。出现在序列位置 = 序列自然终止（非错误），交还外层消费。

### B.2 判定函数（词法知识的唯一汇聚点）

```cpp
// parser.cc — 表达式起始符判定，替代一切黑名单；与 parsePrimary case 表同构
static bool startsExpression(int kind) {
    switch (kind) {
    case tok_identifier: case tok_number: case tok_string:
    case tok_parenthes_open: case tok_brace_open:
    case tok_if: case tok_for: case tok_var:
        return true;
    default:
        return false;   // ; } ) else in def eof 等都不是表达式起始
    }
}
```

### B.3 三个解析器重写（parser.cc）

```cpp
// 1) 顶层：裸 ';' 非法；def 分派后 continue（def 是语句，不要求 ';'）
std::unique_ptr<FCExprAST> Parser::parseProgram() {
    std::vector<std::unique_ptr<FCExprAST>> statements;
    while (m_curTok.kind != tok_eof) {
        if (m_curTok.kind == tok_def) {
            if (auto fn = parseDefinition()) statements.push_back(std::move(fn));
            continue;
        }
        if (m_curTok.kind == tok_semicolon)
            return logError("empty statement at top level");
        if (auto stmt = parseSeqExpr()) statements.push_back(std::move(stmt));
    }
    return statements.size() == 1 ? std::move(statements[0])
        : std::make_unique<FCProgramAST>(std::move(statements));
}

// 2) 块：';' 分隔、'}' 终止；块内裸 ';' 非法；x; } 与 x } 均合法
std::unique_ptr<FCExprAST> Parser::parseBlockExpr() {
    advance();  // '{'
    std::vector<std::unique_ptr<FCExprAST>> exprs;
    while (m_curTok.kind != tok_brace_close) {
        if (m_curTok.kind == tok_eof) return logError("unterminated block");
        if (m_curTok.kind == tok_semicolon) return logError("empty statement in block");
        auto e = parseExpression();
        if (!e) return nullptr;
        exprs.push_back(std::move(e));
        if (m_curTok.kind == tok_semicolon) { advance(); continue; }
        if (m_curTok.kind == tok_brace_close) break;
        return logError("expected ';' or '}' in block");
    }
    advance();  // '}'
    return std::make_unique<FCBlockExprAST>(std::move(exprs));
}

// 3) 通用序列：startsExpression 终止，删除 5-token 黑名单
std::unique_ptr<FCExprAST> Parser::parseSeqExpr() {
    auto first = parseExpression();
    if (!first) return nullptr;
    std::vector<std::unique_ptr<FCExprAST>> exprs;
    exprs.push_back(std::move(first));
    while (m_curTok.kind == tok_semicolon) {
        advance();
        if (!startsExpression(m_curTok.kind)) break;  // 交还外层
        auto next = parseExpression();
        if (!next) return nullptr;
        exprs.push_back(std::move(next));
    }
    return exprs.size() == 1 ? std::move(exprs[0])
        : std::make_unique<FCSeqExprAST>(std::move(exprs));
}
```

终止示例（`if c then (a;b) else d`）：括号 seq 遇 `else` → `startsExpression=false` → seq 终止 → paren 消费 `)` → if 消费 `else`。三层各拿各的 token。

### B.4 词法清理

```diff
- token.h: tok_end            // 死枚举，Lexer 永不产生，grep 清零残留
- parseSeqExpr 5-token 停止表  // 全部由 startsExpression 替代
```

### B.5 兼容性影响（唯一行为收紧）

| 现行输入 | 新规则 | 处置 |
|---------|--------|------|
| 顶层 `; 1`、`1; ; 2`（裸分号） | 变为错误 | **有意收紧**；scanner 组 4 个裸分号用例改 expectError |
| 块内 `{ x; }` | 保持合法 | README:185 承诺不破坏 ✓ |
| 括号 `(a;)` | 复核后仍合法（seq 终止 → paren 消费 `)`） | 无变化 ✓ |
| `tok_end` | 删除 | 无输入依赖 ✓ |

---

## 四、执行计划（用户指定顺序）

```
A-1 → A-2 → B-1 → B-2 → A-3(可选) → 全量回归+覆盖率 → 更新 ARCHITECTURE.md/README
```

| 步 | 动作 | 验证 |
|----|------|------|
| **A-1** | semantic.h/cc：`m_frameAnchor` + `m_frameLayouts`，重写 push/pop/declare/lookup/register；删 `isGlobalScope/globalLayout/currentFunction`；`registerFunction("<global>")` 自动化 | 675+ 断言全绿（重点：semantic-analyzer 22 断言的 frameIndex=-1、scope guard 系列、compiled 布局系列） |
| **A-2** | unit_test：直接调 `pushFunctionScope` 的用例改经 `analyzeFunction`；新增"函数定义出现在顶层块内不再 assert 崩溃"正向用例 | 全绿 |
| **B-1** | parser.cc：`startsExpression` + 三函数重写；token.h 删 `tok_end` + grep 清零；runParserTests 新增 6 用例（else/`)`/`}`/def 终止 seq、顶层裸 `;` 报错、块内 `x; }` 合法）；scanner 组 4 个裸分号用例改 expectError | 全绿 |
| **B-2** | README:185/277 修订一行（顶层空语句非法）；ARCHITECTURE.md 5.5 错误契约表补"裸 `;` → Parser error" | 文档一致性 |
| **A-3**（可选） | `lookupGlobalVariable` 改查 `functions["<global>"]->symbols`；globalSymbols 标注兼容视图 | 全绿 + 覆盖率 ≥95% |
| **收尾** | 全量回归 + 覆盖率 ≥95%；更新 ARCHITECTURE.md（5.1/5.2 节设计决策、模块职责表）与 README；grep 验证 `isGlobalScope/tok_end/黑名单` 零残留 | 694+ 全绿、覆盖率 ≥95% |

### Commit 划分（两个改动目标两个 commit）

| Commit | 内容 |
|--------|------|
| `refactor: unify global scope as <global> function frame` | A-1 + A-2（+A-3 若执行），semantic.h/cc + unit_test |
| `refactor: unify statement separators via startsExpression` | B-1 + B-2，parser.cc + token.h + unit_test + README/ARCHITECTURE 对应行 |

### 完成后动作

两个 commit 完成、全量回归通过后，`git push` 推送到远程 `origin/refactor/var-symbol-binding`。

---

## 五、风险与防线

| 风险 | 防线 |
|------|------|
| A：`registerFunction("<global>")` 时序与 functions 表污染 | `compiled - *` 全部布局断言 + `semantic-analyzer - *` 22 断言 |
| A：`popFunctionScope` resize 语义破坏块作用域 | `scope guard`/`shadowing`/`slot allocation` 系列断言 |
| B：块/括号边界既有用例（`trailing semicolon in block` 等） | 先跑后改，逐条对号 |
| B：黑名单删除后 `else` 误入 seq | runParserTests 新增 else 终止用例 |
| 覆盖率滑坡 | semantic.cc/parser.cc 改动区域均有既有套件直接命中 |

规模预估：A-1 约 150 行净变化；B-1 约 80 行；测试调整约 60 行。

---

> 方案作者：AI Agent，2026-10-20  
> 前置文档：`docs/superpowers/plans/2025-06-20-lexer-parser-semantic-decoupling.md`（已全部落地）、`docs/ARCHITECTURE.md`（5.1-5.5 节为本方案修改对象）
