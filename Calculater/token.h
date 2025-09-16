#pragma once

#include <string>
#include <vector>
#include <stack>
#include <memory>
#include <utility>
#include <map>
#include <cassert>
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

	struct VarDecl {
		std::string name;
		std::string typeName; // "int"/"double"/"string"
		int slot = -1; // -1 表示尚未分配
		VarDecl(const std::string &n, const std::string &t) : name(n), typeName(t) {}
	};
	using VarDeclPtr = std::shared_ptr<VarDecl>;
	using Scope = std::unordered_map<std::string, VarDeclPtr>;
	using ScopeStack = std::vector<Scope>;

	struct Frame {
		std::string funcName;
		std::vector<FCMarks::FCValue> locals; // 按 decl->slot 访问
	};

	extern std::vector<Frame> g_callStack;
	
	// 解析期全局结构：每个函数名对应一个 ScopeStack（解析时使用）
	extern std::unordered_map<std::string, ScopeStack> g_varTableInFunc;
	// 每个函数的所有声明（按出现顺序），用于分配 slot
	extern std::unordered_map<std::string, std::vector<VarDeclPtr>> g_funcDeclList;
	// 函数每次调用需要多少 local slots
	extern std::unordered_map<std::string, int> g_funcLocalCount;


	void pushScopeForFunc(const std::string &func);
	void popScopeForFunc(const std::string &func);
	VarDeclPtr lookupVariableDecl(const std::string &func, const std::string &name);
	void insertVariableInCurrentScope(const std::string &func, const std::string &name, VarDeclPtr decl);
	void pushFrame(const std::string &func);
	void popFrame();
	Frame& currentFrame();
}


namespace FCExprClass
{
	using namespace FCMarks;

		// 将参数类型映射到 C++ 类型
	auto mapType = [](const std::string &type) -> std::string {
		if (type == "int") return "int";
		if (type == "double") return "double";
		if (type == "string") return "std::string";
		return "auto"; // 未知类型
	};

	class CodeGenContext {
	private:
		std::stack<std::unordered_map<std::string, std::string>> m_scopes;
		int m_varCounter = 0;
		std::unordered_map<std::string, std::string> m_funcDecls;
		
	public:
		void pushScope() {
			m_scopes.push({});
		}
		
		void popScope() {
			m_scopes.pop();
		}
		
		// 将语言中的变量名映射到 C++ 变量名
		std::string getVarName(const std::string &origName) {
			if (m_scopes.empty()) return origName;
			
			auto &currentScope = m_scopes.top();
			auto it = currentScope.find(origName);
			if (it != currentScope.end()) {
				return it->second;
			}
			
			// 创建新的映射
			std::string cppName = "var_" + std::to_string(m_varCounter++);
			currentScope[origName] = cppName;
			return cppName;
		}
		
		void addFunctionDecl(const std::string &funcName, const std::string &cppDecl) {
			m_funcDecls[funcName] = cppDecl;
		}
		
		std::string getFunctionDecl(const std::string &funcName) {
			auto it = m_funcDecls.find(funcName);
			if (it != m_funcDecls.end()) return it->second;
			return "";
		}
		
		// 生成唯一的临时变量名
		std::string createTempVar() {
			return "tmp_" + std::to_string(m_varCounter++);
		}
	};

	struct FCExprAST
	{
	public:
		FCExprAST() = default;
		virtual ~FCExprAST() {};
		virtual void info() = 0;
		virtual FCValue evaluate() = 0;
		// 新增：C++ 代码生成方法
		virtual void codeGenCpp(std::ostream &os, CodeGenContext &ctx) = 0;

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

		// FCNumberExprAST::codeGenCpp
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
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

		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
	};

	/// FCVariableExprAST - Expression struct for referencing a variable, like "a".
	struct FCVariableExprAST : public FCExprAST
	{
		VarDeclPtr decl;
		FCValue m_exprVal, m_refVal;
	public:
		FCVariableExprAST(VarDeclPtr v);
		~FCVariableExprAST();
		FCTypeDescribe type = FCMarks::FCTypeDescribe::VariableExpr;
		void info() override;
		FCValue evaluate() override;
		void setValue(FCValue val) { m_exprVal = val; }
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;

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
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;


	private:
		FCValue assignExpression(FCValue lhs_eva, FCValue rhs_eva);
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
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;

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
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
		std::vector<FCVariableExprAST>& getArgs() { return m_funcArgsVar; }
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
		std::unique_ptr<FCPrototypeAST>& getProto() { return mup_funcProto; }
		void info() override;
		FCValue evaluate() override;
		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;

	};

	class FCIfExprAST : public FCExprAST {
		std::unique_ptr<FCExprAST> m_cond, m_then, m_else;
		FCValue m_exprVal;
	public:
	FCIfExprAST(std::unique_ptr<FCExprAST> Cond, std::unique_ptr<FCExprAST> Then,
				std::unique_ptr<FCExprAST> Else)
		: m_cond(std::move(Cond)), m_then(std::move(Then)), m_else(std::move(Else)) {
		m_exprVal.type = FCValueCategory::Dangle;
	}

		void info() override;
		FCValue evaluate() override;

		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
	};

	class FCForExprAST : public FCExprAST {
		VarDeclPtr decl;
		std::unique_ptr<FCExprAST> m_start, m_end, m_step, m_body;
		FCValue m_exprVal;
	public:
	FCForExprAST(VarDeclPtr decl,
		std::unique_ptr<FCExprAST> start,
		std::unique_ptr<FCExprAST> end,
		std::unique_ptr<FCExprAST> step,
		std::unique_ptr<FCExprAST> body)
		: decl(std::move(decl)), m_start(std::move(start)), m_end(std::move(end)),
		m_step(std::move(step)), m_body(std::move(body)) {
		m_exprVal.type = FCValueCategory::Dangle;
	}

		void info() override;
		FCValue evaluate() override;

		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
	};

	struct FCSeqExprAST : public FCExprAST {
		std::vector<std::unique_ptr<FCExprAST>> exprs;
		FCSeqExprAST(std::vector<std::unique_ptr<FCExprAST>> e) : exprs(std::move(e)) {}
		FCValue evaluate() override {
			FCValue last;
			for (auto& e : exprs) {
				last = e->evaluate();
			}
			return last;
		}

		void info() override {
			for (auto& i : exprs) {
				i->info();
			}

		}

		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
	};

	struct FCVarDeclExprAST : public FCExprAST {
	public:
		VarDeclPtr decl;
		std::unique_ptr<FCExprAST> initExpr;  // 初始化表达式
		FCValue m_exprVal;
		FCVarDeclExprAST(VarDeclPtr d, std::unique_ptr<FCExprAST> init)
			: decl(std::move(d)), initExpr(std::move(init)) {
			m_exprVal.type = FCValueCategory::Dangle;
		}
		~FCVarDeclExprAST() {}
		void info() override;
		FCValue evaluate() override;

		void codeGenCpp(std::ostream &os, CodeGenContext &ctx) override;
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
		
		FCValue evaluate() override {
			FCValue lastResult{ FCValueCategory::Dangle };
			for (auto& stmt : m_statements) {
				lastResult = stmt->evaluate();
			}
			return lastResult;
		}
		
		const std::vector<std::unique_ptr<FCExprAST>>& getStatements() const {
			return m_statements;
		}
		
		void codeGenCpp(std::ostream& os, CodeGenContext& ctx) override;
	};
}