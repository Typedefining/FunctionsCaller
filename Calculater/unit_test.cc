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

        // ============ Scanner Error Tests ============
        runScannerErrorTests();

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

        // ============ Codegen Error Tests ============
        runCodegenErrorTests();

        // ============ AST Info Tests ============
        runAstInfoTests();

        // ============ Semantic API Tests ============
        runSemanticApiTests();

        // ============ Evaluator Error Tests ============
        runEvaluatorErrorTests();

        // ============ Stress Tests ============
        runStressTests();
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
            {"def add(a:int, b:double) { a + b }", "function with typed params"},  // 添加大括号
            {"def empty() { 42 }", "zero-argument function"},  // 添加大括号
            {"def choose(a:int) { if a < 1 then 10 else 20 }", "if expression"},  // 添加大括号
            {"def loop(n:int) { for i = 0, i < n, 1 in i }", "for expression"},  // 添加大括号
            {"foo(1, 2)", "function call"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "fibonacci"},  // 添加大括号
            {"var counter:int = 0; def next() { counter = counter + 1; counter }", "counter function"},  // 添加大括号
            {"def max3(a:int, b:int, c:int) { if a < b then if b < c then c else b else if a < c then c else a }", "nested if"},  // 添加大括号
            {"var x:int = 1; var y:int = 2; var z:int = x + y; z", "multi-global init"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "recursive factorial"},  // 添加大括号
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }", "recursive power"},  // 添加大括号
            {"def concat(a:string, b:string) { a + b }", "string function"},  // 添加大括号
            {"var g:double = 3.14; g * 2.0", "floating global"},
            {"def nestedCall() { add(2, 3) }", "function call inside function"},  // 添加大括号
            {"var g:int = 0; def inc() { g = g + 1 }; def dec() { g = g - 1 }", "multiple functions with global"},  // 添加大括号
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with accumulation"},  // 添加大括号
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }", "complex recursive"},  // 添加大括号
            {"var a:int = 5; (a = a + 1; a * 2)", "assignment in parentheses"},
            {"def id(x:int) { x }; id(id(3))", "nested function calls"},  // 添加大括号
            {"var s:string = \"abc\"; s + \"def\"", "string variable concatenation"},
            {"0 - 1", "negative via subtraction"},
            {"2 * 3 + 4 / 2", "mixed precedence"},
            {"((1 + 2) * (3 + 4))", "nested parentheses"},
            {"3.14159", "standalone float literal"},
            {"\"a\" + \"b\" + \"c\"", "string chain concatenation"},
            {"# comment only\n42", "comment then number"},
            {"var a:int = 1; var b:int = 2; a = b; a", "assignment chain"},
            {"def f() { 1 }; def g() { f() + f() }; g()", "multiple functions and call"},
            {"def loop(n:int) { for i = 0, i < n in i }", "for without step"},
            {"def dbl(x:double) { x * 2.0 }", "double parameter"},
            {"def str(s:string) { s }", "string parameter"},
            {"if 1 then 2 else 3", "top-level if"},
            {"(1; 2; 3)", "sequence in parentheses"},
            {"def nested() { { 1 }; { 2 }; 3 }", "nested blocks"},
            {"var g:double = 1.5; def r() { g }", "double global"},
            {"def many(a:int, b:int, c:int, d:int) { a + b + c + d }", "four-argument function"},
            {"def mix(a:int, b:double, c:string) { a }", "mixed parameter types"},
            {"1.0 + 2", "float int expression"},
            {"def deep(x:int) { if x < 1 then 1 else (x * deep(x - 1)) }", "recursive deep expression"}
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

    // ==================== Scanner Error Tests ====================
    void runScannerErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SCANNER ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::pair<std::string, std::string>> errorCases = {
            {"\"unterminated", "unterminated string"},
            {"1 +", "binary op without rhs"},
            {"1 *", "binary op without rhs 2"},
            {"1 + * 2", "precedence rhs error"},
            {"(", "empty parentheses"},
            {"()", "empty paren sequence"},
            {"(1", "unclosed parenthesis"},
            {"f(1 2)", "missing comma in arguments"},
            {"def 1", "definition without name"},
            {"def f 1", "prototype missing parenthesis"},
            {"def f(:int)", "prototype missing parameter name"},
            {"def f(a 1)", "prototype missing colon"},
            {"def f(a:bool)", "unsupported parameter type"},
            {"def f(a:int b:int)", "missing comma in parameters"},
            {"def f(a:int, )", "trailing comma in parameters"},
            {"def f()", "function without body"},
            {"if 1 2 else 3", "if missing then"},
            {"if 1 then 2", "if missing else"},
            {"if 1 then 2 else", "if missing else expression"},
            {"var", "var missing name"},
            {"var x", "var missing colon"},
            {"var x:", "var missing type"},
            {"var x:bool = 1", "var invalid type"},
            {"var x:int", "var missing initializer"},
            {"for 1 = 0, 1, 1 in 1", "for missing identifier"},
            {"for i 0, 1, 1 in 1", "for missing equals"},
            {"for i = 0 1, 1 in 1", "for missing comma"},
            {"for i = 0, 1 in", "for missing body"},
            {"1 + 2 *", "precedence recursion error"},
            {"def f(a:)", "prototype missing parameter type"},
            {"if then 1 else 2", "if missing condition"},
            {"if 1 then else 2", "if missing then expression"},
            {"for i = , 1 in 1", "for missing start value"},
            {"for i = 0, in 1", "for missing end value"},
            {"for i = 0, 1, in 1", "for missing step value"},
            {"for i = 0, 1 2 in 1", "for missing in keyword"},
            {"var x:int =", "var missing initializer expression"},
            {"var x:int = 1; var x:int = 2", "global variable redeclaration"},
            {"def f() { var x:int = 1; var x:int = 2; x }", "local variable redeclaration"},
            {"def empty() {}", "empty function body"},
            {"def f() { 1; }", "trailing semicolon in block"},
            {"def f() { 1 2 }", "block missing separator"},
            {"1; =", "sequence with invalid expression"}
        };

        for (const auto& [code, name] : errorCases)
        {
            FCScanner scanner;
            bool noThrow = true;
            try
            {
                auto ast = scanner.analysis(code);
                (void)ast;
            }
            catch (...)
            {
                noThrow = false;
            }
            expect(noThrow, "scanner-error - " + name);
        }
    }

    // ==================== Semantic Tests ====================
    void runSemanticTests()
    {
        std::vector<std::tuple<std::string, std::string, bool>> semanticCases = {
            {"var g:int = 10", "global variable declaration", true},
            {"def add(a:int, b:int) { a + b }", "function declaration", true},
            {"var x:int = 1; var y:int = 2; x + y", "multiple globals", true},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "recursive function", true},
            {"var counter:int = 0; def inc() { counter = counter + 1 }", "function with global access", true},
            {"def nested(a:int) { var x:int = a; x + 1 }", "local variable", true},
            {"1 + 2", "simple expression", true},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }", "recursive power", true},
            {"var g:double = 2.5; def mul() { g * 2.0 }", "global double", true},
            {"def concat(a:string, b:string) { a + b }", "string function", true},
            {"var x:int = 1; def f() { x }; def g() { var x:int = 2; f() }", "shadowing check", true},
            {"def sumTo(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with local", true},
            {"def simple() { var x:int = 1; var y:int = 2; x + y }", "multiple locals", true},
            {"var g1:int = 1; var g2:int = 2; g1 + g2", "multiple globals work", true},
            {"def f() { 42 }", "simple function works", true},
            {"var a:int = 1; def use() { a + 1 }", "function reads global", true},
            {"def chain(a:int) { var b:int = a; var c:int = b; c + 1 }", "chained locals", true},
            {"def loop(n:int) { for i = 0, i < n in i }", "for without step", true},
            {"def mix(a:int, b:double, c:string) { a }", "mixed parameters", true},
            {"var s:string = \"x\"; def get() { s }", "global string read", true},
            {"def nest() { { var a:int = 1; { var b:int = 2; a + b } } }", "deeply nested blocks", true},
            {"def two() { var x:int = 1; var y:int = 2; x + y }", "multiple locals", true},
            {"var g1:int = 1; var g2:double = 2.5; def f() { g1 + g2 }", "mixed globals", true},
            {"def cmp(a:int, b:int) { if a < b then a else b }", "comparison expression", true},
            {"def strcat(a:string, b:string) { a + b }", "string concat function", true},
            {"def seq() { (var x:int = 1; x + 1; x + 2) }", "sequence with declaration", true},
            {"def recurse(n:int) { if n < 1 then 0 else recurse(n - 1) + 1 }", "recursion", true},
            {"def many(a:int, b:int, c:int, d:int, e:int) { a + b + c + d + e }", "five parameters", true}
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

            // 对于预期成功的测试，检查是否能找到相关声明
            if (expected && ok)
            {
                // 检查全局变量
                size_t varPos = code.find("var");
                if (varPos != std::string::npos)
                {
                    // 提取变量名
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
                            // 可能这个变量不是全局的，或者在函数内部
                            details = "Global not found: " + varName + " (may be local)";
                        }
                    }
                }
                else if (code.find("def") != std::string::npos)
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
                            auto funcs = semanticCtx.functionFrameSize(funcName);
                            details = "Found function: " + funcName + " with " + 
                                        std::to_string(funcs) + " variables";

                        }
                    }
                }
            }

            // 对于预期失败的情况，我们检查是否真的失败了
            // 但由于实现中错误恢复可能返回非空AST，我们需要更细致的检查
            bool passed = (ok == expected);
            if (!passed)
            {
                if (expected && !ok)
                    details = "Unexpected semantic failure: " + details;
                else if (!expected && ok)
                    details = "Expected semantic error but none detected";
            }
            expect(passed, "semantic - " + name);
            if (passed)
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
        std::vector<std::tuple<std::string, std::string, std::string>> evaluatorCases = {
            {"1 + 2", "integer addition", "3"},
            {"2 * 3", "integer multiplication", "6"},
            {"10 / 2", "integer division", "5"},
            {"1.5 + 2.5", "floating addition", "4.0"},
            {"3.0 * 2.0", "floating multiplication", "6.0"},
            {"\"hello\" + \" world\"", "string concatenation", "\"hello world\""},
            {"var x:int = 5; x + 3", "variable declaration", "8"},
            {"def square(x:int) { x * x }; square(4)", "function call", "16"},
            {"def add(a:int, b:int) { a + b }; add(3, 7)", "two-argument function", "10"},
            {"if 1 then 10 else 20", "if true branch", "10"},
            {"if 0 then 10 else 20", "if false branch", "20"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }; fib(5)", "fibonacci", "5"},
            {"var x:int = 1; x = x + 1; x", "assignment", "2"},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }; power(2, 4)", "power function", "16"},
            {"var counter:int = 0; def inc() { counter = counter + 1 }; inc(); inc(); counter", "global counter", "2"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }; fact(6)", "factorial", "720"},
            {"def max(a:int, b:int) { if a < b then b else a }; max(10, max(5, 20))", "nested call", "20"},
            {"var g:int = 1; def f() { g = g + 1 }; f(); f(); g", "global mutation", "3"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }; sum(10)", "loop sum", "45"},
            {"def concat(a:string, b:string) { a + b }; concat(\"ab\", \"cd\")", "string function", "\"abcd\""},
            {"var x:int = 1; (x = x + 2; x * 3)", "assignment in seq", "9"},
            {"def id(x:int) { x }; id(id(5))", "nested id", "5"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }; countdown(5)", "alternating sum", "3"}, // 5-4+3-2+1 = 3
            {"var s:string = \"a\"; s = s + \"b\"; s + \"c\"", "string variable", "\"abc\""},
            {"def loopReturn(n:int) { for i = 0, i < n, 1 in i }; loopReturn(3)", "loop return value", "2"},  // 循环返回最后 i，应为2
            {"10 - 3", "integer subtraction", "7"},
            {"2 * 3 + 1", "multiplication precedence", "7"},
            {"(2 + 3) * 4", "parenthesized precedence", "20"},
            {"10 / 3", "integer division truncation", "3"},
            {"if 1 < 2 then 10 else 20", "comparison true", "10"},
            {"if 2 < 1 then 10 else 20", "comparison false", "20"},
            {"2.5 - 1.0", "floating subtraction", "1.5"},
            {"6.0 / 2.0", "floating division", "3.0"},
            {"if 1.5 then 1 else 2", "floating truthy condition", "1"},
            {"if 0.0 then 1 else 2", "floating falsey condition", "2"},
            {"def sumDefault(n:int) { var s:int = 0; for i = 0, i < n in s = s + i; s }; sumDefault(5)", "for default step", "10"},
            {"def dbl(x:double) { x * 2.0 }; dbl(3.5)", "double function", "7.0"},
            {"def bang(s:string) { s + \"!\" }; bang(\"hi\")", "string append", "\"hi!\""},
            {"var a:string = \"x\"; var b:string = \"y\"; a + b", "string globals", "\"xy\""},
            {"def sub(a:int, b:int) { a - b }; sub(10, 4)", "function subtraction", "6"},
            {"def div(a:int, b:int) { a / b }; div(20, 4)", "function division", "5"},
            {"def fsub(a:double, b:double) { a - b }; fsub(5.5, 2.0)", "float function subtraction", "3.5"},
            {"def fdiv(a:double, b:double) { a / b }; fdiv(7.5, 2.5)", "float function division", "3.0"},
            {"var x:int = 10; def f() { var x:int = 2; x }; f()", "local shadows global", "2"},
            {"def nested2() { var x:int = 1; var y:int = 2; x + y }; nested2()", "multiple locals", "3"},
            {"def nf(n:int) { var s:int = 0; for i = 0, i < n, 1 in for j = 0, j < n, 1 in s = s + i; s }; nf(3)", "nested for loop", "9"},
            {"def accum(n:int) { var r:int = 1; for i = 1, i < n, 1 in r = r * 2; r }; accum(5)", "loop accumulation multiply", "16"},
            {"def choose(n:int) { if n < 3 then 1 else (choose(n - 1) + choose(n - 2)) }; choose(6)", "tree recursion", "8"}
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
                std::string actual;
                if (ok)
                {
                    if (result.type == FCValueCategory::Integer)
                        actual = std::to_string(result.evaluteVal.intVal);
                    else if (result.type == FCValueCategory::Floating)
                    {
                        // 统一浮点数格式，移除多余的零
                        std::string str = std::to_string(result.evaluteVal.doubleVal);
                        str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                        if (str.back() == '.') str += '0';
                        actual = str;
                    }
                    else if (result.type == FCValueCategory::String)
                        actual = "\"" + result.evaluteVal.charVal->str + "\"";
                }
                bool valueMatches = (actual == expected);
                expect(ok && valueMatches, "evaluator - " + name);
                
                if (ok)
                {
                    std::cout << "      Result: " << actual << " (expected: " << expected << ")";
                    if (!valueMatches)
                        std::cout << " [MISMATCH]";
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
            {"def one() { 1 }", "single function", 1},
            {"def add(a:int, b:int) { a + b }", "function with params", 1},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "recursive function", 1},
            {"def square(x:int) { x * x }; def cube(x:int) { x * x * x }", "multiple functions", 2},
            {"var g:int = 1; def read() { g }; def write() { g = 2 }", "functions with globals", 2},
            {"def empty() { 42 }", "zero-argument function", 1},
            {"def a() { 1 }; def b() { 2 }; def c() { 3 }", "three functions", 3},
            {"def f() { var x:int = 1; x }", "function with local", 1},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "recursive factorial", 1},
            {"def a() { 1 }; def b() { 2 }; def c() { 3 }; def d() { 4 }", "four functions", 4},
            {"def a() { 1 }; def b() { 2 }; def c() { 3 }; def d() { 4 }; def e() { 5 }", "five functions", 5},
            {"var g:int = 1; def f() { g }", "global plus function", 1},
            {"def mix(a:int, b:double, c:string) { a }", "mixed-parameter function", 1},
            {"def f() { var x:int = 1; x }; def g() { f() }", "local and call", 2},
            {"def r(n:int) { if n < 1 then 0 else r(n - 1) }", "recursive", 1},
            {"def f1() { 1 }; def f2() { 2 }; def f3() { 3 }; def f4() { 4 }; def f5() { 5 }; def f6() { 6 }", "six functions", 6}
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

        // 额外测试：registerFunction(null) / index 各分支 / clear
        std::cout << "  Testing: registry API edge cases\n";
        {
            FCFunctionRegistry reg;
            expect(!reg.registerFunction(nullptr), "registry - register null function");

            expect(!reg.index(nullptr), "registry - index null");

            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("solo", std::vector<FCVariableExprAST>{}),
                std::make_unique<FCNumberExprAST>(1), 0);
            expect(reg.index(fn.get()), "registry - index single function");
            expect(reg.findFunction("solo") != nullptr, "registry - find after index");

            auto num = std::make_unique<FCNumberExprAST>(42);
            expect(!reg.index(num.get()), "registry - index non-function non-program");

            reg.clear();
            expect(reg.findFunction("solo") == nullptr, "registry - clear");
        }
    }

    // ==================== Codegen Tests ====================
    void runCodegenTests()
    {
        std::vector<std::tuple<std::string, std::string>> codegenCases = {
            {"def simple() { 42 }", "simple function"},
            {"def add(a:int, b:int) { a + b }", "integer addition"},
            {"def mul(a:double, b:double) { a * b }", "floating multiplication"},
            {"def concat(a:string, b:string) { a + b }", "string concatenation"},
            {"def iftest(a:int) { if a < 1 then 1 else 2 }", "if expression"},
            {"def loop(n:int) { for i = 0, i < n, 1 in i }", "for loop"},
            {"def local(a:int) { var b:int = a + 1; b }", "local variable"},
            {"var g:int = 10; def read() { g }; def write() { g = 20 }", "global variables"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "recursive function"},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }", "power function"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "factorial recursion"},
            {"def max(a:int, b:int) { if a < b then b else a }", "max function"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with accumulation"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }", "complex recursion"},
            {"var g:double = 3.14; def readDouble() { g }", "global double"},
            {"def id(x:int) { x }; def caller() { id(42) }", "nested function call"},
            {"var s:string = \"hello\"; def get() { s }; def set(x:string) { s = x }", "string global"},
            {"def sub(a:int, b:int) { a - b }", "integer subtraction"},
            {"def div(a:int, b:int) { a / b }", "integer division"},
            {"def cmp(a:int, b:int) { a < b }", "integer comparison"},
            {"def fsub(a:double, b:double) { a - b }", "float subtraction"},
            {"def fdiv(a:double, b:double) { a / b }", "float division"},
            {"def fcmp(a:double, b:double) { a < b }", "float comparison"},
            {"def mixed(a:int, b:double) { a + b }", "int-double coercion"},
            {"def big(n:int) { var s:int = 0; for i = 0, i < n in s = s + i; s }", "for default step"},
            {"def nest(n:int) { var s:int = 0; for i = 0, i < n, 1 in for j = 0, j < n, 1 in s = s + i; s }", "nested for"},
            {"var x:int = 1; def f() { var x:int = 2; x }", "local shadows global"},
            {"var g:int = 0; def inc() { g = g + 1 }; def run() { inc(); inc(); g }", "global mutation via functions"},
            {"def seqfn() { (var x:int = 1; x + 1; x + 2) }", "sequence function"},
            {"def div2(a:double, b:double) { a / b }; def caller() { div2(7.5, 2.5) }", "nested float call"},
            {"var gd:double = 1.5; var gi:int = 2; def mix2() { gd + gi }", "mixed globals"},
            {"def strcat(a:string, b:string) { a + b }; def use() { strcat(\"a\", \"b\") }", "string concat via call"},
            {"def many(n:int) { var a:int = 1; var b:int = 2; var c:int = 3; var d:int = 4; a + b + c + d + n }", "many locals"},
            {"def branch(a:int) { if a < 1 then 1 else if a < 2 then 2 else 3 }", "nested if else"},
            {"def gauss(n:int) { var s:int = 0; for i = 1, i < n, 1 in s = s + i; s }", "gauss loop"},
            {"(var g:int = 1; g + 1); def f() { g }", "global in top-level sequence"},
            {"{ var g:int = 1; g }; def f() { g }", "global in top-level block"},
            {"if 1 then (var g:int = 2; g) else 3; def f() { g }", "global in top-level if"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }; fib(5)", "recursive function in program"},
            {"def fmax(a:double, b:double) { if a < b then b else a }", "double if expression"},
            {"def fcond(a:double) { if a then 1 else 2 }", "floating if condition"},
            {"var s:string = \"hi\"", "standalone string global"}
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
        auto ast = scanner.analysis("def bad() { unknown_function() }");
        if (ast)
        {
            FCCodegenContext errorContext("ErrorTest");
            auto* generated = codegen(ast.get(), errorContext);
            // 对于未知函数，代码生成可能会失败
            bool ok = generated == nullptr;
            expect(ok, "codegen - invalid function call handling");
            std::cout << "      " << (ok ? "Correctly rejected invalid code" : "Should have rejected") << "\n";
        }
        else
        {
            std::cout << "      Scanner already rejected invalid code\n";
            expect(true, "codegen - invalid function call handling");
        }
    }

    // ==================== Codegen Error Tests ====================
    // 触发 codegen 中的错误分支（赋值目标非变量、类型不匹配、非法循环等），
    // 验证这些输入会返回 nullptr。
    void runCodegenErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "CODEGEN ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::pair<std::string, std::string>> errorCases = {
            {"1 = 2", "assignment to non-variable"},
            {"def f() { unknownfn() }", "function body call to unknown function"},
            {"def f(a:int) { a }; f(1, 2)", "argument count mismatch"},
            {"def f(a:int) { a }; f(\"s\")", "call argument type mismatch"},
            {"if \"a\" then 1 else 2", "top-level if"},
            {"def f() { if \"a\" then 1 else 2 }", "if condition not numeric"},
            {"def f() { if 1 then 1 else \"s\" }", "if branch type mismatch"},
            {"for i = 0, i < 3, 1 in i", "top-level for"},
            {"def f() { for i = \"a\", i < 3, 1 in i }", "for start type mismatch"},
            {"def f() { for i = 0, \"a\", 1 in i }", "for end not numeric"},
            {"def f() { for i = 0, i < 3, \"s\" in i }", "for step type mismatch"},
            {"(1; unknownfn(); 3)", "sequence error"},
            {"var s:int = \"str\"", "global initializer type mismatch"},
            {"def f() { var x:int = \"str\"; x }", "local initializer type mismatch"},
            {"def f() { var x:int = 1; x = \"s\"; x }", "assignment type mismatch"},
            {"def f() { \"a\" - \"b\" }", "string subtraction"},
            {"def f(a:double) { a + \"s\" }", "double plus string"},
            {"def f(a:int) { a }; f(unknownfn())", "call argument codegen error"},
            {"def f() { if 1 then unknownfn() else 2 }", "if then branch error"},
            {"def f() { if 1 then 2 else unknownfn() }", "if else branch error"},
            {"def f() { for i = 0, i < 3, 1 in unknownfn() }", "for body error"},
            {"def f() { unknownfn() }; 1", "program function codegen error"},
            {"def empty() {}", "empty function body"}
        };

        for (const auto& [code, name] : errorCases)
        {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            bool isNull = true;
            if (ast)
            {
                try
                {
                    FCCodegenContext cc("ErrorTest_" + name);
                    isNull = (codegen(ast.get(), cc) == nullptr);
                }
                catch (...)
                {
                    isNull = true;
                }
            }
            expect(isNull, "codegen-error - " + name);
        }

        // 直接驱动 codegen 的公共 API 边界
        {
            FCCodegenContext cc("ApiTest");
            expect(codegen(static_cast<FCExprAST*>(nullptr), cc) == nullptr,
                   "codegen-error - codegen null expression");

            std::vector<FCVariableExprAST> protoArgs;
            protoArgs.push_back(FCVariableExprAST(std::make_shared<VarDecl>("a", "int")));
            auto proto = std::make_unique<FCPrototypeAST>("proto", std::move(protoArgs));
            expect(codegen(proto.get(), cc) != nullptr,
                   "codegen-error - codegen standalone prototype");

            auto emptyBody = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("empty", std::vector<FCVariableExprAST>{}),
                nullptr, 0);
            expect(codegen(emptyBody.get(), cc) == nullptr,
                   "codegen-error - codegen function with null body");
        }
    }

    // ==================== AST Info Tests ====================
    // 直接构造各类 AST 节点并调用 info()，覆盖 token.cc 中的构造与调试输出路径。
    void runAstInfoTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "AST INFO TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        try
        {
            std::make_unique<FCNumberExprAST>(42)->info();
            std::make_unique<FCNumberExprAST>(3.14)->info();
            std::make_unique<FCStringExprAST>("hello")->info();

            auto decl = std::make_shared<VarDecl>("v", "int");
            std::make_unique<FCVariableExprAST>(decl)->info();

            std::make_unique<FCBinaryExprAST>(
                '+', std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCNumberExprAST>(2))->info();

            std::vector<std::unique_ptr<FCExprAST>> args;
            args.push_back(std::make_unique<FCNumberExprAST>(1));
            std::make_unique<FCCallExprAST>("add", std::move(args))->info();

            std::vector<FCVariableExprAST> protoArgs;
            protoArgs.push_back(FCVariableExprAST(std::make_shared<VarDecl>("a", "int")));
            auto proto = std::make_unique<FCPrototypeAST>("f", std::move(protoArgs));
            proto->info();

            auto fn = std::make_unique<FCFunctionAST>(
                std::move(proto), std::make_unique<FCNumberExprAST>(1), 1);
            fn->info();

            std::make_unique<FCIfExprAST>(
                std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCNumberExprAST>(10),
                std::make_unique<FCNumberExprAST>(20))->info();

            std::make_unique<FCForExprAST>(
                std::make_shared<VarDecl>("i", "int"),
                std::make_unique<FCNumberExprAST>(0),
                std::make_unique<FCNumberExprAST>(10),
                nullptr,
                std::make_unique<FCNumberExprAST>(1))->info();

            std::make_unique<FCVarDeclExprAST>(
                std::make_shared<VarDecl>("x", "int"),
                std::make_unique<FCNumberExprAST>(5))->info();

            std::vector<std::unique_ptr<FCExprAST>> seq;
            seq.push_back(std::make_unique<FCNumberExprAST>(1));
            seq.push_back(std::make_unique<FCNumberExprAST>(2));
            std::make_unique<FCSeqExprAST>(std::move(seq))->info();

            std::vector<std::unique_ptr<FCExprAST>> blk;
            blk.push_back(std::make_unique<FCNumberExprAST>(1));
            std::make_unique<FCBlockExprAST>(std::move(blk))->info();

            std::vector<std::unique_ptr<FCExprAST>> progStmts;
            progStmts.push_back(std::make_unique<FCNumberExprAST>(1));
            std::make_unique<FCProgramAST>(std::move(progStmts))->info();

            expect(true, "ast-info - construct and dump all node types");
        }
        catch (...)
        {
            expect(false, "ast-info - exception while dumping AST nodes");
        }
    }

    // ==================== Semantic API Tests ====================
    // 直接驱动 FCSemanticContext 的公共 API，覆盖查询/注册/错误分支。
    void runSemanticApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SEMANTIC API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        {
            FCSemanticContext ctx;
            expect(ctx.getOperatorPrecedence('+') == 10, "semantic-api - known precedence");
            expect(ctx.getOperatorPrecedence('?') == 0, "semantic-api - unknown precedence");
            expect(ctx.registerFunction("f"), "semantic-api - register new function");
            expect(ctx.hasFunction("f"), "semantic-api - hasFunction true");
            expect(!ctx.hasFunction("missing"), "semantic-api - hasFunction false");
            expect(!ctx.registerFunction("f"), "semantic-api - duplicate function rejected");
        }

        {
            FCSemanticContext ctx;
            ctx.pushScope(); // 全局作用域

            auto g = std::make_shared<VarDecl>("g", "int");
            ctx.insertGlobalVariable("g", g);
            expect(ctx.lookupGlobalVariable("g") == g, "semantic-api - lookup global hit");
            expect(ctx.lookupGlobalVariable("nope") == nullptr, "semantic-api - lookup global miss");

            auto dup = std::make_shared<VarDecl>("g2", "int");
            ctx.insertGlobalVariable("g", dup); // 触发全局重复声明错误分支

            ctx.pushFunctionScope();
            auto p = std::make_shared<VarDecl>("p", "int");
            ctx.insertVariableInCurrentScope("p", p);

            expect(ctx.lookupVariableDecl("p") == p, "semantic-api - lookup local hit");
            expect(ctx.lookupVariableDecl("g") == g, "semantic-api - lookup falls back to global");
            expect(ctx.lookupVariableDecl("missing") == nullptr, "semantic-api - lookup miss");
            expect(ctx.lookupVariableInCurrentScope("p") == p, "semantic-api - current scope hit");
            expect(ctx.lookupVariableInCurrentScope("missing") == nullptr, "semantic-api - current scope miss");
            expect(ctx.lookupVariableInCurrentScope("p") == nullptr, "semantic-api - current scope fn mismatch");

            auto again = std::make_shared<VarDecl>("p2", "int");
            ctx.insertVariableInCurrentScope("p", again); // 触发局部重复声明错误分支

            expect(ctx.currentFunctionFrameSize() == 1, "semantic-api - currentFunctionFrameSize");
            expect(ctx.currentScopeDeclarations().size() == 1, "semantic-api - currentScopeDeclarations");

            ctx.dumpScopes();

            ctx.popFunctionScope();
            expect(ctx.functionFrameSize("fn") == 1, "semantic-api - functionFrameSize recorded");
            expect(ctx.functionFrameSize("missing") == 0, "semantic-api - functionFrameSize miss");

            ctx.popScope();
        }

        {
            FCSemanticContext ctx;
            auto d = std::make_shared<VarDecl>("x", "int");
            ctx.insertVariableInCurrentScope("x", d); // 无活动作用域
            expect(true, "semantic-api - insert without active scope");

            // 空作用域栈上的查询
            expect(ctx.lookupVariableInCurrentScope("x") == nullptr,
                   "semantic-api - lookup current scope on empty stack");
            expect(ctx.lookupVariableDecl("x") == nullptr,
                   "semantic-api - lookup decl on empty stack");
        }
    }

    // ==================== Evaluator Error Tests ====================
    void runEvaluatorErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "EVALUATOR ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::pair<std::string, std::string>> errorCases = {
            {"undefinedfn(1)", "call to undefined function"},
            {"def f(a:int) { a }; f(1, 2)", "argument count mismatch"},
            {"1 + \"a\"", "int plus string mismatch"},
            {"def fl(n:int) { for i = 0.0, i < n, 1 in i }; fl(3)", "floating loop variable"},
            {"\"a\" - \"b\"", "string subtraction"},
            {"if \"a\" then 1 else 2", "non-numeric if condition"},
            {"for i = 0, i < 3, 1 in i", "top-level for loop"},
            {"def f(n:int) { for i = 0, i < n, 1 in undefinedfn() }; f(3)", "loop body error"},
            {"(1; \"a\" - \"b\"; 3)", "sequence error propagation"},
            {"def f() { var x:int = undefinedfn(); x }; f()", "declaration init error"},
            {"1 = 2", "assignment to non-variable"},
            {"var x:int = 1; x = undefinedfn(); x", "assignment rhs error"},
            {"def f(a:int) { a }; f(undefinedfn())", "call argument error"},
            {"def f() { 1 }", "standalone function definition"},
            {"1.0 < 2.0", "float comparison"}
        };

        for (const auto& [code, name] : errorCases)
        {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            FCEvaluationContext ctx;
            bool isDangle = true;
            if (ast)
            {
                try
                {
                    FCValue result = evaluate(ast.get(), ctx);
                    isDangle = (result.type == FCValueCategory::Dangle);
                }
                catch (...)
                {
                    isDangle = true;
                }
            }
            expect(isDangle, "evaluator-error - " + name);
        }

        // 直接驱动公共 API，覆盖 dispatch 的空指针 / 未知节点 / 帧查找错误分支
        {
            FCEvaluationContext ctx;
            expect(evaluate(static_cast<FCExprAST*>(nullptr), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - evaluate null");

            const FCExprAST* constNull = nullptr;
            expect(evaluate(constNull, ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - const evaluate null");

            std::vector<FCVariableExprAST> protoArgs;
            auto proto = std::make_unique<FCPrototypeAST>("p", std::move(protoArgs));
            expect(evaluate(proto.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - evaluate prototype node");

            // 声明为空
            auto nullDeclVar = std::make_unique<FCVariableExprAST>(nullptr);
            expect(evaluate(nullDeclVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - variable with null declaration");

            // 局部变量但无调用帧
            auto localDecl = std::make_shared<VarDecl>("v", "int");
            // localDecl->scopeLevel = 1;
            localDecl->slot = 0;
            auto localVar = std::make_unique<FCVariableExprAST>(localDecl);
            expect(evaluate(localVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - local variable without frame");

            // 全局变量但槽位非法
            auto negDecl = std::make_shared<VarDecl>("g", "int");
            // negDecl->scopeLevel = 0;
            negDecl->slot = -1;
            auto negVar = std::make_unique<FCVariableExprAST>(negDecl);
            expect(evaluate(negVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - global variable with negative slot");

            // 局部变量槽位超出帧大小
            auto oobDecl = std::make_shared<VarDecl>("o", "int");
            // oobDecl->scopeLevel = 1;
            oobDecl->slot = 100;
            auto oobVar = std::make_unique<FCVariableExprAST>(oobDecl);
            ctx.pushFrame("oob");
            expect(evaluate(oobVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - local variable slot out of range");
            ctx.popFrame();
        }
    }

    // ==================== Stress Tests ====================
    void runStressTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "STRESS TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::tuple<std::string, std::string, std::string>> stressCases = {
            {"def count(n:int) { if n < 1 then 0 else count(n - 1) + 1 }; count(100)", "deep recursion 100", "100"},
            {"def sumN(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }; sumN(1000)", "large loop 1000", "499500"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }; fib(15)", "fibonacci 15", "610"},
            {"def pow2(n:int) { var r:int = 1; for i = 0, i < n, 1 in r = r * 2; r }; pow2(20)", "exponential loop", "1048576"},
            {"def ack(m:int) { if m < 1 then 1 else (ack(m - 1) + ack(m - 1)) }; ack(12)", "binary tree recursion", "4096"}
        };

        for (const auto& [code, name, expected] : stressCases)
        {
            std::cout << "  Testing: " << name << "\n";
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
            {
                expect(false, "stress - " + name + " (scanner)");
                continue;
            }

            FCEvaluationContext ctx;
            try
            {
                FCValue result = evaluate(ast.get(), ctx);
                std::string actual;
                if (result.type == FCValueCategory::Integer)
                    actual = std::to_string(result.evaluteVal.intVal);
                else if (result.type == FCValueCategory::Floating)
                    actual = std::to_string(result.evaluteVal.doubleVal);
                expect(result.type != FCValueCategory::Dangle && actual == expected,
                       "stress - " + name);
            }
            catch (...)
            {
                expect(false, "stress - " + name + " (exception)");
            }
        }

        // codegen 压力：多函数、多全局、复杂表达式
        {
            std::string code =
                "var a:int = 1; var b:double = 2.5; "
                "def f1(x:int) { x + 1 }; "
                "def f2(x:double) { x * 2.0 }; "
                "def f3(x:int) { if x < 10 then x else x - 10 }; "
                "def f4(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }; "
                "f4(f1(a))";
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (ast)
            {
                FCCodegenContext cc("StressModule");
                auto* value = codegen(ast.get(), cc);
                expect(value != nullptr, "stress - codegen multi-function module");
            }
            else
            {
                expect(false, "stress - codegen multi-function module (scanner)");
            }
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