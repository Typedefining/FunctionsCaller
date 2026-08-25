#pragma once

#include "semantic.h"

class FCScanner
{
public:
	FCScanner();
	void resetState();
	::std::unique_ptr<FCExprClass::FCExprAST> analysis(const ::std::string &);
	const FCMarks::FCSemanticContext& semanticContext() const { return m_semanticContext; }

private:
	FCMarks::FCSemanticContext m_semanticContext;

	// 当前处理的标识
	int m_curTok;
	// 最新待处理字符
	int m_lastChar;
	// 输入字符串迭代器
	::std::string::iterator m_idx;

	// 当前id
	::std::string m_identifierStr;
	// 当前激活的函数
	::std::string m_currentFunc;
	// 记录字符串字面量
	::std::string m_stringLiteral;

	// 分发标志
	bool m_curDouble;
	// 记录整型字面量
	int m_numIntgerVal;
	// 记录浮点字面量
	double m_numFloatVal;

	// 输入的字符串
	::std::string m_inputsBuffer;

	private:
	int getNextToken();
	int getTok();
	int getTokPrecedence();

	::std::unique_ptr<FCExprClass::FCExprAST> logError(const char *Str);
	::std::unique_ptr<FCExprClass::FCPrototypeAST> logErrorP(const char *Str);

	::std::unique_ptr<FCExprClass::FCExprAST> parseExpression();
	::std::unique_ptr<FCExprClass::FCExprAST> parseNumberExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parseStringExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parseParenExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parseBraceExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parseIdentifierExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parsePrimary();
	::std::unique_ptr<FCExprClass::FCExprAST> parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprClass::FCExprAST> LHS);
	::std::unique_ptr<FCExprClass::FCPrototypeAST> parsePrototype();
	::std::unique_ptr<FCExprClass::FCFunctionAST> parseDefinition();
	::std::unique_ptr<FCExprClass::FCExprAST> parseSeqExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> ParseIfExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> ParseForExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> ParseVarExpr();
	::std::unique_ptr<FCExprClass::FCExprAST> parseBlockExpr();
};
