#include <cctype>
#include <cstdlib>
#include <functional>
#include <map>
#include <iostream>
#include "scanner.h"

using namespace FCExprClass;
using namespace FCMarks;

FCScanner::FCScanner(FCMyFunctional* funcTable) :mp_findFunc(funcTable)
{
	//初始状态
	m_curTok = 0;
	m_lastChar = ' ';
	m_numIntgerVal = 0;
	m_numFloatVal = 0.0;
	m_curDouble = false;
	m_currentFunc = "";
	m_identifierStr = "";
	m_stringLiteral = "";
	m_inputsBuffer = "";
	m_idx = m_identifierStr.begin();
}
FCScanner::~FCScanner()
{
}

::std::unique_ptr<FCExprClass::FCExprAST> FCScanner::analysis(const ::std::string& inputStr)
{
	m_inputsBuffer = inputStr;
	m_idx = m_inputsBuffer.begin();

	using namespace FCMarks;

	//有效表达式要么为def函数定义
	//要么为匿名函数调用
	getNextToken();
	switch (m_curTok)
	{
	case static_cast<int>(FCToken::tok_eof):
		break;
	case ';':
		break;
	case static_cast<int>(FCToken::tok_def):
		return handledDefinition();
	default:
		return handledTopLevelExpression();
	}
	return nullptr;
}


void FCScanner::resetState()
{
	//解析过程出错时，重置扫描器状态
	m_curTok = 0;
	m_lastChar = ' ';
	m_numIntgerVal = 0;
	m_numFloatVal = 0.0;
	m_curDouble = false;
	m_currentFunc = "";
	m_identifierStr = "";
	m_stringLiteral = "";
	m_inputsBuffer = "";
	m_idx = m_identifierStr.begin();
}

int FCScanner::getNextToken()
{
	//启动分析后，m_curTok为当前token
	//getTok将返回下一个token
	return m_curTok = getTok();
}
int FCScanner::getTok()
{
	using namespace FCMarks;

	//跳过空白符
	while (isspace(m_lastChar))
		m_lastChar = *m_idx++;

	//此时为标识符
	//函数标识符、变量标识符、函数定义标识符
	if (isalpha(m_lastChar))
	{
		m_identifierStr = m_lastChar;
		while (isalnum((m_lastChar = *m_idx++)))
			m_identifierStr += m_lastChar;
		if (m_identifierStr == "def")
			return static_cast<int>(FCToken::tok_def);
		return static_cast<int>(FCToken::tok_identifier);
	}
	//此时为数字字面量
	//支持整数、浮点数
	if (isdigit(m_lastChar) || m_lastChar == '.')
	{
		::std::string NumStr;
		do
		{
			NumStr += m_lastChar;
			m_lastChar = *m_idx++;
			if (m_lastChar == '.')
				m_curDouble = true;
		} while (isdigit(m_lastChar) || m_lastChar == '.');
		if (m_curDouble)
		{
			//浮点数支持
			m_numFloatVal = strtod(NumStr.c_str(), 0);
		}
		else
		{
			m_numIntgerVal = atoi(NumStr.c_str());
		}
		return static_cast<int>(FCToken::tok_number);
	}
	//字符串标识符
	if (m_lastChar == '"')
	{
		m_lastChar = *m_idx++;
		m_identifierStr = m_lastChar;
		while (isalpha((m_lastChar = *m_idx++)))
			m_identifierStr += m_lastChar;
		if (m_lastChar != '"')
			return static_cast<int>(FCToken::tok_eof);
		m_lastChar = *m_idx++;
		m_stringLiteral = m_identifierStr;
		return static_cast<int>(FCToken::tok_string);
	}
	//注释标识符
	if (m_lastChar == '#')
	{
		do
			m_lastChar = *m_idx++;
		while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');
		if (m_lastChar != EOF)
			return getTok();
	}
	//解析结束
	if (m_lastChar == EOF || m_idx == m_inputsBuffer.end())
	{
		m_lastChar = ' ';
		return static_cast<int>(FCToken::tok_eof);
	}
	//其他值
	int ThisChar = m_lastChar;
	m_lastChar = *m_idx++;
	return ThisChar;
}

std::unique_ptr<FCExprAST> FCScanner::logError(const char* Str)
{
	fprintf(stderr, "LogError: %s\n", Str);
	return nullptr;
}
std::unique_ptr<FCPrototypeAST> FCScanner::logErrorP(const char* Str)
{
	(void)logError(Str);
	return nullptr;
}

int FCScanner::getTokPrecedence()
{
	using namespace FCMarks;
	if (!isascii(m_curTok))
		return -1;

	//是否为二元操作
	int TokPrec = binopPrecedence[m_curTok];
	if (TokPrec <= 0)
		return -1;
	return TokPrec;
}

