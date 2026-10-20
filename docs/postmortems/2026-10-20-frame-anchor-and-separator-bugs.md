# 三个硬卡点复盘：帧锚点模型与分隔符统一

> 分支：`refactor/var-symbol-binding`
> 涉及提交：`d9203e8`（方案 A：全局并入 `<global>` 帧）、`732ea2d`（方案 B：分隔符统一）
> 最终结果：**721/721 测试通过，覆盖率 95.35%**（evaluator 97.29%、semantic 96.87%、parser 97.76%，lexer/scanner/token 100%）

---

## 0. 三个问题的共同形状

三个 bug 的单行归纳：

| # | 一句话 | 本质 | 修复手段 |
|---|--------|------|----------|
| 1 | 占位名 `<frame:N>` 冲突 → `CompiledFunction` 记录串用 | **身份用字符串表达** | 改 `shared_ptr` 指针同一性（帧记录移交制） |
| 2 | 帧锚点误用作用域栈深度 → `operator[]` 越界死循环 | **两个深度共用一个数字** | 锚点走帧计数域 + `FrameAnchorEntry` 显式保存双状态 |
| 3 | `parseSeqExpr` 失败后无 `advance` → 同一 token 静默空转 | **失败路径没有单一契约** | `startsExpression()` 唯一终止权威 + 失败即终止 |

共同点：**局部自洽、跨层组合才暴露**。所以三者都不能靠打补丁修掉，必须把隐式约定变成显式结构。

> 说明：问题 1、2 的故障版本是方案 A 落地过程中的工作区迭代（修好后才提交），未进入 git 历史；下文的"故障代码"为当时的等价形态，用于说明机制。问题 3 的故障版本同样是重写 `parseProgram` 时的中间态，修复后的代码在 `732ea2d` 中可查。

---

## 1. 案例一：`CompiledFunction` 记录串用 —— 从单指针 `currentFunction` 到帧记录栈

### 1.0 溯源：哪个提交引入了它？

**结论先行：占位名方案从未进入 git 历史，因此没有"引入它的提交"。**

```powershell
git log --all --oneline -S "<frame"          # 零命中：历史中不存在该字面量
git reflog --date=iso | Select-String "commit:"
# 030fc51 2026-10-20 00:21  docs: add ... implementation plan
# d9203e8 2026-10-20 02:00  refactor(A): ... frame-anchor model
#   → 两次提交之间没有任何中间提交
```

`<frame:N>` 只存在于方案 A 落地时的**工作区迭代**，改好之后才一次性提交，所以 `d9203e8` 里看到的是最终形态（帧记录移交制），看不到故障形态。但"记录串用"要修的**病根**有明确的提交来源，可以逐提交对比：

| 提交 | 时间 | 对"当前正在编译的函数记录"的处理 | 改动动机 |
|------|------|--------------------------------|----------|
| `99336b3` | 2026-08-29 00:26 | `assert(m_scopeStack.size() == 1)` + `m_currentFuncScopeIdx` | "分离 scope 与 AST 的耦合"：把"同时只有一个函数"写成断言 |
| `53ef58f` | 2026-08-30 15:56 | 引入 `CurrentCompilationState{ functionName, currentFunction, isActive }` —— **单指针** | "分离 scope（可见性）与 frame（存储）"，把 Semantic 输出收进 `CompiledProgram` |
| `2edf139` | 2026-10-18 23:37 | SemanticAnalyzer 复用同一 context；先 `registerFunction` 再声明参数；无 frameSize 回填 | 三阶段解耦，新增语义分析阶段 |
| `519f10c` | 2026-10-19 23:02 | 加回填 `if (isActive && currentFunction) currentFunction->frameSize = ...` | 帧大小唯一权威改为 `CompiledFunction::frameSize` |
| `d9203e8` | 2026-10-20 02:00 | **删断言、删单指针**，改 `m_frameRecords` 栈 + `m_frameAnchor` | 全局并入 `<global>` 帧；函数可在任意嵌套位置定义（closure-ready） |

旧形态（`030fc51` 时的 `semantic.cc`）：

```cpp
void FCSemanticContext::pushFunctionScope() {
  assert(m_scopeStack.size() == 1);          // 前提：全局之下最多压一层
  m_scopeStack.emplace_back();
  m_frameLayout.reset();
  m_currentState.currentFunction = std::make_shared<CompiledFunction>();  // ← 单格
  m_currentState.isActive = true;
}

void FCSemanticContext::popFunctionScope() {
  assert(m_scopeStack.size() > 1);
  m_scopeStack.resize(1);                    // ← 硬编码"回到只剩全局"
  m_currentState.currentFunction = nullptr;  // ← 单格置空
  m_currentState.isActive = false;
  m_frameLayout.reset();
}
```

