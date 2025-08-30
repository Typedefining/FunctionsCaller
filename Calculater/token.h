#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>

namespace FCMarks
{
	enum struct FCToken
	{
		tok_eof = -1,
		tok_def = -2,
		tok_identifier = -3,
		tok_number = -4,
		tok_string = -5,
		tok_if = -6,
		tok_then = -7,
		tok_else = -8,
		tok_for = -9,
		tok_in = -10,
	};
	enum struct FCTypeDescribe
	{
		Expr,		  //抽象表达式
		NumberExpr,	  //数字字面量
		StringExpr,//字符串字面量
		VariableExpr, //对象标识
		BinaryExpr,	  //二元表达式
		CallExpr,	  //可调用对象表达式
		Prototype,	  //函数原型表达式,弃用
		Function	  //函数定义
	};

	enum struct FCValueCategory
	{
		Integer,
		Floating,
		String,
		Dangle
	};

	union FCValueUnion
	{
		int intVal;
		double doubleVal;
		char charVal[2048];
		void* danglingVal;
	};

	struct FCValue {
		FCValueCategory type;
		FCValueUnion evaluteVal;
	};

	extern ::std::map<char, int> binopPrecedence;
}

namespace FCExprClass
{
	using namespace FCMarks;
	struct FCExprAST
	{
	public:
		FCExprAST() = default;
		virtual ~FCExprAST() {};
		virtual void info() = 0;
		virtual FCValue evaluate() = 0;
		FCTypeDescribe type = FCTypeDescribe::Expr;
	};
	struct FCNumberExprAST : public FCExprAST
	{
		int m_intVal;
		double m_doubleVal;
		FCValue m_exprVal;
	public:
		FCNumberExprAST(int val);
		FCNumberExprAST(double val);
		~FCNumberExprAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::NumberExpr;
		void info() override;
		FCValue evaluate() override;
	};

	struct FCStringExprAST : public FCExprAST
	{
		std::string m_stringVal;
		FCValue m_exprVal;
	public:
		FCStringExprAST(std::string val);
		~FCStringExprAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::StringExpr;
		void info() override;
		FCValue evaluate() override;
	};

	/// FCVariableExprAST - Expression struct for referencing a variable, like "a".
	struct FCVariableExprAST : public FCExprAST
	{
		::std::string Name;
		::std::string typeName;
		::std::string funcName;
		FCValue m_exprVal, m_refVal;
	public:
		FCVariableExprAST(const ::std::string& name, const ::std::string& typeName, const ::std::string& funcName);
		FCVariableExprAST(const ::std::string& name);
		FCVariableExprAST(const FCVariableExprAST& othVarObj);
		~FCVariableExprAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::VariableExpr;
		void info() override;
		FCValue evaluate() override;
		void setValue(FCValue val) { m_exprVal = val; }
	};

	/// FCBinaryExprAST - Expression struct for a binary operator.
	struct FCBinaryExprAST : public FCExprAST
	{
		char m_Op;
		::std::unique_ptr<FCExprAST> mup_LHS, mup_RHS;
		FCValue m_exprVal;
	public:
		FCBinaryExprAST(char op,
			::std::unique_ptr<FCExprAST> lhs,
			::std::unique_ptr<FCExprAST> rhs);
		~FCBinaryExprAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::BinaryExpr;
		void info() override;
		FCValue evaluate() override;
	};

	/// FCCallExprAST - Expression struct for function calls.
	struct FCCallExprAST : public FCExprAST
	{
		std::string m_callee;
		std::vector<std::unique_ptr<FCExprAST>> m_args;
		FCValue m_exprVal;
	public:
		FCCallExprAST(const std::string& callee,
			std::vector<std::unique_ptr<FCExprAST>> args);
		~FCCallExprAST();
		const ::std::string& getName();
		const std::vector<std::unique_ptr<FCExprAST>>& getArgs();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::CallExpr;
		void info() override;
		FCValue evaluate() override;
	};

	struct FCPrototypeAST : public FCExprAST
	{
		std::string m_funcName;
		std::vector<FCVariableExprAST> m_funcArgsVar;
		FCValue m_exprVal;
	public:
		FCPrototypeAST(const std::string& name, std::vector<FCVariableExprAST> args);
		~FCPrototypeAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::Prototype;

		void info() override;
		::std::string getProtoName();
		FCValue evaluate() override;
	};

	/// FCFunctionAST - This struct represents a function definition itself.
	struct FCFunctionAST : public FCExprAST
	{
		std::unique_ptr<FCPrototypeAST> mup_funcProto;
		std::unique_ptr<FCExprAST> mup_funcBody;
		FCValue m_exprVal;
	public:
		FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
			std::unique_ptr<FCExprAST> body);
		~FCFunctionAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::Function;
		::std::string getProtoName();
		FCExprAST* getBody();
		void info() override;
		FCValue evaluate() override;
	};

	class FCIfExprAST : public FCExprAST {
		std::unique_ptr<FCExprAST> Cond, Then, Else;
		FCValue m_exprVal;
	public:
	FCIfExprAST(std::unique_ptr<FCExprAST> Cond, std::unique_ptr<FCExprAST> Then,
				std::unique_ptr<FCExprAST> Else)
		: Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

		void info() override;
		FCValue evaluate() override;
	};

	class FCForExprAST : public FCExprAST {
		std::string VarName;
		std::unique_ptr<FCExprAST> Start, End, Step, Body;
		FCValue m_exprVal;
	public:
	FCForExprAST(const std::string& varName,
		std::unique_ptr<FCExprAST> start,
		std::unique_ptr<FCExprAST> end,
		std::unique_ptr<FCExprAST> step,
		std::unique_ptr<FCExprAST> body)
		: VarName(varName), Start(std::move(start)), End(std::move(end)),
		Step(std::move(step)), Body(std::move(body)) {}

		void info() override;
		FCValue evaluate() override;
	};
}