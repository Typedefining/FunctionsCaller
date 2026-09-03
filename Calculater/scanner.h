#pragma once

#include "token.h"
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

	int m_curTok;
	int m_lastChar;
	::std::string::iterator m_idx;

	::std::string m_identifierStr;
	::std::string m_stringLiteral;
	bool m_curDouble;
	int m_numIntgerVal;
	double m_numFloatVal;
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
