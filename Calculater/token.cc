#include "token.h"
#include <iostream>
#include <numeric>
#include <unordered_set>
#include "llvm/Support/raw_ostream.h"

using namespace FCExprClass;

namespace FCMarks {
	::std::map<char, int> binopPrecedence =
	{
		{'=', 5},
		{'<',9},
		{'+',10},
		{'-',10},
		{'*',20},
		{'/',20}
	};
	// 解析期全局结构：每个函数名对应一个 ScopeStack（解析时使用）
	std::unordered_map<std::string, ScopeStack> g_varTableInFunc;
	// 每个函数的所有声明（按出现顺序），用于分配 slot
	std::unordered_map<std::string, std::vector<VarDeclPtr>> g_funcDeclList;
	// 函数每次调用需要多少 local slots
	std::unordered_map<std::string, int> g_funcLocalCount;

	void pushScopeForFunc(const std::string &func)
	{
		g_varTableInFunc[func].emplace_back();
	}
	void popScopeForFunc(const std::string &func)
	{
		auto &stack = g_varTableInFunc[func];
		if (!stack.empty()) stack.pop_back();
	}
	VarDeclPtr lookupVariableDecl(const std::string &func, const std::string &name)
	{
		auto fit = g_varTableInFunc.find(func);
		if (fit == g_varTableInFunc.end()) return nullptr;
		auto &stack = fit->second;
		for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
			auto f = it->find(name);
			if (f != it->end()) return f->second;
		}
		return nullptr;
	}
	void insertVariableInCurrentScope(const std::string &func, const std::string &name, VarDeclPtr decl)
	{
		auto &stack = g_varTableInFunc[func];
		assert(!stack.empty() && "must have a scope before insert");
		stack.back()[name] = decl;
		g_funcDeclList[func].push_back(decl);
	}

}

namespace {
FCValue makeDangleValue()
{
	FCValue value{};
	value.type = FCValueCategory::Dangle;
	value.evaluteVal.danglingVal = nullptr;
	return value;
}
}

bool FCFunctionRegistry::registerFunction(FCFunctionAST* function)
{
	if (function == nullptr)
		return false;

	const auto name = function->getProtoName();
	auto it = m_functions.find(name);
	if (it != m_functions.end())
		return it->second == function;

	m_functions.emplace(name, function);
	return true;
}

FCFunctionAST* FCFunctionRegistry::findFunction(const std::string& name) const
{
	auto it = m_functions.find(name);
	return it == m_functions.end() ? nullptr : it->second;
}

void FCFunctionRegistry::clear()
{
	m_functions.clear();
}

bool FCFunctionRegistry::index(FCExprAST* root)
{
	if (root == nullptr)
		return false;

	bool success = true;
	if (auto* function = dynamic_cast<FCFunctionAST*>(root))
		return registerFunction(function);

	if (auto* program = dynamic_cast<FCProgramAST*>(root))
	{
		for (const auto& statement : program->getStatements())
		{
			if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
				success = registerFunction(function) && success;
		}
	}

	return success;
}

void FCEvaluationContext::pushFrame(const std::string& functionName)
{
	Frame frame;
	frame.funcName = functionName;

	auto it = g_funcLocalCount.find(functionName);
	if (it != g_funcLocalCount.end())
		frame.locals.resize(it->second);

	callStack.push_back(std::move(frame));
}

void FCEvaluationContext::popFrame()
{
	assert(!callStack.empty());
	callStack.pop_back();
}

Frame& FCEvaluationContext::currentFrame()
{
	assert(!callStack.empty());
	return callStack.back();
}


FCNumberExprAST::FCNumberExprAST(int val) : m_intVal(val)
{
	m_exprVal.evaluteVal.intVal = m_intVal;
	m_exprVal.type = FCValueCategory::Integer;
	m_doubleVal = 0;
}
FCNumberExprAST::FCNumberExprAST(double val) : m_doubleVal(val)
{
	m_exprVal.evaluteVal.doubleVal = m_doubleVal;
	m_exprVal.type = FCValueCategory::Floating;
	m_intVal = 0;
}
FCNumberExprAST::~FCNumberExprAST()
{
}
void FCNumberExprAST::info()
{
	if (m_exprVal.type == FCValueCategory::Integer)
		::std::cout << "FCNumberExprAST Val is Integer: " << m_intVal << ::std::endl;
	else
		::std::cout << "FCNumberExprAST Val is Floating: " << m_doubleVal << ::std::endl;
	return;
}

FCValue FCNumberExprAST::evaluate(FCEvaluationContext&)
{
	return m_exprVal;
}

FCStringExprAST::FCStringExprAST(std::string val) : m_stringVal(val)
{
	memset(m_exprVal.evaluteVal.charVal, 0, 1024);
	memcpy(m_exprVal.evaluteVal.charVal, m_stringVal.c_str(), m_stringVal.size());
	m_exprVal.type = FCValueCategory::String;
}
FCStringExprAST::~FCStringExprAST()
{
}
void FCStringExprAST::info()
{
	::std::cout << "FCStringExprAST Val: " << m_exprVal.evaluteVal.charVal << ::std::endl;
}

FCValue FCStringExprAST::evaluate(FCEvaluationContext&)
{
	return m_exprVal;
}

FCVariableExprAST::FCVariableExprAST(VarDeclPtr d) : decl(std::move(d))
{
	m_exprVal.evaluteVal.danglingVal = nullptr;
	m_exprVal.type = FCMarks::FCValueCategory::Dangle;
}

FCVariableExprAST::~FCVariableExprAST()
{
}

