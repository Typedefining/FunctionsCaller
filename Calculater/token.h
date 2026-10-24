#pragma once

// 词法类别与二元运算符映射。
// 本头文件由原 token.h 拆分而来（2026-10 结构与职责重构 Task 1）：
//   token.h  —— 词法类别枚举与二元运算符映射（本文件）
//   ast.h    —— AST 节点家族
//   symbol.h —— 符号表 / 编译记录 / 绑定侧表

namespace FCMarks {
enum struct FCToken {
  tok_begin = 0,
  tok_eof = 1,
  tok_def = 2,
  tok_identifier = 3,
  tok_number = 4,
  tok_string = 5,
  tok_if = 6,
  tok_then = 7,
  tok_else = 8,
  tok_for = 9,
  tok_in = 10,
  tok_var = 11,
  //(
  tok_parenthes_open = 12,
  //)
  tok_parenthes_close = 13,
  //{
  tok_brace_open = 14,
  //}
  tok_brace_close = 15,
  tok_comma = 16,
  tok_assign,
  tok_colon,
  tok_semicolon,
  // 运算符与字面量标点一样，以枚举值表示，不在 Token::kind 中携带 ASCII 码
  tok_less,
  tok_plus,
  tok_minus,
  tok_star,
  tok_slash,
  // 词法错误：lexeme 携带可直接展示给用户的错误说明
  tok_error,
};

// 二元运算符的统一表示：Parser 依据 FCToken 映射后写入 AST，后端全程使用，杜绝 ASCII 泄露
enum struct FCBinaryOp {
  Assign,   // =
  Less,     // <
  Add,      // +
  Sub,      // -
  Mul,      // *
  Div,      // /
  Unknown   // 非法运算符（错误路径）
};

// kind → 运算符的唯一映射点：Token 只表达词法类别，运算符含义由解析层从 kind 推得
inline FCBinaryOp binaryOpOf(FCToken kind) {
  switch (kind) {
  case FCToken::tok_assign: return FCBinaryOp::Assign;
  case FCToken::tok_less:   return FCBinaryOp::Less;
  case FCToken::tok_plus:   return FCBinaryOp::Add;
  case FCToken::tok_minus:  return FCBinaryOp::Sub;
  case FCToken::tok_star:   return FCBinaryOp::Mul;
  case FCToken::tok_slash:  return FCBinaryOp::Div;
  default:                  return FCBinaryOp::Unknown;
  }
}
} // namespace FCMarks
