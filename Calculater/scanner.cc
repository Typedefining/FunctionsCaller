#include "scanner.h"

using namespace FCExprClass;
using namespace FCMarks;

FCScanner::FCScanner()
    : m_lexer("")
    , m_parser(m_lexer) {}

::std::unique_ptr<FCExprAST>
FCScanner::analysis(const ::std::string &inputStr) {
    m_hadSemanticError = false;

    // 阶段1：Lexer — 字符流 -> Token 流
    m_lexer.reset(inputStr);
    m_parser.reset();

    // 阶段2：Parser — Token 流 -> 纯语法 AST（不触及符号表、作用域、frameIndex）
    auto ast = m_parser.parseProgram();
    if (!ast || m_parser.hadError()) {
        return nullptr;
    }

    // 阶段3：SemanticAnalyzer — 纯语法 AST -> 语义绑定、frameIndex 赋值
    FCSemanticContext::SemanticAnalyzer analyzer(m_semanticContext);
    if (!analyzer.analyze(ast.get())) {
        m_hadSemanticError = true;
        return nullptr;
    }

    return ast;
}

void FCScanner::resetState() {
    m_lexer.reset("");
    m_semanticContext.reset();
    m_hadSemanticError = false;
}