void FCVariableExprAST::info()
{
	::std::cout << "FCVariableExprAST (decl) Name: " << decl->name << " slot=" << decl->slot << ::std::endl;
	if (m_exprVal.type == FCValueCategory::Dangle)
	{
		::std::cout << " Has no FCValue" << ::std::endl;
	}
	else {

	}
}

FCValue FCVariableExprAST::evaluate(FCEvaluationContext& context)
{
	if (!decl) {
		FCMarks::FCValue r; r.evaluteVal.danglingVal = nullptr; r.type = FCMarks::FCValueCategory::Dangle; return r;
	}
	if (decl->slot < 0) {
	// slot 未分配 — 说明 parse/codegen 流程有问题
		FCMarks::FCValue r; r.evaluteVal.danglingVal = nullptr; r.type = FCMarks::FCValueCategory::Dangle; return r;
	}
	// 通过当前帧访问
	Frame &fr = context.currentFrame();
	if (decl->slot >= (int)fr.locals.size()) {
		FCMarks::FCValue r; r.evaluteVal.danglingVal = nullptr; r.type = FCMarks::FCValueCategory::Dangle; return r;
	}
	return fr.locals[decl->slot];
}

FCBinaryExprAST::FCBinaryExprAST(char op,
	::std::unique_ptr<FCExprAST> lhs,
	::std::unique_ptr<FCExprAST> rhs)
	: m_Op(op), mup_LHS(::std::move(lhs)), mup_RHS(::std::move(rhs))
{
	m_exprVal = {};
}
FCBinaryExprAST::~FCBinaryExprAST()
{
}

void FCBinaryExprAST::info()
{
	::std::cout << "FCBinaryExprAST Op: " << m_Op << " LHS: ";
	mup_LHS->info();
	::std::cout << " RHS: " << ::std::endl;
	mup_RHS->info();
}

FCValue FCBinaryExprAST::assignExpression(
	FCValue lhs_eva,
	FCValue rhs_eva,
	FCEvaluationContext& context)
{
	auto* varLHS = dynamic_cast<FCVariableExprAST*>(mup_LHS.get());
	if (!varLHS) {
		fprintf(stderr, "LogError: LHS of assignment must be a variable\n");
		m_exprVal.type = FCValueCategory::Dangle;
		return m_exprVal;
	}
	if (!varLHS->decl) {
		// 不应发生，因为解析期已检查
		fprintf(stderr, "LogError: Variable %s not declared\n", varLHS->decl->name.c_str());
		m_exprVal.type = FCValueCategory::Dangle;
		return m_exprVal;
	}
	auto rhsVal = mup_RHS->evaluate(context);
	if (rhsVal.type == FCValueCategory::Dangle) return rhsVal;
	
	Frame& frame = context.currentFrame();
	int slot = varLHS->decl->slot;
	if (slot >= 0 && slot < (int)frame.locals.size()) {
		frame.locals[slot] = rhsVal;
	} else {
		fprintf(stderr, "LogError: Invalid slot for %s\n", varLHS->decl->name.c_str());
		m_exprVal.type = FCValueCategory::Dangle;
		return m_exprVal;
	}
	m_exprVal = rhsVal;
	return m_exprVal;
}

FCValue FCBinaryExprAST::evaluate(FCEvaluationContext& context)
{
	auto lhs_eva = mup_LHS->evaluate(context);
	auto rhs_eva = mup_RHS->evaluate(context);

	if (lhs_eva.type == FCValueCategory::Integer && rhs_eva.type == FCValueCategory::Integer)
	{
		switch (m_Op)
		{
		default:
			fprintf(stderr, "LogError: %c\n", m_Op);
			m_exprVal.evaluteVal.danglingVal = nullptr;
			m_exprVal.type = FCValueCategory::Dangle;
			return m_exprVal;
			break;
		case '+':
			m_exprVal.type = FCValueCategory::Integer;
			m_exprVal.evaluteVal.intVal = lhs_eva.evaluteVal.intVal +
				rhs_eva.evaluteVal.intVal;
			return m_exprVal;
			break;
		case '-':
			m_exprVal.type = FCValueCategory::Integer;
			m_exprVal.evaluteVal.intVal = lhs_eva.evaluteVal.intVal -
				rhs_eva.evaluteVal.intVal;
			return m_exprVal;
			break;
		case '*':
			m_exprVal.type = FCValueCategory::Integer;
			m_exprVal.evaluteVal.intVal = lhs_eva.evaluteVal.intVal *
				rhs_eva.evaluteVal.intVal;
			return m_exprVal;
			break;
		case '/':
			m_exprVal.type = FCValueCategory::Integer;
			m_exprVal.evaluteVal.intVal = lhs_eva.evaluteVal.intVal /
				rhs_eva.evaluteVal.intVal;
			return m_exprVal;
			break;
		case '<':
			m_exprVal.type = FCValueCategory::Integer;
			m_exprVal.evaluteVal.intVal = lhs_eva.evaluteVal.intVal < rhs_eva.evaluteVal.intVal;
			return m_exprVal;
			break;
		case '=':
			assignExpression(lhs_eva, rhs_eva, context);
			return m_exprVal;
			break;
		}
	}
	else if (lhs_eva.type == FCValueCategory::String && rhs_eva.type == FCValueCategory::String) {
		m_exprVal.type = FCValueCategory::String;
		::std::string lwithr = lhs_eva.evaluteVal.charVal;
		lwithr.append(rhs_eva.evaluteVal.charVal);
		memset(m_exprVal.evaluteVal.charVal, 0, 1024);
		memcpy(m_exprVal.evaluteVal.charVal, lwithr.c_str(), lwithr.size());
		return m_exprVal;
	}
	else if (lhs_eva.type == FCValueCategory::Floating && rhs_eva.type == FCValueCategory::Floating)
	{
		switch (m_Op)
		{
		default:
			fprintf(stderr, "LogError: %c\n", m_Op);
			m_exprVal.evaluteVal.danglingVal = nullptr;
			m_exprVal.type = FCValueCategory::Dangle;
			return m_exprVal;
			break;
		case '+':
			m_exprVal.type = FCValueCategory::Floating;
			m_exprVal.evaluteVal.doubleVal = lhs_eva.evaluteVal.doubleVal +
				rhs_eva.evaluteVal.doubleVal;
			return m_exprVal;
			break;
		case '-':
			m_exprVal.type = FCValueCategory::Floating;
			m_exprVal.evaluteVal.doubleVal = lhs_eva.evaluteVal.doubleVal -
				rhs_eva.evaluteVal.doubleVal;
			return m_exprVal;
			break;
		case '*':
			m_exprVal.type = FCValueCategory::Floating;
			m_exprVal.evaluteVal.doubleVal = lhs_eva.evaluteVal.doubleVal *
				rhs_eva.evaluteVal.doubleVal;
			return m_exprVal;
			break;
		case '/':
			m_exprVal.type = FCValueCategory::Floating;
			m_exprVal.evaluteVal.doubleVal = lhs_eva.evaluteVal.doubleVal /
				rhs_eva.evaluteVal.doubleVal;
			return m_exprVal;
			break;
		}
	}
	FCMarks::FCValue errorRes;
	errorRes.evaluteVal.danglingVal = nullptr;
	errorRes.type = FCMarks::FCValueCategory::Dangle;
	return errorRes;
}

