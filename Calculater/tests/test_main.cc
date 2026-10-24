#include "test_harness.h"

// ==================== runAllTests ====================
// 按原 unit_test.cc 中的调用顺序执行各测试家族。
void ComprehensiveTestSuite::runAllTests()
{
    // ============ Scanner Tests ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "SCANNER TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runScannerTests();

    // ============ Scanner Error Tests ============
    runScannerErrorTests();

    // ============ Diagnostic Tests (结构化诊断通道：FCDiagnostic) ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "DIAGNOSTIC TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runDiagnosticTests();

    // ============ Semantic Tests ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "SEMANTIC TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runSemanticTests();

    // ============ Evaluator Tests ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "EVALUATOR TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runEvaluatorTests();
    runEvaluatorEdgeCaseTests();

    // ============ Registry Tests ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "REGISTRY TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runRegistryTests();

    // ============ Codegen Tests ============
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "CODEGEN TESTS\n";
    std::cout << std::string(60, '=') << "\n";
    runCodegenTests();

    // ============ Codegen Error Tests ============
    runCodegenErrorTests();

    // ============ Codegen API Tests ============
    runCodegenApiTests();

    // ============ AST Info Tests ============
    runAstInfoTests();

    // ============ Token / AST API Tests ============
    runTokenApiTests();

    // ============ Type Mapping Tests (FCTypeId 与 VarDecl::type) ============
    runTypeMappingTests();

    // ============ Lexer Tests (独立词法测试，不依赖 Parser/Semantic) ============
    runLexerTests();

    // ============ Parser Tests (独立语法测试，只组 Lexer，不依赖 Semantic) ============
    runParserTests();

    // ============ AST Dump Tests (语法树文本形式，供代码阅读章节引用) ============
    runAstDumpTests();

    // ============ Semantic Dump Tests (语义阶段中间状态，供代码阅读章节引用) ============
    runSemanticDumpTests();

    // ============ Evaluator Trace Tests ============
    runEvaluatorTraceTests();

    // ============ Symbol Table Tests ============
    runSymbolTableTests();

    // ============ Compiled Program Tests ============
    runCompiledProgramTests();

    // ============ Semantic API Tests ============
    runSemanticApiTests();

    // ============ Semantic Analyzer Tests ============
    runSemanticAnalyzerTests();

    // ============ Evaluator Error Tests ============
    runEvaluatorErrorTests();

    // ============ Evaluator Value Semantics Tests (FCValue 变体迁移钉住) ============
    runEvaluatorValueSemanticsTests();

    // ============ Evaluator Void Tests (Dangle→Void/Error 拆分钉住) ============
    runEvaluatorVoidTests();

    // ============ Error Diagnostics Invariant Tests (Error/诊断耦合不变式) ============
    runErrorDiagnosticsInvariantTests();

    // ============ Stress Tests ============
    runStressTests();
}

int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    std::cout << "COMPREHENSIVE TOY LANGUAGE TEST SUITE\n";
    std::cout << std::string(60, '=') << "\n";

    ComprehensiveTestSuite tests;
    tests.runAllTests();
    return tests.result();
}
