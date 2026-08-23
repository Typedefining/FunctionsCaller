#include <cctype>
#include <cstdlib>
#include "scanner.h"

using namespace FCExprClass;
using namespace FCMarks;

FCScanner::FCScanner()
{
	// 初始状态
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
// def aaa(a:int,b:double) a+b*2.0;
::std::unique_ptr<FCExprAST> FCScanner::analysis(const ::std::string &inputStr)
{
	resetState();
	m_inputsBuffer = inputStr;
	m_idx = m_inputsBuffer.begin();

	getNextToken();

	std::vector<std::unique_ptr<FCExprAST>> statements;

	// 循环解析所有语句，直到遇到文件结束
	while (m_curTok != static_cast<int>(FCToken::tok_eof))
	{
		std::unique_ptr<FCExprAST> stmt;

		switch (m_curTok)
		{
		case static_cast<int>(FCToken::tok_def):
			stmt = parseDefinition();
			break;
		case ';':
			// 空语句，跳过
			getNextToken();
			continue;
		default:
			stmt = parseSeqExpr();
			break;
		}

		if (stmt)
		{
			statements.push_back(std::move(stmt));
		}
		else
		{
			// 解析失败，跳过当前token继续尝试
			getNextToken();
		}
	}

	// 如果只有一个语句，直接返回它
	if (statements.size() == 1)
	{
		return std::move(statements[0]);
	}

	// 如果有多个语句，包装成程序节点
	return std::make_unique<FCProgramAST>(std::move(statements));
}

void FCScanner::resetState()
{
	// 解析过程出错时，重置扫描器状态
	m_semanticContext.reset();
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
	// 启动分析后，m_curTok为当前token
	// getTok将返回下一个token
	return m_curTok = getTok();
}
int FCScanner::getTok()
{
	auto readChar = [this]() -> int
	{
		if (m_idx == m_inputsBuffer.end())
			return EOF;
		return static_cast<unsigned char>(*m_idx++);
	};

	// 跳过空白符
	while (m_lastChar != EOF && isspace(static_cast<unsigned char>(m_lastChar)))
		m_lastChar = readChar();

	// 此时为标识符
	// 函数标识符、变量标识符、函数定义标识符
	if (m_lastChar != EOF && isalpha(static_cast<unsigned char>(m_lastChar)))
	{
		m_identifierStr = m_lastChar;
		while ((m_lastChar = readChar()) != EOF &&
			isalnum(static_cast<unsigned char>(m_lastChar)))
			m_identifierStr += m_lastChar;
		if (m_identifierStr == "def")
			return static_cast<int>(FCToken::tok_def);
		if (m_identifierStr == "if")
			return static_cast<int>(FCToken::tok_if);
		if (m_identifierStr == "for")
			return static_cast<int>(FCToken::tok_for);
		if (m_identifierStr == "then")
			return static_cast<int>(FCToken::tok_then);
		if (m_identifierStr == "else")
			return static_cast<int>(FCToken::tok_else);
		if (m_identifierStr == "in")
			return static_cast<int>(FCToken::tok_in);
		if (m_identifierStr == "var")
			return static_cast<int>(FCToken::tok_var);
		// 默认视为变量标识符

		return static_cast<int>(FCToken::tok_identifier);
	}
	// 此时为数字字面量
	// 支持整数、浮点数
	if (m_lastChar != EOF &&
		(isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.'))
	{
		::std::string NumStr;
		do
		{
			NumStr += m_lastChar;
			m_lastChar = readChar();
			if (m_lastChar == '.')
				m_curDouble = true;
		} while (m_lastChar != EOF &&
			(isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.'));
		if (m_curDouble)
		{
			// 浮点数支持
			m_numFloatVal = strtod(NumStr.c_str(), 0);
		}
		else
		{
			m_numIntgerVal = atoi(NumStr.c_str());
		}
		return static_cast<int>(FCToken::tok_number);
	}
	// 字符串标识符
	if (m_lastChar == '"')
	{
		m_lastChar = readChar();
		m_identifierStr.clear();
		while (m_lastChar != EOF && m_lastChar != '"')
		{
			m_identifierStr += static_cast<char>(m_lastChar);
			m_lastChar = readChar();
		}
		if (m_lastChar != '"')
			return static_cast<int>(FCToken::tok_eof);
		m_lastChar = readChar();
		m_stringLiteral = m_identifierStr;
		return static_cast<int>(FCToken::tok_string);
	}
	// 注释标识符
	if (m_lastChar == '#')
	{
		do
			m_lastChar = readChar();
		while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');
		if (m_lastChar != EOF)
			return getTok();
	}
	// 解析结束
	if (m_lastChar == EOF)
	{
		m_lastChar = ' ';
		return static_cast<int>(FCToken::tok_eof);
	}
	// 其他值
	int ThisChar = m_lastChar;
	m_lastChar = readChar();
	return ThisChar;
}

std::unique_ptr<FCExprAST> FCScanner::logError(const char *Str)
{
	fprintf(stderr, "LogError: %s\n", Str);
	return nullptr;
}
std::unique_ptr<FCPrototypeAST> FCScanner::logErrorP(const char *Str)
{
	(void)logError(Str);
	return nullptr;
}

int FCScanner::getTokPrecedence()
{
	if (!isascii(m_curTok))
		return -1;

	// 是否为二元操作
	int TokPrec = m_semanticContext.getOperatorPrecedence(
		static_cast<char>(m_curTok));
	if (TokPrec <= 0)
		return -1;
	return TokPrec;
}

::std::unique_ptr<FCExprAST> FCScanner::parsePrimary()
{
	switch (m_curTok)
	{
	default:
		return logError("unknown FCToken when expecting an expression");
	case static_cast<int>(FCToken::tok_identifier):
		return parseIdentifierExpr();
	case static_cast<int>(FCToken::tok_number):
		return parseNumberExpr();
	case static_cast<int>(FCToken::tok_string):
		return parseStringExpr();
	case '(':
		return parseParenExpr();
	case static_cast<int>(FCToken::tok_if):
		return ParseIfExpr();
	case static_cast<int>(FCToken::tok_for):
		return ParseForExpr();
	case static_cast<int>(FCToken::tok_var):
		return ParseVarExpr();
	}
}

::std::unique_ptr<FCExprAST> FCScanner::parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprAST> LHS)
{
	while (1)
	{
		int TokPrec = getTokPrecedence();

		// 无合法右操作数
		if (TokPrec < ExprPrec)
			return LHS;

		int Binop = m_curTok;
		getNextToken();

		// 获取右操作数
		auto RHS = parsePrimary();
		if (!RHS)
			return nullptr;
		int NextPrec = getTokPrecedence();
		// 当前操作符优先级低于下一个，构建子树
		if (TokPrec < NextPrec)
		{
			RHS = parseBinOpRHS(TokPrec + 1, ::std::move(RHS));
			if (!RHS)
				return nullptr;
		}

		// 合并子树
		LHS = ::std::make_unique<FCBinaryExprAST>(Binop,
												  ::std::move(LHS),
												  ::std::move(RHS));
	}
}

::std::unique_ptr<FCExprAST> FCScanner::parseExpression()
{
	// 将非函数定义表达式视为二元操作
	auto LHS = parsePrimary();
	if (!LHS)
		return nullptr;
	return parseBinOpRHS(0, std::move(LHS));
}
::std::unique_ptr<FCExprAST> FCScanner::parseNumberExpr()
{
	// 区分浮点、整型
	if (m_curDouble)
	{
		m_curDouble = false;
		auto Result = ::std::make_unique<FCNumberExprAST>(m_numFloatVal);
		getNextToken();
		return Result;
	}
	else
	{
		auto Result = ::std::make_unique<FCNumberExprAST>(m_numIntgerVal);
		getNextToken();
		return Result;
	}
}
::std::unique_ptr<FCExprAST> FCScanner::parseStringExpr()
{
	auto Result = ::std::make_unique<FCStringExprAST>(m_stringLiteral);
	getNextToken();
	return Result;
}

::std::unique_ptr<FCExprAST> FCScanner::parseParenExpr()
{
	getNextToken(); // 拿掉(
	auto V = parseSeqExpr();
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
	getNextToken(); // 拿掉 identifier

	// 如果后面不是 '(', 那就是变量引用（静态绑定：在 parse 时查找 VarDecl 并把 decl 绑入 AST）
	if (m_curTok != '(')
	{
		if (m_currentFunc.empty())
		{
			return nullptr; // 在顶层使用变量，当前语法不允许
		}
		auto decl = m_semanticContext.lookupVariableDecl(m_currentFunc, IdName);
		if (!decl)
		{
			// 找不到则解析错误（静态检查）
			return nullptr;
		}
		return std::make_unique<FCVariableExprAST>(decl);
	}

	// 函数调用分支
	getNextToken(); // 拿掉 '('

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
	// 吃掉 ')'
	getNextToken();

	return std::make_unique<FCCallExprAST>(IdName, std::move(funCallArgs));
}

::std::unique_ptr<FCPrototypeAST> FCScanner::parsePrototype()
{
	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
		return logErrorP("Expected function name in prototype");

	// 变量存在的函数
	std::string funName = m_identifierStr;
	getNextToken();

	if (m_curTok != '(')
		return logErrorP("Expected '(' in prototype");

	// 获取所有的参数
	std::vector<std::tuple<std::string, std::string>> Args;
	while (getNextToken() == static_cast<int>(FCToken::tok_identifier))
	{
		std::string varName = m_identifierStr;
		getNextToken();
		if (m_curTok != ':')
			return nullptr;

		getNextToken();
		std::string typeName = m_identifierStr;
		if (typeName != "int" && typeName != "double" && typeName != "string")
			return nullptr;

		Args.push_back(std::make_tuple(varName, typeName));
	}

	if (m_curTok != ')')
		return logErrorP("Expected ')' in prototype");
	getNextToken(); // 拿掉).

	m_semanticContext.pushScopeForFunc(funName);

	std::vector<FCVariableExprAST> ArgNames;
	for (auto &arg : Args)
	{
		auto decl = std::make_shared<VarDecl>(std::get<0>(arg), std::get<1>(arg));
		m_semanticContext.insertVariableInCurrentScope(
			funName, std::get<0>(arg), decl);
		ArgNames.push_back(FCVariableExprAST(decl));
	}
	return std::make_unique<FCPrototypeAST>(funName, std::move(ArgNames));
}
::std::unique_ptr<FCFunctionAST> FCScanner::parseDefinition()
{
	getNextToken(); // 吃掉 'def'
	auto Proto = parsePrototype();
	if (!Proto)
		return nullptr;

	// 将当前函数名设置为 Proto 名
	m_currentFunc = Proto->m_funcName;

	if (auto E = parseSeqExpr())
	{
		// 在函数解析完成后，为该函数的所有 VarDecl 分配连续的 slot（包括形参 & 局部）
		int slot = 0;
		auto& decls = m_semanticContext.functionDeclarations(m_currentFunc);
		for (auto &d : decls)
		{
			if (d->slot < 0)
				d->slot = slot++;
		}
		m_semanticContext.popScopeForFunc(m_currentFunc);

		// 重置当前函数名
		m_currentFunc = "";
		return std::make_unique<FCFunctionAST>(std::move(Proto), std::move(E), slot);
	}

	return nullptr;
}

::std::unique_ptr<FCExprAST> FCScanner::ParseIfExpr()
{
	getNextToken(); // eat the if.

	m_semanticContext.pushScopeForFunc(m_currentFunc);
	m_semanticContext.popScopeForFunc(m_currentFunc);
	// condition.
	auto Cond = parseExpression();
	if (!Cond)
		return nullptr;

	if (m_curTok != static_cast<int>(FCToken::tok_then))
		return logError("expected then");
	getNextToken();

	m_semanticContext.pushScopeForFunc(m_currentFunc);
	auto Then = parseSeqExpr();
	m_semanticContext.popScopeForFunc(m_currentFunc);
	if (!Then)
		return nullptr;

	if (m_curTok != static_cast<int>(FCToken::tok_else))
		return logError("expected else");

	getNextToken();

	m_semanticContext.pushScopeForFunc(m_currentFunc);
	auto Else = parseSeqExpr();
	m_semanticContext.popScopeForFunc(m_currentFunc);
	if (!Else)
		return nullptr;

	return std::make_unique<FCIfExprAST>(std::move(Cond), std::move(Then),
										 std::move(Else));
}

::std::unique_ptr<FCExprAST> FCScanner::ParseForExpr()
{
	getNextToken(); // 吃掉 'for'

	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
		return logError("expected identifier after for");

	std::string VarName = m_identifierStr;
	getNextToken(); // 吃掉 identifier

	// 在解析期引入新作用域并声明循环变量
	m_semanticContext.pushScopeForFunc(m_currentFunc);
	// 默认使用 double 类型（或根据你语法读取类型信息）
	VarDeclPtr decl = std::make_shared<VarDecl>(VarName, "int");
	m_semanticContext.insertVariableInCurrentScope(m_currentFunc, VarName, decl);

	if (m_curTok != '=')
		return logError("expected '=' after for variable");
	getNextToken(); // 吃掉 '='

	auto Start = parseExpression();
	if (!Start)
		return nullptr;

	if (m_curTok != ',')
		return logError("expected ',' after for start value");
	getNextToken();

	auto End = parseExpression();
	if (!End)
		return nullptr;

	std::unique_ptr<FCExprAST> Step;
	if (m_curTok == ',')
	{
		getNextToken();
		Step = parseExpression();
		if (!Step)
			return nullptr;
	}

	if (m_curTok != static_cast<int>(FCToken::tok_in))
		return logError("expected 'in' after for");
	getNextToken(); // 吃掉 'in'

	auto Body = parseSeqExpr();

	m_semanticContext.popScopeForFunc(m_currentFunc);

	// 注意：这里返回的 ForExprAST 应该包含 decl 或者 Body 已经通过 VarExpr 绑定了 decl
	// 下面假设存在一个 FCForExprAST 构造函数接收 decl
	return std::make_unique<FCForExprAST>(decl, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}

std::unique_ptr<FCExprAST> FCScanner::ParseVarExpr()
{
	getNextToken(); // 吃掉 var
	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
	{
		return logError("expected identifier after var");
	}
	std::string varName = m_identifierStr;
	getNextToken(); // 吃掉 name
	if (m_curTok != ':')
	{
		return logError("expected ':' after var name");
	}
	getNextToken(); // 吃掉 :
	if (m_curTok != static_cast<int>(FCToken::tok_identifier))
	{
		return logError("expected type after ':'");
	}
	std::string typeName = m_identifierStr;
	if (typeName != "int" && typeName != "double" && typeName != "string")
	{
		return logError("invalid type");
	}
	getNextToken(); // 吃掉 type
	if (m_curTok != '=')
	{
		return logError("expected '=' after type (initialization required)");
	}
	getNextToken(); // 吃掉 =
	auto init = parseExpression();
	if (!init)
		return nullptr;

	// 静态插入作用域
	if (m_currentFunc.empty())
	{
		return logError("var declaration outside function not allowed");
	}
	if (m_semanticContext.lookupVariableDecl(m_currentFunc, varName))
	{
		return logError("variable redeclaration");
	}
	auto decl = std::make_shared<VarDecl>(varName, typeName);
	m_semanticContext.insertVariableInCurrentScope(m_currentFunc, varName, decl);

	return std::make_unique<FCVarDeclExprAST>(decl, std::move(init));
}

/// 解析表达式序列：expr (','|';' expr)*
::std::unique_ptr<FCExprAST> FCScanner::parseSeqExpr()
{
	auto first = parseExpression(); // 先解析一个表达式
	if (!first)
		return nullptr;

	std::vector<std::unique_ptr<FCExprAST>> exprList;
	exprList.push_back(std::move(first));

	while (m_curTok == ';')
	{
		getNextToken(); // 跳过分隔符
		if (m_curTok == static_cast<int>(FCToken::tok_eof) ||
			m_curTok == static_cast<int>(FCToken::tok_def) ||
			m_curTok == static_cast<int>(FCToken::tok_else) ||
			m_curTok == static_cast<int>(FCToken::tok_end))
			break;
		auto next = parseExpression();
		if (!next)
			break;
		exprList.push_back(std::move(next));
	}

	// 如果只有一个表达式，直接返回，不用包成 SeqExprAST
	if (exprList.size() == 1)
		return std::move(exprList[0]);

	return std::make_unique<FCSeqExprAST>(std::move(exprList));
}