FCCallExprAST::FCCallExprAST(const std::string& callee,
	std::vector<std::unique_ptr<FCExprAST>> args)
	: m_callee(callee), m_args(std::move(args))
{
	m_exprVal = {};
}
FCCallExprAST::~FCCallExprAST()
{
}

void FCCallExprAST::info()
{
	::std::cout << "FCCallExprAST Callee: " << m_callee;
	::std::cout << " Args: " << ::std::endl;
	for (auto& i : m_args)
	{
		::std::cout << "\t\t";
		i->info();
	}
}
const ::std::string& FCCallExprAST::getName()
{
	return m_callee;
}
const ::std::string& FCCallExprAST::getName() const
{
	return m_callee;
}
const std::vector<std::unique_ptr<FCExprAST>>& FCCallExprAST::getArgs()
{
	return m_args;
}
const std::vector<std::unique_ptr<FCExprAST>>& FCCallExprAST::getArgs() const
{
	return m_args;
}

FCValue FCCallExprAST::evaluate(FCEvaluationContext& context)
{
	auto* function = context.functions.findFunction(m_callee);
	if (function == nullptr)
	{
		fprintf(stderr, "Function not found: %s\n", m_callee.c_str());
		return makeDangleValue();
	}

	auto& prototype = function->getProto();
	if (m_args.size() != prototype->getArgs().size())
	{
		fprintf(stderr, "Argument count mismatch in %s\n", m_callee.c_str());
		return makeDangleValue();
	}

	// 求值实参必须发生在创建被调用函数栈帧之前，
	// 这样实参中的变量仍然从调用者栈帧读取。
	std::vector<FCValue> argumentValues;
	argumentValues.reserve(m_args.size());
	for (const auto& argument : m_args)
	{
		FCValue value = argument->evaluate(context);
		if (value.type == FCValueCategory::Dangle)
			return value;
		argumentValues.push_back(value);
	}

	context.pushFrame(m_callee);
	Frame& frame = context.currentFrame();

	for (size_t i = 0; i < argumentValues.size(); ++i)
	{
		auto& parameter = prototype->getArgs()[i];
		if (!parameter.decl)
		{
			context.popFrame();
			return makeDangleValue();
		}

		const int slot = parameter.decl->slot;
		if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
		{
			fprintf(stderr, "Invalid parameter slot in %s\n", m_callee.c_str());
			context.popFrame();
			return makeDangleValue();
		}

		frame.locals[slot] = argumentValues[i];
	}

	FCValue result = function->getBody()->evaluate(context);
	context.popFrame();
	return result;
}

FCPrototypeAST::FCPrototypeAST(const std::string& name, std::vector<FCVariableExprAST> args)
	: m_funcName(name), m_funcArgsVar(std::move(args))
{
	m_exprVal.evaluteVal.danglingVal = nullptr;
	m_exprVal.type = FCValueCategory::Dangle;
}
FCPrototypeAST::~FCPrototypeAST() {}

void FCPrototypeAST::info()
{
	::std::cout << "FCPrototypeAST Name: " << m_funcName;
	::std::cout << " Args: " << ::std::endl;
	for (auto& i : m_funcArgsVar)
	{
		i.info();
	}
}

::std::string FCPrototypeAST::getProtoName()
{
	return m_funcName;
}
FCValue FCPrototypeAST::evaluate(FCEvaluationContext&)
{
	return m_exprVal;
}

FCFunctionAST::FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
	std::unique_ptr<FCExprAST> body)
	: mup_funcProto(std::move(proto)), mup_funcBody(std::move(body))
{
	m_exprVal = {};
}
FCFunctionAST::~FCFunctionAST() {}

void FCFunctionAST::info()
{
	::std::cout << "FCFunctionAST Proto: ";
	mup_funcProto->info();
	::std::cout << " Body: ";
	mup_funcBody->info();
}

