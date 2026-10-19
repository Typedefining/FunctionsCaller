#pragma once

#include <memory>
#include <string>

#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "token.h"

/// 扫描器 Facade：组合三个独立阶段
///   1. Lexer   : 字符流 -> Token 流
///   2. Parser  : Token 流 -> 纯语法 AST（不依赖 Semantic）
///   3. SemanticAnalyzer: 纯语法 AST -> 语义绑定、frameIndex 赋值
///
/// 对外保持 analysis(std::string) 入口，兼容现有调用方。
class FCScanner
{
public:
    FCScanner();
    void resetState();
    ::std::unique_ptr<FCExprClass::FCExprAST> analysis(const ::std::string &input);

    const FCMarks::FCSemanticContext &semanticContext() const { return m_semanticContext; }
    FCMarks::FCSemanticContext &semanticContextMutable() { return m_semanticContext; }

    bool hadParseError() const { return m_parser.hadError(); }
    bool hadSemanticError() const { return m_hadSemanticError; }

private:
    FC::Lexer m_lexer;
    FC::Parser m_parser;
    FCMarks::FCSemanticContext m_semanticContext;
    bool m_hadSemanticError = false;
};