**改动原因的对比**：

- `53ef58f` 的选择在当时是**正确**的：既然 `99336b3` 的断言保证"同时只有一个函数帧"，那"当前函数记录"就是唯一的那一个，用一个字段保存最省事，也不必维护一个栈。
- `d9203e8` 的动机不是"改掉单指针"，而是"允许函数在任意嵌套块位置定义"。这个能力一开，`assert(size()==1)` 必须删；断言一删，`currentFunction` 这个"唯一槽位"就不再唯一——**内层 push 覆盖外层，内层 pop 把它置空，外层再也拿不回自己的记录**。前提被移除、数据结构没跟着换，单指针就变成了共享格。

### 1.0.1 用已提交的代码复现失败

```
def outer() {
    var outerVar:int = 1;
    { def inner() { outerVar } }
}
```

| 步骤 | 对 `m_currentState` / `m_scopeStack` 的影响 |
|------|---------------------------------------------|
| `pushFunctionScope()`（outer） | `currentFunction = R_outer`，`isActive = true`，scope 深度 2 |
| `registerFunction("outer")` | 新建 `C_outer`；`symbols = std::move(R_outer->symbols)`；`functions["outer"] = C_outer`；`currentFunction = C_outer` |
| 声明 `outerVar` | 写进 `C_outer->symbols`（因为 `currentFunction == C_outer`）✓ |
| 进入 `{}` → `pushScope()` | scope 深度 3 |
| `pushFunctionScope()`（inner） | **`currentFunction = R_inner`，`C_outer` 被覆盖** |
| `registerFunction("inner")` | `functions["inner"] = C_inner`；`currentFunction = C_inner` |
| inner 分析完 → `popFunctionScope()` | **`m_scopeStack.resize(1)`**：outer 的函数作用域层连同 `outerVar` 的 scope 记录一起被丢弃；`currentFunction = nullptr`；`isActive = false`（外层其实仍在分析中） |
| 回到 outer 继续分析 `outerVar` | `lookupVariableShared("outerVar")` → `nullptr`（scope 记录已被销毁）→ 未绑定 → `Dangle` |
| outer 的 frameSize 回填 | `if (isActive && currentFunction)` → 两者皆假 → **`C_outer->frameSize` 永远停在参数个数** |

后端拿到的是"键还在、内容已被内层顶替"的对象：

- `frameSize` 只含参数 → `evaluator pushFrame` 的 `frame.locals.resize()` 不足，局部写入越界；
- 外层局部变量在语义期就丢失绑定 → 求值返回 `Dangle`；
- 若有多层定义，`currentFunction` 的覆盖顺序决定"谁的记录被改"，完全依赖嵌套顺序。

这是"记录串用"在**可提交证据**上的形态：不是两个 `shared_ptr` 指向同一对象，而是**单格被内层覆盖/置空，外层的记录与回填全部丢失**。它的症状（局部变量未绑定、帧大小不足）与"指向同一对象"高度相似，排查时极易归错因——下面 1.1 记录的是另一种更强的形态。

### 1.1 未提交的中间迭代：用占位名模拟匿名帧

`pushFunctionScope()` 需要一个"当前函数的编译记录"来接收参数符号。但 `CompiledFunction` 最终要按**函数名**登记进 `m_compiledProgram->functions`，而 `pushFunctionScope()` 在解析 `def` 原型时就被调用——**此时还不知道函数名**吗？知道，但工程上想复用"先 push 一个匿名帧、后续 rename"的写法：

```cpp
// 故障版本（等价形态，未提交）——用占位名模拟匿名帧
void FCSemanticContext::pushFunctionScope() {
    m_frameAnchor = m_frameFunctionNames.size();          // 锚点 = 帧计数
    const std::string placeholder =
        "<frame:" + std::to_string(m_frameAnchor) + ">";  // ① 造一个占位名
    m_frameFunctionNames.push_back(placeholder);          //    下标 = 锚点
    auto record = std::make_shared<CompiledFunction>();
    m_frameRecords.push_back(record);
    m_compiledProgram->functions[placeholder] = record;    // ② 登记进函数表
    m_scopeStack.emplace_back();
    m_frameLayouts.push_back(std::make_unique<FrameLayout>());
}

bool FCSemanticContext::registerFunction(const std::string &name,
                                         const FCFunctionAST *fn) {
    auto &placeholder = m_frameFunctionNames[m_frameAnchor]; // ③ 靠下标取回名字
    auto record = m_compiledProgram->functions[placeholder];
    m_compiledProgram->functions.erase(placeholder);         // ④ 删占位
    record->ast = const_cast<FCFunctionAST *>(fn);
    m_compiledProgram->functions[name] = record;              // ⑤ 换成真名
    ...
}

void FCSemanticContext::popFunctionScope() {
    m_frameFunctionNames.resize(m_frameAnchor + 1);  // ⑥ 只裁到锚点，保留锚点位置的名字
    ...
}
```

