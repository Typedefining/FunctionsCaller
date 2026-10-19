#include "parser.h"
#include <cctype>
#include <cstdio>
#include <tuple>

using namespace FCExprClass;

namespace FC {

Parser::Parser(Lexer& lexer)
	: m_lexer(lexer) {
	advance();
}

void Parser::reset() {
	m_hadError = false;
	advance();
}

void Parser::advance() {
	m_curTok = m_lexer.nextToken();
}

bool Parser::check(int kind) const {
	return m_curTok.kind == kind;
}

// 表达式起始符判定：序列终止的唯一权威。
// 与 parsePrimary 的 case 表同构——结构性 token（} ) else in def eof ;）
// 一律不是表达式起始，出现在序列位置即序列自然终止，交还外层结构消费。
static bool startsExpression(int kind) {
	switch (kind) {
	case static_cast<int>(FCToken::tok_identifier):
	case static_cast<int>(FCToken::tok_number):
	case static_cast<int>(FCToken::tok_string):
	case static_cast<int>(FCToken::tok_parenthes_open):
	case static_cast<int>(FCToken::tok_brace_open):
	case static_cast<int>(FCToken::tok_if):
	case static_cast<int>(FCToken::tok_for):
	case static_cast<int>(FCToken::tok_var):
		return true;
	default:
		return false;
	}
}

// 优先级表与 FCSemanticContext 的 m_binopPrecedence 保持一致
int Parser::tokenPrecedence(FCMarks::FCBinaryOp op) {
	switch (op) {
	case FCMarks::FCBinaryOp::Assign:
		return 5;
	case FCMarks::FCBinaryOp::Less:
		return 9;
	case FCMarks::FCBinaryOp::Add:
	case FCMarks::FCBinaryOp::Sub:
		return 10;
	case FCMarks::FCBinaryOp::Mul:
	case FCMarks::FCBinaryOp::Div:
		return 20;
	default:
		return -1;
	}
}

std::unique_ptr<FCExprAST> Parser::logError(const char* msg) {
	m_hadError = true;
	fprintf(stderr, "LogError: %s\n", msg);
	return nullptr;
}

std::unique_ptr<FCPrototypeAST> Parser::logErrorP(const char* msg) {
	logError(msg);
	return nullptr;
}

std::unique_ptr<FCExprAST> Parser::parseProgram() {
	std::vector<std::unique_ptr<FCExprAST>> statements;

	while (m_curTok.kind != static_cast<int>(FCToken::tok_eof)) {
		// def 是语句而非表达式：交回 parseDefinition；
		// def 块的 '}' 之后允许一个收尾 ';'（惯例写法 "def ...}; expr"）
		if (m_curTok.kind == static_cast<int>(FCToken::tok_def)) {
			auto fn = parseDefinition();
			if (fn) {
				statements.push_back(std::move(fn));
				if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon))
					advance();
				continue;
			}
			// 函数定义失败（含函数体错误）即整体失败
			return nullptr;
		}
		// 顶层裸 ';' 是空语句，非法（与其他层级规则一致）
		if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon)) {
			logError("empty statement at top level");
			return nullptr;
		}
		if (auto stmt = parseSeqExpr()) {
			statements.push_back(std::move(stmt));
			continue;
		}
		// seq 失败即整体失败：不再空转（避免同一 token 反复进入 seq）
		return nullptr;
	}

	if (statements.size() == 1)
		return std::move(statements[0]);

	return std::make_unique<FCProgramAST>(std::move(statements));
}

std::unique_ptr<FCExprAST> Parser::parsePrimary() {
	switch (m_curTok.kind) {
	default:
		return logError("unknown FCToken when expecting an expression");
	case static_cast<int>(FCToken::tok_identifier):
		return parseIdentifierExpr();
	case static_cast<int>(FCToken::tok_number):
		return parseNumberExpr();
	case static_cast<int>(FCToken::tok_string):
		return parseStringExpr();
	case static_cast<int>(FCToken::tok_parenthes_open):
		return parseParenExpr();
	case static_cast<int>(FCToken::tok_brace_open):
		return parseBlockExpr();
	case static_cast<int>(FCToken::tok_if):
		return parseIfExpr();
	case static_cast<int>(FCToken::tok_for):
		return parseForExpr();
	case static_cast<int>(FCToken::tok_var):
		return parseVarDeclExpr();
	}
}

