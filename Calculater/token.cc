#include "token.h"
#include <iostream>

using namespace FCExprClass;

FCNumberExprAST::FCNumberExprAST(int val)
	: m_intVal(val), m_doubleVal(0.0), m_isFloating(false)
{
}
FCNumberExprAST::FCNumberExprAST(double val)
	: m_intVal(0), m_doubleVal(val), m_isFloating(true)
{
}
FCNumberExprAST::~FCNumberExprAST()
{
}
void FCNumberExprAST::info()
{
	if (!m_isFloating)
		::std::cout << "FCNumberExprAST Val is Integer: " << m_intVal << ::std::endl;
	else
		::std::cout << "FCNumberExprAST Val is Floating: " << m_doubleVal << ::std::endl;
	return;
}

FCStringExprAST::FCStringExprAST(std::string val) : m_stringVal(val)
{
}
FCStringExprAST::~FCStringExprAST()
{
}
void FCStringExprAST::info()
{
	::std::cout << "FCStringExprAST Val: " << m_stringVal << ::std::endl;
}

FCVariableExprAST::FCVariableExprAST(VarDeclPtr d) : decl(std::move(d))
{
}

FCVariableExprAST::~FCVariableExprAST()
{
}

void FCVariableExprAST::info()
{
	::std::cout << "FCVariableExprAST (decl) Name: " << decl->name << " slot=" << decl->slot << ::std::endl;
}

FCBinaryExprAST::FCBinaryExprAST(char op,
	::std::unique_ptr<FCExprAST> lhs,
	::std::unique_ptr<FCExprAST> rhs)
	: m_Op(op), mup_LHS(::std::move(lhs)), mup_RHS(::std::move(rhs))
{
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

FCCallExprAST::FCCallExprAST(const std::string& callee,
	std::vector<std::unique_ptr<FCExprAST>> args)
	: m_callee(callee), m_args(std::move(args))
{
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

FCPrototypeAST::FCPrototypeAST(const std::string& name, std::vector<FCVariableExprAST> args)
	: m_funcName(name), m_funcArgsVar(std::move(args))
{
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
::std::string FCPrototypeAST::getProtoName() const
{
	return m_funcName;
}
FCFunctionAST::FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
	std::unique_ptr<FCExprAST> body, int localCount)
	: mup_funcProto(std::move(proto)), mup_funcBody(std::move(body)),
	  m_localCount(localCount)
{
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

void FCIfExprAST::info()
{
	::std::cout << "FCIfExprAST Cond: ";
	Cond->info();
	::std::cout << " Then: ";
	Then->info();
	::std::cout << " Else: ";
	Else->info();
}

void FCForExprAST::info()
{

}

void FCVarDeclExprAST::info() {
	std::cout << "FCVarDeclExprAST Name: " << decl->name << " Type: " << decl->typeName << std::endl;
	if (initExpr) {
		std::cout << " Init: ";
		initExpr->info();
	}
}