FCExprAST* FCFunctionAST::getBody()
{
	return mup_funcBody.get();
}
const FCExprAST* FCFunctionAST::getBody() const
{
	return mup_funcBody.get();
}
::std::string FCFunctionAST::getProtoName()
{
	return mup_funcProto->getProtoName();
}

FCValue FCFunctionAST::evaluate(FCEvaluationContext& context)
{
	context.functions.registerFunction(this);
	return makeDangleValue();
}

void FCIfExprAST::info()
{
	::std::cout << "FCIfExprAST Cond: ";
	Cond->info();
	::std::cout << " Then: ";
	Then->info();
	::std::cout << " Else: ";
	Else->info();
}

FCValue FCIfExprAST::evaluate(FCEvaluationContext& context)
{
	auto condVal = Cond->evaluate(context);
	if (condVal.type == FCValueCategory::Integer)
	{
		if (condVal.evaluteVal.intVal != 0)
		{
			m_exprVal = Then->evaluate(context);
		}
		else
		{
			m_exprVal = Else->evaluate(context);
		}
	}
	else if (condVal.type == FCValueCategory::Floating)
	{
		if (condVal.evaluteVal.doubleVal != 0.0)
		{
			m_exprVal = Then->evaluate(context);
		}
		else
		{
			m_exprVal = Else->evaluate(context);
		}
	}
	else
	{
		fprintf(stderr, "LogError: If condition is not Integer or Floating!\n");
		m_exprVal.evaluteVal.danglingVal = nullptr;
		m_exprVal.type = FCValueCategory::Dangle;
	}
	return m_exprVal;
}

void FCForExprAST::info()
{

}

FCValue FCForExprAST::evaluate(FCEvaluationContext& context)
{
	auto startVal = Start->evaluate(context);
	int slot = decl->slot;
	auto& frame = context.currentFrame();
	frame.locals[slot] = startVal;

	auto endVal = End->evaluate(context);
	auto stepVal = Step->evaluate(context);
	if (startVal.type != FCValueCategory::Integer || endVal.type != FCValueCategory::Integer || stepVal.type != FCValueCategory::Integer)
	{
		fprintf(stderr, "LogError: For loop parameters must be Integer!\n");
		m_exprVal.evaluteVal.danglingVal = nullptr;
		m_exprVal.type = FCValueCategory::Dangle;
		return m_exprVal;
	}
	for (; End->evaluate(context).evaluteVal.intVal; frame.locals[slot].evaluteVal.intVal += stepVal.evaluteVal.intVal) {
		auto tmpValue = Body->evaluate(context);
		::std::cout << " Body: " << tmpValue.evaluteVal.intVal << ::std::endl;
	}

	return m_exprVal;
}

void FCVarDeclExprAST::info() {
	std::cout << "FCVarDeclExprAST Name: " << decl->name << " Type: " << decl->typeName << std::endl;
	if (initExpr) {
		std::cout << " Init: ";
		initExpr->info();
	}
}

FCValue FCVarDeclExprAST::evaluate(FCEvaluationContext& context) {
	if (initExpr) {
		FCValue val = initExpr->evaluate(context);
		if (val.type == FCValueCategory::Dangle) return val;
		Frame& frame = context.currentFrame();
		if (decl->slot >= 0 && decl->slot < (int)frame.locals.size()) {
			frame.locals[decl->slot] = val;
		} else {
			// 槽位错误（不应发生，因为静态分配）
			m_exprVal.type = FCValueCategory::Dangle;
			return m_exprVal;
		}
		m_exprVal = val;  // 返回初始化值
		return m_exprVal;
	}
	// 无初始化，返回 Dangle 或默认值
	m_exprVal.type = FCValueCategory::Dangle;
	return m_exprVal;
}

