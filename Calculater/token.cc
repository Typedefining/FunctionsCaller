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
	std::vector<Frame> g_callStack;

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

	void pushFrame(const std::string &func)
	{
		Frame f;
		f.funcName = func;
		int n = 0;
		auto it = g_funcLocalCount.find(func);
		if (it != g_funcLocalCount.end()) n = it->second;
		f.locals.resize(n);
		g_callStack.push_back(std::move(f));
	}
	void popFrame()
	{
		assert(!g_callStack.empty());
		g_callStack.pop_back();
	}
	Frame& currentFrame()
	{
		assert(!g_callStack.empty());
		return g_callStack.back();
	}
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

FCValue FCNumberExprAST::evaluate()
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

FCValue FCStringExprAST::evaluate()
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

FCValue FCVariableExprAST::evaluate()
{
	if (!decl) {
		FCMarks::FCValue r; r.evaluteVal.danglingVal = nullptr; r.type = FCMarks::FCValueCategory::Dangle; return r;
	}
	if (decl->slot < 0) {
	// slot 未分配 — 说明 parse/codegen 流程有问题
		FCMarks::FCValue r; r.evaluteVal.danglingVal = nullptr; r.type = FCMarks::FCValueCategory::Dangle; return r;
	}
	// 通过当前帧访问
	Frame &fr = currentFrame();
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

FCValue FCBinaryExprAST::assignExpression(FCValue lhs_eva, FCValue rhs_eva)
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
	auto rhsVal = mup_RHS->evaluate();
	if (rhsVal.type == FCValueCategory::Dangle) return rhsVal;
	
	Frame& frame = currentFrame();
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

FCValue FCBinaryExprAST::evaluate()
{
	auto lhs_eva = mup_LHS->evaluate();
	auto rhs_eva = mup_RHS->evaluate();

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
			assignExpression(lhs_eva, rhs_eva);
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

FCValue FCCallExprAST::evaluate()
{
	// m_exprVal.evaluteVal.danglingVal = nullptr;
	// m_exprVal.type = FCValueCategory::Dangle;


	// FCFunctionAST* sfitr = nullptr;
	// for (auto& i : *specailFunc)
	// {
	// 	if (i->getProtoName() == m_callee)
	// 	{
	// 		sfitr = i;
	// 	}
	// }
	// auto func = funcManager->getFuncByName(m_callee);
	// if (func == nullptr)
	// {
	// 	fprintf(stderr, "LogError: %s is not FOUND!", m_callee.c_str());
	// 	FCMarks::FCValue errorRes;
	// 	errorRes.evaluteVal.danglingVal = nullptr;
	// 	errorRes.type = FCMarks::FCValueCategory::Dangle;
	// 	return errorRes;
	// }

	// if (sfitr != nullptr)
	// {
	// 	// === 创建新的 Frame 并写入实参 ===
	// 	pushFrame(m_callee); // 为当前函数调用创建局部变量数组

	// 	auto& frame = currentFrame();
	// 	auto& declList = g_funcDeclList[m_callee]; // 静态声明列表，slot 已分配
	// 	auto& prototype = sfitr->getProto();
	// 	if (m_args.size() != prototype->getArgs().size()) {
	// 		fprintf(stderr, "LogError: argument count mismatch in %s\n", m_callee.c_str());
	// 		popFrame();
	// 		return m_exprVal;
	// 	}

	// 	// 将实参 evaluate 后写入对应 slot
	// 	for (size_t i = 0; i < m_args.size(); ++i) {
	// 		FCValue val = m_args[i]->evaluate();
	// 		prototype->getArgs()[i].setValue(val);
	// 		int slot = prototype->getArgs()[i].decl->slot;
	// 		frame.locals[slot] = val;
	// 	}

	// 	m_exprVal = sfitr->mup_funcBody->evaluate();
	// 	popFrame();
	// }
	// else
	// {
	// 	m_exprVal = func(&m_args);
	// }

	return m_exprVal;
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
FCValue FCPrototypeAST::evaluate()
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

FCValue FCFunctionAST::evaluate()
{
	m_exprVal = mup_funcBody->evaluate();
	return m_exprVal;
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

FCValue FCIfExprAST::evaluate()
{
	auto condVal = Cond->evaluate();
	if (condVal.type == FCValueCategory::Integer)
	{
		if (condVal.evaluteVal.intVal != 0)
		{
			m_exprVal = Then->evaluate();
		}
		else
		{
			m_exprVal = Else->evaluate();
		}
	}
	else if (condVal.type == FCValueCategory::Floating)
	{
		if (condVal.evaluteVal.doubleVal != 0.0)
		{
			m_exprVal = Then->evaluate();
		}
		else
		{
			m_exprVal = Else->evaluate();
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

FCValue FCForExprAST::evaluate()
{
	auto startVal = Start->evaluate();
	int slot = decl->slot;
	auto& frame = currentFrame();
	frame.locals[slot] = startVal;

	auto endVal = End->evaluate();
	auto stepVal = Step->evaluate();
	if (startVal.type != FCValueCategory::Integer || endVal.type != FCValueCategory::Integer || stepVal.type != FCValueCategory::Integer)
	{
		fprintf(stderr, "LogError: For loop parameters must be Integer!\n");
		m_exprVal.evaluteVal.danglingVal = nullptr;
		m_exprVal.type = FCValueCategory::Dangle;
		return m_exprVal;
	}
	for (; End->evaluate().evaluteVal.intVal; frame.locals[slot].evaluteVal.intVal += stepVal.evaluteVal.intVal) {
		auto tmpValue = Body->evaluate();
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

FCValue FCVarDeclExprAST::evaluate() {
	if (initExpr) {
		FCValue val = initExpr->evaluate();
		if (val.type == FCValueCategory::Dangle) return val;
		Frame& frame = currentFrame();
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