#include "scanner.h"
#include <cctype>
#include <cstdlib>
#include <tuple>

using namespace FCExprClass;
using namespace FCMarks;

FCScanner::FCScanner() {
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
// def aaa(a:int, b:double) a+b*2.0;
::std::unique_ptr<FCExprAST>
FCScanner::analysis(const ::std::string &inputStr) {
  resetState();
  m_inputsBuffer = inputStr;
  m_idx = m_inputsBuffer.begin();

	m_semanticContext.pushScope();
  getNextToken();

  std::vector<std::unique_ptr<FCExprAST>> statements;

  // 循环解析所有语句，直到遇到文件结束
  while (m_curTok != static_cast<int>(FCToken::tok_eof)) {
    std::unique_ptr<FCExprAST> stmt;

    switch (m_curTok) {
    case static_cast<int>(FCToken::tok_def):
      stmt = parseDefinition();
      break;
    case ';':
      getNextToken();
      continue;
    default:
      stmt = parseSeqExpr();
      break;
    }

    if (stmt) {
      statements.push_back(std::move(stmt));
    } else {
      // 解析失败，跳过当前token继续尝试
      getNextToken();
    }
  }

	m_semanticContext.popScope();
  // 如果只有一个语句，直接返回它
  if (statements.size() == 1) {
    return std::move(statements[0]);
  }

  // 如果有多个语句，包装成程序节点
  return std::make_unique<FCProgramAST>(std::move(statements));
}

void FCScanner::resetState() {
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

int FCScanner::getNextToken() {
  // 启动分析后，m_curTok为当前token
  // getTok将返回下一个token
  return m_curTok = getTok();
}
int FCScanner::getTok() {
  auto readChar = [this]() -> int {
    if (m_idx == m_inputsBuffer.end())
      return EOF;
    return static_cast<unsigned char>(*m_idx++);
  };

  // 跳过空白符
  while (m_lastChar != EOF && isspace(static_cast<unsigned char>(m_lastChar)))
    m_lastChar = readChar();

  // 此时为标识符
  // 函数标识符、变量标识符、函数定义标识符
  if (m_lastChar != EOF && isalpha(static_cast<unsigned char>(m_lastChar))) {
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
      (isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.')) {
    ::std::string NumStr;
    do {
      NumStr += m_lastChar;
      m_lastChar = readChar();
      if (m_lastChar == '.')
        m_curDouble = true;
    } while (
        m_lastChar != EOF &&
        (isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.'));
    if (m_curDouble) {
      // 浮点数支持
      m_numFloatVal = strtod(NumStr.c_str(), 0);
    } else {
      m_numIntgerVal = atoi(NumStr.c_str());
    }
    return static_cast<int>(FCToken::tok_number);
  }
  // 字符串标识符
  if (m_lastChar == '"') {
    m_lastChar = readChar();
    m_identifierStr.clear();
    while (m_lastChar != EOF && m_lastChar != '"') {
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
  if (m_lastChar == '#') {
    do
      m_lastChar = readChar();
    while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');
    if (m_lastChar != EOF)
      return getTok();
  }
  // 解析结束
  if (m_lastChar == EOF) {
    m_lastChar = ' ';
    return static_cast<int>(FCToken::tok_eof);
  }
  // 其他值
  int ThisChar = m_lastChar;
  m_lastChar = readChar();
  if (ThisChar == '(')
    return static_cast<int>(FCToken::tok_parenthes_open);
  if (ThisChar == ')')
    return static_cast<int>(FCToken::tok_parenthes_close);
  if (ThisChar == '{')
    return static_cast<int>(FCToken::tok_brace_open);
  if (ThisChar == '}')
    return static_cast<int>(FCToken::tok_brace_close);
  if (ThisChar == ',')
    return static_cast<int>(FCToken::tok_comma);

  return ThisChar;
}

std::unique_ptr<FCExprAST> FCScanner::logError(const char *Str) {
  fprintf(stderr, "LogError: %s\n", Str);
  return nullptr;
}
std::unique_ptr<FCPrototypeAST> FCScanner::logErrorP(const char *Str) {
  (void)logError(Str);
  return nullptr;
}

int FCScanner::getTokPrecedence() {
  if (!isascii(m_curTok))
    return -1;

  // 是否为二元操作
  int TokPrec =
      m_semanticContext.getOperatorPrecedence(static_cast<char>(m_curTok));
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

::std::unique_ptr<FCExprAST> FCScanner::parsePrimary() {
  switch (m_curTok) {
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
    return ParseIfExpr();
  case static_cast<int>(FCToken::tok_for):
    return ParseForExpr();
  case static_cast<int>(FCToken::tok_var):
    return ParseVarExpr();
  }
}

::std::unique_ptr<FCExprAST>
FCScanner::parseBinOpRHS(int ExprPrec, std::unique_ptr<FCExprAST> LHS) {
  while (1) {
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
    if (TokPrec < NextPrec) {
      RHS = parseBinOpRHS(TokPrec + 1, ::std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // 合并子树
    LHS = ::std::make_unique<FCBinaryExprAST>(Binop, ::std::move(LHS),
                                              ::std::move(RHS));
  }
}

::std::unique_ptr<FCExprAST> FCScanner::parseExpression() {
  // 将非函数定义表达式视为二元操作
  auto LHS = parsePrimary();
  if (!LHS)
    return nullptr;
  return parseBinOpRHS(0, std::move(LHS));
}
::std::unique_ptr<FCExprAST> FCScanner::parseNumberExpr() {
  if (m_curDouble) {
    m_curDouble = false;
    auto Result = ::std::make_unique<FCNumberExprAST>(m_numFloatVal);
    getNextToken();
    return Result;
  } else {
    auto Result = ::std::make_unique<FCNumberExprAST>(m_numIntgerVal);
    getNextToken();
    return Result;
  }
}
::std::unique_ptr<FCExprAST> FCScanner::parseStringExpr() {
  auto Result = ::std::make_unique<FCStringExprAST>(m_stringLiteral);
  getNextToken();
  return Result;
}

::std::unique_ptr<FCExprAST> FCScanner::parseParenExpr() {
  getNextToken();
  auto V = parseSeqExpr();
  if (!V)
    return nullptr;

  if (m_curTok != static_cast<int>(FCToken::tok_parenthes_close))
    return logError("expected ')'");
  getNextToken();
  return V;
}

::std::unique_ptr<FCExprAST> FCScanner::parseIdentifierExpr() {
  std::string IdName = m_identifierStr;
  getNextToken();

  // 如果后面不是 '(', 那就是变量引用（静态绑定：在 parse 时查找 VarDecl 并把
  // decl 绑入 AST）
  if (m_curTok != static_cast<int>(FCToken::tok_parenthes_open)) {
    auto decl = m_semanticContext.lookupVariableDecl(m_currentFunc, IdName);
    if (!decl)
      return logError("unknown variable");
    return std::make_unique<FCVariableExprAST>(decl);
  }

  // 函数调用分支
  getNextToken();

  std::vector<std::unique_ptr<FCExprAST>> funCallArgs;
  if (m_curTok != static_cast<int>(FCToken::tok_parenthes_close)) {
    while (1) {
      if (auto funCallArg = parseExpression())
        funCallArgs.push_back(std::move(funCallArg));
      else
        return nullptr;
      if (m_curTok == static_cast<int>(FCToken::tok_parenthes_close))
        break;
      if (m_curTok != static_cast<int>(FCToken::tok_comma))
        return logError("Expected ')' or ',' in argument list");
      getNextToken();
    }
  }

  getNextToken();

  return std::make_unique<FCCallExprAST>(IdName, std::move(funCallArgs));
}

::std::unique_ptr<FCPrototypeAST> FCScanner::parsePrototype() {
  if (m_curTok != static_cast<int>(FCToken::tok_identifier))
    return logErrorP("Expected function name in prototype");

  // 变量存在的函数
  std::string funName = m_identifierStr;
  getNextToken();

  if (m_curTok != static_cast<int>(FCToken::tok_parenthes_open))
    return logErrorP("Expected '(' in prototype");

  std::vector<std::tuple<std::string, std::string>> Args;
  getNextToken();
  while (m_curTok != static_cast<int>(FCToken::tok_parenthes_close)) {
    if (m_curTok != static_cast<int>(FCToken::tok_identifier))
      return logErrorP("Expected parameter name in prototype");

    std::string varName = m_identifierStr;
    getNextToken();
    if (m_curTok != ':')
      return logErrorP("Expected ':' after parameter name");

    getNextToken();
    if (m_curTok != static_cast<int>(FCToken::tok_identifier))
      return logErrorP("Expected parameter type in prototype");
    std::string typeName = m_identifierStr;
    if (typeName != "int" && typeName != "double" && typeName != "string")
      return logErrorP("Unsupported parameter type");

    Args.push_back(std::make_tuple(varName, typeName));
    getNextToken();
    if (m_curTok == static_cast<int>(FCToken::tok_parenthes_close))
      break;
    if (m_curTok != static_cast<int>(FCToken::tok_comma))
      return logErrorP("Expected ',' between parameters");
    getNextToken();
    if (m_curTok == static_cast<int>(FCToken::tok_parenthes_close))
      return logErrorP("Expected parameter after ','");
  }

  getNextToken();



  std::vector<FCVariableExprAST> ArgNames;
  for (auto &arg : Args) {
		//仅记录函数参数个数，函数栈帧size分配在define中进行
    auto decl = std::make_shared<VarDecl>(std::get<0>(arg), std::get<1>(arg));
    ArgNames.push_back(FCVariableExprAST(decl));
  }
  return std::make_unique<FCPrototypeAST>(funName, std::move(ArgNames));
}

std::unique_ptr<FCFunctionAST> FCScanner::parseDefinition() {
  getNextToken();
  auto Proto = parsePrototype();
  if (!Proto)
    return nullptr;

  m_currentFunc = Proto->m_funcName;

  m_semanticContext.pushFunctionScope(m_currentFunc);
	for (const auto &arg : Proto->getArgs()) {
		m_semanticContext.insertVariableInCurrentScope(m_currentFunc, arg.decl->name, arg.decl);
	}

  // 将brace内的表达式视为函数体，解析后将函数体与原型绑定
  if (m_curTok != static_cast<int>(FCToken::tok_brace_open)) {
    logErrorP("expected '{' in function definition");
    return nullptr;
  }

  auto body = parseBlockExpr();
  if (!body) {
    logErrorP("failed to parse function body");
    return nullptr;
  }

  m_semanticContext.popFunctionScope(m_currentFunc);
  m_currentFunc = "";
  return std::make_unique<FCFunctionAST>(std::move(Proto), std::move(body),
                                         m_semanticContext.currentFunctionDeclarations().size());
}

std::unique_ptr<FCExprAST> FCScanner::ParseIfExpr() {
  getNextToken();

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

std::unique_ptr<FCExprAST> FCScanner::ParseForExpr() {
  getNextToken();

  if (m_curTok != static_cast<int>(FCToken::tok_identifier))
    return logError("expected identifier after for");

  std::string VarName = m_identifierStr;
  getNextToken();

  m_semanticContext.pushFunctionScope(m_currentFunc);
  VarDeclPtr decl = std::make_shared<VarDecl>(VarName, "int");
  m_semanticContext.insertVariableInCurrentScope(m_currentFunc, VarName, decl);

  if (m_curTok != '=')
    return logError("expected '=' after for variable");
  getNextToken();

  auto Start = parseExpression();
  if (!Start)
    return nullptr;

  if (m_curTok != static_cast<int>(FCToken::tok_comma))
    return logError("expected ',' after for start value");
  getNextToken();

  auto End = parseExpression();
  if (!End)
    return nullptr;

  std::unique_ptr<FCExprAST> Step;
  if (m_curTok == static_cast<int>(FCToken::tok_comma)) {
    getNextToken();
    Step = parseExpression();
    if (!Step)
      return nullptr;
  }

  if (m_curTok != static_cast<int>(FCToken::tok_in))
    return logError("expected 'in' after for");
  getNextToken();

  auto Body = parseExpression();

  m_semanticContext.popFunctionScope(m_currentFunc);

  return std::make_unique<FCForExprAST>(decl, std::move(Start), std::move(End),
                                        std::move(Step), std::move(Body));
}

std::unique_ptr<FCExprAST> FCScanner::ParseVarExpr() {
  getNextToken();
  if (m_curTok != static_cast<int>(FCToken::tok_identifier)) {
    return logError("expected identifier after var");
  }
  std::string varName = m_identifierStr;
  getNextToken();
  if (m_curTok != ':') {
    return logError("expected ':' after var name");
  }
  getNextToken();
  if (m_curTok != static_cast<int>(FCToken::tok_identifier)) {
    return logError("expected type after ':'");
  }
  std::string typeName = m_identifierStr;
  if (typeName != "int" && typeName != "double" && typeName != "string") {
    return logError("invalid type");
  }
  getNextToken();
  if (m_curTok != '=') {
    return logError("expected '=' after type (initialization required)");
  }
  getNextToken();
  auto init = parseExpression();
  if (!init)
    return nullptr;

  // 顶层声明进入程序全局作用域；函数内声明进入当前函数作用域。
  if (m_currentFunc.empty()) {
    if (m_semanticContext.lookupGlobalVariable(varName))
      return logError("global variable redeclaration");
    auto decl = std::make_shared<VarDecl>(varName, typeName);
    m_semanticContext.insertGlobalVariable(varName, decl);
    return std::make_unique<FCVarDeclExprAST>(decl, std::move(init));
  }
  if (m_semanticContext.lookupVariableInCurrentScope(m_currentFunc, varName)) {
    return logError("variable redeclaration");
  }
  auto decl = std::make_shared<VarDecl>(varName, typeName);
  m_semanticContext.insertVariableInCurrentScope(m_currentFunc, varName, decl);

  return std::make_unique<FCVarDeclExprAST>(decl, std::move(init));
}

::std::unique_ptr<FCExprAST> FCScanner::parseBlockExpr() {
  if (m_curTok != static_cast<int>(FCToken::tok_brace_open))
    return logError("expected '{' to start block");
  getNextToken();

  std::vector<std::unique_ptr<FCExprAST>> expressions;
  // 允许空块
  if (m_curTok == static_cast<int>(FCToken::tok_brace_close)) {
    getNextToken();
    return std::make_unique<FCBlockExprAST>(std::move(expressions));
  }

  m_semanticContext.pushScope();

  while (true) {
    auto expr = parseExpression();
    if (!expr)
      return nullptr;
    expressions.push_back(std::move(expr));

    if (m_curTok == ';') {
      getNextToken();
      if (m_curTok == static_cast<int>(FCToken::tok_brace_close)) {
        getNextToken();
        break;
      }
      continue;
    } else if (m_curTok == static_cast<int>(FCToken::tok_brace_close)) {
      getNextToken();
      break;
    } else {
      return logError("expected ';' or '}' in block");
    }
  }

  m_semanticContext.popScope();

  return std::make_unique<FCBlockExprAST>(std::move(expressions));
}

/// 解析表达式序列：expr (','|';' expr)*
::std::unique_ptr<FCExprAST> FCScanner::parseSeqExpr() {
  auto first = parseExpression();
  if (!first)
    return nullptr;

  std::vector<std::unique_ptr<FCExprAST>> exprList;
  exprList.push_back(std::move(first));

  while (m_curTok == ';') {
    getNextToken();
    if (m_curTok == static_cast<int>(FCToken::tok_eof) ||
        m_curTok == static_cast<int>(FCToken::tok_def) ||
        m_curTok == static_cast<int>(FCToken::tok_else) ||
        m_curTok == static_cast<int>(FCToken::tok_brace_close) ||
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