std::unique_ptr<FCExprAST>
Parser::parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprAST> LHS) {
	while (true) {
		FCMarks::FCBinaryOp op = m_curTok.binop;
		int TokPrec = tokenPrecedence(op);

		if (TokPrec < ExprPrec)
			return LHS;

		advance();

		auto RHS = parsePrimary();
		if (!RHS)
			return nullptr;

		int NextPrec = tokenPrecedence(m_curTok.binop);
		if (TokPrec < NextPrec) {
			RHS = parseBinOpRHS(TokPrec + 1, std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		LHS = std::make_unique<FCBinaryExprAST>(op, std::move(LHS), std::move(RHS));
	}
}

std::unique_ptr<FCExprAST> Parser::parseExpression() {
	auto LHS = parsePrimary();
	if (!LHS)
		return nullptr;
	return parseBinOpRHS(0, std::move(LHS));
}

std::unique_ptr<FCExprAST> Parser::parseNumberExpr() {
	const bool isFloating = m_curTok.lexeme.find('.') != std::string::npos;
	std::unique_ptr<FCNumberExprAST> result;
	if (isFloating)
		result = std::make_unique<FCNumberExprAST>(std::stod(m_curTok.lexeme));
	else
		result = std::make_unique<FCNumberExprAST>(std::stoi(m_curTok.lexeme));
	advance();
	return result;
}

std::unique_ptr<FCExprAST> Parser::parseStringExpr() {
	auto Result = std::make_unique<FCStringExprAST>(m_curTok.lexeme);
	advance();
	return Result;
}

std::unique_ptr<FCExprAST> Parser::parseParenExpr() {
	advance();
	auto V = parseSeqExpr();
	if (!V)
		return nullptr;

	if (m_curTok.kind != static_cast<int>(FCToken::tok_parenthes_close))
		return logError("expected ')'");
	advance();
	return V;
}

std::unique_ptr<FCExprAST> Parser::parseIdentifierExpr() {
	std::string IdName = m_curTok.lexeme;
	advance();

	// 纯语法：只构造变量引用节点，符号解析与绑定留给 SemanticAnalyzer
	if (m_curTok.kind != static_cast<int>(FCToken::tok_parenthes_open)) {
		return std::make_unique<FCVariableExprAST>(IdName);
	}

	// 函数调用分支
	advance();

	std::vector<std::unique_ptr<FCExprAST>> funCallArgs;
	if (m_curTok.kind != static_cast<int>(FCToken::tok_parenthes_close)) {
		while (true) {
			auto funCallArg = parseExpression();
			if (!funCallArg)
				return nullptr;
			funCallArgs.push_back(std::move(funCallArg));
			if (m_curTok.kind == static_cast<int>(FCToken::tok_parenthes_close))
				break;
			if (m_curTok.kind != static_cast<int>(FCToken::tok_comma))
				return logError("Expected ')' or ',' in argument list");
			advance();
		}
	}

	advance();

	return std::make_unique<FCCallExprAST>(IdName, std::move(funCallArgs));
}

std::unique_ptr<FCPrototypeAST> Parser::parsePrototype() {
	if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier))
		return logErrorP("Expected function name in prototype");

	std::string funName = m_curTok.lexeme;
	advance();

	if (m_curTok.kind != static_cast<int>(FCToken::tok_parenthes_open))
		return logErrorP("Expected '(' in prototype");

	std::vector<std::tuple<std::string, std::string>> Args;
	advance();
	while (m_curTok.kind != static_cast<int>(FCToken::tok_parenthes_close)) {
		if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier))
			return logErrorP("Expected parameter name in prototype");

		std::string varName = m_curTok.lexeme;
		advance();
		if (m_curTok.kind != static_cast<int>(FCToken::tok_colon))
			return logErrorP("Expected ':' after parameter name");

		advance();
		if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier))
			return logErrorP("Expected parameter type in prototype");
		std::string typeName = m_curTok.lexeme;
		if (typeName != "int" && typeName != "double" && typeName != "string")
			return logErrorP("Unsupported parameter type");

		Args.push_back(std::make_tuple(varName, typeName));
		advance();
		if (m_curTok.kind == static_cast<int>(FCToken::tok_parenthes_close))
			break;
		if (m_curTok.kind != static_cast<int>(FCToken::tok_comma))
			return logErrorP("Expected ',' between parameters");
		advance();
		if (m_curTok.kind == static_cast<int>(FCToken::tok_parenthes_close))
			return logErrorP("Expected parameter after ','");
	}

	advance();

	std::vector<VarDeclPtr> ArgNames;
	for (auto& arg : Args) {
		// 仅记录参数名与类型，函数栈帧 size 分配留给语义期
		auto decl = std::make_shared<VarDecl>(std::get<0>(arg), std::get<1>(arg));
		ArgNames.push_back(decl);
	}
	return std::make_unique<FCPrototypeAST>(funName, std::move(ArgNames));
}

std::unique_ptr<FCFunctionAST> Parser::parseDefinition() {
	advance();

	auto Proto = parsePrototype();
	if (!Proto)
		return nullptr;

	// 纯语法：不进入函数作用域、不声明参数符号，由 SemanticAnalyzer 负责

	if (m_curTok.kind != static_cast<int>(FCToken::tok_brace_open)) {
		logErrorP("expected '{' in function definition");
		return nullptr;
	}

	auto body = parseBlockExpr();
	if (!body) {
		logErrorP("failed to parse function body");
		return nullptr;
	}

	return std::make_unique<FCFunctionAST>(std::move(Proto), std::move(body));
}

