#include "token.h"
#include "functionmanager.h"
#include <iostream>
#include <numeric>

using namespace FCExprClass;

namespace FCMarks {
	::std::map<char, int> binopPrecedence =
	{
		{'+',10},
		{'-',10},
		{'*',20},
		{'/',20}
	};
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

FCVariableExprAST::FCVariableExprAST(const ::std::string& Name, const ::std::string& typeName, const ::std::string& funcName) : Name(Name), typeName(typeName), funcName(funcName)
{
	m_exprVal.evaluteVal.danglingVal = nullptr;
	m_exprVal.type = FCValueCategory::Dangle;

	if (typeName == "int")
		m_refVal.type = FCValueCategory::Integer;
	else if (typeName == "double")
		m_refVal.type = FCValueCategory::Floating;
	else
		m_refVal.type = FCValueCategory::String;
}

FCVariableExprAST::FCVariableExprAST(const FCVariableExprAST& othVarObj)
{
	*this = othVarObj;
}

FCVariableExprAST::~FCVariableExprAST()
{
}

void FCVariableExprAST::info()
{
	::std::cout << "FCVariableExprAST Name: " << Name;
	if (m_exprVal.type == FCValueCategory::Dangle)
	{
		::std::cout << " Has no FCValue" << ::std::endl;
	}
	else {

	}
}

FCValue FCVariableExprAST::evaluate()
{
	//对变量求值，即对变量表中对象求值
	//变量表中对象的值在运行期确定
	auto varOfCurrentFunc = (*varTableInFunc)[funcName];
	auto& targetArg = varOfCurrentFunc.at(Name);
	m_exprVal = targetArg.m_exprVal;
	return m_exprVal;
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
	m_exprVal.evaluteVal.danglingVal = nullptr;
	m_exprVal.type = FCValueCategory::Dangle;

	FCFunctionAST* sfitr = nullptr;
	for (auto& i : *specailFunc)
	{
		if (i->getProtoName() == m_callee)
		{
			sfitr = i;
		}
	}
	auto func = funcManager->getFuncByName(m_callee);
	if (func == nullptr)
	{
		fprintf(stderr, "LogError: %s is not FOUND!", m_callee.c_str());
		FCMarks::FCValue errorRes;
		errorRes.evaluteVal.danglingVal = nullptr;
		errorRes.type = FCMarks::FCValueCategory::Dangle;
		return errorRes;;
	}
	if (sfitr != nullptr)
	{
		auto generatedFunc = sfitr;
		auto& needReplaceArgs = generatedFunc->mup_funcProto->m_funcArgsVar;
		auto& needReplaceFunc = (*varTableInFunc)[generatedFunc->mup_funcProto->m_funcName];
		for (size_t i = 0; i < m_args.size(); i++)
		{
			auto& theName = needReplaceArgs[i].Name;
			auto& needReplaceItem = needReplaceFunc.at(theName);
			needReplaceItem.m_exprVal = m_args[i]->evaluate();
		}
		m_exprVal = generatedFunc->mup_funcBody->evaluate();
	}
	else
	{
		m_exprVal = func(&m_args);
	}

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
		::std::cout << "\t\t" << i.Name << ::std::endl;
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
	return m_exprVal;
}