namespace {

llvm::Type* integerType(FCCodegenContext& context)
{
	return llvm::Type::getInt32Ty(context.llvmContext);
}

llvm::Type* stringType(FCCodegenContext& context)
{
	return llvm::PointerType::getUnqual(context.llvmContext);
}

llvm::Value* logCodegenError(const char* message)
{
	fprintf(stderr, "Codegen error: %s\n", message);
	return nullptr;
}

llvm::Type* inferExprType(const FCExprAST* expression,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions);

llvm::Type* inferFunctionReturnType(const FCFunctionAST* function,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions)
{
	if (function == nullptr || function->getBody() == nullptr)
		return integerType(context);

	if (!visitingFunctions.insert(function).second)
		return integerType(context);

	auto* result = inferExprType(function->getBody(), context, visitingFunctions);
	visitingFunctions.erase(function);
	return result == nullptr ? integerType(context) : result;
}

llvm::Type* inferExprType(const FCExprAST* expression,
	FCCodegenContext& context,
	std::unordered_set<const FCFunctionAST*>& visitingFunctions)
{
	if (expression == nullptr)
		return integerType(context);

	if (auto* number = dynamic_cast<const FCNumberExprAST*>(expression))
	{
		return number->m_exprVal.type == FCValueCategory::Floating
			? llvm::Type::getDoubleTy(context.llvmContext)
			: integerType(context);
	}

	if (dynamic_cast<const FCStringExprAST*>(expression) != nullptr)
		return stringType(context);

	if (auto* variable = dynamic_cast<const FCVariableExprAST*>(expression))
		return variable->decl == nullptr
			? integerType(context)
			: context.getType(variable->decl->typeName);

	if (auto* binary = dynamic_cast<const FCBinaryExprAST*>(expression))
	{
		if (binary->getOperator() == '=')
			return inferExprType(binary->getRHS(), context, visitingFunctions);

		auto* lhsType = inferExprType(binary->getLHS(), context, visitingFunctions);
		auto* rhsType = inferExprType(binary->getRHS(), context, visitingFunctions);
		if (binary->getOperator() == '<')
			return integerType(context);
		if (lhsType->isDoubleTy() || rhsType->isDoubleTy())
			return llvm::Type::getDoubleTy(context.llvmContext);
		return lhsType;
	}

	if (auto* call = dynamic_cast<const FCCallExprAST*>(expression))
	{
		auto definition = context.definitions.find(call->getName());
		if (definition != context.definitions.end())
			return inferFunctionReturnType(definition->second, context, visitingFunctions);

		if (auto* function = context.module->getFunction(call->getName()))
			return function->getReturnType();

		return integerType(context);
	}

	if (auto* conditional = dynamic_cast<const FCIfExprAST*>(expression))
	{
		auto* thenType = inferExprType(conditional->getThen(), context, visitingFunctions);
		auto* elseType = inferExprType(conditional->getElse(), context, visitingFunctions);
		if (thenType->isDoubleTy() || elseType->isDoubleTy())
			return llvm::Type::getDoubleTy(context.llvmContext);
		return thenType;
	}

	if (auto* loop = dynamic_cast<const FCForExprAST*>(expression))
		return inferExprType(loop->getBody(), context, visitingFunctions);

	if (auto* sequence = dynamic_cast<const FCSeqExprAST*>(expression))
	{
		if (sequence->getExpressions().empty())
			return integerType(context);
		return inferExprType(sequence->getExpressions().back().get(), context, visitingFunctions);
	}

	if (auto* declaration = dynamic_cast<const FCVarDeclExprAST*>(expression))
		return declaration->decl == nullptr
			? inferExprType(declaration->initExpr.get(), context, visitingFunctions)
			: context.getType(declaration->decl->typeName);

	if (auto* function = dynamic_cast<const FCFunctionAST*>(expression))
		return inferFunctionReturnType(function, context, visitingFunctions);

	return integerType(context);
}

llvm::Type* inferFunctionReturnType(const FCFunctionAST* function,
	FCCodegenContext& context)
{
	std::unordered_set<const FCFunctionAST*> visitingFunctions;
	return inferFunctionReturnType(function, context, visitingFunctions);
}

llvm::Value* castValue(FCCodegenContext& context, llvm::Value* value,
	llvm::Type* targetType)
{
	if (value == nullptr || targetType == nullptr)
		return nullptr;
	if (value->getType() == targetType)
		return value;

	if (value->getType()->isIntegerTy() && targetType->isDoubleTy())
		return context.builder.CreateSIToFP(value, targetType, "inttodouble");
	if (value->getType()->isDoubleTy() && targetType->isIntegerTy())
		return context.builder.CreateFPToSI(value, targetType, "doubletoint");
	if (value->getType()->isIntegerTy(1) && targetType->isIntegerTy())
		return context.builder.CreateZExt(value, targetType, "booltoint");

	return nullptr;
}

llvm::Value* createCondition(FCCodegenContext& context, llvm::Value* value)
{
	if (value == nullptr)
		return nullptr;
	if (value->getType()->isIntegerTy(1))
		return value;
	if (value->getType()->isIntegerTy())
		return context.builder.CreateICmpNE(
			value,
			llvm::ConstantInt::get(value->getType(), 0),
			"ifcond");
	if (value->getType()->isFloatingPointTy())
		return context.builder.CreateFCmpONE(
			value,
			llvm::ConstantFP::get(value->getType(), 0.0),
			"ifcond");

	return nullptr;
}

llvm::Function* createFunctionDeclaration(FCCodegenContext& context,
	const std::string& name,
	const std::vector<FCVariableExprAST>& arguments,
	llvm::Type* returnType)
{
	if (auto* existing = context.module->getFunction(name))
		return existing;

	std::vector<llvm::Type*> argumentTypes;
	argumentTypes.reserve(arguments.size());
	for (const auto& argument : arguments)
	{
		argumentTypes.push_back(argument.decl == nullptr
			? integerType(context)
			: context.getType(argument.decl->typeName));
	}

	auto* functionType = llvm::FunctionType::get(returnType, argumentTypes, false);
	auto* function = llvm::Function::Create(
		functionType,
		llvm::Function::ExternalLinkage,
		name,
		context.module.get());

	unsigned index = 0;
	for (auto& argument : function->args())
	{
		if (index < arguments.size() && arguments[index].decl != nullptr)
			argument.setName(arguments[index].decl->name);
		++index;
	}
	return function;
}

llvm::Function* declareFunction(FCFunctionAST* function,
	FCCodegenContext& context)
{
	if (function == nullptr)
		return nullptr;

	return createFunctionDeclaration(
		context,
		function->getProtoName(),
		function->getProto()->getArgs(),
		inferFunctionReturnType(function, context));
}

} // namespace

FCCodegenContext::FCCodegenContext(const std::string& moduleName)
	: builder(llvmContext),
	  module(std::make_unique<llvm::Module>(moduleName, llvmContext))
{
}

llvm::Type* FCCodegenContext::getType(const std::string& typeName)
{
	if (typeName == "double")
		return llvm::Type::getDoubleTy(llvmContext);
	if (typeName == "string")
		return llvm::PointerType::getUnqual(llvmContext);
	return llvm::Type::getInt32Ty(llvmContext);
}

llvm::AllocaInst* FCCodegenContext::createEntryBlockAlloca(
	llvm::Function* function, const std::string& name, llvm::Type* type)
{
	llvm::IRBuilder<> entryBuilder(
		&function->getEntryBlock(), function->getEntryBlock().begin());
	return entryBuilder.CreateAlloca(type, nullptr, name);
}