::std::unique_ptr<FCExprAST> FCScanner::parsePrimary()
{
	using namespace FCMarks;
	switch (m_curTok)
	{
	default:
		return logError("unknown FCToken when expecting an expression");
		break;
	case static_cast<int>(FCToken::tok_identifier):
		return parseIdentifierExpr();
		break;
	case static_cast<int>(FCToken::tok_number):
		return parseNumberExpr();
		break;
	case static_cast<int>(FCToken::tok_string):
		return parseStringExpr();
		break;
	case '(':
		return parseParenExpr();
		break;
	case static_cast<int>(FCToken::tok_if):
		return ParseIfExpr();
		break;
	case static_cast<int>(FCToken::tok_for):
		return ParseForExpr();
		break;
	}
}

::std::unique_ptr<FCExprAST> FCScanner::parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprAST> LHS)
{
	while (1)
	{
		int TokPrec = getTokPrecedence();

		//无合法右操作数
		if (TokPrec < ExprPrec)
			return LHS;

		int Binop = m_curTok;
		getNextToken();

		//获取右操作数
		auto RHS = parsePrimary();
		if (!RHS)
			return nullptr;
		int NextPrec = getTokPrecedence();
		//当前操作符优先级低于下一个，构建子树
		if (TokPrec < NextPrec)
		{
			RHS = parseBinOpRHS(TokPrec + 1, ::std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		//合并子树
		LHS = ::std::make_unique<FCBinaryExprAST>(Binop,
			::std::move(LHS),
			::std::move(RHS));
	}
}

::std::unique_ptr<FCExprAST> FCScanner::parseExpression()
{
	//将非函数定义表达式视为二元操作
	auto LHS = parsePrimary();
	if (!LHS)
		return nullptr;
	return parseBinOpRHS(0, std::move(LHS));
}
::std::unique_ptr<FCExprAST> FCScanner::parseNumberExpr()
{
	//区分浮点、整型
	if (m_curDouble)
	{
		m_curDouble = false;
		auto Result = ::std::make_unique<FCNumberExprAST>(m_numFloatVal);
		getNextToken();
		return ::std::move(Result);
	}
	else
	{
		auto Result = ::std::make_unique<FCNumberExprAST>(m_numIntgerVal);
		getNextToken();
		return ::std::move(Result);
	}
}
::std::unique_ptr<FCExprAST> FCScanner::parseStringExpr()
{
	auto Result = ::std::make_unique<FCStringExprAST>(m_stringLiteral);
	getNextToken();
	return ::std::move(Result);
}

::std::unique_ptr<FCExprAST> FCScanner::parseParenExpr()
{
	getNextToken(); // 拿掉(
	auto V = parseExpression();
	if (!V)
		return nullptr;

	if (m_curTok != ')')
		return logError("expected ')'");
	getNextToken(); // 拿掉)
	return V;
}
::std::unique_ptr<FCExprAST> FCScanner::parseIdentifierExpr()
{
	std::string IdName = m_identifierStr;

	getNextToken(); //拿掉identifier.

	if (m_curTok != '(') //变量名
	{
		//找到符号表中当前函数内的变量
		if (m_currentFunc == "")
		{//不在函数定义时使用变量，语法错误
			return nullptr;
		}
		//从符号表中找到待处理函数
		auto tarFuncItr = varTableInFunc->find(m_currentFunc);
		if (tarFuncItr == varTableInFunc->end())
		{
			return nullptr;
		}
		FCExprClass::FCVariableExprAST itm2{ "","","" };
		auto tarFunc = tarFuncItr->second;
		//在待处理函数中找到待处理变量
		auto findResItr = tarFunc.find(IdName);
		if (findResItr == tarFunc.end())
		{
			return nullptr;
		}
		//获取符号表中的变量
		auto& realItem = findResItr->second;
		return std::make_unique<FCVariableExprAST>(realItem);
	}

	//函数调用
	getNextToken(); // 拿掉(
	std::vector<std::unique_ptr<FCExprAST>> funCallArgs;
	if (m_curTok != ')')
	{
		while (1)
		{
			if (auto funCallArg = parseExpression())
				funCallArgs.push_back(std::move(funCallArg));
			else
				return nullptr;
			if (m_curTok == ')')
				break;
			if (m_curTok != ',')
				return logError("Expected ')' or ',' in argument list");
			getNextToken();
		}
	}
	//拿掉).
	getNextToken();

	return std::make_unique<FCCallExprAST>(IdName, std::move(funCallArgs));
}

::std::unique_ptr<FCPrototypeAST> FCScanner::parsePrototype()
{
	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
		return logErrorP("Expected function name in prototype");

	//变量存在的函数
	std::string funName = m_identifierStr;
	getNextToken();

	if (m_curTok != '(')
		return logErrorP("Expected '(' in prototype");

	//获取所有的参数
	std::vector<FCVariableExprAST> ArgNames;
	std::string varName = "";
	std::string typeName = "";
	while (getNextToken() == static_cast<int>(FCToken::tok_identifier))
	{
		varName = m_identifierStr;
		getNextToken();
		if (m_curTok != ':')
			return nullptr;

		getNextToken();
		typeName = m_identifierStr;
		if (typeName != "int" && typeName != "double" && typeName != "string")
			return nullptr;

		//记录参数名和类型名
		FCVariableExprAST varItem{ varName, typeName,funName };
		ArgNames.push_back(varItem);
		auto varOfFuncItr = varTableInFunc->find(funName);
		//并放入符号表
		if (varOfFuncItr != varTableInFunc->end())
		{
			varOfFuncItr->second.insert(
				{
					varName,
					varItem
				}
			);
		}
		else {
			varTableInFunc->insert(
				{
					funName,
					{
						{
							varName,
							varItem
						}
					}
				}
			);
		}
	}
	if (m_curTok != ')')
		return logErrorP("Expected ')' in prototype");
	getNextToken(); //拿掉).
	return std::make_unique<FCPrototypeAST>(funName, std::move(ArgNames));
}
::std::unique_ptr<FCFunctionAST> FCScanner::parseDefinition()
{
	getNextToken(); //拿掉def
	auto Proto = parsePrototype();
	if (!Proto)
		return nullptr;
	//该变量被用于生成正确的变量-函数对应关系
	m_currentFunc = Proto->m_funcName;
	if (auto E = parseExpression())
	{
		//清除标记
		m_currentFunc = "";
		return std::make_unique<FCFunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

::std::unique_ptr<FCExprAST> FCScanner::ParseIfExpr()
{
	getNextToken();  // eat the if.

	// condition.
	auto Cond = parseExpression();
	if (!Cond)
		return nullptr;

	if (m_curTok != static_cast<int>(FCToken::tok_then))
		return logError("expected then");
	getNextToken();

	auto Then = parseExpression();
	if (!Then)
		return nullptr;

	if (m_curTok != static_cast<int>(FCToken::tok_else))
		return logError("expected else");

	getNextToken();

	auto Else = parseExpression();
	if (!Else)
		return nullptr;

	return std::make_unique<FCIfExprAST>(std::move(Cond), std::move(Then),
										std::move(Else));
}

::std::unique_ptr<FCExprAST> FCScanner::ParseForExpr()
{
	getNextToken();  // eat the for.

	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
		return logError("expected identifier after for");

	std::string IdName = m_identifierStr;
	getNextToken();  // eat identifier.

	if (m_curTok != '=')
		return logError("expected '=' after for");
	getNextToken();  // eat '='.


	auto Start = parseExpression();
	if (!Start)
		return nullptr;
	if (m_curTok != ',')
		return logError("expected ',' after for start value");
	getNextToken();

	auto End = parseExpression();
	if (!End)
		return nullptr;

	// The step value is optional.
	std::unique_ptr<FCExprAST> Step;
	if (m_curTok == ',') {
		getNextToken();
		Step = parseExpression();
		if (!Step)
			return nullptr;
	}

	if (m_curTok != static_cast<int>(FCToken::tok_in))
		return logError("expected 'in' after for");
	getNextToken();  // eat 'in'.

	auto Body = parseExpression();
	if (!Body)
		return nullptr;

	return std::make_unique<FCForExprAST>(IdName, std::move(Start),
										std::move(End), std::move(Step),
										std::move(Body));
}

::std::unique_ptr<FCExprAST> FCScanner::parseTopLevelExpr()
{
	if (auto E = parseExpression())
	{
		//有名函数亦被封装为匿名函数调用
		auto Proto = std::make_unique<FCPrototypeAST>("__anon_expr",
			std::vector<FCVariableExprAST>());
		return std::make_unique<FCFunctionAST>(std::move(Proto), std::move(E));
	}
	return nullptr;
}

::std::unique_ptr<FCExprClass::FCExprAST> FCScanner::handledDefinition()
{
	return parseDefinition();
}
::std::unique_ptr<FCExprClass::FCExprAST> FCScanner::handledTopLevelExpression()
{
	if (auto itm = parseTopLevelExpr())
	{
		return itm;
	}
	else
	{
		::std::fprintf(stderr, "Syntax Error\n");
	}
	return nullptr;
}