std::unique_ptr<FCExprAST> Parser::parseIfExpr() {
	// 纯语法：不进入作用域守卫，由 SemanticAnalyzer 负责

	advance();

	auto Cond = parseExpression();
	if (!Cond)
		return nullptr;

	if (m_curTok.kind != static_cast<int>(FCToken::tok_then))
		return logError("expected then");
	advance();

	auto Then = parseExpression();
	if (!Then)
		return nullptr;

	if (m_curTok.kind != static_cast<int>(FCToken::tok_else))
		return logError("expected else");
	advance();

	auto Else = parseExpression();
	if (!Else)
		return nullptr;

	return std::make_unique<FCIfExprAST>(std::move(Cond), std::move(Then),
		std::move(Else));
}

std::unique_ptr<FCExprAST> Parser::parseForExpr() {
	// 纯语法：不进入作用域守卫、不声明循环变量，由 SemanticAnalyzer 负责

	advance();

	if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier))
		return logError("expected identifier after for");

	std::string VarName = m_curTok.lexeme;
	advance();

	VarDeclPtr decl = std::make_shared<VarDecl>(VarName, "int");

	if (m_curTok.kind != static_cast<int>(FCToken::tok_assign))
		return logError("expected '=' after for variable");
	advance();

	auto Start = parseExpression();
	if (!Start)
		return nullptr;

	if (m_curTok.kind != static_cast<int>(FCToken::tok_comma))
		return logError("expected ',' after for start value");
	advance();

	auto End = parseExpression();
	if (!End)
		return nullptr;

	std::unique_ptr<FCExprAST> Step;
	if (m_curTok.kind == static_cast<int>(FCToken::tok_comma)) {
		advance();
		Step = parseExpression();
		if (!Step)
			return nullptr;
	}

	if (m_curTok.kind != static_cast<int>(FCToken::tok_in))
		return logError("expected 'in' after for");
	advance();

	auto Body = parseExpression();
	if (!Body)
		return nullptr;

	return std::make_unique<FCForExprAST>(decl, std::move(Start), std::move(End),
		std::move(Step), std::move(Body));
}

std::unique_ptr<FCExprAST> Parser::parseVarDeclExpr() {
	advance();
	if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier)) {
		return logError("expected identifier after var");
	}
	std::string varName = m_curTok.lexeme;
	advance();
	if (m_curTok.kind != static_cast<int>(FCToken::tok_colon)) {
		return logError("expected ':' after var name");
	}
	advance();
	if (m_curTok.kind != static_cast<int>(FCToken::tok_identifier)) {
		return logError("expected type after ':'");
	}
	std::string typeName = m_curTok.lexeme;
	if (typeName != "int" && typeName != "double" && typeName != "string") {
		return logError("invalid type");
	}
	advance();
	if (m_curTok.kind != static_cast<int>(FCToken::tok_assign)) {
		return logError("expected '=' after type (initialization required)");
	}
	advance();
	auto init = parseExpression();
	if (!init)
		return nullptr;

	auto decl = std::make_shared<VarDecl>(varName, typeName);
	// 纯语法：不调用 declareVariable/bindVariable，由 SemanticAnalyzer 负责
	return std::make_unique<FCVarDeclExprAST>(decl, std::move(init));
}

std::unique_ptr<FCExprAST> Parser::parseBlockExpr() {
	// 纯语法：不进入作用域守卫，由 SemanticAnalyzer 负责

	if (m_curTok.kind != static_cast<int>(FCToken::tok_brace_open))
		return logError("expected '{' to start block");
	advance();

	std::vector<std::unique_ptr<FCExprAST>> expressions;
	// 块的终止符是 '}'，由块自己消费；';' 只是表达式分隔符
	while (m_curTok.kind != static_cast<int>(FCToken::tok_brace_close)) {
		if (m_curTok.kind == static_cast<int>(FCToken::tok_eof))
			return logError("unterminated block");
		if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon))
			return logError("empty statement in block");

		auto expr = parseExpression();
		if (!expr)
			return nullptr;
		expressions.push_back(std::move(expr));

		if (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon)) {
			advance();
			continue;   // ';' 后允许直接 '}'（末尾分隔符合法）
		}
		if (m_curTok.kind == static_cast<int>(FCToken::tok_brace_close))
			break;      // 最后一条不带 ';' 直接收尾，同样合法
		return logError("expected ';' or '}' in block");
	}
	advance();  // 消费 '}'

	return std::make_unique<FCBlockExprAST>(std::move(expressions));
}

std::unique_ptr<FCExprAST> Parser::parseSeqExpr() {
	auto first = parseExpression();
	if (!first)
		return nullptr;

	std::vector<std::unique_ptr<FCExprAST>> exprList;
	exprList.push_back(std::move(first));

	// 终止判定唯一权威：下一个 token 不是表达式起始符即终止，
	// 由外层结构（}、)、else、in、def、eof）消费，序列不做猜测
	while (m_curTok.kind == static_cast<int>(FCToken::tok_semicolon)) {
		advance();
		if (!startsExpression(m_curTok.kind))
			break;
		auto next = parseExpression();
		if (!next)
			return nullptr;
		exprList.push_back(std::move(next));
	}

	// 单个表达式无需包装成 SeqExprAST
	if (exprList.size() == 1)
		return std::move(exprList[0]);

	return std::make_unique<FCSeqExprAST>(std::move(exprList));
}

} // namespace FC
