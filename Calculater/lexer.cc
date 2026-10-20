#include "lexer.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include "trace.h"

using FCMarks::FCToken;

namespace FC {

Lexer::Lexer(std::string input)
	: m_input(std::move(input))
	, m_curToken{ static_cast<int>(FCToken::tok_eof), "", 1, 1 } {
}

void Lexer::reset(const std::string& input) {
	m_input = input;
	m_cursor = Cursor{};
	m_curToken = { static_cast<int>(FCToken::tok_eof), "", 1, 1 };
}

char Lexer::Cursor::readChar(const std::string& input) {
	if (pos >= input.size()) {
		lastChar = EOF;
		// EOF 没有对应字符：位置取下一个字符本该出现的位置
		lastCharLine = line;
		lastCharColumn = column;
		return static_cast<char>(EOF);
	}
	// 直接从输入缓冲区按下标读取，不依赖任何迭代器或共享状态
	char ch = static_cast<char>(input[pos]);
	pos++;
	lastChar = static_cast<unsigned char>(ch);
	// 先记录该字符自身的位置，再推进行列，Token 起点据此确定
	lastCharLine = line;
	lastCharColumn = column;
	if (ch == '\n') {
		line++;
		column = 1;
	} else {
		column++;
	}
	return ch;
}

Token Lexer::Cursor::getTok(const std::string& input) {
	// 跳过空白符
	while (lastChar != EOF && std::isspace(static_cast<unsigned char>(lastChar))) {
		readChar(input);
	}
	// 此时 lastChar 是 Token 的第一个字符，其位置由 readChar 记录
	const int startLine = lastCharLine;
	const int startColumn = lastCharColumn;

	// 此时为标识符
	// 函数标识符、变量标识符、函数定义标识符
	if (lastChar != EOF && std::isalpha(static_cast<unsigned char>(lastChar))) {
		std::string identifierStr;
		identifierStr += static_cast<char>(lastChar);
		while ((lastChar = readChar(input)) != EOF &&
		       std::isalnum(static_cast<unsigned char>(lastChar))) {
			identifierStr += static_cast<char>(lastChar);
		}
		if (identifierStr == "def")
			return { static_cast<int>(FCToken::tok_def), identifierStr, startLine, startColumn };
		if (identifierStr == "if")
			return { static_cast<int>(FCToken::tok_if), identifierStr, startLine, startColumn };
		if (identifierStr == "for")
			return { static_cast<int>(FCToken::tok_for), identifierStr, startLine, startColumn };
		if (identifierStr == "then")
			return { static_cast<int>(FCToken::tok_then), identifierStr, startLine, startColumn };
		if (identifierStr == "else")
			return { static_cast<int>(FCToken::tok_else), identifierStr, startLine, startColumn };
		if (identifierStr == "in")
			return { static_cast<int>(FCToken::tok_in), identifierStr, startLine, startColumn };
		if (identifierStr == "var")
			return { static_cast<int>(FCToken::tok_var), identifierStr, startLine, startColumn };
		// 默认视为变量标识符
		return { static_cast<int>(FCToken::tok_identifier), identifierStr, startLine, startColumn };
	}
	// 此时为数字字面量
	// 支持整数、浮点数；整数与浮点的区别由 lexeme 中的小数点体现，
	// 具体类型在语义阶段依据声明类型确定
	if (lastChar != EOF &&
	    (std::isdigit(static_cast<unsigned char>(lastChar)) || lastChar == '.')) {
		std::string numStr;
		do {
			numStr += static_cast<char>(lastChar);
			lastChar = readChar(input);
		} while (lastChar != EOF &&
		         (std::isdigit(static_cast<unsigned char>(lastChar)) || lastChar == '.'));
		return { static_cast<int>(FCToken::tok_number), numStr, startLine, startColumn };
	}
	// 字符串标识符
	if (lastChar == '"') {
		readChar(input); // 跳过开头的 "
		std::string strVal;
		while (lastChar != EOF && lastChar != '"') {
			strVal += static_cast<char>(lastChar);
			lastChar = readChar(input);
		}
		if (lastChar != '"') {
			// 未闭合字符串是词法错误，不能退化成 EOF（否则错误信息失真）
			return { static_cast<int>(FCToken::tok_error), "unterminated string literal",
				     startLine, startColumn };
		}
		readChar(input); // 跳过结尾的 "
		return { static_cast<int>(FCToken::tok_string), strVal, startLine, startColumn };
	}
	// 注释标识符
	if (lastChar == '#') {
		do {
			lastChar = readChar(input);
		} while (lastChar != EOF && lastChar != '\n' && lastChar != '\r');
		if (lastChar != EOF) {
			return getTok(input);
		}
	}
	// 解析结束
	if (lastChar == EOF) {
		lastChar = ' ';
		return { static_cast<int>(FCToken::tok_eof), "", startLine, startColumn };
	}
	// 标点与运算符：一律返回枚举值，Token::kind 不携带 ASCII 码
	int thisChar = lastChar;
	lastChar = readChar(input);
	switch (thisChar) {
	case '(':
		return { static_cast<int>(FCToken::tok_parenthes_open), "(", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case ')':
		return { static_cast<int>(FCToken::tok_parenthes_close), ")", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case '{':
		return { static_cast<int>(FCToken::tok_brace_open), "{", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case '}':
		return { static_cast<int>(FCToken::tok_brace_close), "}", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case ',':
		return { static_cast<int>(FCToken::tok_comma), ",", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case '=':
		return { static_cast<int>(FCToken::tok_assign), "=", startLine, startColumn, FCMarks::FCBinaryOp::Assign };
	case ':':
		return { static_cast<int>(FCToken::tok_colon), ":", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case ';':
		return { static_cast<int>(FCToken::tok_semicolon), ";", startLine, startColumn, FCMarks::FCBinaryOp::Unknown };
	case '<':
		return { static_cast<int>(FCToken::tok_less), "<", startLine, startColumn, FCMarks::FCBinaryOp::Less };
	case '+':
		return { static_cast<int>(FCToken::tok_plus), "+", startLine, startColumn, FCMarks::FCBinaryOp::Add };
	case '-':
		return { static_cast<int>(FCToken::tok_minus), "-", startLine, startColumn, FCMarks::FCBinaryOp::Sub };
	case '*':
		return { static_cast<int>(FCToken::tok_star), "*", startLine, startColumn, FCMarks::FCBinaryOp::Mul };
	case '/':
		return { static_cast<int>(FCToken::tok_slash), "/", startLine, startColumn, FCMarks::FCBinaryOp::Div };
	default:
		// 未识别字符是词法错误；lexeme 携带错误说明，供 Parser 直接展示
		return { static_cast<int>(FCToken::tok_error),
			     std::string("unexpected character '") + static_cast<char>(thisChar) + "'",
			     startLine, startColumn };
	}
}

Token Lexer::nextToken() {
	m_curToken = m_cursor.getTok(m_input);
	if (traceEnabled("lexer")) {
		fprintf(stderr, "[lexer] kind=%d lexeme='%s' binop=%d @%d:%d\n", m_curToken.kind,
		        m_curToken.lexeme.c_str(), static_cast<int>(m_curToken.binop),
		        m_curToken.line, m_curToken.column);
	}
	return m_curToken;
}

Token Lexer::peekToken() const {
	// 在游标副本上识别：主游标不被推进，peek 之后 nextToken 仍产出同一个 Token。
	// 因此这里不需要缓存、mutable 或 const_cast。
	Cursor lookahead = m_cursor;
	return lookahead.getTok(m_input);
}

Token Lexer::currentToken() const {
	return m_curToken;
}

bool Lexer::isAtEnd() const {
	return m_curToken.kind == static_cast<int>(FCToken::tok_eof);
}

} // namespace FC
