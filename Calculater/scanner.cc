#include "scanner.h"

#include "ast_dump.h"
#include "semantic_dump.h"
#include "trace.h"

#include <iostream>

using namespace FCExprClass;
using namespace FCMarks;

FCScanner::FCScanner()
    : m_lexer("")
    , m_diagnostics()
    , m_parser(m_lexer, m_diagnostics) {}

::std::unique_ptr<FCExprAST>
FCScanner::analysis(const ::std::string &inputStr) {
    m_hadSemanticError = false;
    m_diagnostics.clear();

    // 阶段1：Lexer — 字符流 -> Token 流
    m_lexer.reset(inputStr);
    m_parser.reset();

    // 阶段2：Parser — Token 流 -> 纯语法 AST（不触及符号表、作用域、frameIndex）
    auto ast = m_parser.parseProgram();
    if (!ast || m_parser.hadError()) {
        return nullptr;
    }

    // 阶段2 的诊断输出：FC_TRACE=ast 时把纯语法树打印到 stderr
    if (FC::traceEnabled("ast")) {
        std::cerr << describeAst(*ast) << "\n";
    }

    // 阶段3：SemanticAnalyzer — 纯语法 AST -> 语义绑定、frameIndex 赋值
    FCSemanticContext::SemanticAnalyzer analyzer(m_semanticContext);
    if (!analyzer.analyze(ast.get())) {
        m_hadSemanticError = true;
        return nullptr;
    }

    // 阶段3 的诊断输出：FC_TRACE=semantic 时打印每个函数帧的符号与槽位
    if (FC::traceEnabled("semantic")) {
        std::cerr << describeCompiledProgram(m_semanticContext.getCompiledProgram());
    }

    return ast;
}

void FCScanner::resetState() {
    m_lexer.reset("");
    m_parser.reset();
    m_semanticContext.reset();
    m_hadSemanticError = false;
    m_diagnostics.clear();
}