### 1.2 死在哪一步：索引与锚点脱钩

关键在于 ⑥ —— `resize(anchor + 1)` **保留**锚点位置上的旧名字，但下一次 `pushFunctionScope()` 的 `push_back` 是**追加**，不是覆盖：

```
初始状态（<global> 帧）
  anchor                 = 0
  m_frameFunctionNames   = ["<global>"]          ← 下标 0
  m_frameRecords         = [R_global]
  functions              = { "<global>" → R_global }

注册 isEven（帧下标 1）
  push：m_frameFunctionNames.push_back("<frame:1>")   → ["<global>", "<frame:1>"]
        m_frameRecords          .push_back(R_even)    → [R_global, R_even]
        functions["<frame:1>"] = R_even
        anchor = 1
  register("isEven")：③ 取 m_frameFunctionNames[1] = "<frame:1>"  ← 正确
                      ④ erase("<frame:1>")，⑤ functions["isEven"] = R_even  ← 正确
  pop： m_frameFunctionNames.resize(1 + 1) = resize(2) → 不变（仍是 2 个元素！）

注册 isOdd（帧下标应为 2）
  push：m_frameFunctionNames.push_back("<frame:2>")
        → ["<global>", "<frame:1>", "<frame:2>"]      ← 下标 1 仍是旧串 "<frame:1>"
        m_frameRecords.push_back(R_odd) → [R_global, R_even, R_odd]
        functions["<frame:2>"] = R_odd
        anchor = 2                                        ← 但 register 用的是 …
  register("isOdd")：③ 取 m_frameFunctionNames[2] = "<frame:2>"  ← 此时侥幸正确
```

**真正的错位发生在"锚点回退"之后**：`popFunctionScope` 把 `anchor` 恢复成 `prevAnchor`，而 `m_frameFunctionNames` 因为 `resize(anchor+1)` 保留了尾部元素，**长度与 `m_frameRecords` 不再同步**。只要出现"函数内定义函数"（案例 2 的嵌套场景），内层 pop 之后 `anchor` 回到 1，但 `m_frameFunctionNames` 尾巴上还挂着 `<frame:2>`；下一次 `push` 追加到下标 3，而 `m_frameRecords` 的新元素在下标 2 —— **两个按同一锚点索引的向量，元素数不同**，于是：

- ③ 取到的名字属于**另一个帧** → ④ `erase` 删掉的是别人的键；
- ⑤ 把**同一个 `shared_ptr`** 写到两个函数名下（`functions["isEven"]` 与 `functions["isOdd"]` 指向同一对象）。

### 1.3 观测到的症状

```
Error: no compiled record for 'isEven'
Error: no compiled record for 'mul2'
fn isEven frameSize=2 params=[0 0]        ← 两次注册把形参槽位 push 进同一个对象
Error: function 'isOdd' already defined   ← 或者：别名导致重定义误判
```

四类症状全都能用"同一记录被两个名字共享 / 某记录键被误删"解释：

| 症状 | 直接原因 |
|------|----------|
| `getFunction("mul2") == nullptr` 但 `hasFunction("mul2") == true` | 键被后来者的 `erase(placeholder)` 删掉；`m_functionSet` 是独立的 `set`，没同步 |
| `isEven` / `isOdd` 的 `frameSize` 相同且异常 | 两个名字指向同一 `shared_ptr`，后写覆盖前写 |
| `parametersIdx == [0 0]` | 同一对象的 `parametersIdx` 被 `push_back` 两次 |
| 递归函数返回错值 / `Dangle` | 帧大小与实际槽位不符，`frame.locals.resize()` 越界或错位 |

### 1.4 最小复现

互相递归的两个函数——**只要连续注册两个函数**就会触发：

```
def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) };
def isOdd(n:int)  { if n < 1 then 0 else isEven(n - 1) };
isEven(10)
```

