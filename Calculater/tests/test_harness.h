#pragma once

// 各模块测试文件共用的测试骨架：
// - ComprehensiveTestSuite 类声明（成员函数在各 test_*.cc 中定义）
// - 共用 include 集中于此
#include "codegen.h"
#include "diagnostic.h"
#include "evaluator.h"
#include "scanner.h"
#include "semantic.h"
#include "language_options.h"
#include "ast_dump.h"
#include "semantic_dump.h"
#include "lexer.h"
#include "parser.h"
#include "trace.h"
#include "llvm/IR/Verifier.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace FCExprClass;
using namespace FCMarks;
using FC::FCDiagnostic; // 诊断测试按 brief 使用非限定名

class ComprehensiveTestSuite
{
public:
    struct TestCase
    {
        std::string code;
        std::string name;
        bool expectSuccess;
        std::string category;
    };

    void expect(bool condition, const std::string& name)
    {
        ++m_total;
        if (!condition)
        {
            ++m_failed;
            std::cerr << "[FAIL] " << name << "\n";
        }
        else
        {
            std::cout << "[PASS] " << name << "\n";
        }
    }

    int result() const
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SUMMARY\n";
        std::cout << std::string(60, '=') << "\n";
        std::cout << "Total tests: " << m_total << "\n";
        std::cout << "Passed: " << (m_total - m_failed) << "\n";
        std::cout << "Failed: " << m_failed << "\n";
        std::cout << "Success rate: " << std::fixed << std::setprecision(1)
                  << (m_total > 0 ? 100.0 * (m_total - m_failed) / m_total : 0.0) << "%\n";
        return m_failed == 0 ? 0 : 1;
    }

    void runAllTests();

private:
    std::vector<TestCase> m_testCases;
    int m_total = 0;
    int m_failed = 0;

    // ============================================================
    // 辅助函数：把扫描器的语义输出（CompiledProgram）注入求值/代码生成上下文。
    // 这是当前设计的明确契约：CompiledProgram 作为语义输出传递给 evaluator 与 codegen。
    // ============================================================
    static FCEvaluationContext makeEvalContext(const FCScanner& scanner)
    {
        FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
        // 挂载语义绑定侧表（AST 节点 -> 符号）
        return ctx;
    }

    // FCCodegenContext 不可移动/复制（成员含 LLVMContext / unique_ptr），
    // 因此代码生成上下文就地构造后，再注入语义输出。
    static void attachSemanticOutput(FCCodegenContext& cc, const FCScanner& scanner)
    {
        cc.compiledProgram = scanner.semanticContext().getCompiledProgram();
        // 挂载语义绑定侧表（AST 节点 -> 符号）
        cc.semantic = &scanner.semanticContext();
    }

    // 把 FCValue 渲染成与期望值比较的字符串（与原有测试一致）。
    static std::string valueToString(const FCValue& value)
    {
        if (value.type == FCValueCategory::Integer)
            return std::to_string(value.evaluteVal.intVal);
        if (value.type == FCValueCategory::Floating)
        {
            std::string str = std::to_string(value.evaluteVal.doubleVal);
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if (str.back() == '.') str += '0';
            return str;
        }
        if (value.type == FCValueCategory::String)
            return "\"" + value.evaluteVal.charVal->str + "\"";
        return "<dangle>";
    }

    // ==================== run* 成员函数在各 test_*.cc 中定义 ====================
    void runScannerTests();
    void runScannerErrorTests();
    void runSemanticTests();
    void runSemanticApiTests();
    void runSemanticAnalyzerTests();
    void runSymbolTableTests();
    void runCompiledProgramTests();
    void runSemanticDumpTests();
    void runEvaluatorTests();
    void runEvaluatorEdgeCaseTests();
    void runEvaluatorErrorTests();
    void runEvaluatorTraceTests();
    void runStressTests();
    void runRegistryTests();
    void runCodegenTests();
    void runCodegenErrorTests();
    void runCodegenApiTests();
    void runAstInfoTests();
    void runAstDumpTests();
    void runTokenApiTests();
    void runDiagnosticTests();
    void runLexerTests();
    void runParserTests();
};
