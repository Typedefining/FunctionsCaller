#include "lexer.h"
#include <cctype>
#include <cstdlib>

using FCMarks::FCToken;

namespace FC {

Lexer::Lexer(std::string input)
	: m_input(std::move(input))
	, m_idx(m_input.begin())
	, m_lastChar(' ')
	, m_curLine(1)
	, m_curColumn(1) {
	m_curToken = { static_cast<int>(FCToken::tok_eof), "", 1, 1 };
}

void Lexer::reset(const std::string& input) {
	m_input = input;
	m_idx = m_input.begin();
	m_lastChar = ' ';
	m_curLine = 1;
	m_curColumn = 1;
	m_hasPeek = false;
	m_curToken = { static_cast<int>(FCToken::tok_eof), "", 1, 1 };
}

char Lexer::readChar() {
	if (m_idx == m_input.end()) {
		m_lastChar = EOF;
		return static_cast<char>(EOF);
	}
	char ch = static_cast<char>(*m_idx++);
	m_lastChar = static_cast<unsigned char>(ch);
	if (ch == '\n') {
		m_curLine++;
		m_curColumn = 1;
	} else {
		m_curColumn++;
	}
	return ch;
}

Token Lexer::getTok() {
	int startLine = m_curLine;
	int startColumn = m_curColumn;

	// 跳过空白符
	while (m_lastChar != EOF && std::isspace(static_cast<unsigned char>(m_lastChar))) {
		readChar();
	}
	startLine = m_curLine;
	startColumn = m_curColumn;

	// 此时为标识符
	// 函数标识符、变量标识符、函数定义标识符
	if (m_lastChar != EOF && std::isalpha(static_cast<unsigned char>(m_lastChar))) {
		std::string identifierStr;
		identifierStr += static_cast<char>(m_lastChar);
		while ((m_lastChar = readChar()) != EOF &&
		       std::isalnum(static_cast<unsigned char>(m_lastChar))) {
			identifierStr += static_cast<char>(m_lastChar);
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
	// 支持整数、浮点数
	if (m_lastChar != EOF &&
	    (std::isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.')) {
		std::string numStr;
		bool curDouble = false;
		do {
			numStr += static_cast<char>(m_lastChar);
			m_lastChar = readChar();
			if (m_lastChar == '.')
				curDouble = true;
		} while (
			m_lastChar != EOF &&
			(std::isdigit(static_cast<unsigned char>(m_lastChar)) || m_lastChar == '.'));
		if (curDouble) {
			// 浮点数支持
			return { static_cast<int>(FCToken::tok_number), numStr, startLine, startColumn };
		} else {
			return { static_cast<int>(FCToken::tok_number), numStr, startLine, startColumn };
		}
	}
	// 字符串标识符
	if (m_lastChar == '"') {
		readChar(); // 跳过开头的 "
		std::string strVal;
		while (m_lastChar != EOF && m_lastChar != '"') {
			strVal += static_cast<char>(m_lastChar);
			m_lastChar = readChar();
		}
		if (m_lastChar != '"') {
			return { static_cast<int>(FCToken::tok_eof), "", startLine, startColumn };
		}
		readChar(); // 跳过结尾的 "
		return { static_cast<int>(FCToken::tok_string), strVal, startLine, startColumn };
	}
	// 注释标识符
	if (m_lastChar == '#') {
		do {
			m_lastChar = readChar();
		} while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');
		if (m_lastChar != EOF) {
			return getTok();
		}
	}
	// 解析结束
	if (m_lastChar == EOF) {
		m_lastChar = ' ';
		return { static_cast<int>(FCToken::tok_eof), "", startLine, startColumn };
	}
	// 其他值
	int thisChar = m_lastChar;
	m_lastChar = readChar();
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
	default:
		return { thisChar, std::string(1, static_cast<char>(thisChar)), startLine, startColumn,
			     FCMarks::charToBinaryOp(static_cast<char>(thisChar)) };
	}
}

Token Lexer::nextToken() {
	if (m_hasPeek) {
		m_hasPeek = false;
		m_curToken = m_peekToken;
	} else {
		m_curToken = getTok();
	}
	return m_curToken;
}

Token Lexer::peekToken() const {
	if (!m_hasPeek) {
		// const 方法中缓存预读 token（可变成员）
		Lexer* nonConstThis = const_cast<Lexer*>(this);
		nonConstThis->m_peekToken = nonConstThis->getTok();
		nonConstThis->m_hasPeek = true;
	}
	return m_peekToken;
}

Token Lexer::currentToken() const {
	return m_curToken;
}

bool Lexer::isAtEnd() const {
	return m_curToken.kind == static_cast<int>(FCToken::tok_eof);
}

} // namespace FC

