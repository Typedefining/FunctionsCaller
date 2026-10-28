#include <cassert>
#include <cstdio>

#include "semantic.h"
#include "language_options.h"
#include "symbol.h"

namespace FCMarks {

FCSemanticContext::FCSemanticContext() {
  reset();
}

FCSemanticContext::~FCSemanticContext() = default;

void FCSemanticContext::reset() {
  m_scopeStack.clear();
  m_functionSet.clear();
  m_frameLayouts.clear();
  m_frameRecords.clear();

  m_compiledProgram = std::make_shared<CompiledProgram>();

  // <global> 帧是作用域栈的普通成员（栈底），拥有自己的布局与编译记录；
  // 其记录同时登记进 functions 表，作为函数/全局归一后的单一存放地
  m_scopeStack.emplace_back();
  m_frameLayouts.push_back(std::make_shared<SlotLayout>());
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
  assert(m_frameRecords.size() == 1);
  assert(m_scopeStack.size() == 1);
  m_scopeStack.emplace_back();
  m_frameLayouts.push_back(std::make_shared<SlotLayout>());
  m_frameRecords.push_back(std::make_shared<CompiledFunction>());
}


void FCSemanticContext::popFunctionScope() {
  assert(m_frameRecords.size() == 2);
  m_frameRecords.pop_back();
  m_frameLayouts.pop_back();
  m_scopeStack.resize(1);               // 顶层定义 ⇒ 外层作用域恒为 <global>
}

bool FCSemanticContext::isCurrentFrameGlobal() const {
  return m_frameRecords.size() == 1;
}

bool FCSemanticContext::isInFuncDef() const {
  return m_scopeStack.size() > 1;
}

std::shared_ptr<CompiledFunction>
FCSemanticContext::currentFunction() const {
  return m_frameRecords.back();
}

std::shared_ptr<SlotLayout>
FCSemanticContext::currentFuncLayout() const {
  return m_frameLayouts.back();
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
    // 失败静默：报告职责在调用方（SemanticAnalyzer），此处只返回失败值
    return nullptr;
  }

  // 唯一分配路径：从当前帧基址的布局分配槽位（<global> 帧也是普通布局）
  const int slot = currentFuncLayout()->allocateSlot();

  const std::string name = declaration->name;
  auto symbol = std::make_shared<VariableSymbol>(std::move(declaration), slot,
                                                 scope.depth);
  symbol->runtime.frameIndex = isCurrentFrameGlobal() ? -1 : 0;
  scope.variables.emplace(name, symbol);

  // 唯一登记路径：登记进当前帧的编译记录（<global> 记录也存 functions 表）
  currentFunction()->symbols.addSymbol(name, symbol);
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
    return false;
  }

  auto compiledFunc = currentFunction();
  compiledFunc->ast = const_cast<FCFunctionAST *>(function);
  compiledFunc->frameSize = currentFuncLayout()->size();
  m_compiledProgram->functions[functionName] = compiledFunc;

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
  return currentFuncLayout()->size();
}

int FCSemanticContext::currentGlobalLayoutSize() const {
  return m_frameLayouts[0]->size();
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
  printf("Current function frame size: %d\n", currentFuncLayout()->size());
  printf("Current global layout size: %d\n", m_frameLayouts[0]->size());

  for (size_t i = 0; i < m_scopeStack.size(); ++i) {
    const auto &scope = m_scopeStack[i];

    printf(": %zu variables\n", scope.variables.size());
    for (const auto &pair : scope.variables) {
      const auto &symbol = *pair.second;

      if (!symbol.declaration)
        continue;

      const auto &decl = symbol.declaration;

      std::printf("  - %s: type=%s, slot=%d, depth=%d\n",
                  decl->name.c_str(), decl->typeName.c_str(), symbol.slot,
                  symbol.scopeDepth);
    }
  }
}

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

    printf("  %s: slot=%d, kind=%s, depth=%d\n", name.c_str(),
           symbol->slot, kind, symbol->scopeDepth);
  }
}

// ==================== SemanticAnalyzer ====================

FCSemanticContext::SemanticAnalyzer::SemanticAnalyzer(FCSemanticContext &ctx,
                                                       FC::FCDiagnostics &diagnostics)
    : m_ctx(ctx), m_diagnostics(diagnostics) {}

void FCSemanticContext::SemanticAnalyzer::reportError(const std::string &message) {
  m_diagnostics.add("semantic", {}, message);
}