llvm::Value* FCNumberExprAST::codegen(FCCodegenContext& context)
{
	if (m_exprVal.type == FCValueCategory::Floating)
		return llvm::ConstantFP::get(
			llvm::Type::getDoubleTy(context.llvmContext), m_doubleVal);
	return llvm::ConstantInt::get(
		llvm::Type::getInt32Ty(context.llvmContext), m_intVal, true);
}

llvm::Value* FCStringExprAST::codegen(FCCodegenContext& context)
{
	return context.builder.CreateGlobalStringPtr(m_stringVal, "str");
}

llvm::Value* FCVariableExprAST::codegen(FCCodegenContext& context)
{
	if (decl == nullptr)
		return logCodegenError("variable declaration is missing");

	auto it = context.namedValues.find(decl.get());
	if (it == context.namedValues.end())
		return logCodegenError("variable is not allocated in the current function");

	return context.builder.CreateLoad(
		it->second->getAllocatedType(), it->second, decl->name);
}

llvm::Value* FCBinaryExprAST::codegen(FCCodegenContext& context)
{
	if (m_Op == '=')
	{
		auto* variable = dynamic_cast<FCVariableExprAST*>(mup_LHS.get());
		if (variable == nullptr || variable->decl == nullptr)
			return logCodegenError("left side of assignment must be a variable");

		auto address = context.namedValues.find(variable->decl.get());
		if (address == context.namedValues.end())
			return logCodegenError("assigned variable is not allocated");

		auto* rhs = mup_RHS->codegen(context);
		if (rhs == nullptr)
			return nullptr;
		rhs = castValue(context, rhs, address->second->getAllocatedType());
		if (rhs == nullptr)
			return logCodegenError("assignment type mismatch");

		context.builder.CreateStore(rhs, address->second);
		return rhs;
	}

	auto* lhs = mup_LHS->codegen(context);
	auto* rhs = mup_RHS->codegen(context);
	if (lhs == nullptr || rhs == nullptr)
		return nullptr;

	if (lhs->getType()->isPointerTy() || rhs->getType()->isPointerTy())
	{
		if (m_Op != '+' || !lhs->getType()->isPointerTy() ||
			!rhs->getType()->isPointerTy())
			return logCodegenError("only string concatenation is supported");

		auto* i8Pointer = stringType(context);
		auto* sizeType = llvm::Type::getInt64Ty(context.llvmContext);
		auto* strlenType = llvm::FunctionType::get(sizeType, { i8Pointer }, false);
		auto strlen = context.module->getOrInsertFunction("strlen", strlenType);
		auto* lhsLength = context.builder.CreateCall(strlen, { lhs }, "lhslen");
		auto* rhsLength = context.builder.CreateCall(strlen, { rhs }, "rhslen");
		auto* totalLength = context.builder.CreateAdd(lhsLength, rhsLength, "strlen");
		totalLength = context.builder.CreateAdd(
			totalLength, llvm::ConstantInt::get(sizeType, 1), "strlenwithnull");

		auto* mallocType = llvm::FunctionType::get(i8Pointer, { sizeType }, false);
		auto mallocFunction = context.module->getOrInsertFunction("malloc", mallocType);
		auto* buffer = context.builder.CreateCall(mallocFunction, { totalLength }, "strbuf");

		auto* copyType = llvm::FunctionType::get(
			i8Pointer, { i8Pointer, i8Pointer }, false);
		auto strcpyFunction = context.module->getOrInsertFunction("strcpy", copyType);
		auto strcatFunction = context.module->getOrInsertFunction("strcat", copyType);
	context.builder.CreateCall(strcpyFunction, { buffer, lhs });
	context.builder.CreateCall(strcatFunction, { buffer, rhs });
	return buffer;
	}

	if (lhs->getType()->isDoubleTy() || rhs->getType()->isDoubleTy())
	{
		lhs = castValue(context, lhs, llvm::Type::getDoubleTy(context.llvmContext));
		rhs = castValue(context, rhs, llvm::Type::getDoubleTy(context.llvmContext));
		if (lhs == nullptr || rhs == nullptr)
			return logCodegenError("numeric type conversion failed");

		switch (m_Op)
		{
		case '+': return context.builder.CreateFAdd(lhs, rhs, "addtmp");
		case '-': return context.builder.CreateFSub(lhs, rhs, "subtmp");
		case '*': return context.builder.CreateFMul(lhs, rhs, "multmp");
		case '/': return context.builder.CreateFDiv(lhs, rhs, "divtmp");
		case '<':
		{
			auto* comparison = context.builder.CreateFCmpULT(lhs, rhs, "cmptmp");
			return context.builder.CreateZExt(comparison,
				integerType(context), "booltmp");
		}
		default: break;
		}
	}
	else if (lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy())
	{
		if (lhs->getType() != rhs->getType())
		{
			lhs = castValue(context, lhs, integerType(context));
			rhs = castValue(context, rhs, integerType(context));
		}

		switch (m_Op)
		{
		case '+': return context.builder.CreateAdd(lhs, rhs, "addtmp");
		case '-': return context.builder.CreateSub(lhs, rhs, "subtmp");
		case '*': return context.builder.CreateMul(lhs, rhs, "multmp");
		case '/': return context.builder.CreateSDiv(lhs, rhs, "divtmp");
		case '<':
		{
			auto* comparison = context.builder.CreateICmpSLT(lhs, rhs, "cmptmp");
			return context.builder.CreateZExt(comparison,
				integerType(context), "booltmp");
		}
		default: break;
		}
	}

	return logCodegenError("unknown binary operator");
}

