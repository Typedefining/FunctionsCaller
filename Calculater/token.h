#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <map>
#include <cassert>
#include <unordered_map>
#include <iostream>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Value.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Instructions.h"


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
}


namespace FCExprClass
{
	using namespace FCMarks;
	struct FCEvaluationContext;
	struct FCCodegenContext;
	struct FCExprAST
	{
	public:
		FCExprAST() = default;
		virtual ~FCExprAST() {};
		virtual void info() = 0;
		virtual FCValue evaluate(FCEvaluationContext&) = 0;
		virtual llvm::Value *codegen(FCCodegenContext&) = 0;
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
		char getOperator() const { return m_Op; }
		const FCExprAST* getLHS() const { return mup_LHS.get(); }
		const FCExprAST* getRHS() const { return mup_RHS.get(); }

	private:
		FCValue assignExpression(FCValue lhs_eva, FCValue rhs_eva,
			FCEvaluationContext& context);
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
		const ::std::string& getName() const;
		const std::vector<std::unique_ptr<FCExprAST>>& getArgs();
		const std::vector<std::unique_ptr<FCExprAST>>& getArgs() const;
		FCTypeDescribe type = FCMarks::FCTypeDescribe::CallExpr;
		void info() override;
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
		std::vector<FCVariableExprAST>& getArgs() { return m_funcArgsVar; }
		const std::vector<FCVariableExprAST>& getArgs() const { return m_funcArgsVar; }
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
		const FCExprAST* getBody() const;
		std::unique_ptr<FCPrototypeAST>& getProto() { return mup_funcProto; }
		const std::unique_ptr<FCPrototypeAST>& getProto() const { return mup_funcProto; }
		void info() override;
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
	};

	class FCFunctionRegistry
	{
	public:
		bool registerFunction(FCFunctionAST* function);
		FCFunctionAST* findFunction(const std::string& name) const;
		bool index(FCExprAST* root);
		void clear();

	private:
		std::unordered_map<std::string, FCFunctionAST*> m_functions;
	};

	struct FCEvaluationContext
	{
		FCFunctionRegistry functions;
		std::vector<Frame> callStack;

		void pushFrame(const std::string& functionName);
		void popFrame();
		Frame& currentFrame();
	};

	class FCIfExprAST : public FCExprAST {
		std::unique_ptr<FCExprAST> Cond, Then, Else;
		FCValue m_exprVal;
	public:
	FCIfExprAST(std::unique_ptr<FCExprAST> Cond, std::unique_ptr<FCExprAST> Then,
				std::unique_ptr<FCExprAST> Else)
		: Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

		void info() override;
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
		const FCExprAST* getCondition() const { return Cond.get(); }
		const FCExprAST* getThen() const { return Then.get(); }
		const FCExprAST* getElse() const { return Else.get(); }
	};

	class FCForExprAST : public FCExprAST {
		VarDeclPtr decl;
		std::unique_ptr<FCExprAST> Start, End, Step, Body;
		FCValue m_exprVal;
	public:
	FCForExprAST(VarDeclPtr decl,
		std::unique_ptr<FCExprAST> start,
		std::unique_ptr<FCExprAST> end,
		std::unique_ptr<FCExprAST> step,
		std::unique_ptr<FCExprAST> body)
		: decl(std::move(decl)), Start(std::move(start)), End(std::move(end)),
		Step(std::move(step)), Body(std::move(body)) {}

		void info() override;
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
		const VarDeclPtr& getDecl() const { return decl; }
		const FCExprAST* getStart() const { return Start.get(); }
		const FCExprAST* getEnd() const { return End.get(); }
		const FCExprAST* getStep() const { return Step.get(); }
		const FCExprAST* getBody() const { return Body.get(); }
	};

	struct FCSeqExprAST : public FCExprAST {
		std::vector<std::unique_ptr<FCExprAST>> exprs;
		FCSeqExprAST(std::vector<std::unique_ptr<FCExprAST>> e) : exprs(std::move(e)) {}
		FCValue evaluate(FCEvaluationContext& context) override {
			FCValue last;
			for (auto& e : exprs) {
				last = e->evaluate(context);
			}
			return last;
		}

		void info() override {
			for (auto& i : exprs) {
				i->info();
			}

		}
		llvm::Value *codegen(FCCodegenContext&) override;
		const std::vector<std::unique_ptr<FCExprAST>>& getExpressions() const { return exprs; }
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
		FCValue evaluate(FCEvaluationContext&) override;
		llvm::Value *codegen(FCCodegenContext&) override;
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
		
		FCValue evaluate(FCEvaluationContext& context) override {
			context.functions.index(this);
			FCValue lastResult;
			for (auto& stmt : m_statements) {
				if (dynamic_cast<FCFunctionAST*>(stmt.get()) != nullptr)
					continue;
				lastResult = stmt->evaluate(context);
			}
			return lastResult;
		}
		
		const std::vector<std::unique_ptr<FCExprAST>>& getStatements() const {
			return m_statements;
		}
		llvm::Value *codegen(FCCodegenContext&) override;
	};

	struct FCCodegenContext
	{
		llvm::LLVMContext llvmContext;
		llvm::IRBuilder<> builder;
		std::unique_ptr<llvm::Module> module;
		std::map<const VarDecl*, llvm::AllocaInst*> namedValues;
		std::unordered_map<std::string, FCFunctionAST*> definitions;
		llvm::Function* currentFunction = nullptr;

		explicit FCCodegenContext(const std::string& moduleName);
		llvm::Type* getType(const std::string& typeName);
		llvm::AllocaInst* createEntryBlockAlloca(llvm::Function* function,
			const std::string& name, llvm::Type* type);
	};
}
