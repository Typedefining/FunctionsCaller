#pragma once

#include <cstddef>
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
	// 查看下一个 Token，不改变分析器状态
	Token peekToken() const;
	// 当前已消费的 Token
	Token currentToken() const;
	bool isAtEnd() const;

private:
	// 词法游标：识别一个 Token 所需的全部可变状态。
	// getTok 只读写游标与输入字符串，不接触 Lexer 的其他成员；
	// 因此复制游标即可在不推进主游标的前提下识别下一个 Token。
	struct Cursor {
		std::string::size_type pos = 0; // 下一个待读字符在输入中的下标
		int lastChar = ' ';             // 已读入、尚未消费的字符
		int lastCharLine = 1;           // lastChar 自身的位置
		int lastCharColumn = 1;
		int line = 1;                   // 下一个待读字符的位置
		int column = 1;

		char readChar(const std::string& input);
		Token getTok(const std::string& input);
	};

	std::string m_input;
	Cursor m_cursor;
	Token m_curToken;
};

} // namespace FC