llvm::Value* FCCallExprAST::codegen(FCCodegenContext& context)
{
	auto* function = context.module->getFunction(m_callee);
	if (function == nullptr)
	{
		fprintf(stderr, "Codegen error: function not found: %s\n", m_callee.c_str());
		return nullptr;
	}
	if (m_args.size() != function->arg_size())
	{
		fprintf(stderr, "Codegen error: argument count mismatch in %s\n", m_callee.c_str());
		return nullptr;
	}

	std::vector<llvm::Value*> arguments;
	arguments.reserve(m_args.size());
	unsigned index = 0;
	for (const auto& argument : m_args)
	{
		auto* value = argument->codegen(context);
		if (value == nullptr)
			return nullptr;
		value = castValue(context, value, function->getArg(index)->getType());
		if (value == nullptr)
			return logCodegenError("call argument type mismatch");
		arguments.push_back(value);
		++index;
	}

	return context.builder.CreateCall(function, arguments, "calltmp");
}

llvm::Value* FCPrototypeAST::codegen(FCCodegenContext& context)
{
	return createFunctionDeclaration(
		context, m_funcName, m_funcArgsVar, integerType(context));
}

llvm::Value* FCFunctionAST::codegen(FCCodegenContext& context)
{
	auto* function = declareFunction(this, context);
	if (function == nullptr)
		return nullptr;
	if (!function->empty())
		return function;

	auto oldInsertPoint = context.builder.saveIP();
	auto oldFunction = context.currentFunction;
	auto oldNamedValues = std::move(context.namedValues);
	context.namedValues.clear();
	context.currentFunction = function;

	auto* entry = llvm::BasicBlock::Create(context.llvmContext, "entry", function);
	context.builder.SetInsertPoint(entry);

	unsigned index = 0;
	for (auto& argument : function->args())
	{
		auto& parameter = getProto()->getArgs()[index];
		if (parameter.decl == nullptr)
		{
			context.namedValues = std::move(oldNamedValues);
			context.currentFunction = oldFunction;
			context.builder.restoreIP(oldInsertPoint);
			return logCodegenError("function parameter declaration is missing");
		}

		auto* alloca = context.createEntryBlockAlloca(
			function, parameter.decl->name, argument.getType());
		context.builder.CreateStore(&argument, alloca);
		context.namedValues.emplace(parameter.decl.get(), alloca);
		++index;
	}

	auto* body = mup_funcBody == nullptr ? nullptr : mup_funcBody->codegen(context);
	if (body == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return nullptr;
	}

	auto* returnValue = castValue(context, body, function->getReturnType());
	if (returnValue == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return logCodegenError("function return type mismatch");
	}
	context.builder.CreateRet(returnValue);

	if (llvm::verifyFunction(*function, &llvm::errs()))
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return nullptr;
	}

	context.namedValues = std::move(oldNamedValues);
	context.currentFunction = oldFunction;
	context.builder.restoreIP(oldInsertPoint);
	return function;
}

llvm::Value* FCIfExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr)
		return logCodegenError("if expression is outside a function");

	auto* condition = createCondition(context, Cond->codegen(context));
	if (condition == nullptr)
		return logCodegenError("if condition must be numeric");

	auto* function = context.currentFunction;
	auto* thenBlock = llvm::BasicBlock::Create(context.llvmContext, "then", function);
	auto* elseBlock = llvm::BasicBlock::Create(context.llvmContext, "else");
	auto* mergeBlock = llvm::BasicBlock::Create(context.llvmContext, "ifcont");
	context.builder.CreateCondBr(condition, thenBlock, elseBlock);

	context.builder.SetInsertPoint(thenBlock);
	auto* thenValue = Then->codegen(context);
	if (thenValue == nullptr)
		return nullptr;
	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
		context.builder.CreateBr(mergeBlock);
	thenBlock = context.builder.GetInsertBlock();

	function->insert(function->end(), elseBlock);
	context.builder.SetInsertPoint(elseBlock);
	auto* elseValue = Else->codegen(context);
	if (elseValue == nullptr)
		return nullptr;
	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
		context.builder.CreateBr(mergeBlock);
	elseBlock = context.builder.GetInsertBlock();

	function->insert(function->end(), mergeBlock);
	context.builder.SetInsertPoint(mergeBlock);

	std::unordered_set<const FCFunctionAST*> noFunctions;
	// The branch type is inferred independently from the two branches.
	auto* thenType = inferExprType(Then.get(), context, noFunctions);
	auto* elseType = inferExprType(Else.get(), context, noFunctions);
	auto* phiType = thenType->isDoubleTy() || elseType->isDoubleTy()
		? llvm::Type::getDoubleTy(context.llvmContext) : thenType;
	thenValue = castValue(context, thenValue, phiType);
	elseValue = castValue(context, elseValue, phiType);
	if (thenValue == nullptr || elseValue == nullptr)
		return logCodegenError("if branch types do not match");

	auto* phi = context.builder.CreatePHI(phiType, 2, "iftmp");
	phi->addIncoming(thenValue, thenBlock);
	phi->addIncoming(elseValue, elseBlock);
	return phi;
}