对应测试：[`Calculater/unit_test.cc:538-540`](../../Calculater/unit_test.cc#L538-L540)（`mutual recursion even/odd`）。

### 1.5 修复：帧记录移交制（`d9203e8`）

彻底删掉占位名，让"匿名帧"真正匿名——**用下标寻址，不借字符串键存活**：

```cpp
// Calculater/semantic.h:221-225
size_t m_frameAnchor = 0;                          // 帧计数域；0 = <global>
std::vector<std::unique_ptr<FrameLayout>>       m_frameLayouts;  // 每帧一个布局
std::vector<std::shared_ptr<CompiledFunction>>  m_frameRecords;  // 每帧一个记录
```

```cpp
// Calculater/semantic.cc:52-64
void FCSemanticContext::pushFunctionScope() {
    const size_t scopeDepthAtPush = m_scopeStack.size();
    m_frameAnchorStack.push_back({m_frameAnchor, scopeDepthAtPush});
    m_frameAnchor = m_frameRecords.size();          // 只做追加，不做命名

    m_scopeStack.emplace_back();
    m_frameLayouts.push_back(std::make_unique<FrameLayout>());
    m_frameRecords.push_back(std::make_shared<CompiledFunction>());
    m_currentState.isActive = true;
}
```

```cpp
// Calculater/semantic.cc:171-203 —— 注册即移交，没有搬移、没有 erase、没有 rename
bool FCSemanticContext::registerFunction(const std::string &functionName,
                                         const FCFunctionAST *function) {
    if (hasFunction(functionName)) { /* ... */ return false; }

    auto compiledFunc = currentFrameRecord();        // ← 同一个 shared_ptr
    compiledFunc->ast = const_cast<FCFunctionAST *>(function);
    m_compiledProgram->functions[functionName] = compiledFunc;

    compiledFunc->frameSize     = m_frameLayouts[m_frameAnchor]->frameSize();
    compiledFunc->maxTempSlots  = m_frameLayouts[m_frameAnchor]->frameSize();

    if (function != nullptr) {
        for (const auto &varDecl : function->getProto()->getArgs()) {
            auto param = compiledFunc->symbols.lookupShared(varDecl->name);
            if (param) compiledFunc->parametersIdx.push_back(param->slot);
        }
    }
    m_functionSet.insert(functionName);
    return true;
}
```

参数符号从出生起就写在**最终那个对象**里（`declareVariable` 直接写 `currentFrameFunction()`），注册只是把这个 `shared_ptr` 挂到函数名下，所以 `parametersIdx`/`frameSize` 天然一致，不存在"搬移时漏字段"的可能。

```cpp
// Calculater/semantic.cc:149-158 —— 声明即登记到当前锚点帧
const int slot = m_frameLayouts[m_frameAnchor]->allocateSlot();
...
currentFrameFunction().symbols.addSymbol(name, symbol);
```

### 1.6 为什么原有测试没抓到

所有既有用例都只定义**一个**函数，或定义多个但**不嵌套**——`resize(anchor+1)` 在"顺序定义、逐个 pop"时恰好让 `m_frameFunctionNames.size() == m_frameRecords.size()`，两个向量数值一致，bug 隐身。直到方案 A 允许"函数内定义函数"，锚点回退导致长度分叉才爆发。

### 1.7 检查清单

- [ ] 任何"先造匿名实体、事后改名"的写法，都要问：改名用的**句柄**是什么？它和实体的生命周期是否同一个容器？
- [ ] 多个 `vector` 按同一索引访问时，是否有 `assert(v1.size() == v2.size())`？
- [ ] 共享所有权的对象，身份应通过 `shared_ptr` 同一性比较（`innerRecord != outerRecord`），而不是靠名字重新查找。

---

## 2. 案例二：帧锚点误用作用域栈深度 → `operator[]` 越界死循环

### 2.1 两个容器的两套计数

| 容器 | 计数单位 | 一段代码中的值 |
|------|----------|----------------|
| `m_scopeStack` | **每个块作用域**一层（全局、每个 `{}`、每个函数体各一层） | 变化频繁 |
| `m_frameLayouts` / `m_frameRecords` | **每个函数帧**一个（含 `<global>`） | 只随函数进出变化 |

只有在"全局 + 恰好一个函数体、且函数体内没有块"时，两者数值相等。这个巧合掩盖了整个 bug。

### 2.2 故障代码

```cpp
// 故障版本（等价形态，未提交）
void FCSemanticContext::pushFunctionScope() {
    m_frameAnchor = m_scopeStack.size();      // ← 致命：用"作用域深度"当"帧下标"
    m_scopeStack.emplace_back();
    m_frameLayouts.push_back(std::make_unique<FrameLayout>());
    m_frameRecords.push_back(std::make_shared<CompiledFunction>());
}

void FCSemanticContext::popFunctionScope() {
    assert(m_scopeStack.size() > m_frameAnchor);  // 同一个混淆的第二处
    m_scopeStack.resize(m_frameAnchor + 1);       // ← 扩展而非裁剪：填充 null unique_ptr
    m_frameLayouts.resize(m_frameAnchor);         // ← 与 scope stack 语义不一致
}
```

### 2.3 索引推演：什么时候越界

```cpp
// 输入
def outer() {
    var outerVar:int = 1;
    {                                   // ← 块：analyzeBlock 会 pushScope()
        def inner() { outerVar }        // ← 嵌套函数定义
    }
}
```

```
① 进入 outer：analyzeFunction → FunctionScopeGuard → pushFunctionScope()
   m_scopeStack.size() = 1（仅全局）  →  m_frameAnchor = 1
   push 后：scope=2, layouts=2, records=2    ← 一致

② 进入 outer 函数体：analyzeBlock → pushScope()
   m_scopeStack.size() = 3           ← 但 layouts/records 仍是 2！
   （帧向量不随块作用域增长）

③ 进入内层块：pushScope()
   m_scopeStack.size() = 4           ← layouts/records 仍是 2

④ 分析嵌套 def inner：pushFunctionScope()
   m_frameAnchor = m_scopeStack.size() = 4        ← 锚点取 4
   m_frameLayouts.push_back(...)：新元素下标 = 2   ← 实际落在 2
   m_frameRecords.push_back(...)：新元素下标 = 2

⑤ analyzeFunction(inner) 内任意 declareVariable / currentFrameFunction：
   m_frameLayouts[m_frameAnchor]  →  m_frameLayouts[4]
   而 vector 只有 3 个元素（下标 0,1,2）      ← 越界读 UB
```

**数值对不上的瞬间**：`anchor`（4）与 `size()-1`（2）相差 2，差值恰好是"块作用域的层数"。

### 2.4 为什么表现为 HANG 而不是崩溃

| 构建 | `std::vector::operator[]` 越界后的行为 | 观测 |
|------|----------------------------------------|------|
| **Debug**（MSVC `_ITERATOR_DEBUG_LEVEL=2`） | 触发 `_STL_VERIFY` / `abort()` → 弹 **Abort / Retry / Ignore** 对话框 | 无 stderr、无控制台输出；进程等待不存在的按钮 → `ctest` 60 秒超时被 KILL |
| **Release** | 读到堆上相邻对象的指针，当作 `shared_ptr<VariableSymbol>`/`FrameLayout` 解引用或析构 | 随机 `frameSize`、随机 `free`、或静默死循环 |

这次是 **Debug**，所以现象是"打一行 `analyzeFunction enter inner` 之后永远没动静"。因为没有 assert 文本、没有栈，第一反应会误判成"递归没收敛"。

### 2.5 连带灾难

同一个混淆在 `popFunctionScope` 里还有第二、第三处：

- `assert(m_scopeStack.size() > m_frameAnchor)`：在嵌套函数里 `size()` 与 `anchor` 关系随时会被块作用域污染，**断言先在别处失败**（同样弹框 → 同样表现为 HANG）；
- `m_scopeStack.resize(m_frameAnchor + 1)`：当 `anchor` 大于当前 scope 深度时，这是**扩展**，`vector<Scope>` 扩展本身无妨，但 `m_frameLayouts.resize(m_frameAnchor)` 若用错方向会填充 `null unique_ptr`，后续 `*m_frameLayouts[..]` 直接解引用空指针 → 又一次崩溃。

三个症状是**同一个混淆的不同切面**，所以修的时候必须一起改，否则会陷入"修好一个、冒出另一个"的错觉。

### 2.6 最小复现

"函数内定义函数"即可，见 [`Calculater/unit_test.cc:2461-2504`](../../Calculater/unit_test.cc#L2461-L2504)（`semantic-analyzer - nested function definition parses`）。缩到最小：

```cpp
def outer() { { def inner() { 1 } } }
```

只要函数体内**多一层 `{}`**，锚点就与布局下标错位。

### 2.7 修复：锚点只走帧计数域 + 显式保存双状态（`d9203e8`）

```cpp
// Calculater/semantic.h:224-225
struct FrameAnchorEntry {
    size_t prevAnchor;       // 外层帧锚点（帧计数域）
    size_t scopeDepthAtPush; // 进入时作用域栈深度（作用域域）
};
std::vector<FrameAnchorEntry> m_frameAnchorStack;
```

```cpp
// Calculater/semantic.cc:52-79
void FCSemanticContext::pushFunctionScope() {
    const size_t scopeDepthAtPush = m_scopeStack.size();   // 两个深度分别记录
    m_frameAnchorStack.push_back({m_frameAnchor, scopeDepthAtPush});
    m_frameAnchor = m_frameRecords.size();                 // 锚点 = 帧计数域
    m_scopeStack.emplace_back();
    m_frameLayouts.push_back(std::make_unique<FrameLayout>());
    m_frameRecords.push_back(std::make_shared<CompiledFunction>());
    m_currentState.isActive = true;
}

void FCSemanticContext::popFunctionScope() {
    assert(m_frameRecords.size() == m_frameAnchor + 1);    // ← 域一致性显式校验
    assert(m_frameAnchorStack.size() >= 1);

    const auto entry = m_frameAnchorStack.back();
    m_frameAnchorStack.pop_back();
    m_scopeStack.resize(entry.scopeDepthAtPush);           // 作用域裁回进入前深度
    m_frameLayouts.resize(m_frameAnchor);                  // 帧向量只裁不扩
    m_frameRecords.resize(m_frameAnchor);
    m_frameAnchor = entry.prevAnchor;                      // 恢复外层锚点（支持嵌套）

    m_currentState.isActive = (m_frameAnchor != 0);
}
```

关键点：

1. **锚点只来自 `m_frameRecords.size()`**，与 `m_scopeStack` 彻底脱钩；
2. 两个深度**分别**存在 `FrameAnchorEntry` 里，pop 时各回各家；
3. `assert(m_frameRecords.size() == m_frameAnchor + 1)` 把"隐式假设"变成"可崩溃的显式检查"——下一个犯同样错误的人会立刻拿到栈，而不是一个挂死的进程。

### 2.8 为什么 `assert` 当时没救你

因为断言写在了**另一个容器**上（`m_scopeStack.size() > m_frameAnchor`）。断言本身没写错代码，它只是诚实地反映了一个错误的模型——两个数值碰巧在简单用例里满足，在嵌套用例里不满足。**断言的价值取决于它比较的两个量是否属于同一个语义域。**

### 2.9 检查清单

- [ ] 两个容器描述同一层级结构时，必须有"域一致性"断言（如 `records.size() == anchor+1`），不能假设"数值相等所以随便用"；
- [ ] 用"深度/下标"命名的变量，注释里写明属于哪个域；
- [ ] 调试时若进程无输出挂死：**先把"最后进入点"打到 stderr**，不要等断言文本（Debug 断言会弹框，控制台什么都不会有）；
- [ ] 删除位置约束（如 `assert(size()==1)`）时，要同时拆掉依赖该约束的隐式耦合。

---

## 3. 案例三：失败路径无契约 → 同一 token 静默空转

### 3.1 递归下降的失败契约

`Parser::parseX()` 的约定只有两条可选：

| 约定 | 含义 | 调用方义务 |
|------|------|------------|
| **A. 失败不消费** | 返回 `nullptr`，`m_curTok` 停在失败点 | 调用方必须**终止**，或自己 `advance()` |
| **B. 失败必消费** | 返回 `nullptr`，且已跳过出错 token | 调用方可以继续循环 |

`parseSeqExpr()` 采用 A（第一步 `parseExpression()` 失败时 token 未动）。这个 bug 是**第三种**：失败既不消费、也不终止。

### 3.2 三个版本的演进

```cpp
// ── v1（旧版，030fc51 之前）：静默吞错 ────────────────────────
if (auto stmt = parseSeqExpr()) {
    statements.push_back(std::move(stmt));
} else {
    advance();          // 失败也前进：不空转，但把语法错误静默跳过，
}                       // 继续拼出一个"截断的"程序（错误被伪装成成功）
```

```cpp
// ── v2（重写时的故障版本）：既不前进也不终止 ─────────────────
if (auto stmt = parseSeqExpr()) {
    statements.push_back(std::move(stmt));
    continue;
} else if (!m_hadError) {   // ← 意图"不要在无错误时静默返回 nullptr"
    return nullptr;         //    但"错误已置位"时两个分支都不走
}
// 循环回到 while 顶部，m_curTok 与原样 → 再次失败 → 无限循环
```

```cpp
// ── v3（修复后，732ea2d）：失败即终止 ────────────────────────
if (auto stmt = parseSeqExpr()) {
    statements.push_back(std::move(stmt));
    continue;
}
return nullptr;             // seq 失败即整体失败，绝不在同一 token 上空转
```

### 3.3 死循环推演

输入：`1 + 2 )`（`1 + 2` 之后多了一个右括号）

```
while (m_curTok != eof):
  ① tok = tok_number(1) → parseSeqExpr()
       parseExpression → parseBinOpRHS → 得到 1+2
       while (tok == ';') 不成立 → 返回 1+2        ← 成功，push
  ② tok = tok_parenthes_close → parseSeqExpr()
       parseExpression → parsePrimary(tok=`)`) → default 分支
       → logError("unknown FCToken when expecting an expression")  ← 置 m_hadError，返回 nullptr
       parseSeqExpr 返回 nullptr，token 未动
  ③ if (stmt) 假；else if (!m_hadError) 假（m_hadError 已置位）
     → 两个分支都不执行 → 循环回到 ② ，tok 仍是 `)`
  ④ 无限重复 ②③
```

