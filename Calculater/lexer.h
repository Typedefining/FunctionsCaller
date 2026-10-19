#pragma once

#include <string>
#include "token.h"

namespace FC {

struct Token {
	int kind;          // FCToken 枚举值（int）
	std::string lexeme; // 原始文本
	int line;
	int column;
	FCMarks::FCBinaryOp binop = FCMarks::FCBinaryOp::Unknown; // 运算符 token 的统一表示
};

/// 词法分析器：字符流 -> Token 流
/// 独立模块，不依赖 Parser/Semantic，可单独单元测试
class Lexer {
public:
	explicit Lexer(std::string input);
	void reset(const std::string& input);

	// 获取下一个 Token 并推进
	Token nextToken();
	// 不推进，只查看下一个 Token
	Token peekToken() const;
	// 当前已消费的 Token
	Token currentToken() const;
	bool isAtEnd() const;

private:
	std::string m_input;
	std::string::iterator m_idx;
	int m_lastChar;
	int m_curLine;
	int m_curColumn;

	Token m_curToken;
	mutable Token m_peekToken;
	mutable bool m_hasPeek = false;

	Token getTok();
	char readChar();
};

} // namespace FC
