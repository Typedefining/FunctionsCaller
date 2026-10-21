#include <cassert>
#include <cstdio>

#include "semantic.h"
#include "language_options.h"
#include "token.h"

namespace FCMarks {

FCSemanticContext::FCSemanticContext() {
  reset();
}

FCSemanticContext::~FCSemanticContext() = default;

void FCSemanticContext::reset() {
  m_scopeStack.clear();
  m_functionSet.clear();
  m_frameAnchor = 0;
  m_frameLayouts.clear();
  m_frameRecords.clear();
  m_frameAnchorStack.clear();

  m_compiledProgram = std::make_shared<CompiledProgram>();
  m_currentState = CurrentCompilationState();

  // <global> 帧是作用域栈的普通成员（栈底），拥有自己的布局与编译记录；
  // 其记录同时登记进 functions 表，作为函数/全局归一后的单一存放地
  m_scopeStack.emplace_back();
  m_frameLayouts.push_back(std::make_unique<FrameLayout>());
  m_frameRecords.push_back(std::make_shared<CompiledFunction>());
  m_compiledProgram->functions["<global>"] = m_frameRecords[0];
}

void FCSemanticContext::pushScope() {
  m_scopeStack.emplace_back(Scope{
      .variables = {}, .depth = static_cast<int>(m_scopeStack.size() + 1)});
}

void FCSemanticContext::popScope() {
  assert(!m_scopeStack.empty());
  m_scopeStack.pop_back();
}

void FCSemanticContext::pushFunctionScope() {
  // 关闭嵌套定义时，帧记录栈至多只有 <global> 与当前函数两层；
  // 有人绕过 analyzer 直接压第二个函数帧，Debug 下立刻暴露
  assert(kNestedFunctionDefinitions || m_frameAnchor == 0);

  // 帧基址走帧计数域（layout/records 的下标），与作用域栈深度解耦；
  // 函数体自带的作用域压在记录的 scopeDepthAtPush+1 位置
  const size_t scopeDepthAtPush = m_scopeStack.size();
  m_frameAnchorStack.push_back({m_frameAnchor, scopeDepthAtPush});
  m_frameAnchor = m_frameRecords.size();

  m_scopeStack.emplace_back();
  m_frameLayouts.push_back(std::make_unique<FrameLayout>());
  m_frameRecords.push_back(std::make_shared<CompiledFunction>());

  m_currentState.isActive = true;
}

void FCSemanticContext::popFunctionScope() {
  assert(m_frameRecords.size() == m_frameAnchor + 1);
  assert(m_frameAnchorStack.size() >= 1);
  // 弹出本帧：作用域栈裁回进入前深度，帧级向量裁到锚点（本帧条目移除），
  // 再从帧基址栈恢复外层帧基址（嵌套函数定义由此支持）
  const auto entry = m_frameAnchorStack.back();
  m_frameAnchorStack.pop_back();
  m_scopeStack.resize(entry.scopeDepthAtPush);
  m_frameLayouts.resize(m_frameAnchor);
  m_frameRecords.resize(m_frameAnchor);
  m_frameAnchor = entry.prevAnchor;

  m_currentState.isActive = (m_frameAnchor != 0);
}

bool FCSemanticContext::isInFunctionScope() const {
  return m_currentState.isActive;
}

bool FCSemanticContext::isCurrentFrameGlobal() const {
  // 肯定式谓词：锚点即 <global> 帧
  return m_frameAnchor == 0;
}

CompiledFunction &FCSemanticContext::currentFrameFunction() const {
  return *m_frameRecords[m_frameAnchor];
}

std::shared_ptr<CompiledFunction>
FCSemanticContext::currentFrameRecord() const {
  return m_frameRecords[m_frameAnchor];
}

void FCSemanticContext::discardFunction(const std::string &functionName) {
  m_compiledProgram->functions.erase(functionName);
  m_functionSet.erase(functionName);
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupVariableShared(const std::string &name) const {
  for (auto it = m_scopeStack.rbegin(); it != m_scopeStack.rend(); ++it) {
    auto found = it->variables.find(name);
    if (found != it->variables.end())
      return found->second;
  }
  return nullptr;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupVariableInCurrentScope(const std::string &name) const {
  if (m_scopeStack.empty())
    return nullptr;

  auto &scope = m_scopeStack.back();

  auto it = scope.variables.find(name);

  if (it == scope.variables.end())
    return nullptr;

  return it->second;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::lookupGlobalVariable(const std::string &name) const {
  // 单一查找路径：全局变量登记在 <global> 帧记录的符号表
  // （globalSymbols 向量仅作为槽位序兼容视图保留）
  auto it = m_compiledProgram->functions.find("<global>");
  if (it == m_compiledProgram->functions.end())
    return nullptr;
  return it->second->symbols.lookupShared(name);
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::declareVariable(VarDeclPtr declaration) {
  assert(declaration != nullptr);
  assert(!m_scopeStack.empty());

  // 变量记录在当前作用域（栈顶），保持块作用域 RAII 语义
  Scope &scope = m_scopeStack.back();

  if (scope.variables.find(declaration->name) != scope.variables.end()) {
    fprintf(stderr, "Error: variable '%s' already declared in current scope\n",
            declaration->name.c_str());
    return nullptr;
  }

  // 唯一分配路径：从当前帧基址的布局分配槽位（<global> 帧也是普通布局）
  const int slot = m_frameLayouts[m_frameAnchor]->allocateSlot();

  const std::string name = declaration->name;
  auto symbol = std::make_shared<VariableSymbol>(std::move(declaration), slot,
                                                 scope.depth);
  symbol->runtime.frameIndex = isCurrentFrameGlobal() ? -1 : 0;
  scope.variables.emplace(name, symbol);

  // 唯一登记路径：登记进当前帧的编译记录（<global> 记录也存 functions 表）
  currentFrameFunction().symbols.addSymbol(name, symbol);
  if (isCurrentFrameGlobal()) {
    m_compiledProgram->globalSymbols.push_back(symbol);
  }


  return symbol;
}

bool FCSemanticContext::hasFunction(const std::string &functionName) const {
  return m_functionSet.find(functionName) != m_functionSet.end();
}

bool FCSemanticContext::registerFunction(const std::string &functionName,
                                         const FCFunctionAST *function) {
  if (hasFunction(functionName)) {
    fprintf(stderr, "Error: function '%s' already defined\n",
            functionName.c_str());
    return false;
  }

  // 锚点帧记录（含已声明的参数符号）整体移交给 functions 表：
  // shared_ptr 同一对象，declareVariable 的登记与后端消费天然一致
  auto compiledFunc = currentFrameRecord();
  compiledFunc->ast = const_cast<FCFunctionAST *>(function);
  m_compiledProgram->functions[functionName] = compiledFunc;

  // 布局快照：注册时刻的参数槽位数；函数体分析结束后由
  // SemanticAnalyzer::analyzeFunction 回填最终 frameSize
  compiledFunc->frameSize = m_frameLayouts[m_frameAnchor]->frameSize();
  compiledFunc->maxTempSlots =
      m_frameLayouts[m_frameAnchor]->frameSize(); // 暂与 frameSize 相同

  // 按原型声明顺序记录形参槽位，调用期据此把实参写入帧的固定位置
  if (function != nullptr) {
    for (const auto &varDecl : function->getProto()->getArgs()) {
      auto param = compiledFunc->symbols.lookupShared(varDecl->name);
      if (param) {
        compiledFunc->parametersIdx.push_back(param->slot);
      }
    }
  }

  m_functionSet.insert(functionName);
  return true;
}

const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> &
FCSemanticContext::currentScopeDeclarations() const {
  return m_scopeStack.back().variables;
}

int FCSemanticContext::currentFrameLayoutSize() const {
  return m_frameLayouts[m_frameAnchor]->frameSize();
}

int FCSemanticContext::currentGlobalLayoutSize() const {
  return m_frameLayouts[0]->frameSize();
}

bool FCSemanticContext::mayDefineFunctionHere() const {
  // <global> 帧上定义函数始终合法；函数体内定义只有在开启嵌套时才合法
  return kNestedFunctionDefinitions || m_frameAnchor == 0;
}

const CompiledProgram &FCSemanticContext::getCompiledProgram() const {
  return *m_compiledProgram;
}

const CompiledFunction *
FCSemanticContext::getCompiledFunction(const std::string &name) const {
  return m_compiledProgram->getFunction(name);
}

void FCSemanticContext::dumpScopes() const {
  printf("=== Scope Debug Info ===\n");
  printf("Scope depth: %zu\n", m_scopeStack.size());
  printf("Current function frame size: %d\n", m_frameLayouts[m_frameAnchor]->frameSize());
  printf("Current global layout size: %d\n", m_frameLayouts[0]->frameSize());

  for (size_t i = 0; i < m_scopeStack.size(); ++i) {
    const auto &scope = m_scopeStack[i];

    printf(": %zu variables\n", scope.variables.size());
    for (const auto &pair : scope.variables) {
      const auto &symbol = *pair.second;

      if (!symbol.declaration)
        continue;

      const auto &decl = symbol.declaration;

      std::printf("  - %s: type=%s, slot=%d, depth=%d, mutable=%s\n",
                  decl->name.c_str(), decl->typeName.c_str(), symbol.slot,
                  symbol.scopeDepth, symbol.isMutable ? "yes" : "no");
    }
  }
}

bool SymbolTable::addSymbol(const std::string &name,
                            std::shared_ptr<VariableSymbol> symbol) {
  bool inserted = m_symbols.find(name) == m_symbols.end();
  if (!inserted) {
    // 不允许重定义同名符号
    return false;
  }
  m_symbols.insert_or_assign(name, std::move(symbol));
  return inserted;
}

std::shared_ptr<VariableSymbol>
SymbolTable::lookupShared(const std::string &name) const {
  auto it = m_symbols.find(name);
  return it != m_symbols.end() ? it->second : nullptr;
}

bool SymbolTable::removeSymbol(const std::string &name) {
  return m_symbols.erase(name) > 0;
}

const std::unordered_map<std::string, std::shared_ptr<VariableSymbol>> &
SymbolTable::getAllSymbols() const {
  return m_symbols;
}

void SymbolTable::clear() { m_symbols.clear(); }

size_t SymbolTable::size() const { return m_symbols.size(); }

bool FCSemanticContext::bindVariable(const void *astNode,
                                     std::shared_ptr<VariableSymbol> symbol) {
  if (astNode == nullptr || symbol == nullptr)
    return false;

  return m_compiledProgram->m_binding->bind(astNode, std::move(symbol));
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::boundSymbol(const void *astNode) const {
  return m_compiledProgram->m_binding->find(astNode);
}

// 调试输出
void SymbolTable::dump() const {
  printf("Symbol Table (%zu symbols):\n", m_symbols.size());
  for (const auto &[name, symbol] : m_symbols) {
    const char *kind = symbol->slot < 0 ? "Unassigned"
                                        : "Local"; // 符号表仅容纳函数域符号

    printf("  %s: slot=%d, kind=%s, depth=%d, mutable=%s\n", name.c_str(),
           symbol->slot, kind, symbol->scopeDepth,
           symbol->isMutable ? "yes" : "no");
  }
}

// ==================== SemanticAnalyzer ====================

FCSemanticContext::SemanticAnalyzer::SemanticAnalyzer(FCSemanticContext &ctx)
    : m_ctx(ctx) {}

bool FCSemanticContext::SemanticAnalyzer::analyze(const FCExprClass::FCExprAST *root) {
  if (!root) {
    return false;
  }
  return analyzeExpr(root);
}

bool FCSemanticContext::SemanticAnalyzer::analyzeExpr(const FCExprClass::FCExprAST *expr) {
  if (!expr) {
    return true;
  }

  if (auto *n = dynamic_cast<const FCExprClass::FCProgramAST *>(expr))
    return analyzeProgram(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCBlockExprAST *>(expr))
    return analyzeBlock(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCSeqExprAST *>(expr))
    return analyzeSeq(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCVarDeclExprAST *>(expr))
    return analyzeVarDecl(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCVariableExprAST *>(expr))
    return analyzeVariable(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCForExprAST *>(expr))
    return analyzeFor(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCIfExprAST *>(expr))
    return analyzeIf(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCCallExprAST *>(expr))
    return analyzeCall(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCBinaryExprAST *>(expr))
    return analyzeBinary(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCFunctionAST *>(expr))
    return analyzeFunction(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCNumberExprAST *>(expr))
    return analyzeNumber(n);
  if (auto *n = dynamic_cast<const FCExprClass::FCStringExprAST *>(expr))
    return analyzeString(n);

  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeProgram(const FCExprClass::FCProgramAST *node) {
  for (const auto &stmt : node->getStatements()) {
    if (!analyzeExpr(stmt.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeBlock(const FCExprClass::FCBlockExprAST *node) {
  FCSemanticContext::ScopeGuard guard(m_ctx);
  for (const auto &expr : node->getExpressions()) {
    if (!analyzeExpr(expr.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeSeq(const FCExprClass::FCSeqExprAST *node) {
  for (const auto &expr : node->getExpressions()) {
    if (!analyzeExpr(expr.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeVarDecl(const FCExprClass::FCVarDeclExprAST *node) {
  auto symbol = m_ctx.declareVariable(node->decl);
  if (!symbol) {
    return false;
  }
  m_ctx.bindVariable(node, symbol);
  if (node->initExpr) {
    return analyzeExpr(node->initExpr.get());
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeVariable(const FCExprClass::FCVariableExprAST *node) {
  auto symbol = resolveVariable(node->name);
  if (!symbol) {
    fprintf(stderr, "Error: variable '%s' is not declared\n", node->name.c_str());
    return false;
  }
  return m_ctx.bindVariable(node, symbol);
}

bool FCSemanticContext::SemanticAnalyzer::analyzeFor(const FCExprClass::FCForExprAST *node) {
  FCSemanticContext::ScopeGuard guard(m_ctx);

  auto symbol = m_ctx.declareVariable(node->getDecl());
  if (!symbol) {
    return false;
  }
  m_ctx.bindVariable(node, symbol);

  if (!analyzeExpr(node->getStart())) return false;
  if (!analyzeExpr(node->getEnd())) return false;
  if (!analyzeExpr(node->getStep())) return false;
  if (!analyzeExpr(node->getBody())) return false;
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeIf(const FCExprClass::FCIfExprAST *node) {
  if (!analyzeExpr(node->getCondition())) return false;
  if (!analyzeExpr(node->getThen())) return false;
  if (node->getElse()) {
    if (!analyzeExpr(node->getElse())) return false;
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeCall(const FCExprClass::FCCallExprAST *node) {
  for (const auto &arg : node->getArgs()) {
    if (!analyzeExpr(arg.get())) {
      return false;
    }
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeBinary(const FCExprClass::FCBinaryExprAST *node) {
  if (!analyzeExpr(node->getLHS())) return false;
  if (!analyzeExpr(node->getRHS())) return false;
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeFunction(const FCExprClass::FCFunctionAST *node) {
  // 守卫必须在压帧与声明参数之前：被拒绝时不得留下任何帧副作用
  if (!m_ctx.mayDefineFunctionHere()) {
    fprintf(stderr,
            "Error: nested function definition '%s' requires "
            "FC_ENABLE_NESTED_FUNCTIONS\n",
            node->getProtoName().c_str());
    return false;
  }

  FCSemanticContext::FunctionScopeGuard guard(m_ctx);

  // 先声明参数，分配帧内槽位（slot 从 0 开始累加）
  for (const auto &arg : node->getProto()->getArgs()) {
    auto symbol = m_ctx.declareVariable(arg);
    if (!symbol) {
      return false;
    }
  }

  // 注册函数：此时 frameLayout 已反映参数个数，frameSize/parametersIdx 可正确填充
  if (!m_ctx.registerFunction(node->getProtoName(), node)) {
    return false;
  }

  // 分析函数体，局部变量继续分配帧槽位
  bool ok = analyzeExpr(node->getBody());

  // 函数体分析完成后，回填最终的帧大小（参数 + 局部）
  {
    auto &fn = m_ctx.currentFrameFunction();
    fn.frameSize = m_ctx.m_frameLayouts[m_ctx.m_frameAnchor]->frameSize();
    fn.maxTempSlots = fn.frameSize;
  }

  if (!ok) {
    // 半成品记录会被后端当成完整函数消费，失败即撤销
    m_ctx.discardFunction(node->getProtoName());
    return false;
  }

  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeNumber(const FCExprClass::FCNumberExprAST *) {
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeString(const FCExprClass::FCStringExprAST *) {
  return true;
}

std::shared_ptr<VariableSymbol>
FCSemanticContext::SemanticAnalyzer::resolveVariable(const std::string &name) {
  auto symbol = m_ctx.lookupVariableShared(name);
  if (!symbol) {
    return nullptr;
  }

  // 作用域栈连续，内层函数因此能看到外层函数的局部变量与全局变量。
  // 这里只做名字解析；嵌套函数是否可执行由求值器与 codegen 决定，
  // 它们一律拒绝在帧内执行函数定义，捕获不会发生在运行期。
  return symbol;
}

const SemanticBinding &
FCSemanticContext::semanticBinding() const {
  return *m_compiledProgram->m_binding;
}

} // namespace FCMarks