### 3.4 观测到的症状

```
LogError: unknown FCToken when expecting an expression
LogError: unknown FCToken when expecting an expression
... （437 行完全相同的输出）
[ctest] Test timeout: 60 seconds, process killed
```

三个可复用的判读信号：

1. **同一条错误无限重复** = 状态没有前进（token / 索引 / 指针之一没变）；
2. **日志里 token 值恒定** = 循环体在"读同一个位置"；
3. **输出被超时截断** = 别用 `| Select-Object -Last N` 抓（管道缓冲在进程被杀时不会回吐），改重定向到文件。

### 3.5 v1 也是错的

v1 的 `advance()` 让进程不挂，但它在**静默吞错**：

```
def f() { 1           ← 未闭合块
```

`parseDefinition` 失败 → `stmt == nullptr` → `advance()` → 继续循环 → 最终返回一个**非空的 `FCProgramAST`**（里面只有半截东西或为空）。调用方拿到"成功"的信号，语义分析和执行都照跑，错误被推迟到不可预测的地方。

所以正确的问题不是"要不要 advance"，而是**"失败时谁负责终止"**——必须有唯一答案。

### 3.6 最小复现

```cpp
// 顶层一个无法起始表达式的 token
"1 + 2 )"                       // v2 下 437 行 + 60s 超时
"def f() { 1"                   // v1 下静默返回空程序而非报错
```

