#include "token.h"
#include <iostream>
#include <numeric>

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
const std::vector<std::unique_ptr<FCExprAST>>& FCCallExprAST::getArgs()
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
