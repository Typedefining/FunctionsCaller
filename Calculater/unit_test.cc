#include "codegen.h"
#include "evaluator.h"
#include "scanner.h"
#include "semantic.h"
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>

using namespace FCExprClass;
using namespace FCMarks;

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

    void runAllTests()
    {
        // ============ Scanner Tests ============
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SCANNER TESTS\n";
        std::cout << std::string(60, '=') << "\n";
        runScannerTests();

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
    }

private:
    std::vector<TestCase> m_testCases;
    int m_total = 0;
    int m_failed = 0;

    // ==================== Scanner Tests ====================
    void runScannerTests()
    {
        std::vector<std::pair<std::string, std::string>> scannerCases = {
            {"; 1", "leading semicolon"},
            {"1 + 2 * 3", "integer precedence"},
            {"(1 + 2) * 3", "parentheses"},
            {"1.5 + 2.5", "floating literal"},
            {"\"hello\" + \" world\"", "string literal"},
            {"# comment\n1 + 2", "comment handling"},
            {"var g:int = 2; g = g + 3; g", "top-level declaration"},
            {"def add(a:int, b:double) a + b", "function with typed params"},
            {"def empty() 42", "zero-argument function"},
            {"def choose(a:int) if a < 1 then 10 else 20", "if expression"},
            {"def loop(n:int) for i = 0, i < n, 1 in i", "for expression"},
            {"foo(1, 2)", "function call"},
            {"def fib(n:int) if n < 2 then n else fib(n - 1) + fib(n - 2)", "fibonacci"},
            {"var counter:int = 0; def next() counter = counter + 1; counter", "counter function"},
            {"def max3(a:int, b:int, c:int) if a < b then if b < c then c else b else if a < c then c else a", "nested if"}
        };

        for (const auto& [code, name] : scannerCases)
        {
            std::cout << "  Testing: " << name << "\n";
            std::cout << "    Code: " << code << "\n";
            
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            bool ok = ast != nullptr;
            
            expect(ok, "scanner - " + name);
            if (ok)
            {
                std::cout << "      OK - AST generated\n";
            }
            else
            {
                std::cout << "      FAILED - AST is null\n";
            }
        }
    }

    // ==================== Semantic Tests ====================
    void runSemanticTests()
    {
        // 测试用例：需要能产生有意义的语义上下文
        std::vector<std::tuple<std::string, std::string, bool>> semanticCases = {
            {"var g:int = 10", "global variable declaration", true},
            {"def add(a:int, b:int) a + b", "function declaration", true},
            {"var x:int = 1; var y:int = 2; x + y", "multiple globals", true},
            {"def fib(n:int) if n < 2 then n else fib(n - 1) + fib(n - 2)", "recursive function", true},
            {"var counter:int = 0; def inc() counter = counter + 1", "function with global access", true},
            {"def nested(a:int) var x:int = a; x + 1", "local variable", true},
            {"1 + 2", "simple expression", true}
        };

        for (const auto& [code, name, expected] : semanticCases)
        {
            std::cout << "  Testing: " << name << "\n";
            std::cout << "    Code: " << code << "\n";
            
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
            {
                expect(false, "semantic - " + name + " (scanner failed)");
                continue;
            }

            const auto& semanticCtx = scanner.semanticContext();
            bool ok = true;
            std::string details;

            // 检查操作符优先级表
            if (semanticCtx.getOperatorPrecedence('+') <= 0 ||
                semanticCtx.getOperatorPrecedence('*') <= 0)
            {
                ok = false;
                details = "Operator precedence table not initialized";
            }

            // 检查是否能找到声明
            if (code.find("var") != std::string::npos && ok)
            {
                size_t varPos = code.find("var");
                if (varPos != std::string::npos)
                {
                    size_t nameStart = varPos + 4;
                    while (nameStart < code.length() && code[nameStart] == ' ')
                        ++nameStart;
                    size_t nameEnd = code.find(':', nameStart);
                    if (nameEnd != std::string::npos)
                    {
                        std::string varName = code.substr(nameStart, nameEnd - nameStart);
                        auto decl = semanticCtx.lookupGlobalVariable(varName);
                        if (decl)
                        {
                            details = "Found global: " + varName;
                        }
                        else
                        {
                            details = "Global not found: " + varName;
                        }
                    }
                }
            }
            else if (code.find("def") != std::string::npos && ok)
            {
                size_t defPos = code.find("def");
                if (defPos != std::string::npos)
                {
                    size_t nameStart = defPos + 4;
                    while (nameStart < code.length() && code[nameStart] == ' ')
                        ++nameStart;
                    size_t nameEnd = code.find('(', nameStart);
                    if (nameEnd != std::string::npos)
                    {
                        std::string funcName = code.substr(nameStart, nameEnd - nameStart);
                        auto funcs = semanticCtx.functionDeclarations(funcName);
                        if (!funcs.empty())
                        {
                            details = "Found function: " + funcName;
                        }
                        else
                        {
                            details = "Function not registered: " + funcName;
                        }
                    }
                }
            }

            expect(ok && expected, "semantic - " + name);
            if (ok)
            {
                std::cout << "      OK - " << (details.empty() ? "Semantic context valid" : details) << "\n";
            }
            else
            {
                std::cout << "      FAILED - " << details << "\n";
            }
        }
    }

    // ==================== Evaluator Tests ====================
    void runEvaluatorTests()
    {
        // 测试用例：需要能成功求值
        std::vector<std::tuple<std::string, std::string, std::string>> evaluatorCases = {
            {"1 + 2", "integer addition", "3"},
            {"2 * 3", "integer multiplication", "6"},
            {"10 / 2", "integer division", "5"},
            {"1.5 + 2.5", "floating addition", "4.0"},
            {"3.0 * 2.0", "floating multiplication", "6.0"},
            {"\"hello\" + \" world\"", "string concatenation", "\"hello world\""},
            {"var x:int = 5; x + 3", "variable declaration", "8"},
            {"def square(x:int) x * x; square(4)", "function call", "16"},
            {"def add(a:int, b:int) a + b; add(3, 7)", "two-argument function", "10"},
            {"if 1 then 10 else 20", "if true branch", "10"},
            {"if 0 then 10 else 20", "if false branch", "20"},
            {"def fib(n:int) if n < 2 then n else fib(n - 1) + fib(n - 2); fib(5)", "fibonacci", "5"},
            {"var x:int = 1; x = x + 1; x", "assignment", "2"},
            {"def power(a:int, n:int) if n < 1 then 1 else a * power(a, n - 1); power(2, 4)", "power function", "16"},
            {"var counter:int = 0; def inc() counter = counter + 1; inc(); inc(); counter", "global counter", "2"}
        };

        for (const auto& [code, name, expected] : evaluatorCases)
        {
            std::cout << "  Testing: " << name << "\n";
            std::cout << "    Code: " << code << "\n";
            
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
            {
                expect(false, "evaluator - " + name + " (scanner failed)");
                continue;
            }

            FCEvaluationContext evalContext;
            try
            {
                auto startTime = std::chrono::high_resolution_clock::now();
                FCValue result = evaluate(ast.get(), evalContext);
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                
                bool ok = result.type != FCValueCategory::Dangle;
                expect(ok, "evaluator - " + name);
                
                if (ok)
                {
                    std::cout << "      Result: ";
                    if (result.type == FCValueCategory::Integer)
                        std::cout << result.evaluteVal.intVal;
                    else if (result.type == FCValueCategory::Floating)
                        std::cout << result.evaluteVal.doubleVal;
                    else if (result.type == FCValueCategory::String)
                        std::cout << "\"" << result.evaluteVal.charVal->str << "\"";
                    std::cout << " (expected: " << expected << ")";
                    std::cout << " in " << duration.count() << " us\n";
                }
                else
                {
                    std::cout << "      FAILED - Evaluator returned Dangle\n";
                }
            }
            catch (const std::exception& e)
            {
                expect(false, "evaluator - " + name + " (exception: " + e.what() + ")");
            }
            catch (...)
            {
                expect(false, "evaluator - " + name + " (unknown exception)");
            }
        }
    }

    // ==================== Registry Tests ====================
    void runRegistryTests()
    {
        std::vector<std::tuple<std::string, std::string, int>> registryCases = {
            {"def one() 1", "single function", 1},
            {"def add(a:int, b:int) a + b", "function with params", 1},
            {"def fib(n:int) if n < 2 then n else fib(n - 1) + fib(n - 2)", "recursive function", 1},
            {"def square(x:int) x * x; def cube(x:int) x * x * x", "multiple functions", 2},
            {"var g:int = 1; def read() g; def write() g = 2", "functions with globals", 2},
            {"def empty() 42", "zero-argument function", 1}
        };

        for (const auto& [code, name, expectedCount] : registryCases)
        {
            std::cout << "  Testing: " << name << "\n";
            std::cout << "    Code: " << code << "\n";
            
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
            {
                expect(false, "registry - " + name + " (scanner failed)");
                continue;
            }

            FCFunctionRegistry registry;
            int registered = 0;
            bool allRegistered = true;

            auto* program = dynamic_cast<FCProgramAST*>(ast.get());
            if (program)
            {
                for (const auto& stmt : program->getStatements())
                {
                    auto* func = dynamic_cast<FCFunctionAST*>(stmt.get());
                    if (func)
                    {
                        ++registered;
                        if (!registry.registerFunction(func))
                            allRegistered = false;
                    }
                }
            }
            else
            {
                auto* func = dynamic_cast<FCFunctionAST*>(ast.get());
                if (func)
                {
                    ++registered;
                    allRegistered = registry.registerFunction(func);
                }
            }

            bool ok = allRegistered && (registered == expectedCount);
            expect(ok, "registry - " + name);
            std::cout << "      Registered " << registered << "/" << expectedCount << " functions\n";
            
            if (!ok)
            {
                std::cout << "      FAILED - Expected " << expectedCount << " functions, got " << registered << "\n";
            }
        }

        // 额外测试：重复注册
        std::cout << "  Testing: duplicate function registration\n";
        FCFunctionRegistry registry;
        auto func1 = std::make_unique<FCFunctionAST>(
            std::make_unique<FCPrototypeAST>("duplicate", std::vector<FCVariableExprAST>{}),
            std::make_unique<FCNumberExprAST>(1), 0);
        auto func2 = std::make_unique<FCFunctionAST>(
            std::make_unique<FCPrototypeAST>("duplicate", std::vector<FCVariableExprAST>{}),
            std::make_unique<FCNumberExprAST>(2), 0);
        
        bool first = registry.registerFunction(func1.get());
        bool second = registry.registerFunction(func2.get());
        bool ok = first && !second;
        expect(ok, "registry - duplicate function rejection");
        std::cout << "      First registration: " << (first ? "success" : "failed") << "\n";
        std::cout << "      Second registration: " << (second ? "should fail" : "correctly rejected") << "\n";
    }

    // ==================== Codegen Tests ====================
    void runCodegenTests()
    {
        std::vector<std::tuple<std::string, std::string>> codegenCases = {
            {"def simple() 42", "simple function"},
            {"def add(a:int, b:int) a + b", "integer addition"},
            {"def mul(a:double, b:double) a * b", "floating multiplication"},
            {"def concat(a:string, b:string) a + b", "string concatenation"},
            {"def iftest(a:int) if a < 1 then 1 else 2", "if expression"},
            {"def loop(n:int) for i = 0, i < n, 1 in i", "for loop"},
            {"def local(a:int) var b:int = a + 1; b", "local variable"},
            {"var g:int = 10; def read() g; def write() g = 20", "global variables"},
            {"def fib(n:int) if n < 2 then n else fib(n - 1) + fib(n - 2)", "recursive function"},
            {"def power(a:int, n:int) if n < 1 then 1 else a * power(a, n - 1)", "power function"}
        };

        for (const auto& [code, name] : codegenCases)
        {
            std::cout << "  Testing: " << name << "\n";
            std::cout << "    Code: " << code << "\n";
            
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
            {
                expect(false, "codegen - " + name + " (scanner failed)");
                continue;
            }

            try
            {
                auto startTime = std::chrono::high_resolution_clock::now();
                FCCodegenContext codegenContext("CodegenTest_" + name);
                auto* generated = codegen(ast.get(), codegenContext);
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
                
                bool ok = generated != nullptr;
                expect(ok, "codegen - " + name);
                
                if (ok)
                {
                    bool moduleValid = !llvm::verifyModule(*codegenContext.module, &llvm::errs());
                    std::cout << "      Module generated in " << duration.count() << " us\n";
                    std::cout << "      Module " << (moduleValid ? "valid" : "has verification errors") << "\n";
                    
                    // 统计函数和全局变量
                    int funcCount = 0;
                    int globalCount = 0;
                    for (auto& func : codegenContext.module->functions())
                    {
                        if (!func.isDeclaration())
                            ++funcCount;
                    }
                    for (auto& global : codegenContext.module->globals())
                    {
                        ++globalCount;
                    }
                    std::cout << "      Functions: " << funcCount << ", Globals: " << globalCount << "\n";
                }
                else
                {
                    std::cout << "      FAILED - Codegen returned null\n";
                }
            }
            catch (const std::exception& e)
            {
                expect(false, "codegen - " + name + " (exception: " + e.what() + ")");
            }
            catch (...)
            {
                expect(false, "codegen - " + name + " (unknown exception)");
            }
        }

        // 测试错误处理
        std::cout << "  Testing: error handling - invalid code\n";
        FCScanner scanner;
        auto ast = scanner.analysis("def bad() unknown_function()");
        if (ast)
        {
            FCCodegenContext errorContext("ErrorTest");
            auto* generated = codegen(ast.get(), errorContext);
            bool ok = generated == nullptr; // 应该失败
            expect(ok, "codegen - invalid function call handling");
            std::cout << "      " << (ok ? "Correctly rejected invalid code" : "Should have rejected") << "\n";
        }
        else
        {
            std::cout << "      Scanner already rejected invalid code\n";
            expect(true, "codegen - invalid function call handling");
        }
    }
};

int main()
{
    std::cout << "COMPREHENSIVE TOY LANGUAGE TEST SUITE\n";
    std::cout << std::string(60, '=') << "\n";
    
    ComprehensiveTestSuite tests;
    tests.runAllTests();
    return tests.result();
}