修复后应得：`LogError: unknown FCToken when expecting an expression` **一行**，`hadParseError() == true`，`parseProgram()` 返回 `nullptr`。

### 3.7 修复：唯一终止权威 + 失败即终止（`732ea2d`）

```cpp
// Calculater/parser.cc:28-45 —— 序列终止的唯一权威，与 parsePrimary 的 case 表同构
static bool startsExpression(int kind) {
    switch (kind) {
    case static_cast<int>(FCToken::tok_identifier):
    case static_cast<int>(FCToken::tok_number):
    case static_cast<int>(FCToken::tok_string):
    case static_cast<int>(FCToken::tok_parenthes_open):
    case static_cast<int>(FCToken::tok_brace_open):
    case static_cast<int>(FCToken::tok_if):
    case static_cast<int>(FCToken::tok_for):
    case static_cast<int>(FCToken::tok_var):
        return true;
    default:
        return false;      // } ) else in def eof ; —— 结构性 token，交还外层消费
    }
}
```

```cpp
// Calculater/parser.cc:76-110 —— 顶层：def 失败 / seq 失败一律终止
std::unique_ptr<FCExprAST> Parser::parseProgram() {
    std::vector<std::unique_ptr<FCExprAST>> statements;

    while (m_curTok.kind != static_cast<int>(FCToken::tok_eof)) {
        if (m_curTok.kind == static_cast<int>(FCToken::tok_def)) {
            auto fn = parseDefinition();
            if (fn) {
                statements.push_back(std::move(fn));
                if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon))
                    advance();          // 容忍惯例收尾 ';'（"def ...}; expr"）
                continue;
            }
            return nullptr;             // ← 函数定义失败即整体失败
        }
        if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon)) {
            logError("empty statement at top level");
            return nullptr;             // ← 顶层裸 ';' 非法
        }
        if (auto stmt = parseSeqExpr()) {
            statements.push_back(std::move(stmt));
            continue;
        }
        return nullptr;                 // ← 不再空转
    }
    ...
}
```

