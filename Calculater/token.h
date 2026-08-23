#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <unordered_map>
#include <iostream>

namespace FCMarks
{
	enum struct FCToken
	{
		tok_begin = 0,
		tok_eof = 1,
		tok_def = 2,
		tok_identifier = 3,
		tok_number = 4,
		tok_string = 5,
		tok_if = 6,
		tok_then = 7,
		tok_else = 8,
		tok_for = 9,
		tok_in = 10,
		tok_var = 11,
		tok_end,
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

	struct VarDecl {
		std::string name;
		std::string typeName; // "int"/"double"/"string"
		int slot = -1; // -1 表示尚未分配
		bool isGlobal = false;
		VarDecl(const std::string &n, const std::string &t) : name(n), typeName(t) {}
	};
	using VarDeclPtr = std::shared_ptr<VarDecl>;
	using Scope = std::unordered_map<std::string, VarDeclPtr>;
	using ScopeStack = std::vector<Scope>;

}


namespace FCExprClass
{
	using namespace FCMarks;
	struct FCExprAST
	{
	public:
		FCExprAST() = default;
		virtual ~FCExprAST() = default;
		virtual void info() = 0;
		FCTypeDescribe type = FCTypeDescribe::Expr;
	};
	struct FCNumberExprAST : public FCExprAST
	{
		int m_intVal;
		double m_doubleVal;
		bool m_isFloating = false;
	public:
		FCNumberExprAST(int val);
		FCNumberExprAST(double val);
		bool isFloating() const { return m_isFloating; }
		FCTypeDescribe type = FCMarks::FCTypeDescribe::NumberExpr;
		void info() override;
	};

	struct FCStringExprAST : public FCExprAST
	{
		std::string m_stringVal;
	public:
		FCStringExprAST(std::string val);
		FCTypeDescribe type = FCMarks::FCTypeDescribe::StringExpr;
		void info() override;
	};

	/// FCVariableExprAST - Expression struct for referencing a variable, like "a".
	struct FCVariableExprAST : public FCExprAST
	{
		VarDeclPtr decl;
	public:
		FCVariableExprAST(VarDeclPtr v);
		FCTypeDescribe type = FCMarks::FCTypeDescribe::VariableExpr;
		void info() override;
	};

	/// FCBinaryExprAST - Expression struct for a binary operator.
	struct FCBinaryExprAST : public FCExprAST
	{
		char m_Op;
		::std::unique_ptr<FCExprAST> mup_LHS, mup_RHS;
	public:
		FCBinaryExprAST(char op,
			::std::unique_ptr<FCExprAST> lhs,
			::std::unique_ptr<FCExprAST> rhs);
		FCTypeDescribe type = FCMarks::FCTypeDescribe::BinaryExpr;
		void info() override;
		char getOperator() const { return m_Op; }
		const FCExprAST* getLHS() const { return mup_LHS.get(); }
		const FCExprAST* getRHS() const { return mup_RHS.get(); }

	};

	/// FCCallExprAST - Expression struct for function calls.
	struct FCCallExprAST : public FCExprAST
	{
		std::string m_callee;
		std::vector<std::unique_ptr<FCExprAST>> m_args;
	public:
		FCCallExprAST(const std::string& callee,
			std::vector<std::unique_ptr<FCExprAST>> args);
		const ::std::string& getName() const;
		const std::vector<std::unique_ptr<FCExprAST>>& getArgs() const;
		FCTypeDescribe type = FCMarks::FCTypeDescribe::CallExpr;
		void info() override;
	};

	struct FCPrototypeAST : public FCExprAST
	{
		std::string m_funcName;
		std::vector<FCVariableExprAST> m_funcArgsVar;
	public:
		FCPrototypeAST(const std::string& name, std::vector<FCVariableExprAST> args);
		FCTypeDescribe type = FCMarks::FCTypeDescribe::Prototype;

		void info() override;
		::std::string getProtoName() const;
		const std::vector<FCVariableExprAST>& getArgs() const { return m_funcArgsVar; }
	};

	/// FCFunctionAST - This struct represents a function definition itself.
	struct FCFunctionAST : public FCExprAST
	{
		std::unique_ptr<FCPrototypeAST> mup_funcProto;
		std::unique_ptr<FCExprAST> mup_funcBody;
		int m_localCount = 0;
	public:
		FCFunctionAST(std::unique_ptr<FCPrototypeAST> proto,
			std::unique_ptr<FCExprAST> body, int localCount = 0);
		FCTypeDescribe type = FCMarks::FCTypeDescribe::Function;
		::std::string getProtoName() const;
		const FCExprAST* getBody() const;
		const std::unique_ptr<FCPrototypeAST>& getProto() const { return mup_funcProto; }
		int getLocalCount() const { return m_localCount; }
		void info() override;
	};

	class FCIfExprAST : public FCExprAST {
		std::unique_ptr<FCExprAST> Cond, Then, Else;
	public:
	FCIfExprAST(std::unique_ptr<FCExprAST> Cond, std::unique_ptr<FCExprAST> Then,
				std::unique_ptr<FCExprAST> Else)
		: Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

		void info() override;
		const FCExprAST* getCondition() const { return Cond.get(); }
		const FCExprAST* getThen() const { return Then.get(); }
		const FCExprAST* getElse() const { return Else.get(); }
	};

	class FCForExprAST : public FCExprAST {
		VarDeclPtr decl;
		std::unique_ptr<FCExprAST> Start, End, Step, Body;
	public:
	FCForExprAST(VarDeclPtr decl,
		std::unique_ptr<FCExprAST> start,
		std::unique_ptr<FCExprAST> end,
		std::unique_ptr<FCExprAST> step,
		std::unique_ptr<FCExprAST> body)
		: decl(std::move(decl)), Start(std::move(start)), End(std::move(end)),
		Step(std::move(step)), Body(std::move(body)) {}

		void info() override;
		const VarDeclPtr& getDecl() const { return decl; }
		const FCExprAST* getStart() const { return Start.get(); }
		const FCExprAST* getEnd() const { return End.get(); }
		const FCExprAST* getStep() const { return Step.get(); }
		const FCExprAST* getBody() const { return Body.get(); }
	};

	struct FCSeqExprAST : public FCExprAST {
		std::vector<std::unique_ptr<FCExprAST>> exprs;
		FCSeqExprAST(std::vector<std::unique_ptr<FCExprAST>> e) : exprs(std::move(e)) {}

		void info() override {
			for (auto& i : exprs) {
				i->info();
			}

		}
		const std::vector<std::unique_ptr<FCExprAST>>& getExpressions() const { return exprs; }
	};

	struct FCVarDeclExprAST : public FCExprAST {
	public:
		VarDeclPtr decl;
		std::unique_ptr<FCExprAST> initExpr;  // 初始化表达式
		FCVarDeclExprAST(VarDeclPtr d, std::unique_ptr<FCExprAST> init)
			: decl(std::move(d)), initExpr(std::move(init)) {}
		void info() override;
	};

	class FCProgramAST : public FCExprAST {
	private:
		std::vector<std::unique_ptr<FCExprAST>> m_statements;
		
	public:
		FCProgramAST(std::vector<std::unique_ptr<FCExprAST>> statements)
			: m_statements(std::move(statements)) {}
		
		void info() override {
			std::cout << "FCProgramAST with " << m_statements.size() << " statements:\n";
			for (auto& stmt : m_statements) {
				stmt->info();
			}
		}
		
		const std::vector<std::unique_ptr<FCExprAST>>& getStatements() const {
			return m_statements;
		}
	};
}
