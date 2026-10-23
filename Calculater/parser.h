#pragma once

#include <memory>
#include <string>
#include "diagnostic.h"
#include "lexer.h"
#include "token.h"

namespace FC {

/// 语法分析器：Token 流 -> 纯语法 AST
/// 不依赖 Semantic，不执行符号绑定、作用域管理或 frameIndex 赋值
/// 可独立单元测试（可注入 Lexer，或直接构造 Token 流）
class Parser {
public:
	explicit Parser(Lexer& lexer, FCDiagnostics& diagnostics);

	// 解析完整程序，返回语法树（FCProgramAST 或单个表达式）。
	// 进入解析阶段并读取第一个 token；此前 Parser 不持有有效 token。
	std::unique_ptr<FCExprClass::FCExprAST> parseProgram();

	// 解析过程是否发生过错误
	bool hadError() const { return m_hadError; }

	// 重置解析器状态，以便复用同一 Parser 实例解析多段输入。
	// 复位后 m_curTok 不再有效，需由下一次 parseProgram() 重新读取。
	void reset();

	// 是否已进入解析阶段（构造后、parseProgram() 之前为 false）
	bool started() const { return m_started; }

private:
	// 语句分隔符 ';' 的合法位置：只能出现在非块终结的表达式之后。
	// 块（'}'）已经终止当前语句，其后不得再有 ';'，下一条语句直接开始。
	Lexer& m_lexer;
	FCDiagnostics& m_diagnostics;
	Token m_curTok;
	int m_prevKind = -1;
	bool m_hadError = false;
	bool m_started = false;

	void begin();
	void advance();
	bool check(int kind) const;

	// 上一个被消费的 token 是否是 '}'：用于判定 ';' 是否紧跟块终结符。
	bool lastTokenWasBlockClose() const;
	static int tokenPrecedence(FCMarks::FCBinaryOp op);

	std::unique_ptr<FCExprClass::FCExprAST> logError(const char* msg);
	std::unique_ptr<FCExprClass::FCPrototypeAST> logErrorP(const char* msg);

	// 纯语法解析方法（不触及符号表、作用域、frameIndex）
	std::unique_ptr<FCExprClass::FCExprAST> parseExpression();
	std::unique_ptr<FCExprClass::FCExprAST> parsePrimary();
	std::unique_ptr<FCExprClass::FCExprAST> parseNumberExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseStringExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseParenExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseIdentifierExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseBinOpRHS(int ExprPrec,
		std::unique_ptr<FCExprClass::FCExprAST> LHS);
	std::unique_ptr<FCExprClass::FCPrototypeAST> parsePrototype();
	std::unique_ptr<FCExprClass::FCFunctionAST> parseDefinition();
	std::unique_ptr<FCExprClass::FCExprAST> parseSeqExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseIfExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseForExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseVarDeclExpr();
	std::unique_ptr<FCExprClass::FCExprAST> parseBlockExpr();
};

} // namespace FC
