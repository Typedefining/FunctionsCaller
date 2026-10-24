#pragma once

#include <memory>
#include <string>

#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "symbol.h"
#include "diagnostic.h"

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

    // 结构化诊断通道：记录本帧 analysis() 中 Parser（及后续阶段）报告的诊断。
    // 每轮 analysis() 入口清空。
    const FC::FCDiagnostics &diagnostics() const { return m_diagnostics; }

    bool hadParseError() const { return m_parser.hadError(); }
    bool hadSemanticError() const { return m_hadSemanticError; }

private:
    // 声明顺序即初始化顺序：m_diagnostics 必须先于 m_parser 构造，
    // m_parser 持有其引用。
    FC::Lexer m_lexer;
    FC::FCDiagnostics m_diagnostics;
    FC::Parser m_parser;
    FCMarks::FCSemanticContext m_semanticContext;
    bool m_hadSemanticError = false;
};