bool FCSemanticContext::SemanticAnalyzer::analyze(const FCExprClass::FCExprAST *root) {
  if (!root) {
    return false;
  }
  if (!analyzeExpr(root)) {
    return false;
  }
  // Phase 1 成功后接 Phase 2：返回类型不动点；失败路径（上方）不变
  computeReturnTypes();
  // Phase 3：严格类型走查
  if (!checkTypes(root)) {
    return false;
  }
  return true;
}

bool FCSemanticContext::SemanticAnalyzer::analyzeExpr(const FCExprClass::FCExprAST *expr) {
  if (!expr) {
    return true;
  }

  DispatchVisitor v{ *this };
  return FCExprClass::dispatchAST(*expr, v);
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
    reportError("Error: variable '" + node->decl->name + "' already declared in current scope");
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
    reportError("Error: variable '" + node->name + "' is not declared");
    return false;
  }
  return m_ctx.bindVariable(node, symbol);
}

bool FCSemanticContext::SemanticAnalyzer::analyzeFor(const FCExprClass::FCForExprAST *node) {
  FCSemanticContext::ScopeGuard guard(m_ctx);

  auto symbol = m_ctx.declareVariable(node->getDecl());
  if (!symbol) {
    reportError("Error: variable '" + node->getDecl()->name + "' already declared in current scope");
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
  if (m_ctx.isInFuncDef()) {
    reportError("Error: nested function definition '" + node->getProtoName()
                + "' requires FC_ENABLE_NESTED_FUNCTIONS");
    return false;
  }

  FCSemanticContext::FunctionScopeGuard guard(m_ctx);

  // 先声明参数，分配帧内槽位（slot 从 0 开始累加）
  for (const auto &arg : node->getProto()->getArgs()) {
    auto symbol = m_ctx.declareVariable(arg);
    if (!symbol) {
      reportError("Error: variable '" + arg->name + "' already declared in current scope");
      return false;
    }
  }

  // 注册函数：此时 frameLayout 已反映参数个数，frameSize/parametersIdx 可正确填充
  if (!m_ctx.registerFunction(node->getProtoName(), node)) {
    reportError("Error: function '" + node->getProtoName() + "' already defined");
    return false;
  }

  // 分析函数体，局部变量继续分配帧槽位
  bool ok = analyzeExpr(node->getBody());

  // 函数体分析完成后，回填最终的帧大小（参数 + 局部）
  {
    auto fn = m_ctx.currentFunction();
    fn->frameSize = m_ctx.currentFuncLayout()->size();
  }

  if (!ok) {
    // 半成品记录会被后端当成完整函数消费，失败即撤销
    m_ctx.discardFunction(node->getProtoName());
    return false;
  }

  return true;
}

bool FCSemanticContext::SemanticAnalyzer::checkTypes(const FCExprClass::FCExprAST *expr) {
  if (!expr) return true;

  // 先递归子节点
  if (auto *program = dynamic_cast<const FCExprClass::FCProgramAST *>(expr)) {
    for (const auto &s : program->getStatements()) {
      if (!checkTypes(s.get())) return false;
    }
    return true;
  }
  if (auto *block = dynamic_cast<const FCExprClass::FCBlockExprAST *>(expr)) {
    for (const auto &e : block->getExpressions()) {
      if (!checkTypes(e.get())) return false;
    }
    return true;
  }
  if (auto *seq = dynamic_cast<const FCExprClass::FCSeqExprAST *>(expr)) {
    for (const auto &e : seq->getExpressions()) {
      if (!checkTypes(e.get())) return false;
    }
    return true;
  }
  if (auto *varDecl = dynamic_cast<const FCExprClass::FCVarDeclExprAST *>(expr)) {
    if (varDecl->initExpr) {
      if (!checkTypes(varDecl->initExpr.get())) return false;
      FCExprClass::FCTypeId initType = typeOfExpr(varDecl->initExpr.get(), m_callReturns);
      if (varDecl->decl && initType != varDecl->decl->type) {
        reportError("type mismatch: variable '" + varDecl->decl->name + "' expects " +
                    (varDecl->decl->type == FCExprClass::FCTypeId::Int ? std::string("int") :
                     varDecl->decl->type == FCExprClass::FCTypeId::Double ? std::string("double") :
                     varDecl->decl->type == FCExprClass::FCTypeId::String ? std::string("string") : std::string("error")) +
                    ", initializer has " +
                    (initType == FCExprClass::FCTypeId::Int ? std::string("int") :
                     initType == FCExprClass::FCTypeId::Double ? std::string("double") :
                     initType == FCExprClass::FCTypeId::String ? std::string("string") : std::string("error")));
        return false;
      }
    }
    return true;
  }
  if (auto *binary = dynamic_cast<const FCExprClass::FCBinaryExprAST *>(expr)) {
    if (binary->getOperator() == FCMarks::FCBinaryOp::Assign) {
      // LHS
      auto *lhs = binary->getLHS();
      auto *lhsVar = dynamic_cast<const FCExprClass::FCVariableExprAST *>(lhs);
      if (!lhsVar) {
        reportError("left side of assignment must be a variable");
        return false;
      }
      // recurse children
      if (!checkTypes(binary->getLHS())) return false;
      if (!checkTypes(binary->getRHS())) return false;
      auto symbol = m_ctx.boundSymbol(lhsVar);
      FCExprClass::FCTypeId lhsType = (symbol && symbol->declaration) ? symbol->declaration->type : FCExprClass::FCTypeId::Error;
      FCExprClass::FCTypeId rhsType = typeOfExpr(binary->getRHS(), m_callReturns);
      if (lhsType != rhsType) {
        reportError("type mismatch: variable '" + lhsVar->name + "' expects " +
                    (lhsType == FCExprClass::FCTypeId::Int ? std::string("int") :
                     lhsType == FCExprClass::FCTypeId::Double ? std::string("double") :
                     lhsType == FCExprClass::FCTypeId::String ? std::string("string") : std::string("error")) +
                    ", initializer has " +
                    (rhsType == FCExprClass::FCTypeId::Int ? std::string("int") :
                     rhsType == FCExprClass::FCTypeId::Double ? std::string("double") :
                     rhsType == FCExprClass::FCTypeId::String ? std::string("string") : std::string("error")));
        return false;
      }
      return true;
    }
    // recurse
    if (!checkTypes(binary->getLHS())) return false;
    if (!checkTypes(binary->getRHS())) return false;
    FCExprClass::FCTypeId l = typeOfExpr(binary->getLHS(), m_callReturns);
    FCExprClass::FCTypeId r = typeOfExpr(binary->getRHS(), m_callReturns);
    FCMarks::FCBinaryOp op = binary->getOperator();
    const bool numL = l == FCExprClass::FCTypeId::Int || l == FCExprClass::FCTypeId::Double;
    const bool numR = r == FCExprClass::FCTypeId::Int || r == FCExprClass::FCTypeId::Double;
    // determine validity
    bool ok = false;
    if (op == FCMarks::FCBinaryOp::Less) {
      ok = numL && numR && l == r;
    } else if (op == FCMarks::FCBinaryOp::Add) {
      if (l == FCExprClass::FCTypeId::String && r == FCExprClass::FCTypeId::String) ok = true;
      else if (numL && numR && l == r) ok = true;
      else ok = false;
    } else {
      // + - * / for numeric same type
      if (numL && numR && l == r) ok = true;
      else ok = false;
    }
    if (!ok) {
      reportError("type mismatch: '" + std::string(op == FCMarks::FCBinaryOp::Add ? "+" :
                                                   op == FCMarks::FCBinaryOp::Sub ? "-" :
                                                   op == FCMarks::FCBinaryOp::Mul ? "*" :
                                                   op == FCMarks::FCBinaryOp::Div ? "/" :
                                                   op == FCMarks::FCBinaryOp::Less ? "<" : "?") + "' requires ...");
      return false;
    }
    return true;
  }
  if (auto *ifNode = dynamic_cast<const FCExprClass::FCIfExprAST *>(expr)) {
    if (ifNode->getCondition()) {
      if (!checkTypes(ifNode->getCondition())) return false;
    }
    if (ifNode->getThen()) {
      if (!checkTypes(ifNode->getThen())) return false;
    }
    if (ifNode->getElse()) {
      if (!checkTypes(ifNode->getElse())) return false;
    }
    // condition numeric
    FCExprClass::FCTypeId cond = typeOfExpr(ifNode->getCondition(), m_callReturns);
    if (!(cond == FCExprClass::FCTypeId::Int || cond == FCExprClass::FCTypeId::Double)) {
      reportError("if condition must be numeric");
      return false;
    }
    FCExprClass::FCTypeId t = typeOfExpr(ifNode->getThen(), m_callReturns);
    if (ifNode->getElse()) {
      FCExprClass::FCTypeId e = typeOfExpr(ifNode->getElse(), m_callReturns);
      if (t != e) {
        reportError("if branch types do not match");
        return false;
      }
    }
    return true;
  }
  if (auto *forNode = dynamic_cast<const FCExprClass::FCForExprAST *>(expr)) {
    if (forNode->getStart()) {
      if (!checkTypes(forNode->getStart())) return false;
    }
    if (forNode->getEnd()) {
      if (!checkTypes(forNode->getEnd())) return false;
    }
    if (forNode->getStep()) {
      if (!checkTypes(forNode->getStep())) return false;
    }
    if (forNode->getBody()) {
      if (!checkTypes(forNode->getBody())) return false;
    }
    // start/end/step all int (null step defaults to int 1, treated as int)
    FCExprClass::FCTypeId st = forNode->getStart() ? typeOfExpr(forNode->getStart(), m_callReturns) : FCExprClass::FCTypeId::Int;
    FCExprClass::FCTypeId en = forNode->getEnd() ? typeOfExpr(forNode->getEnd(), m_callReturns) : FCExprClass::FCTypeId::Int;
    FCExprClass::FCTypeId sp = forNode->getStep() ? typeOfExpr(forNode->getStep(), m_callReturns) : FCExprClass::FCTypeId::Int;
    if (st != FCExprClass::FCTypeId::Int) {
      reportError("for start must be int");
      return false;
    }
    // end condition must be numeric (int or double); evaluator allows double end condition
    if (!(en == FCExprClass::FCTypeId::Int || en == FCExprClass::FCTypeId::Double)) {
      reportError("for end condition must be numeric");
      return false;
    }
    if (sp != FCExprClass::FCTypeId::Int) {
      reportError("for step must be int");
      return false;
    }
    return true;
  }
  if (auto *call = dynamic_cast<const FCExprClass::FCCallExprAST *>(expr)) {
    for (const auto &a : call->getArgs()) {
      if (!checkTypes(a.get())) return false;
    }
    // 参数类型严格匹配（Phase 3，m_callReturns 已就绪）
    auto *fn = m_ctx.getCompiledFunction(call->getName());
    if (fn == nullptr) {
      // 函数未注册：不拦截，交给 evaluator/codegen 阶段（前向引用合法）
      return true;
    }
    if (fn->ast == nullptr) {
      return true;
    }
    const auto &args = fn->ast->getProto()->getArgs();
    if (args.size() != call->getArgs().size()) {
      return true;
    }
    for (size_t i = 0; i < args.size(); ++i) {
      FCExprClass::FCTypeId argType = typeOfExpr(call->getArgs()[i].get(), m_callReturns);
      FCExprClass::FCTypeId paramType = args[i]->type;
      if (argType != paramType) {
        reportError("argument type mismatch for " + call->getName());
        return false;
      }
    }
    return true;
  }
  if (auto *fn = dynamic_cast<const FCExprClass::FCFunctionAST *>(expr)) {
    if (fn->getBody()) {
      if (!checkTypes(fn->getBody())) return false;
    }
    return true;
  }
  // number/string/variable leaf
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

// ==================== Phase 2：返回类型不动点 ====================

namespace {
using TypeId = FCExprClass::FCTypeId;
}

FCExprClass::TypeId FCSemanticContext::SemanticAnalyzer::typeOfExpr(
    const FCExprClass::FCExprAST *expr,
    const std::unordered_map<std::string, FCExprClass::FCTypeId> &callReturns) {
  if (expr == nullptr) {
    return TypeId::Error;
  }

  if (auto *number = dynamic_cast<const FCExprClass::FCNumberExprAST *>(expr)) {
    return number->isFloating() ? TypeId::Double : TypeId::Int;
  }

  if (dynamic_cast<const FCExprClass::FCStringExprAST *>(expr) != nullptr) {
    return TypeId::String;
  }

  if (auto *variable = dynamic_cast<const FCExprClass::FCVariableExprAST *>(expr)) {
    auto symbol = m_ctx.boundSymbol(variable);
    if (symbol == nullptr || symbol->declaration == nullptr) {
      return TypeId::Error;
    }
    return symbol->declaration->type;
  }

  if (auto *decl = dynamic_cast<const FCExprClass::FCVarDeclExprAST *>(expr)) {
    if (decl->decl == nullptr) {
      return TypeId::Error;
    }
    return decl->decl->type;
  }

  if (auto *binary = dynamic_cast<const FCExprClass::FCBinaryExprAST *>(expr)) {
    if (binary->getOperator() == FCMarks::FCBinaryOp::Assign) {
      // 赋值表达式取左部符号的声明类型
      auto *lhs = binary->getLHS();
      auto *lhsVar = dynamic_cast<const FCExprClass::FCVariableExprAST *>(lhs);
      if (lhsVar == nullptr) {
        return TypeId::Error;
      }
      auto symbol = m_ctx.boundSymbol(lhsVar);
      if (symbol == nullptr || symbol->declaration == nullptr) {
        return TypeId::Error;
      }
      return symbol->declaration->type;
    }

    TypeId lhs = typeOfExpr(binary->getLHS(), callReturns);
    TypeId rhs = typeOfExpr(binary->getRHS(), callReturns);
    const bool numericLhs = lhs == TypeId::Int || lhs == TypeId::Double;
    const bool numericRhs = rhs == TypeId::Int || rhs == TypeId::Double;

    if (binary->getOperator() == FCMarks::FCBinaryOp::Less) {
      // 同型数值比较 → Int；其余（含混合数值、String）→ Error
      return (numericLhs && lhs == rhs) ? TypeId::Int : TypeId::Error;
    }

    if (binary->getOperator() == FCMarks::FCBinaryOp::Add && lhs == TypeId::String &&
        rhs == TypeId::String) {
      return TypeId::String;
    }

    // 数值四则：两侧同型数值取该型；混合数值与其它组合 → Error
    if (numericLhs && numericRhs && lhs == rhs) {
      return lhs;
    }
    return TypeId::Error;
  }

  if (auto *conditional = dynamic_cast<const FCExprClass::FCIfExprAST *>(expr)) {
    TypeId cond = typeOfExpr(conditional->getCondition(), callReturns);
    const bool numericCond = cond == TypeId::Int || cond == TypeId::Double;
    TypeId thenType = typeOfExpr(conditional->getThen(), callReturns);
    TypeId elseType = typeOfExpr(conditional->getElse(), callReturns);
    if (!numericCond) {
      return TypeId::Error;
    }
    if (thenType == elseType) {
      return thenType;
    }
    // 分支不一致：数值域内沿用 codegen 旧路径取 double 优先的结果
    // （严格同型分支报告属 Phase 3 走查；Phase 2 乐观迭代不得把
    //   double 自递归毒化为 Error——见 Review Focus 2）
    const bool numericThen = thenType == TypeId::Int || thenType == TypeId::Double;
    const bool numericElse = elseType == TypeId::Int || elseType == TypeId::Double;
    if (numericThen && numericElse) {
      return (thenType == TypeId::Double || elseType == TypeId::Double)
                 ? TypeId::Double
                 : TypeId::Int;
    }
    return TypeId::Error;
  }

  if (auto *loop = dynamic_cast<const FCExprClass::FCForExprAST *>(expr)) {
    return typeOfExpr(loop->getBody(), callReturns);
  }

  if (auto *block = dynamic_cast<const FCExprClass::FCBlockExprAST *>(expr)) {
    if (block->getExpressions().empty()) {
      return TypeId::Error;
    }
    return typeOfExpr(block->getExpressions().back().get(), callReturns);
  }

  if (auto *sequence = dynamic_cast<const FCExprClass::FCSeqExprAST *>(expr)) {
    if (sequence->getExpressions().empty()) {
      return TypeId::Error;
    }
    return typeOfExpr(sequence->getExpressions().back().get(), callReturns);
  }

  if (auto *call = dynamic_cast<const FCExprClass::FCCallExprAST *>(expr)) {
    auto found = callReturns.find(call->getName());
    return found != callReturns.end() ? found->second : TypeId::Error;
  }

  if (dynamic_cast<const FCExprClass::FCFunctionAST *>(expr) != nullptr) {
    // 定义出现在值位置：语法不产生，手工树兜底取 Int（与 codegen 旧谎报一致）
    return TypeId::Int;
  }

  return TypeId::Error;
}

void FCSemanticContext::SemanticAnalyzer::computeReturnTypes() {
  auto &program = *m_ctx.m_compiledProgram;

  // 乐观初值 Int：<global>（ast 为空）天然排除
  std::unordered_map<std::string, FCExprClass::FCTypeId> candidates;
  for (const auto &[name, record] : program.functions) {
    if (record != nullptr && record->ast != nullptr) {
      candidates[name] = TypeId::Int;
    }
  }

  // ponytail: 固定 4 轮 = |FCExprClass::FCTypeId|；不做收敛判定，
  // 残余不一致由 Phase 3 走查报告。若引入函数类型/多态，改按调用图拓扑序求解。
  constexpr int kFixpointRounds = 4;
  for (int round = 0; round < kFixpointRounds; ++round) {
    for (const auto &[name, record] : program.functions) {
      if (record == nullptr || record->ast == nullptr) {
        continue;
      }
      candidates[name] = typeOfExpr(record->ast->getBody(), candidates);
    }
  }

  m_callReturns = candidates;
  for (auto &[name, record] : program.functions) {
    if (record != nullptr) {
      record->returnType = record->ast != nullptr
                              ? candidates[name]
                              : TypeId::Error;
    }
  }
}

const SemanticBinding &
FCSemanticContext::semanticBinding() const {
  return *m_compiledProgram->m_binding;
}

} // namespace FCMarks