```cpp
// Calculater/parser.cc:445-470 —— 序列：删掉 5-token 黑名单，只问"能否起始表达式"
std::unique_ptr<FCExprAST> Parser::parseSeqExpr() {
    auto first = parseExpression();
    if (!first) return nullptr;

    std::vector<std::unique_ptr<FCExprAST>> exprList;
    exprList.push_back(std::move(first));

    while (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon)) {
        advance();
        if (!startsExpression(m_curTok.kind))
            break;                   // 结构性 token → 自然终止，交还外层
        auto next = parseExpression();
        if (!next) return nullptr;    // 失败传播，不返回半截序列
        exprList.push_back(std::move(next));
    }
    return exprList.size() == 1 ? std::move(exprList[0])
                                : std::make_unique<FCSeqExprAST>(std::move(exprList));
}
```

删掉的黑名单是：

```cpp
// 旧版：手工枚举"什么之后必须停"
if (m_curTok.kind == tok_eof || m_curTok.kind == tok_def ||
    m_curTok.kind == tok_else || m_curTok.kind == tok_brace_close ||
    m_curTok.kind == tok_end)      // ← 而且 tok_end 从未被 Lexer 产生，是死枚举
    break;
```

黑名单的致命问题是**它必须穷举**：漏一个 `)` 就少一个停止条件。改成 `startsExpression()` 后语义反转——"能开始一个新表达式才继续"，漏项不会导致死循环，只会导致提前终止（可观测、可断言）。

### 3.8 检查清单