llvm::Value* FCForExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr || decl == nullptr)
		return logCodegenError("for expression is outside a function");

	auto oldDeclaration = context.namedValues.find(decl.get());
	llvm::AllocaInst* oldAddress = oldDeclaration == context.namedValues.end()
		? nullptr : oldDeclaration->second;
	auto* variable = context.createEntryBlockAlloca(
		context.currentFunction, decl->name, context.getType(decl->typeName));
	context.namedValues[decl.get()] = variable;

	auto* start = Start->codegen(context);
	start = castValue(context, start, variable->getAllocatedType());
	if (start == nullptr)
		return logCodegenError("for start value type mismatch");
	context.builder.CreateStore(start, variable);

	auto* function = context.currentFunction;
	auto* loopBlock = llvm::BasicBlock::Create(context.llvmContext, "loop", function);
	auto* afterBlock = llvm::BasicBlock::Create(context.llvmContext, "afterloop");
	context.builder.CreateBr(loopBlock);
	context.builder.SetInsertPoint(loopBlock);

	auto* end = End->codegen(context);
	auto* condition = createCondition(context, end);
	if (condition == nullptr)
		return logCodegenError("for end condition must be numeric");
	auto* bodyBlock = llvm::BasicBlock::Create(context.llvmContext, "loopbody");
	context.builder.CreateCondBr(condition, bodyBlock, afterBlock);

	function->insert(function->end(), bodyBlock);
	context.builder.SetInsertPoint(bodyBlock);
	if (Body->codegen(context) == nullptr)
		return nullptr;

	if (context.builder.GetInsertBlock()->getTerminator() == nullptr)
	{
		auto* step = Step == nullptr
			? llvm::ConstantInt::get(integerType(context), 1)
			: Step->codegen(context);
		step = castValue(context, step, variable->getAllocatedType());
		if (step == nullptr)
			return logCodegenError("for step value type mismatch");

		auto* current = context.builder.CreateLoad(
			variable->getAllocatedType(), variable, decl->name);
		auto* next = variable->getAllocatedType()->isFloatingPointTy()
			? context.builder.CreateFAdd(current, step, "nextvar")
			: context.builder.CreateAdd(current, step, "nextvar");
		context.builder.CreateStore(next, variable);
		context.builder.CreateBr(loopBlock);
	}

	function->insert(function->end(), afterBlock);
	context.builder.SetInsertPoint(afterBlock);

	if (oldDeclaration == context.namedValues.end())
		context.namedValues.erase(decl.get());
	else
		context.namedValues[decl.get()] = oldAddress;
	return llvm::ConstantInt::get(integerType(context), 0);
}

llvm::Value* FCSeqExprAST::codegen(FCCodegenContext& context)
{
	llvm::Value* last = nullptr;
	for (const auto& expression : exprs)
	{
		last = expression->codegen(context);
		if (last == nullptr)
			return nullptr;
	}
	return last;
}

llvm::Value* FCVarDeclExprAST::codegen(FCCodegenContext& context)
{
	if (context.currentFunction == nullptr || decl == nullptr)
		return logCodegenError("variable declaration is outside a function");

	auto* variable = context.createEntryBlockAlloca(
		context.currentFunction, decl->name, context.getType(decl->typeName));
	context.namedValues[decl.get()] = variable;

	llvm::Value* initialValue = initExpr == nullptr
		? llvm::Constant::getNullValue(variable->getAllocatedType())
		: initExpr->codegen(context);
	initialValue = castValue(context, initialValue, variable->getAllocatedType());
	if (initialValue == nullptr)
		return logCodegenError("variable initializer type mismatch");

	context.builder.CreateStore(initialValue, variable);
	return initialValue;
}

llvm::Value* FCProgramAST::codegen(FCCodegenContext& context)
{
	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
			context.definitions[function->getProtoName()] = function;
	}

	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
		{
			if (declareFunction(function, context) == nullptr)
				return nullptr;
		}
	}

	for (const auto& statement : m_statements)
	{
		if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
		{
			if (function->codegen(context) == nullptr)
				return nullptr;
		}
	}

	std::vector<const FCExprAST*> runtimeStatements;
	for (const auto& statement : m_statements)
	{
		if (dynamic_cast<FCFunctionAST*>(statement.get()) == nullptr)
			runtimeStatements.push_back(statement.get());
	}
	if (runtimeStatements.empty())
		return m_statements.empty() ? nullptr : m_statements.back()->codegen(context);

	auto* mainFunction = context.module->getFunction("__fc_main");
	if (mainFunction == nullptr)
	{
		auto* mainType = llvm::FunctionType::get(integerType(context), {}, false);
		mainFunction = llvm::Function::Create(
			mainType,
			llvm::Function::InternalLinkage,
			"__fc_main",
			context.module.get());
	}
	if (!mainFunction->empty())
		return mainFunction;

	auto oldInsertPoint = context.builder.saveIP();
	auto oldFunction = context.currentFunction;
	auto oldNamedValues = std::move(context.namedValues);
	context.namedValues.clear();
	context.currentFunction = mainFunction;
	context.builder.SetInsertPoint(
		llvm::BasicBlock::Create(context.llvmContext, "entry", mainFunction));

	llvm::Value* last = nullptr;
	for (const auto* statement : runtimeStatements)
	{
		last = const_cast<FCExprAST*>(statement)->codegen(context);
		if (last == nullptr)
		{
			context.namedValues = std::move(oldNamedValues);
			context.currentFunction = oldFunction;
			context.builder.restoreIP(oldInsertPoint);
			return nullptr;
		}
	}

	last = castValue(context, last, integerType(context));
	if (last == nullptr)
	{
		context.namedValues = std::move(oldNamedValues);
		context.currentFunction = oldFunction;
		context.builder.restoreIP(oldInsertPoint);
		return logCodegenError("top-level expression must produce an integer");
	}
	context.builder.CreateRet(last);

	const bool invalid = llvm::verifyFunction(*mainFunction, &llvm::errs());
	context.namedValues = std::move(oldNamedValues);
	context.currentFunction = oldFunction;
	context.builder.restoreIP(oldInsertPoint);
	return invalid ? nullptr : mainFunction;
}
