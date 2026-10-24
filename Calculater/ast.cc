#include "ast.h"

using namespace FCExprClass;

FCNumberExprAST::FCNumberExprAST(int val)
	: m_intVal(val), m_doubleVal(0.0), m_isFloating(false)
{
}
FCNumberExprAST::FCNumberExprAST(double val)
	: m_intVal(0), m_doubleVal(val), m_isFloating(true)
{
}

FCStringExprAST::FCStringExprAST(std::string val) : m_stringVal(val)
{
}

FCVariableExprAST::FCVariableExprAST(std::string varName) : name(std::move(varName))
{
}

FCBinaryExprAST::FCBinaryExprAST(FCBinaryOp op,
	::std::unique_ptr<FCExprAST> lhs,
	::std::unique_ptr<FCExprAST> rhs)
	: m_Op(op), mup_LHS(::std::move(lhs)), mup_RHS(::std::move(rhs))
{
}

const char* binaryOpName(FCBinaryOp op) {
	switch (op) {
	case FCBinaryOp::Assign: return "=";
	case FCBinaryOp::Less: return "<";
	case FCBinaryOp::Add: return "+";
	case FCBinaryOp::Sub: return "-";
	case FCBinaryOp::Mul: return "*";
	case FCBinaryOp::Div: return "/";
	default: return "?";
	}
}

FCCallExprAST::FCCallExprAST(const std::string& callee,
	std::vector<std::unique_ptr<FCExprAST>> args)
	: m_callee(callee), m_args(std::move(args))
{
}

const ::std::string& FCCallExprAST::getName() const
{
	return m_callee;
}
const std::vector<std::unique_ptr<FCExprAST>>& FCCallExprAST::getArgs() const
{
	return m_args;
}

FCPrototypeAST::FCPrototypeAST(const std::string& name, std::vector<VarDeclPtr> args)
	: m_funcName(name), m_funcArgsVar(std::move(args))
{
}

::std::string FCPrototypeAST::getProtoName() const
{
	return m_funcName;
}
FCFunctionAST::FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
	std::unique_ptr<FCExprAST> body)
	: mup_funcProto(std::move(proto)), mup_funcBody(std::move(body))
{
}

::std::string FCFunctionAST::getProtoName() const
{
	return mup_funcProto->getProtoName();
}

const FCExprAST* FCFunctionAST::getBody() const
{
	return mup_funcBody.get();
}