- [ ] 每个 `if (parseX()) { ... }` 都要回答：`else` 分支里 token 前进了没有？失败是谁终止的？
- [ ] 不要在同一个地方混用"返回值 + 全局 `m_hadError`"两个失败信号；用一个，另一个只做记录；
- [ ] 解析器里的"停止条件"应该是**肯定式白名单**（能继续的条件），而不是否定式黑名单（要停的条件）——黑名单的补集永远在扩张；
- [ ] 测试要覆盖"错误只报一次"，而不只是"有错误"。断言日志行数或 `hadError` 的幂等性。

---

## 4. 其他卡点速查

| # | 卡点 | 症状 | 修复 |
|---|------|------|------|
| 1 | `def` 后惯例收尾 `;` 被"裸 `;` 非法"规则误伤 | 大量 `def f() {...}; f()` 用例集体失败 | def 分支容忍**紧随的一个** `;`；其他位置的裸 `;` 仍报错 |
| 2 | `popFunctionScope` 曾用 `resize(anchor+1)` 裁剪 | 数组被**扩展**并填充 `null unique_ptr`，后续解引用空布局崩溃 | 改为 `resize(m_frameAnchor)`（只裁不扩） |
| 3 | Debug 断言失败弹框 | 无 stderr、进程"卡住"，与真死循环无法区分 | 先打"最后进入点"trace；关键路径用 `fprintf + return false` 代替 `assert` |
| 4 | `LNK1168` 无法写入 EXE | 上一轮超时被 KILL 的 `FunctionsUnitTest.exe` 残留占用 | `Stop-Process -Name FunctionsUnitTest` 后重建 |
| 5 | 管道下长输出被截断 | `\| Select-Object -Last N` 在运行中被杀时不回吐，看不到真正的错误行 | `Start-Process -RedirectStandardOutput/-Error` + `WaitForExit(ms)`，再读文件 |
| 6 | 模块测试基线被新模型打破 | `compiled - no functions` 断言 `functions.empty()`，而 `<global>` 记录常驻 | 改为 `size()==1` 且 `getFunction("<global>") != nullptr`（[`unit_test.cc:1976-1977`](../../Calculater/unit_test.cc#L1976-L1977)） |
| 7 | `evaluateCall` 零参幽灵调用绕过防御 | 0 参数函数跳过 `getFunction` 检查直接执行，本该 `Dangle` 却返回 `Integer` | 把 compiled-record 检查提到参数循环**之前** |
| 8 | 新增代码拉低覆盖率 | 95.35% → 94.98/94.94/94.99，卡在阈值下 | 逐分支补用例：独立 Lexer 套件、scanner facade 错误/复位、`binaryOpName` 全分支、块内裸 `;`/未闭合块、`bindVariable` null、ghost/unresolved parameter → 回到 **95.35%** |

---

## 5. 验证方式

```powershell
# 构建（避免管道问题：显式重定向）
cmd /c "C:\PROGRA~1\CMake\bin\cmake.exe --build build-debug --config Debug --target FunctionsUnitTest --parallel 16 > build.log 2>&1"

# 直接运行（拿到完整 PASS/FAIL 与退出码）
Start-Process -FilePath ".\build-debug\Calculater\Debug\FunctionsUnitTest.exe" `
    -NoNewWindow -Wait -PassThru `
    -RedirectStandardOutput ".\test.out" -RedirectStandardError ".\test.err"

# 覆盖率（阈值 95%）
pwsh -File .\tools\coverage.ps1
```

三个案例对应的回归测试位置：

| 案例 | 测试 | 位置 |
|------|------|------|
| 1 记录串用 | 互相递归 `isEven`/`isOdd` | [`unit_test.cc:538-540`](../../Calculater/unit_test.cc#L538-L540) |
| 2 锚点跨域 | 嵌套函数定义 + 记录独立性断言 | [`unit_test.cc:2461-2504`](../../Calculater/unit_test.cc#L2461-L2504) |
| 3 失败契约 | 顶层 `)` 报错一次、`def` 未闭合报错、裸 `;` 报错 | [`unit_test.cc`](../../Calculater/unit_test.cc) 中 `runParserTests` 结构性 token 终止套件 |

---

## 6. 三条可迁移的结论

1. **身份不要用名字表达**。匿名实体用下标/指针同一性寻址；字符串键只在"确实有名字、且名字稳定"时使用。占位名 + 事后改名的模式，在嵌套/复用下标时必然出错。
2. **一个数字不能同时代表两个深度**。作用域深度与帧深度是两个域，一旦共用变量，简单用例永远是对的，嵌套用例永远是错的。跨域时用结构体显式携带两个值，并加域一致性断言。
3. **失败路径必须有单一契约**。"返回值 + 全局错误标志"是双源状态，最容易写出"既不前进也不终止"的第三种情况。停止条件用肯定式白名单，失败即终止，测试断言"错误只报一次"。
