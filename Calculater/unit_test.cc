#include "codegen.h"
#include "evaluator.h"
#include "scanner.h"
#include "semantic.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

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

        // ============ Codegen API Tests ============
        runCodegenApiTests();

        // ============ AST Info Tests ============
        runAstInfoTests();

        // ============ Token / AST API Tests ============
        runTokenApiTests();

        // ============ Symbol Table Tests ============
        runSymbolTableTests();

        // ============ Compiled Program Tests ============
        runCompiledProgramTests();

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

    // ============================================================
    // 辅助函数：把扫描器的语义输出（CompiledProgram）注入求值/代码生成上下文。
    // 这是当前设计的明确契约：CompiledProgram 作为语义输出传递给 evaluator 与 codegen。
    // ============================================================
    static FCEvaluationContext makeEvalContext(const FCScanner& scanner)
    {
        FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
        return ctx;
    }

    // FCCodegenContext 不可移动/复制（成员含 LLVMContext / unique_ptr），
    // 因此代码生成上下文就地构造后，再注入语义输出。
    static void attachSemanticOutput(FCCodegenContext& cc, const FCScanner& scanner)
    {
        cc.compiledProgram = scanner.semanticContext().getCompiledProgram();
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
            {"def add(a:int, b:double) { a + b }", "function with typed params"},
            {"def empty() { 42 }", "zero-argument function"},
            {"def choose(a:int) { if a < 1 then 10 else 20 }", "if expression"},
            {"def loop(n:int) { for i = 0, i < n, 1 in i }", "for expression"},
            {"foo(1, 2)", "function call"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "fibonacci"},
            {"var counter:int = 0; def next() { counter = counter + 1; counter }", "counter function"},
            {"def max3(a:int, b:int, c:int) { if a < b then if b < c then c else b else if a < c then c else a }", "nested if"},
            {"var x:int = 1; var y:int = 2; var z:int = x + y; z", "multi-global init"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "recursive factorial"},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }", "recursive power"},
            {"def concat(a:string, b:string) { a + b }", "string function"},
            {"var g:double = 3.14; g * 2.0", "floating global"},
            {"def nestedCall() { add(2, 3) }", "function call inside function"},
            {"var g:int = 0; def inc() { g = g + 1 }; def dec() { g = g - 1 }", "multiple functions with global"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with accumulation"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }", "complex recursive"},
            {"var a:int = 5; (a = a + 1; a * 2)", "assignment in parentheses"},
            {"def id(x:int) { x }; id(id(3))", "nested function calls"},
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
            {"def deep(x:int) { if x < 1 then 1 else (x * deep(x - 1)) }", "recursive deep expression"},
            // ---- 新增：边界输入 ----
            {"", "empty input"},
            {"   \t\n", "whitespace only"},
            {"# comment without newline", "comment only at eof"},
            {"#", "bare hash at eof"},
            {"((((1 + 2))))", "deeply nested parentheses"},
            {"1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10", "long additive chain"},
            {"var x1:int = 1; x1 + x1", "identifier containing digit"},
            {"def f(a:int) { a = a + 1; a }", "assignment to parameter"},
            {"; ; ; 1", "multiple leading semicolons"},
            {"1; def f() { 2 }; 3", "mixed statements with def in middle"},
            {"\"\"", "empty string literal"},
            {"2.0", "float with trailing digit"},
            {"def f() { var x:int = 1; { var y:int = 2; x + y } }", "nested block with locals"},
            {"def f() { for i = 0, i < 5, 1 in for j = 0, j < 5, 1 in i + j }", "nested for loops"},
            {"def f() { if 1 then if 2 then 3 else 4 else 5 }", "if nested in then"},
            {"var g:int = 1; def bump() { g = g + 1 }; bump()", "global function call"},
            {"def f() { (1 + 2) * (3 + 4) - 5 / 2 }", "arithmetic with parens"},
            {"0.5 + 0.25", "fractional literals"},
            {"def f(a:int, b:double, c:string, d:int) { a + b }", "four mixed params"}
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
            {"1; =", "sequence with invalid expression"},
            // ---- 新增 ----
            {"def f(a:int,) { a }", "trailing comma after last param"},
            {"if 1 then 2 else 3 4", "trailing expression after if"},
            {"var x:int = 1; 1 = 2", "assignment to literal scans"},
            {"def f(a:int, b) { a }", "parameter missing type"},
            {"def f(a:int b) { a }", "parameter missing comma"},
            {"def f( ) { 1 }", "empty prototype parens"},
            {"def f() { if 1 then 2 }", "if missing else inside body"},
            {"def f() { for i = 0, i < 3 in }", "for missing body inside body"},
            {"\"unterminated\n1 + 2", "unterminated string with newline"},
            {"def f(a:double, b:double) { a + b }", "unused double params are fine"}
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
            {"def many(a:int, b:int, c:int, d:int, e:int) { a + b + c + d + e }", "five parameters", true},
            {"var g:int = 1; var h:double = 2.0; var s:string = \"x\"; g + h", "three typed globals", true},
            {"def f() { var x:int = 1; if x then 2 else 3 }", "local in if", true},
            {"def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "accumulator function", true}
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
                            details = "Global not found: " + varName + " (may be local)";
                        }
                    }
                }
            }

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
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }; countdown(5)", "alternating sum", "3"},
            {"var s:string = \"a\"; s = s + \"b\"; s + \"c\"", "string variable", "\"abc\""},
            {"def loopReturn(n:int) { for i = 0, i < n, 1 in i }; loopReturn(3)", "loop return value", "2"},
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
            // 块级遮蔽：同函数内层块重新声明同名变量，内层声明必须覆盖外层符号
            {"def f() { var x:int = 1; { var x:int = 2; x } }; f()", "block shadowing inner wins", "2"},
            {"def f() { var x:int = 1; { var x:int = 2; }; x }; f()", "block shadowing outer restored", "1"},
            {"def nested2() { var x:int = 1; var y:int = 2; x + y }; nested2()", "multiple locals", "3"},
            {"def nf(n:int) { var s:int = 0; for i = 0, i < n, 1 in for j = 0, j < n, 1 in s = s + i; s }; nf(3)", "nested for loop", "9"},
            {"def accum(n:int) { var r:int = 1; for i = 1, i < n, 1 in r = r * 2; r }; accum(5)", "loop accumulation multiply", "16"},
            {"def choose(n:int) { if n < 3 then 1 else (choose(n - 1) + choose(n - 2)) }; choose(6)", "tree recursion", "8"},
            // ---- 新增 ----
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) }; def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) }; isEven(10)", "mutual recursion even", "1"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) }; def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) }; isOdd(6)", "mutual recursion odd false", "0"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) }; def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) }; isOdd(7)", "mutual recursion odd true", "1"},
            {"def sumdown(n:int) { var s:int = 0; for i = n, 0 < i, 0 - 1 in s = s + i; s }; sumdown(5)", "negative step loop", "15"},
            {"def f() { 1 }; def g() { 2 }; f() + g()", "multiple functions combined", "3"},
            {"var g:int = 0; def bump() { g = g + 2 }; bump(); bump(); g", "global accumulation", "4"},
            {"def dec(n:int) { n - 1 }; dec(dec(dec(5)))", "chained calls", "2"},
            {"def add3(a:int, b:int, c:int) { a + b + c }; add3(1, 2, 3)", "three-argument function", "6"},
            {"var s:string = \"ab\"; def app(x:string) { s = s + x; s }; app(\"c\")", "string param mutation", "\"abc\""},
            {"def dbl2(x:double) { x * 2.0 }; dbl2(dbl2(2.0))", "double chained", "8.0"},
            {"def pick(a:int) { if a then 1 else 0 }; pick(0) + pick(5)", "if as value", "1"},
            {"var a:int = 1; var b:int = 2; def swap() { var t:int = a; a = b; b = t }; swap(); a * 10 + b", "swap via temp", "21"},
            {"def fib10(n:int) { if n < 2 then n else fib10(n - 1) + fib10(n - 2) }; fib10(10)", "fibonacci 10", "55"},
            {"def loop1(n:int) { for i = 0, i < n, 1 in i }; loop1(1)", "single iteration loop", "0"},
            {"def loop0(n:int) { for i = 0, i < n, 1 in i }; loop0(0)", "zero iteration loop", "0"},
            {"\"a\" + \"b\" + \"c\"", "string chain concat top-level", "\"abc\""},
            {"def mul2(a:int, b:int) { a * b }; def twice(x:int) { mul2(x, 2) }; twice(21)", "call through helper", "42"},
            {"var g:double = 1.0; def up() { g = g + 0.5 }; up(); up(); g", "double global mutation", "2.0"},
            {"def f() { var x:int = 1; { var y:int = 2; x + y } }; f()", "block locals", "3"},
            {"def cl(n:int) { if n < 1 then 0 else (cl(n - 1) + 1) }; cl(25)", "linear recursion 25", "25"},
            {"def gauss(n:int) { var s:int = 0; for i = 1, i < n, 1 in s = s + i; s }; gauss(100)", "gauss sum", "4950"},
            {"0 - 7", "negative result", "-7"},
            {"7 * 0", "multiply by zero", "0"},
            {"var s:string = \"x\"; s + \"\"", "string plus empty", "\"x\""}
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

            FCEvaluationContext evalContext = makeEvalContext(scanner);
            try
            {
                auto startTime = std::chrono::high_resolution_clock::now();
                FCValue result = evaluate(ast.get(), evalContext);
                auto endTime = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);

                bool ok = result.type != FCValueCategory::Dangle;
                std::string actual = valueToString(result);
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
            {"def f1() { 1 }; def f2() { 2 }; def f3() { 3 }; def f4() { 4 }; def f5() { 5 }; def f6() { 6 }", "six functions", 6},
            // ---- 新增 ----
            {"def a() { 1 }; var g:int = 1; def b() { g }", "functions interleaved with global", 2}
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
            std::make_unique<FCPrototypeAST>("duplicate", std::vector<VarDeclPtr>{}),
            std::make_unique<FCNumberExprAST>(1), 0);
        auto func2 = std::make_unique<FCFunctionAST>(
            std::make_unique<FCPrototypeAST>("duplicate", std::vector<VarDeclPtr>{}),
            std::make_unique<FCNumberExprAST>(2), 0);

        bool first = registry.registerFunction(func1.get());
        bool second = registry.registerFunction(func2.get());
        bool ok = first && !second;
        expect(ok, "registry - duplicate function rejection");
        std::cout << "      First registration: " << (first ? "success" : "failed") << "\n";
        std::cout << "      Second registration: " << (second ? "should fail" : "correctly rejected") << "\n";

        // 注册同一个函数指针两次：幂等
        expect(registry.registerFunction(func1.get()),
            "registry - re-register same pointer is idempotent");
        expect(registry.findFunction("duplicate") == func1.get(),
            "registry - find returns first registered");

        // 额外测试：registerFunction(null) / index 各分支 / clear / findFunction
        std::cout << "  Testing: registry API edge cases\n";
        {
            FCFunctionRegistry reg;
            expect(!reg.registerFunction(nullptr), "registry - register null function");
            expect(!reg.index(nullptr), "registry - index null");
            expect(reg.findFunction("missing") == nullptr, "registry - find missing returns null");

            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("solo", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1), 0);
            expect(reg.index(fn.get()), "registry - index single function");
            expect(reg.findFunction("solo") != nullptr, "registry - find after index");

            auto num = std::make_unique<FCNumberExprAST>(42);
            expect(!reg.index(num.get()), "registry - index non-function non-program");

            // index 一个只含函数的 program
            std::vector<std::unique_ptr<FCExprAST>> stmts;
            stmts.push_back(std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("progfn", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1), 0));
            auto prog = std::make_unique<FCProgramAST>(std::move(stmts));
            expect(reg.index(prog.get()), "registry - index program with functions");
            expect(reg.findFunction("progfn") != nullptr, "registry - program function found");

            reg.clear();
            expect(reg.findFunction("solo") == nullptr, "registry - clear");
            expect(reg.findFunction("progfn") == nullptr, "registry - clear removes all");
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
            {"{ 1 + 2 }; def f() { 3 }", "top-level block in program"},
            {"if 1 then 2 else 3; def f() { 4 }", "top-level if in program"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }; fib(5)", "recursive function in program"},
            {"def fmax(a:double, b:double) { if a < b then b else a }", "double if expression"},
            {"def fcond(a:double) { if a then 1 else 2 }", "floating if condition"},
            {"var s:string = \"hi\"", "standalone string global"},
            // ---- 新增 ----
            {"def caller() { later(5) }; def later(x:int) { x + 1 }", "forward reference call"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) }; def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) }", "mutual recursion codegen"},
            {"def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in { s = s + i }; s }", "for with block body"},
            {"def f() { 1 + 2 * 3 - 4 / 2 + (5 * 6) }", "deep arithmetic"},
            {"def f(a:double) { a }; def g() { f(1) }", "int arg to double param"},
            {"var g:double = 1.5; def f() { g = 2.5 }", "double global assignment"},
            {"def f(a:int) { a }; def g() { f(3) }; def h() { g() + f(4) }", "call chain across functions"},
            {"def f() { if 1 then 2 else 3 }", "constant condition if"},
            {"var x:int = 1; x + 2", "top-level expression with global"},
            {"def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + (i * 2); s }", "loop with arithmetic body"},
            {"def f(a:int) { var r:int = a; r = r * 2; r = r + 1; r }", "sequential local assignments"},
            {"def f(s:string) { s + s }", "string param self concat"},
            {"def f(a:double, b:double) { if a < b then a else b }; def g() { f(1.0, 2.0) }", "double call with comparison"},
            {"def outer(n:int) { var s:int = 0; for i = 0, i < n, 1 in { var t:int = i * 2; s = s + t }; s }", "loop with inner declaration"},
            {"var g1:int = 1; var g2:int = 2; def f() { g1 + g2 }", "two globals read in function"},
            {"def f() { 3.0 / 2.0 }", "float division literal"}
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
                FCCodegenContext codegenContext("CodegenTest_" + name, scanner.semanticContext().getCompiledProgram());
                attachSemanticOutput(codegenContext, scanner);
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
                    if (!moduleValid)
                        expect(false, "codegen - " + name + " (invalid module)");
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
        // {"{ var g:int = 1; g }; def f() { g }", "top-level block with scoped declaration"},
        auto ast = scanner.analysis("def bad() { unknown_function() }");
        if (ast)
        {
            FCCodegenContext errorContext("ErrorTest", scanner.semanticContext().getCompiledProgram());
            attachSemanticOutput(errorContext, scanner);
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
            {"def empty() {}", "empty function body"},
            // ---- 新增 ----
            {"def f(a:string, b:string) { a < b }", "string comparison"},
            {"def f() { 1.0 = 2.0 }", "assignment to float literal"},
            {"def f() { var x:int = 1; for i = 0, i < 3, 1 in x = \"s\" }", "assignment mismatch in loop body"},
            {"def f(a:int) { a }; f(1, 2, 3)", "too many arguments"},
            {"def f() { g(1) }; g(1)", "call to undefined top-level function"},
            {"def f() { if 1.5 then 1 else \"x\" }", "float condition with string branch"},
            {"def f() { var x:int = 1; x = x + \"s\"; x }", "assignment with mismatched rhs"},
            {"def f() { for i = 0, i < 3, 1 in 1.0 = 2.0 }", "assignment to literal in loop body"},
            {"if 1 then (var g:int = 2; g) else 3; def f() { g }", "top-level if with scoped declaration"}
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
                    FCCodegenContext cc("ErrorTest_" + name,  scanner.semanticContext().getCompiledProgram());
                    attachSemanticOutput(cc, scanner);
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
            FCCodegenContext cc("ApiTest", {});
            expect(codegen(static_cast<FCExprAST*>(nullptr), cc) == nullptr,
                   "codegen-error - codegen null expression");

            std::vector<VarDeclPtr> protoArgs;
            protoArgs.push_back(std::make_shared<VarDecl>("a", "int"));
            auto proto = std::make_unique<FCPrototypeAST>("proto", std::move(protoArgs));
            expect(codegen(proto.get(), cc) != nullptr,
                   "codegen-error - codegen standalone prototype");

            auto emptyBody = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("empty", std::vector<VarDeclPtr>{}),
                nullptr, 0);
            expect(codegen(emptyBody.get(), cc) == nullptr,
                   "codegen-error - codegen function with null body");

            auto nullBody2 = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("empty2", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1), 0);
            (void)nullBody2;
        }
    }

    // ==================== Codegen API Tests ====================
    void runCodegenApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "CODEGEN API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // FCCodegenContext::getType
        {
            FCCodegenContext cc("TypeApi", {});
            expect(cc.getType("double")->isDoubleTy(), "codegen-api - getType double");
            expect(cc.getType("string")->isPointerTy(), "codegen-api - getType string");
            expect(cc.getType("int")->isIntegerTy(), "codegen-api - getType int");
            expect(cc.getType("bool")->isIntegerTy(), "codegen-api - getType unknown defaults int");
            expect(cc.getType("")->isIntegerTy(), "codegen-api - getType empty defaults int");
        }

        // 在函数上下文内直接生成各种字面量与二元运算
        {
            FCCodegenContext cc("ExprApi", {});
            auto* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(cc.llvmContext), {}, false);
            auto* fn = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "api_fn", cc.module.get());
            auto* entry = llvm::BasicBlock::Create(cc.llvmContext, "entry", fn);
            cc.builder.SetInsertPoint(entry);

            auto num = std::make_unique<FCNumberExprAST>(7);
            auto* nv = codegen(num.get(), cc);
            expect(nv != nullptr && nv->getType()->isIntegerTy(), "codegen-api - integer literal");

            auto flt = std::make_unique<FCNumberExprAST>(2.5);
            auto* fv = codegen(flt.get(), cc);
            expect(fv != nullptr && fv->getType()->isDoubleTy(), "codegen-api - float literal");

            auto str = std::make_unique<FCStringExprAST>("hi");
            auto* sv = codegen(str.get(), cc);
            expect(sv != nullptr && sv->getType()->isPointerTy(), "codegen-api - string literal");

            auto bin = std::make_unique<FCBinaryExprAST>(
                '+', std::make_unique<FCNumberExprAST>(1), std::make_unique<FCNumberExprAST>(2));
            auto* bv = codegen(bin.get(), cc);
            expect(bv != nullptr, "codegen-api - binary add");

            auto cmp = std::make_unique<FCBinaryExprAST>(
                '<', std::make_unique<FCNumberExprAST>(1), std::make_unique<FCNumberExprAST>(2));
            auto* cv = codegen(cmp.get(), cc);
            expect(cv != nullptr, "codegen-api - binary compare");

            // createEntryBlockAlloca
            auto* alloca = cc.createEntryBlockAlloca(fn, "tmp", cc.getType("int"));
            expect(alloca != nullptr && alloca->getAllocatedType()->isIntegerTy(),
                "codegen-api - createEntryBlockAlloca");

            // 局部变量分配 + 读取 + 赋值
            auto decl = std::make_shared<VarDecl>("loc", "int");
            VariableSymbol sym(decl, {VariableStorage::Kind::Local, 0}, 1);
            cc.compiledProgram.allSymbols.addSymbol("loc", sym);
            auto* locSymbol = cc.compiledProgram.allSymbols.lookup("loc");
            auto* locAlloca = cc.createEntryBlockAlloca(fn, "loc", cc.getType("int"));
            cc.namedValues[locSymbol] = locAlloca;

            auto var = std::make_unique<FCVariableExprAST>(decl);
            var->resolved = locSymbol;
            auto* vv = codegen(var.get(), cc);
            expect(vv != nullptr && vv->getType()->isIntegerTy(), "codegen-api - local variable load");

            auto assignLHS = std::make_unique<FCVariableExprAST>(decl);
            assignLHS->resolved = locSymbol;
            auto assign = std::make_unique<FCBinaryExprAST>(
                '=', std::move(assignLHS), std::make_unique<FCNumberExprAST>(5));
            auto* av = codegen(assign.get(), cc);
            expect(av != nullptr, "codegen-api - local assignment");

            cc.builder.CreateRet(av ? av : bv);
            expect(!llvm::verifyFunction(*fn, &llvm::errs()), "codegen-api - function verifies");
        }

        // 直接 codegen 一个 prototype 声明，再 codegen 同名同签名函数（复用已有声明）
        {
            FCCodegenContext cc("ReuseApi", {});
            std::vector<VarDeclPtr> args;
            args.push_back(std::make_shared<VarDecl>("x", "int"));
            auto proto = std::make_unique<FCPrototypeAST>("reuse", std::move(args));
            expect(codegen(proto.get(), cc) != nullptr, "codegen-api - prototype declares function");
            expect(cc.module->getFunction("reuse") != nullptr, "codegen-api - declared function present");

            // 同名同签名：完成预先声明的空函数体
            std::vector<VarDeclPtr> fnArgs;
            fnArgs.push_back(std::make_shared<VarDecl>("x", "int"));
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("reuse", std::move(fnArgs)),
                std::make_unique<FCNumberExprAST>(1), 0);
            expect(codegen(fn.get(), cc) != nullptr, "codegen-api - completes pre-declared prototype");

            // 同一个函数对象重复 codegen：直接复用已有实现
            auto fn2 = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("twice", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(2), 0);
            expect(codegen(fn2.get(), cc) != nullptr, "codegen-api - first codegen of function");
            expect(codegen(fn2.get(), cc) != nullptr, "codegen-api - second codegen reuses existing");
            expect(cc.module->getFunction("twice") != nullptr, "codegen-api - twice function present");
        }
    }

    // ==================== AST Info Tests ====================
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

            std::vector<VarDeclPtr> protoArgs;
            protoArgs.push_back(std::make_shared<VarDecl>("a", "int"));
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

    // ==================== Token / AST API Tests ====================
    void runTokenApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "TOKEN / AST API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 数字字面量
        {
            FCNumberExprAST ni(42);
            expect(ni.m_intVal == 42 && !ni.isFloating(), "token-api - int literal value");
            expect(ni.type == FCTypeDescribe::NumberExpr, "token-api - int literal type");
            FCNumberExprAST nd(3.5);
            expect(nd.m_doubleVal == 3.5 && nd.isFloating(), "token-api - double literal value");
        }

        // 字符串字面量
        {
            FCStringExprAST s("hello");
            expect(s.m_stringVal == "hello", "token-api - string value");
            expect(s.type == FCTypeDescribe::StringExpr, "token-api - string type");
        }

        // 变量表达式
        {
            auto decl = std::make_shared<VarDecl>("v", "double");
            FCVariableExprAST v(decl);
            expect(v.decl == decl, "token-api - variable decl pointer");
            expect(v.type == FCTypeDescribe::VariableExpr, "token-api - variable type");
            expect(decl->name == "v" && decl->typeName == "double", "token-api - VarDecl fields");
        }

        // 二元表达式
        {
            auto bin = std::make_unique<FCBinaryExprAST>(
                '*', std::make_unique<FCNumberExprAST>(3), std::make_unique<FCNumberExprAST>(4));
            expect(bin->getOperator() == '*', "token-api - binary operator");
            expect(bin->getLHS() != nullptr && bin->getRHS() != nullptr, "token-api - binary operands");
            expect(bin->type == FCTypeDescribe::BinaryExpr, "token-api - binary type");
        }

        // 调用表达式
        {
            std::vector<std::unique_ptr<FCExprAST>> args;
            args.push_back(std::make_unique<FCNumberExprAST>(1));
            args.push_back(std::make_unique<FCNumberExprAST>(2));
            auto call = std::make_unique<FCCallExprAST>("foo", std::move(args));
            expect(call->getName() == "foo", "token-api - call callee");
            expect(call->getArgs().size() == 2, "token-api - call args count");
            expect(call->type == FCTypeDescribe::CallExpr, "token-api - call type");
        }

        // 原型
        {
            std::vector<VarDeclPtr> args;
            args.push_back(std::make_shared<VarDecl>("a", "int"));
            args.push_back(std::make_shared<VarDecl>("b", "double"));
            auto proto = std::make_unique<FCPrototypeAST>("proto", std::move(args));
            expect(proto->getProtoName() == "proto", "token-api - prototype name");
            expect(proto->getArgs().size() == 2, "token-api - prototype args count");
            expect(proto->getArgs()[0]->name == "a" && proto->getArgs()[0]->typeName == "int",
                "token-api - prototype arg fields");
            expect(proto->type == FCTypeDescribe::Prototype, "token-api - prototype type");
        }

        // 函数
        {
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("fn", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(9), 3);
            expect(fn->getProtoName() == "fn", "token-api - function proto name");
            expect(fn->getBody() != nullptr, "token-api - function body");
            expect(fn->getProto()->getProtoName() == "fn", "token-api - function getProto");
            expect(fn->getLocalCount() == 3, "token-api - function local count");
            expect(fn->type == FCTypeDescribe::Function, "token-api - function type");
        }

        // if / for 访问器
        {
            auto cond = std::make_unique<FCNumberExprAST>(1);
            auto then = std::make_unique<FCNumberExprAST>(2);
            auto els = std::make_unique<FCNumberExprAST>(3);
            auto ifExpr = std::make_unique<FCIfExprAST>(
                std::move(cond), std::move(then), std::move(els));
            expect(ifExpr->getCondition() != nullptr && ifExpr->getThen() != nullptr &&
                   ifExpr->getElse() != nullptr, "token-api - if accessors");

            auto decl = std::make_shared<VarDecl>("i", "int");
            auto forExpr = std::make_unique<FCForExprAST>(
                decl, std::make_unique<FCNumberExprAST>(0), std::make_unique<FCNumberExprAST>(10),
                std::make_unique<FCNumberExprAST>(2), std::make_unique<FCNumberExprAST>(1));
            expect(forExpr->getDecl() == decl, "token-api - for decl");
            expect(forExpr->getStart() != nullptr && forExpr->getEnd() != nullptr &&
                   forExpr->getStep() != nullptr && forExpr->getBody() != nullptr,
                "token-api - for accessors");

            auto forNoStep = std::make_unique<FCForExprAST>(
                decl, std::make_unique<FCNumberExprAST>(0), std::make_unique<FCNumberExprAST>(10),
                nullptr, std::make_unique<FCNumberExprAST>(1));
            expect(forNoStep->getStep() == nullptr, "token-api - for null step");
        }

        // seq / block / program / vardecl
        {
            std::vector<std::unique_ptr<FCExprAST>> seqItems;
            seqItems.push_back(std::make_unique<FCNumberExprAST>(1));
            seqItems.push_back(std::make_unique<FCNumberExprAST>(2));
            auto seq = std::make_unique<FCSeqExprAST>(std::move(seqItems));
            expect(seq->getExpressions().size() == 2, "token-api - seq expressions");

            std::vector<std::unique_ptr<FCExprAST>> blkItems;
            blkItems.push_back(std::make_unique<FCNumberExprAST>(1));
            auto blk = std::make_unique<FCBlockExprAST>(std::move(blkItems));
            expect(blk->getExpressions().size() == 1, "token-api - block expressions");

            std::vector<std::unique_ptr<FCExprAST>> stmts;
            stmts.push_back(std::make_unique<FCNumberExprAST>(1));
            stmts.push_back(std::make_unique<FCNumberExprAST>(2));
            auto prog = std::make_unique<FCProgramAST>(std::move(stmts));
            expect(prog->getStatements().size() == 2, "token-api - program statements");

            auto vd = std::make_unique<FCVarDeclExprAST>(
                std::make_shared<VarDecl>("x", "int"), std::make_unique<FCNumberExprAST>(5));
            expect(vd->decl->name == "x" && vd->decl->typeName == "int", "token-api - vardecl decl");
            expect(vd->initExpr != nullptr, "token-api - vardecl init");
        }

        // VariableStorage / VariableSymbol 默认值
        {
            VariableSymbol sym;
            expect(sym.declaration == nullptr, "token-api - default symbol null decl");
            expect(sym.storage.kind == VariableStorage::Kind::Local, "token-api - default symbol local");
            expect(sym.storage.slot == -1, "token-api - default symbol slot -1");
            expect(sym.scopeDepth == 0 && !sym.isMutable && !sym.isCaptured,
                "token-api - default symbol flags");

            auto decl = std::make_shared<VarDecl>("g", "int");
            VariableSymbol custom(decl, {VariableStorage::Kind::Global, 4}, 2);
            expect(custom.declaration == decl && custom.storage.kind == VariableStorage::Kind::Global &&
                   custom.storage.slot == 4 && custom.scopeDepth == 2,
                "token-api - custom symbol fields");

            VariableStorage st;
            expect(st.slot == -1, "token-api - storage default slot");
        }

        // CompiledProgram / CompiledFunction 默认值
        {
            CompiledProgram prog;
            expect(prog.globalFrameSize == 0, "token-api - compiled program default frame size");
            expect(prog.getFunction("none") == nullptr, "token-api - compiled program missing function");

            CompiledFunction cf;
            expect(cf.ast == nullptr && cf.frameSize == 0 && cf.maxTempSlots == 0,
                "token-api - compiled function defaults");
            expect(cf.parameters.empty(), "token-api - compiled function empty params");
        }

        // FCTypeDescribe 枚举值
        {
            expect(static_cast<int>(FCTypeDescribe::Expr) == 0, "token-api - enum Expr");
            expect(static_cast<int>(FCTypeDescribe::NumberExpr) == 1, "token-api - enum NumberExpr");
            expect(static_cast<int>(FCTypeDescribe::StringExpr) == 2, "token-api - enum StringExpr");
            expect(static_cast<int>(FCTypeDescribe::VariableExpr) == 3, "token-api - enum VariableExpr");
            expect(static_cast<int>(FCTypeDescribe::BinaryExpr) == 4, "token-api - enum BinaryExpr");
            expect(static_cast<int>(FCTypeDescribe::CallExpr) == 5, "token-api - enum CallExpr");
            expect(static_cast<int>(FCTypeDescribe::Prototype) == 6, "token-api - enum Prototype");
            expect(static_cast<int>(FCTypeDescribe::Function) == 7, "token-api - enum Function");
        }
    }

    // ==================== Symbol Table Tests ====================
    void runSymbolTableTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SYMBOL TABLE TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        SymbolTable table;
        auto d1 = std::make_shared<VarDecl>("alpha", "int");
        VariableSymbol s1(d1, {VariableStorage::Kind::Global, 0}, 0);

        expect(table.addSymbol("alpha", s1), "symbol-table - add new symbol");
        expect(!table.addSymbol("alpha", s1), "symbol-table - duplicate add rejected");
        expect(table.size() == 1, "symbol-table - size after add");

        const SymbolTable& ctable = table;
        expect(ctable.lookup("alpha") != nullptr, "symbol-table - const lookup hit");
        expect(ctable.lookup("alpha")->declaration == d1, "symbol-table - const lookup declaration");
        expect(ctable.lookup("alpha")->storage.slot == 0, "symbol-table - const lookup storage");
        expect(ctable.lookup("missing") == nullptr, "symbol-table - const lookup miss");

        expect(table.lookup("alpha") != nullptr, "symbol-table - mutable lookup hit");
        expect(table.lookup("missing") == nullptr, "symbol-table - mutable lookup miss");

        expect(table.getAllSymbols().size() == 1, "symbol-table - getAllSymbols size");
        expect(table.getAllSymbols().count("alpha") == 1, "symbol-table - getAllSymbols content");

        expect(table.removeSymbol("alpha"), "symbol-table - remove hit");
        expect(!table.removeSymbol("alpha"), "symbol-table - remove miss");
        expect(table.size() == 0, "symbol-table - empty after remove");

        auto d2 = std::make_shared<VarDecl>("beta", "double");
        VariableSymbol s2(d2, {VariableStorage::Kind::Local, 3}, 1);
        expect(table.addSymbol("beta", s2), "symbol-table - add second symbol");
        table.dump();
        expect(table.size() == 1, "symbol-table - size after second add");

        table.clear();
        expect(table.size() == 0, "symbol-table - clear");
        expect(table.lookup("beta") == nullptr, "symbol-table - lookup after clear");

        // 通过 FCSemanticContext 间接驱动 SymbolTable（persistent 表）
        {
            FCSemanticContext ctx;
            ctx.declareVariable(std::make_shared<VarDecl>("p1", "int"));
            ctx.declareVariable(std::make_shared<VarDecl>("p2", "double"));
            // persistent 表与 compiledProgram 无关，这里只验证编译输出
            const auto& prog = ctx.getCompiledProgram();
            expect(prog.allSymbols.size() == 2, "symbol-table - semantic globals recorded");
        }
    }

    // ==================== Compiled Program Tests ====================
    // 验证语义输出契约：CompiledProgram 记录全局变量、函数参数、局部变量的存储布局。
    void runCompiledProgramTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "COMPILED PROGRAM TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 全局 + 参数 + 局部
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var g:int = 1; def add(a:int, b:int) { var t:int = a + b; t }");
            expect(ast != nullptr, "compiled - scan ok");
            const auto& sem = scanner.semanticContext();
            const auto& prog = sem.getCompiledProgram();

            int symbolCnt = 0;
            for (auto& func : prog.functions)
            {
                symbolCnt += func.second->frameSize;
            }
            expect(prog.allSymbols.size() + symbolCnt == 4, "compiled - all symbols recorded");

            auto lookupVar = [&](const std::string& varName)
            {
                auto* sym = prog.allSymbols.lookup(varName);
                if (!sym)
                {
                    for (auto& func : prog.functions)
                    {
                        sym = func.second->symbols.lookup(varName);
                        if (sym) break;
                    }
                }

                return sym;
            };

            auto* gsym = lookupVar("g");
            expect(gsym != nullptr && gsym->storage.kind == VariableStorage::Kind::Global &&
                   gsym->storage.slot == 0, "compiled - global symbol layout");

            auto* asym = lookupVar("a");
            expect(asym != nullptr && asym->storage.kind == VariableStorage::Kind::Local &&
                   asym->storage.slot == 0, "compiled - param a layout");

            auto* bsym = lookupVar("b");
            expect(bsym != nullptr && bsym->storage.kind == VariableStorage::Kind::Local &&
                   bsym->storage.slot == 1, "compiled - param b layout");

            auto* tsym = lookupVar("t");
            expect(tsym != nullptr && tsym->storage.kind == VariableStorage::Kind::Local &&
                   tsym->storage.slot == 2, "compiled - local t layout");

            const auto* add = prog.getFunction("add");
            expect(add != nullptr, "compiled - getFunction add");
            expect(add->frameSize == 3, "compiled - frame size includes params and locals");
            expect(add->maxTempSlots == 3, "compiled - max temp slots");
            expect(add->symbols.lookup("a") != nullptr && add->symbols.lookup("b") != nullptr &&
                   add->symbols.lookup("t") != nullptr, "compiled - function symbol table");
            expect(add->symbols.lookup("g") == nullptr, "compiled - globals not in function symbols");

            // AST 指针身份
            auto* program = dynamic_cast<FCProgramAST*>(ast.get());
            expect(program != nullptr, "compiled - program ast");
            bool foundAst = false;
            for (const auto& stmt : program->getStatements())
            {
                auto* fn = dynamic_cast<FCFunctionAST*>(stmt.get());
                if (fn && fn->getProtoName() == "add" && add->ast == fn)
                    foundAst = true;
            }
            expect(foundAst, "compiled - ast pointer identity");

            expect(prog.getFunction("missing") == nullptr, "compiled - missing function");
            expect(sem.getCompiledFunction("add") == add, "compiled - semantic getCompiledFunction");
            expect(sem.getCompiledFunction("missing") == nullptr, "compiled - semantic missing function");
        }

        // 只有全局
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var a:int = 1; var b:double = 2.0");
            expect(ast != nullptr, "compiled - globals scan ok");
            const auto& sem = scanner.semanticContext();
            const auto& prog = sem.getCompiledProgram();
            expect(prog.allSymbols.size() == 2, "compiled - only globals recorded");
            expect(prog.allSymbols.lookup("a")->storage.slot == 0, "compiled - global a slot");
            expect(prog.allSymbols.lookup("b")->storage.slot == 1, "compiled - global b slot");
            expect(sem.currentGlobalLayout().globalSize() == 2, "compiled - global layout size");
            expect(prog.functions.empty(), "compiled - no functions");
        }

        // 无局部变量的函数
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { 1 }");
            const auto& prog = scanner.semanticContext().getCompiledProgram();
            const auto* f = prog.getFunction("f");
            expect(f != nullptr && f->frameSize == 0 && f->maxTempSlots == 0,
                "compiled - no-local function frame");
        }

        // 递归函数：帧布局包含参数
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }");
            const auto& prog = scanner.semanticContext().getCompiledProgram();
            const auto* fib = prog.getFunction("fib");
            expect(fib != nullptr && fib->frameSize == 1, "compiled - recursive function frame");
        }

        // 直接注册函数（非扫描路径）
        {
            FCSemanticContext ctx;
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("solo", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1), 0);
            expect(ctx.registerFunction("solo", fn.get()), "compiled - direct register");
            const auto* cf = ctx.getCompiledFunction("solo");
            expect(cf != nullptr && cf->ast == fn.get(), "compiled - direct registered ast");
            expect(ctx.getCompiledProgram().getFunction("solo") == cf, "compiled - direct getFunction");
            expect(ctx.registerFunction("solo", fn.get()) == false, "compiled - direct duplicate rejected");
        }

        // reset 清空编译输出
        {
            FCSemanticContext ctx;
            ctx.registerFunction("f", nullptr);
            ctx.declareVariable(std::make_shared<VarDecl>("g", "int"));
            expect(ctx.hasFunction("f") && ctx.getCompiledFunction("f") != nullptr,
                "compiled - populated before reset");
            ctx.reset();
            expect(!ctx.hasFunction("f"), "compiled - reset clears function set");
            expect(ctx.getCompiledFunction("f") == nullptr, "compiled - reset clears compiled functions");
            expect(ctx.lookupGlobalVariable("g") == nullptr, "compiled - reset clears scope");
            expect(ctx.currentGlobalLayout().globalSize() == 0, "compiled - reset clears global layout");
            expect(ctx.currentFrameLayoutSize() == 0, "compiled - reset clears frame layout");
        }
    }

    // ==================== Semantic API Tests ====================
    void runSemanticApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SEMANTIC API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // ============================================================
        // Operator / Function Registry
        // ============================================================
        {
            FCSemanticContext ctx;

            expect(ctx.getOperatorPrecedence('=') == 5, "semantic-api - precedence assign");
            expect(ctx.getOperatorPrecedence('<') == 9, "semantic-api - precedence less");
            expect(ctx.getOperatorPrecedence('+') == 10, "semantic-api - precedence plus");
            expect(ctx.getOperatorPrecedence('-') == 10, "semantic-api - precedence minus");
            expect(ctx.getOperatorPrecedence('*') == 20, "semantic-api - precedence mul");
            expect(ctx.getOperatorPrecedence('/') == 20, "semantic-api - precedence div");
            expect(ctx.getOperatorPrecedence('?') == 0, "semantic-api - unknown precedence");

            expect(ctx.registerFunction("f", nullptr), "semantic-api - register new function");
            expect(ctx.hasFunction("f"), "semantic-api - hasFunction true");
            expect(!ctx.hasFunction("missing"), "semantic-api - hasFunction false");
            expect(!ctx.registerFunction("f", nullptr), "semantic-api - duplicate function rejected");
            expect(ctx.getCompiledFunction("f") != nullptr, "semantic-api - compiled record exists");
            expect(ctx.getCompiledFunction("f")->ast == nullptr, "semantic-api - null ast recorded");
        }

        // ============================================================
        // Global Scope
        // ============================================================
        {
            FCSemanticContext ctx;

            auto g = std::make_shared<VarDecl>("g", "int");
            auto* sym = ctx.declareVariable(g);

            expect(sym != nullptr, "semantic-api - declare global");
            expect(sym->storage.kind == VariableStorage::Kind::Global, "semantic-api - global kind");
            expect(sym->storage.slot == 0, "semantic-api - global slot 0");

            expect(ctx.lookupGlobalVariable("g") == sym, "semantic-api - lookup global hit");
            expect(ctx.lookupVariable("g") == sym, "semantic-api - lookup variable finds global");
            expect(ctx.lookupVariableInCurrentScope("g") == sym, "semantic-api - global in current scope");
            expect(ctx.lookupGlobalVariable("missing") == nullptr, "semantic-api - lookup global miss");
            expect(ctx.lookupVariable("missing") == nullptr, "semantic-api - lookup miss");

            auto duplicate = std::make_shared<VarDecl>("g", "int");
            expect(ctx.declareVariable(duplicate) == nullptr, "semantic-api - duplicate global rejected");
            expect(ctx.lookupGlobalVariable("g") == sym, "semantic-api - original global preserved");
        }

        // ============================================================
        // Function Scope
        // ============================================================
        {
            FCSemanticContext ctx;

            auto g = std::make_shared<VarDecl>("g", "int");
            auto* gsym = ctx.declareVariable(g);

            {
                auto guard = ctx.scopedFunction();

                auto p = std::make_shared<VarDecl>("p", "int");
                auto* psym = ctx.declareVariable(p);

                expect(psym != nullptr, "semantic-api - declare local");
                expect(psym->storage.kind == VariableStorage::Kind::Local, "semantic-api - local kind");
                expect(psym->storage.slot == 0, "semantic-api - local slot 0");

                expect(ctx.lookupVariable("p") == psym, "semantic-api - lookup local hit");
                expect(ctx.lookupVariableInCurrentScope("p") == psym, "semantic-api - current scope hit");
                // 注意：作用域栈 push 会导致外层 map 重新分配，
                // 因此在跨作用域比较时重新查找，而不是保存旧的指针。
                expect(ctx.lookupVariable("g") == ctx.lookupGlobalVariable("g"),
                    "semantic-api - lookup falls back to global");
                expect(ctx.lookupVariable("g") != nullptr, "semantic-api - global reachable from function");
                expect(ctx.lookupVariableInCurrentScope("g") == nullptr, "semantic-api - current scope excludes global");
                expect(ctx.lookupVariable("missing") == nullptr, "semantic-api - lookup miss");
                expect(ctx.lookupVariableInCurrentScope("missing") == nullptr, "semantic-api - current scope miss");

                auto duplicate = std::make_shared<VarDecl>("p", "int");
                expect(ctx.declareVariable(duplicate) == nullptr, "semantic-api - duplicate local rejected");
                expect(ctx.lookupVariableInCurrentScope("p") == psym, "semantic-api - original local preserved");

                expect(ctx.currentScopeDeclarations().size() == 1, "semantic-api - currentScopeDeclarations");
                expect(ctx.currentFrameLayoutSize() == 1, "semantic-api - currentFrameLayoutSize");
                expect(ctx.currentFrameLayout().frameSize() == 1, "semantic-api - currentFrameLayout");
            }

            // 离开函数作用域后，局部变量消失
            expect(ctx.lookupVariable("p") == nullptr, "semantic-api - local gone after function scope");
            expect(ctx.lookupGlobalVariable("g") != nullptr, "semantic-api - global survives function scope");
            expect(ctx.lookupVariable("g") != nullptr, "semantic-api - global still reachable after pop");
        }

        // ============================================================
        // Nested Scope / Visibility
        // ============================================================
        {
            FCSemanticContext ctx;

            {
                auto guard = ctx.scopedFunction();

                auto outer = std::make_shared<VarDecl>("outer", "int");
                ctx.declareVariable(outer);
                expect(ctx.lookupVariable("outer") != nullptr, "semantic-api - outer variable visible");

                ctx.pushScope();
                auto inner = std::make_shared<VarDecl>("inner", "int");
                auto* isym = ctx.declareVariable(inner);
                expect(ctx.lookupVariableInCurrentScope("inner") == isym, "semantic-api - nested current hit");
                // 重新查找，避免使用跨 push 失效的旧指针
                expect(ctx.lookupVariable("outer") != nullptr, "semantic-api - nested sees parent");
                expect(ctx.lookupVariableInCurrentScope("outer") == nullptr, "semantic-api - parent not in current scope");
                ctx.popScope();

                expect(ctx.lookupVariable("inner") == nullptr, "semantic-api - nested var gone after pop");
                expect(ctx.lookupVariable("outer") != nullptr, "semantic-api - parent survives child scope");
            }
        }

        // ============================================================
        // Shadowing
        // ============================================================
        {
            FCSemanticContext ctx;

            auto guard = ctx.scopedFunction();

            auto outer = std::make_shared<VarDecl>("x", "int");
            ctx.declareVariable(outer);

            ctx.pushScope();
            auto inner = std::make_shared<VarDecl>("x", "int");
            auto* isym = ctx.declareVariable(inner);
            expect(ctx.lookupVariable("x") == isym, "semantic-api - inner shadows outer");
            expect(ctx.lookupVariableInCurrentScope("x") == isym, "semantic-api - shadowed is current");
            ctx.popScope();

            // 重新查找：外层声明恢复可见，且不再是内层符号
            auto* restored = ctx.lookupVariable("x");
            expect(restored != nullptr && restored != isym, "semantic-api - outer restored after pop");
            expect(ctx.lookupVariableInCurrentScope("x") == restored, "semantic-api - outer is current after pop");
        }

        // ============================================================
        // Slot Allocation
        // ============================================================
        {
            FCSemanticContext ctx;

            auto guard = ctx.scopedFunction();

            auto a = std::make_shared<VarDecl>("a", "int");
            auto* asym = ctx.declareVariable(a);
            expect(asym->storage.slot == 0, "semantic-api - first local slot 0");

            ctx.pushScope();
            auto b = std::make_shared<VarDecl>("b", "int");
            auto* bsym = ctx.declareVariable(b);
            expect(bsym->storage.slot == 1, "semantic-api - nested local slot 1");
            ctx.popScope();

            auto c = std::make_shared<VarDecl>("c", "int");
            auto* csym = ctx.declareVariable(c);
            expect(csym->storage.slot == 2, "semantic-api - slot not reused after scope exit");
            expect(ctx.currentFrameLayoutSize() == 3, "semantic-api - frame size includes all slots");
        }

        // ============================================================
        // Scope Guard RAII
        // ============================================================
        {
            FCSemanticContext ctx;

            auto x = std::make_shared<VarDecl>("x", "int");
            ctx.declareVariable(x);

            {
                auto guard = ctx.scopedScope();
                auto y = std::make_shared<VarDecl>("y", "int");
                ctx.declareVariable(y);
                expect(ctx.lookupVariable("y") != nullptr, "semantic-api - scope guard inner visible");
                expect(ctx.lookupVariable("x") != nullptr, "semantic-api - scope guard outer visible");
            }

            expect(ctx.lookupVariable("y") == nullptr, "semantic-api - scope guard inner gone");
            expect(ctx.lookupVariable("x") != nullptr, "semantic-api - scope guard outer persists");
        }

        // ============================================================
        // Global Layout
        // ============================================================
        {
            FCSemanticContext ctx;
            ctx.declareVariable(std::make_shared<VarDecl>("ga", "int"));
            ctx.declareVariable(std::make_shared<VarDecl>("gb", "double"));
            ctx.declareVariable(std::make_shared<VarDecl>("gc", "string"));
            expect(ctx.currentGlobalLayout().globalSize() == 3, "semantic-api - global layout size");
            expect(ctx.currentScopeDeclarations().size() == 3, "semantic-api - current scope declarations");
        }

        // ============================================================
        // Debug
        // ============================================================
        {
            FCSemanticContext ctx;
            ctx.declareVariable(std::make_shared<VarDecl>("dbg", "int"));
            {
                auto guard = ctx.scopedFunction();
                ctx.declareVariable(std::make_shared<VarDecl>("local", "double"));
                ctx.dumpScopes();
            }
            expect(true, "semantic-api - dumpScopes");
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
            {"1.0 < 2.0", "float comparison"},
            // ---- 新增 ----
            {"1 + 2.0", "int plus double mismatch"},
            {"2.0 + 1", "double plus int mismatch"},
            {"\"a\" + 1", "string plus int"},
            {"if undefinedfn() then 1 else 2", "dangle if condition"},
            {"def f() { 1.0 + 2 }; f()", "mixed arithmetic inside function"},
            {"def f() { \"a\" * 2 }; f()", "string multiplication"},
            {"undefinedfn() + 1", "dangle lhs binary"},
            {"def f(n:int) { for i = 0, i < n, undefinedfn() in i }; f(3)", "dangle for step"},
            {"def f() { if 1 then undefinedfn() else 2 }; f()", "dangle then branch"},
            {"def f() { var x:int = 1; x = x + \"s\"; x }; f()", "mixed assignment rhs"},
            {"var g:int = 1; g + \"s\"", "global plus string"}
        };

        for (const auto& [code, name] : errorCases)
        {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            FCEvaluationContext ctx = makeEvalContext(scanner);
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
            FCEvaluationContext ctx({});
            expect(evaluate(static_cast<FCExprAST*>(nullptr), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - evaluate null");

            const FCExprAST* constNull = nullptr;
            expect(evaluate(constNull, ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - const evaluate null");

            std::vector<VarDeclPtr> protoArgs;
            auto proto = std::make_unique<FCPrototypeAST>("p", std::move(protoArgs));
            expect(evaluate(proto.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - evaluate prototype node");

            // 空序列 / 空块
            std::vector<std::unique_ptr<FCExprAST>> emptyItems;
            auto emptySeq = std::make_unique<FCSeqExprAST>(std::move(emptyItems));
            expect(evaluate(emptySeq.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - empty sequence");

            std::vector<std::unique_ptr<FCExprAST>> emptyBlk;
            auto emptyBlock = std::make_unique<FCBlockExprAST>(std::move(emptyBlk));
            expect(evaluate(emptyBlock.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - empty block");

            // for 的 decl 为空
            auto nullFor = std::make_unique<FCForExprAST>(
                nullptr, std::make_unique<FCNumberExprAST>(0),
                std::make_unique<FCNumberExprAST>(10), nullptr,
                std::make_unique<FCNumberExprAST>(1));
            expect(evaluate(nullFor.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - for with null decl");

            // 局部变量但无调用帧
            auto localDecl = std::make_shared<VarDecl>("v", "int");
            VariableSymbol localSym(localDecl, {VariableStorage::Kind::Local, 0}, 1);
            ctx.compiledProgram.allSymbols.addSymbol("v", localSym);
            auto localVar = std::make_unique<FCVariableExprAST>(localDecl);
            localVar->resolved = ctx.compiledProgram.allSymbols.lookup("v");
            expect(evaluate(localVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - local variable without frame");

            // 全局变量但槽位非法
            auto negDecl = std::make_shared<VarDecl>("g", "int");
            VariableSymbol negSym(negDecl, {VariableStorage::Kind::Global, -1}, 0);
            ctx.compiledProgram.allSymbols.addSymbol("g", negSym);
            auto negVar = std::make_unique<FCVariableExprAST>(negDecl);
            negVar->resolved = ctx.compiledProgram.allSymbols.lookup("g");
            expect(evaluate(negVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - global variable with negative slot");

            // 局部变量槽位超出帧大小
            auto oobDecl = std::make_shared<VarDecl>("o", "int");
            VariableSymbol oobSym(oobDecl, {VariableStorage::Kind::Local, 100}, 1);
            ctx.compiledProgram.allSymbols.addSymbol("o", oobSym);
            auto oobVar = std::make_unique<FCVariableExprAST>(oobDecl);
            oobVar->resolved = ctx.compiledProgram.allSymbols.lookup("o");
            ctx.pushFrame("oob");
            expect(evaluate(oobVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - local variable slot out of range");
            ctx.popFrame();

            // 全局槽位会自动扩展
            auto bigDecl = std::make_shared<VarDecl>("big", "int");
            VariableSymbol bigSym(bigDecl, {VariableStorage::Kind::Global, 5}, 0);
            ctx.compiledProgram.allSymbols.addSymbol("big", bigSym);
            auto bigVar = std::make_unique<FCVariableExprAST>(bigDecl);
            bigVar->resolved = ctx.compiledProgram.allSymbols.lookup("big");
            expect(evaluate(bigVar.get(), ctx).type == FCValueCategory::Dangle,
                   "evaluator-error - uninitialized global slot reads dangle");
            expect(ctx.globalFrame.locals.size() == 6, "evaluator-error - global frame auto-resized");
        }

        // 帧 API 直接测试
        {
            FCEvaluationContext ctx({});
            ctx.pushFrame("unknown");
            expect(ctx.currentFrame().funcName == "unknown", "evaluator-error - push frame name");
            expect(ctx.currentFrame().locals.empty(), "evaluator-error - unregistered frame has no locals");
            expect(ctx.callStack.size() == 1, "evaluator-error - call stack size");
            ctx.popFrame();
            expect(ctx.callStack.empty(), "evaluator-error - pop frame empties stack");

            // 注册一个带局部变量的函数后 pushFrame 会分配帧
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("framed", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1), 3);
            ctx.functions.registerFunction(fn.get());
            ctx.pushFrame("framed");
            expect(ctx.currentFrame().locals.size() == 3, "evaluator-error - frame sized by local count");
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
            {"def ack(m:int) { if m < 1 then 1 else (ack(m - 1) + ack(m - 1)) }; ack(12)", "binary tree recursion", "4096"},
            // ---- 新增 ----
            // {"def count(n:int) { if n < 1 then 0 else count(n - 1) + 1 }; count(250)", "deep recursion 250", "250"},
            {"def sumN(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }; sumN(10000)", "large loop 10000", "49995000"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }; fib(20)", "fibonacci 20", "6765"},
            {"def pow2(n:int) { var r:int = 1; for i = 0, i < n, 1 in r = r * 2; r }; pow2(25)", "exponential loop 25", "33554432"},
            {"def ack(m:int) { if m < 1 then 1 else (ack(m - 1) + ack(m - 1)) }; ack(15)", "binary tree recursion 15", "32768"},
            {"var a:int = 2; def f(x:int) { x * 2 }; def g(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + f(i); s }; g(10)", "mixed program with call in loop", "90"}
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

            FCEvaluationContext ctx = makeEvalContext(scanner);
            try
            {
                FCValue result = evaluate(ast.get(), ctx);
                std::string actual = valueToString(result);
                expect(result.type != FCValueCategory::Dangle && actual == expected,
                       "stress - " + name);
            }
            catch (...)
            {
                expect(false, "stress - " + name + " (exception)");
            }
        }

        // 上下文复用：同一求值上下文连续执行两个程序
        {
            FCScanner s1;
            auto ast1 = s1.analysis("def f1(x:int) { x + 1 }; f1(1)");
            FCEvaluationContext ctx = makeEvalContext(s1);
            expect(evaluate(ast1.get(), ctx).type == FCValueCategory::Integer,
                   "stress - context reuse program 1");

            FCScanner s2;
            auto ast2 = s2.analysis("def g1(x:int) { x * 2 }; g1(21)");
            ctx.compiledProgram = s2.semanticContext().getCompiledProgram();
            FCValue r2 = evaluate(ast2.get(), ctx);
            expect(r2.type == FCValueCategory::Integer && r2.evaluteVal.intVal == 42,
                   "stress - context reuse program 2");
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
                FCCodegenContext cc("StressModule", scanner.semanticContext().getCompiledProgram());
                attachSemanticOutput(cc, scanner);
                auto* value = codegen(ast.get(), cc);
                expect(value != nullptr, "stress - codegen multi-function module");
                expect(!llvm::verifyModule(*cc.module, &llvm::errs()),
                       "stress - codegen multi-function module verifies");
            }
            else
            {
                expect(false, "stress - codegen multi-function module (scanner)");
            }
        }

        // 深嵌套代码生成
        {
            std::string code = "def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in { var t:int = i; if t < 10 then s = s + t else s = s - t }; s }";
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (ast)
            {
                FCCodegenContext cc("DeepNestModule", scanner.semanticContext().getCompiledProgram());
                attachSemanticOutput(cc, scanner);
                auto* value = codegen(ast.get(), cc);
                expect(value != nullptr, "stress - codegen deeply nested");
            }
            else
            {
                expect(false, "stress - codegen deeply nested (scanner)");
            }
        }
    }
};

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
