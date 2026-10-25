#include "test_harness.h"

    // ==================== Scanner Tests ====================
    void ComprehensiveTestSuite::runScannerTests()
    {
        std::vector<std::pair<std::string, std::string>> scannerCases = {
            {"1 + 2 * 3", "integer precedence"},
            {"(1 + 2) * 3", "parentheses"},
            {"1.5 + 2.5", "floating literal"},
            {"\"hello\" + \" world\"", "string literal"},
            {"# comment\n1 + 2", "comment handling"},
            {"var g:int = 2; g = g + 3; g", "top-level declaration"},
            {"def add(a:int, b:double) { a }", "function with typed params"},
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
            {"var g:int = 0; def inc() { g = g + 1 } def dec() { g = g - 1 }", "multiple functions with global"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with accumulation"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }", "complex recursive"},
            {"var a:int = 5; (a = a + 1; a * 2)", "assignment in parentheses"},
            {"def id(x:int) { x } id(id(3))", "nested function calls"},
            {"var s:string = \"abc\"; s + \"def\"", "string variable concatenation"},
            {"0 - 1", "negative via subtraction"},
            {"2 * 3 + 4 / 2", "mixed precedence"},
            {"((1 + 2) * (3 + 4))", "nested parentheses"},
            {"3.14159", "standalone float literal"},
            {"\"a\" + \"b\" + \"c\"", "string chain concatenation"},
            {"# comment only\n42", "comment then number"},
            {"var a:int = 1; var b:int = 2; a = b; a", "assignment chain"},
            {"def f() { 1 } def g() { f() + f() } g()", "multiple functions and call"},
            {"def loop(n:int) { for i = 0, i < n in i }", "for without step"},
            {"def dbl(x:double) { x * 2.0 }", "double parameter"},
            {"def str(s:string) { s }", "string parameter"},
            {"if 1 then 2 else 3", "top-level if"},
            {"(1; 2; 3)", "sequence in parentheses"},
            {"def nested() { { 1 } { 2 } 3 }", "nested blocks"},
            {"var g:double = 1.5; def r() { g }", "double global"},
            {"def many(a:int, b:int, c:int, d:int) { a + b + c + d }", "four-argument function"},
            {"def mix(a:int, b:double, c:string) { a }", "mixed parameter types"},
            {"1.0 + 2.0", "float int expression"},
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
            {"1; def f() { 2 } 3", "mixed statements with def in middle"},
            {"\"\"", "empty string literal"},
            {"2.0", "float with trailing digit"},
            {"def f() { var x:int = 1; { var y:int = 2; x + y } }", "nested block with locals"},
            {"def f() { for i = 0, i < 5, 1 in for j = 0, j < 5, 1 in i + j }", "nested for loops"},
            {"def f() { if 1 then if 2 then 3 else 4 else 5 }", "if nested in then"},
            {"var g:int = 1; def bump() { g = g + 1 } bump()", "global function call"},
            {"def f() { (1 + 2) * (3 + 4) - 5 / 2 }", "arithmetic with parens"},
            {"0.5 + 0.25", "fractional literals"},
            {"def f(a:int, b:double, c:string, d:int) { a }", "four mixed params"}
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

        // Facade 阶段错误标记与状态重置
        {
            // 解析错误：hadParseError 为 true
            FCScanner parseErr;
            auto bad = parseErr.analysis("1 +");
            expect(bad == nullptr, "scanner-facade - parse error returns null");
            expect(parseErr.hadParseError(), "scanner-facade - hadParseError flag set");
            expect(!parseErr.hadSemanticError(), "scanner-facade - no semantic error on parse failure");

            // 语义错误：引用未声明变量，hadSemanticError 为 true
            FCScanner semErr;
            auto undeclared = semErr.analysis("def f() { undeclared }");
            expect(undeclared == nullptr, "scanner-facade - semantic error returns null");
            expect(!semErr.hadParseError(), "scanner-facade - no parse error on semantic failure");
            expect(semErr.hadSemanticError(), "scanner-facade - hadSemanticError flag set");

            // resetState 后可复用同一实例继续解析
            semErr.resetState();
            auto recovered = semErr.analysis("1 + 2");
            expect(recovered != nullptr, "scanner-facade - resetState recovers scanner");
            expect(!semErr.hadParseError() && !semErr.hadSemanticError(),
                   "scanner-facade - flags cleared after reset");

            // resetState 同时复位 Parser：此前的解析错误不得跨复位存活
            parseErr.resetState();
            expect(!parseErr.hadParseError(),
                   "scanner-facade - resetState clears parse error flag");
            expect(parseErr.analysis("1 + 2") != nullptr,
                   "scanner-facade - parses after clearing parse error");

            // 分隔符统一规则：顶层裸 ';' 是空语句，非法
            FCScanner bareSemi;
            expect(bareSemi.analysis("; 1") == nullptr,
                   "scanner-facade - top-level bare semicolon rejected");
            expect(bareSemi.hadParseError(),
                   "scanner-facade - bare semicolon flags parse error");

            // 分隔符统一规则：';' 只能跟在非块终结的表达式之后。
            // def 体以 '}' 收尾，其后不得再有 ';'（"def ...} expr" 现为非法）
            FCScanner defTail;
            auto tailAst = defTail.analysis("def f() { 1 }; f()");
            expect(tailAst == nullptr,
                   "scanner-facade - ';' after a definition is rejected");
            expect(defTail.hadParseError(),
                   "scanner-facade - ';' after a definition flags parse error");

            // 但下一条语句可以直接写，无需分隔符
            FCScanner defAdjacent;
            expect(defAdjacent.analysis("def f() { 1 } f()") != nullptr,
                   "scanner-facade - definition followed directly by a statement");

            // 块终结后同一条规则：下一条语句直接开始，不得再用 ';' 连接
            FCScanner blockSeq;
            expect(blockSeq.analysis("var g:int = 0; { g = 1 } { g = 2 } g") != nullptr,
                   "scanner-facade - statements may follow a block without ';'");

            FCScanner blockSemi;
            expect(blockSemi.analysis("var g:int = 0; { g = 1 }; g") == nullptr,
                   "scanner-facade - ';' after a block is rejected");
            expect(blockSemi.hadParseError(),
                   "scanner-facade - ';' after a block flags parse error");

            FCScanner ifBlocks;
            expect(ifBlocks.analysis("def pick(c:int) { if c then { 1 } else { 2 } 3 }") != nullptr,
                   "scanner-facade - if arms end blocks, next statement follows directly");

            // 块内同样不得用 ';' 连接在块之后
            FCScanner innerBlockSemi;
            expect(innerBlockSemi.analysis("def f() { var g:int = 0; { g = 1 }; g }") == nullptr,
                   "scanner-facade - ';' after a block inside a body is rejected");

            // 词法错误在语法阶段之前暴露：返回 null 并标记解析错误
            FCScanner lexErr;
            expect(lexErr.analysis("\"unterminated") == nullptr,
                   "scanner-facade - unterminated string returns null");
            expect(lexErr.hadParseError(),
                   "scanner-facade - unterminated string flags parse error");

            FCScanner charErr;
            expect(charErr.analysis("@") == nullptr,
                   "scanner-facade - unexpected character returns null");
            expect(charErr.hadParseError(),
                   "scanner-facade - unexpected character flags parse error");
        }
    }
    // ==================== Scanner Error Tests ====================
    void ComprehensiveTestSuite::runScannerErrorTests()
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
    void ComprehensiveTestSuite::runSemanticTests()
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
            {"var x:int = 1; def f() { x } def g() { var x:int = 2; f() }", "shadowing check", true},
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
            {"var g1:int = 1; var g2:double = 2.5; def f() { g1 + g2 }", "mixed globals", false},
            {"def cmp(a:int, b:int) { if a < b then a else b }", "comparison expression", true},
            {"def strcat(a:string, b:string) { a + b }", "string concat function", true},
            {"def seq() { (var x:int = 1; x + 1; x + 2) }", "sequence with declaration", true},
            {"def recurse(n:int) { if n < 1 then 0 else recurse(n - 1) + 1 }", "recursion", true},
            {"def many(a:int, b:int, c:int, d:int, e:int) { a + b + c + d + e }", "five parameters", true},
            {"var g:int = 1; var h:double = 2.0; var s:string = \"x\"; g", "three typed globals", true},
            {"var g:int = 1; var h:double = 2.0; var s:string = \"x\"; g + h", "mixed globals top", false},
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
                if (expected)
                {
                    expect(false, "semantic - " + name + " (scanner failed)");
                    continue;
                }
                else
                {
                    expect(true, "semantic - " + name + " (expected error)");
                    continue;
                }
            }

            const auto& semanticCtx = scanner.semanticContext();
            std::string details;

            // 对于预期成功的测试，检查是否能找到相关声明
            if (expected)
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

            bool passed = expected;
            if (!passed)
            {
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
    // ==================== Semantic API Tests ====================
    void ComprehensiveTestSuite::runSemanticApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SEMANTIC API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // ============================================================
        // Operator / Function Registry
        // ============================================================
        {
            FCSemanticContext ctx;

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
            auto sym = ctx.declareVariable(g);

            expect(sym != nullptr, "semantic-api - declare global");
            expect(sym->runtime.frameIndex == -1, "semantic-api - global frameIndex");
            expect(sym->slot == 0, "semantic-api - global slot 0");

            expect(ctx.lookupGlobalVariable("g") == sym, "semantic-api - lookup global hit");
            expect(ctx.lookupVariableInCurrentScope("g") == sym, "semantic-api - global in current scope");
            expect(ctx.lookupGlobalVariable("missing") == nullptr, "semantic-api - lookup global miss");

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
            auto gsym = ctx.declareVariable(g);

            {
                auto guard = ctx.scopedFunction();

                auto p = std::make_shared<VarDecl>("p", "int");
                auto psym = ctx.declareVariable(p);

                expect(psym != nullptr, "semantic-api - declare local");
                expect(psym->runtime.frameIndex != -1, "semantic-api - local frameIndex");
                expect(psym->slot == 0, "semantic-api - local slot 0");

                expect(ctx.lookupVariableShared("p") == psym, "semantic-api - lookup local hit");
                expect(ctx.lookupVariableInCurrentScope("p") == psym, "semantic-api - current scope hit");
                // 注意：作用域栈 push 会导致外层 map 重新分配，
                // 因此在跨作用域比较时重新查找，而不是保存旧的指针。
                expect(ctx.lookupVariableShared("g") == ctx.lookupGlobalVariable("g"),
                    "semantic-api - lookup falls back to global");
                expect(ctx.lookupVariableShared("g") != nullptr, "semantic-api - global reachable from function");
                expect(ctx.lookupVariableInCurrentScope("g") == nullptr, "semantic-api - current scope excludes global");
                expect(ctx.lookupVariableShared("missing") == nullptr, "semantic-api - lookup miss");
                expect(ctx.lookupVariableInCurrentScope("missing") == nullptr, "semantic-api - current scope miss");

                auto duplicate = std::make_shared<VarDecl>("p", "int");
                expect(ctx.declareVariable(duplicate) == nullptr, "semantic-api - duplicate local rejected");
                expect(ctx.lookupVariableInCurrentScope("p") == psym, "semantic-api - original local preserved");

                expect(ctx.currentScopeDeclarations().size() == 1, "semantic-api - currentScopeDeclarations");
                expect(ctx.currentFrameLayoutSize() == 1, "semantic-api - currentFrameLayoutSize");
            }

            // 离开函数作用域后，局部变量消失
            expect(ctx.lookupVariableShared("p") == nullptr, "semantic-api - local gone after function scope");
            expect(ctx.lookupGlobalVariable("g") != nullptr, "semantic-api - global survives function scope");
            expect(ctx.lookupVariableShared("g") != nullptr, "semantic-api - global still reachable after pop");
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
                expect(ctx.lookupVariableShared("outer") != nullptr, "semantic-api - outer variable visible");

                ctx.pushScope();
                auto inner = std::make_shared<VarDecl>("inner", "int");
                auto isym = ctx.declareVariable(inner);
                expect(ctx.lookupVariableInCurrentScope("inner") == isym, "semantic-api - nested current hit");
                // 重新查找，避免使用跨 push 失效的旧指针
                expect(ctx.lookupVariableShared("outer") != nullptr, "semantic-api - nested sees parent");
                expect(ctx.lookupVariableInCurrentScope("outer") == nullptr, "semantic-api - parent not in current scope");
                ctx.popScope();

                expect(ctx.lookupVariableShared("inner") == nullptr, "semantic-api - nested var gone after pop");
                expect(ctx.lookupVariableShared("outer") != nullptr, "semantic-api - parent survives child scope");
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
            auto isym = ctx.declareVariable(inner);
            expect(ctx.lookupVariableShared("x") == isym, "semantic-api - inner shadows outer");
            expect(ctx.lookupVariableInCurrentScope("x") == isym, "semantic-api - shadowed is current");
            ctx.popScope();

            // 重新查找：外层声明恢复可见，且不再是内层符号
            auto restored = ctx.lookupVariableShared("x");
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
            auto asym = ctx.declareVariable(a);
            expect(asym->slot == 0, "semantic-api - first local slot 0");

            ctx.pushScope();
            auto b = std::make_shared<VarDecl>("b", "int");
            auto bsym = ctx.declareVariable(b);
            expect(bsym->slot == 1, "semantic-api - nested local slot 1");
            ctx.popScope();

            auto c = std::make_shared<VarDecl>("c", "int");
            auto csym = ctx.declareVariable(c);
            expect(csym->slot == 2, "semantic-api - slot not reused after scope exit");
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
                expect(ctx.lookupVariableShared("y") != nullptr, "semantic-api - scope guard inner visible");
                expect(ctx.lookupVariableShared("x") != nullptr, "semantic-api - scope guard outer visible");
            }

            expect(ctx.lookupVariableShared("y") == nullptr, "semantic-api - scope guard inner gone");
            expect(ctx.lookupVariableShared("x") != nullptr, "semantic-api - scope guard outer persists");
        }

        // ============================================================
        // Global Layout
        // ============================================================
        {
            FCSemanticContext ctx;
            ctx.declareVariable(std::make_shared<VarDecl>("ga", "int"));
            ctx.declareVariable(std::make_shared<VarDecl>("gb", "double"));
            ctx.declareVariable(std::make_shared<VarDecl>("gc", "string"));
            expect(ctx.currentGlobalLayoutSize() == 3, "semantic-api - global layout size");
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
    // ==================== Semantic Analyzer Tests ====================
    void ComprehensiveTestSuite::runSemanticAnalyzerTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SEMANTIC ANALYZER TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // ============================================================
        // Global variable declared at top level
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto gDecl = std::make_shared<VarDecl>("g", "int");
            auto gVar = std::make_unique<FCVariableExprAST>(std::string("g"));
            // 先声明再使用：全局声明
            auto gvd = std::make_unique<FCVarDeclExprAST>(gDecl, nullptr);
            expect(analyzer.analyze(gvd.get()), "semantic-analyzer - declare global g");

            // 现在变量应被绑定（使用节点再走一遍分析，触发 bindVariable）
            expect(analyzer.analyze(gVar.get()), "semantic-analyzer - use global g");

            auto bound = ctx.semanticBinding().find(gVar.get());
            expect(bound != nullptr, "semantic-analyzer - global g bound");
            expect(bound->runtime.frameIndex == -1, "semantic-analyzer - global g frameIndex -1");
            expect(bound->slot == 0, "semantic-analyzer - global g slot 0");
        }

        // ============================================================
        // Top-level block: var inside { } at top level must be global
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto blk = std::make_unique<FCBlockExprAST>(
                [] {
                    std::vector<std::unique_ptr<FCExprAST>> v;
                    v.emplace_back(std::make_unique<FCVarDeclExprAST>(
                        std::make_shared<VarDecl>("inner", "int"), nullptr));
                    return v;
                }());
            expect(analyzer.analyze(blk.get()), "semantic-analyzer - top-level block inner");

            auto sym = ctx.lookupGlobalVariable("inner");
            expect(sym != nullptr, "semantic-analyzer - block var is global");
            expect(sym->runtime.frameIndex == -1, "semantic-analyzer - block var frameIndex -1");
        }

        // ============================================================
        // Local variable inside function scope
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("f", std::vector<VarDeclPtr>{}),
                std::make_unique<FCBlockExprAST>(
                    [] {
                        std::vector<std::unique_ptr<FCExprAST>> v;
                        v.emplace_back(std::make_unique<FCVarDeclExprAST>(
                            std::make_shared<VarDecl>("local", "int"), nullptr));
                        return v;
                    }()));
            expect(analyzer.analyze(fn.get()), "semantic-analyzer - function local");

            // 函数分析完成后，局部变量记录在 CompiledFunction::symbols 中，
            // 而非 FCSemanticContext 的 scope stack（已 pop）
            auto compiledFn = ctx.getCompiledFunction("f");
            expect(compiledFn != nullptr, "semantic-analyzer - compiled function f");
            auto sym = compiledFn->symbols.lookupShared("local");
            expect(sym != nullptr, "semantic-analyzer - local visible");
            expect(sym->runtime.frameIndex != -1, "semantic-analyzer - local frameIndex not -1");

            // 函数作用域结束后，局部不可见于全局查找
            expect(ctx.lookupVariableShared("local") == nullptr, "semantic-analyzer - local gone after function");
        }

        // ============================================================
        // Shadowing: inner x shadows outer x, they are distinct symbols
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto outer = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("outer", std::vector<VarDeclPtr>{}),
                std::make_unique<FCBlockExprAST>(
                    [] {
                        std::vector<std::unique_ptr<FCExprAST>> v;
                        v.emplace_back(std::make_unique<FCVarDeclExprAST>(
                            std::make_shared<VarDecl>("x", "int"), nullptr));
                        v.emplace_back(std::make_unique<FCBlockExprAST>(
                            [] {
                                std::vector<std::unique_ptr<FCExprAST>> inner;
                                inner.emplace_back(std::make_unique<FCVarDeclExprAST>(
                                    std::make_shared<VarDecl>("x", "int"), nullptr));
                                return inner;
                            }()));
                        return v;
                    }()));
            expect(analyzer.analyze(outer.get()), "semantic-analyzer - shadowing");

            auto globX = ctx.lookupGlobalVariable("x");
            expect(globX == nullptr, "semantic-analyzer - x not global");

            // 函数符号表中 x 应为外层符号（pop 后可见）
            auto fn = ctx.getCompiledFunction("outer");
            expect(fn != nullptr, "semantic-analyzer - compiled function outer");
            auto restored = fn->symbols.lookupShared("x");
            expect(restored != nullptr, "semantic-analyzer - x in function symbol table");
        }

        // ============================================================
        // Undeclared variable use is rejected
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto bad = std::make_unique<FCVariableExprAST>(std::string("undeclared"));
            expect(!analyzer.analyze(bad.get()), "semantic-analyzer - undeclared variable rejected");
        }

        // ============================================================
        // Variable binding: analyzer binds AST node to its symbol
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto gDecl = std::make_shared<VarDecl>("gb", "int");
            auto gvd = std::make_unique<FCVarDeclExprAST>(gDecl, nullptr);
            expect(analyzer.analyze(gvd.get()), "semantic-analyzer - declare gb");

            auto use = std::make_unique<FCVariableExprAST>(std::string("gb"));
            expect(analyzer.analyze(use.get()), "semantic-analyzer - use gb");

            auto bound = ctx.semanticBinding().find(use.get());
            expect(bound != nullptr, "semantic-analyzer - use node bound");
            expect(bound == ctx.lookupGlobalVariable("gb"), "semantic-analyzer - bound symbol is global gb");
        }

        // ============================================================
        // For loop variable declared in function scope is local
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto fLoop = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("loopfn", std::vector<VarDeclPtr>{}),
                std::make_unique<FCForExprAST>(
                    std::make_shared<VarDecl>("i", "int"),
                    std::make_unique<FCNumberExprAST>(0),
                    std::make_unique<FCNumberExprAST>(10),
                    std::make_unique<FCNumberExprAST>(1),
                    std::make_unique<FCVariableExprAST>(std::string("i"))));
            expect(analyzer.analyze(fLoop.get()), "semantic-analyzer - for loop");

            auto compiledLoop = ctx.getCompiledFunction("loopfn");
            expect(compiledLoop != nullptr, "semantic-analyzer - compiled function loopfn");
            auto sym = compiledLoop->symbols.lookupShared("i");
            expect(sym != nullptr, "semantic-analyzer - for var i visible");
            expect(sym->runtime.frameIndex != -1, "semantic-analyzer - for var i is local");
        }

        // ============================================================
        // If expression: variables in branches analyzed
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

            auto br = std::make_unique<FCIfExprAST>(
                std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCVarDeclExprAST>(
                    std::make_shared<VarDecl>("branchVar", "int"), nullptr),
                nullptr);
            expect(analyzer.analyze(br.get()), "semantic-analyzer - if branch var");

            expect(ctx.lookupGlobalVariable("branchVar") != nullptr,
                   "semantic-analyzer - branch var is global (top level)");
        }

        // ============================================================
        // 嵌套函数定义：能否表示由编译开关决定
        // 关闭态：语义期拒绝，且拒绝不留下帧副作用
        // 打开态：语义期允许并登记，但仅建模，不可执行
        // ============================================================
        {
            auto buildNested = [](bool captureOuter) {
                std::vector<std::unique_ptr<FCExprAST>> body;
                body.emplace_back(std::make_unique<FCVarDeclExprAST>(
                    std::make_shared<VarDecl>("outerVar", "int"), nullptr));
                body.emplace_back(std::make_unique<FCFunctionAST>(
                    std::make_unique<FCPrototypeAST>("inner", std::vector<VarDeclPtr>{}),
                    captureOuter
                        ? std::unique_ptr<FCExprAST>(
                              std::make_unique<FCVariableExprAST>(std::string("outerVar")))
                        : std::unique_ptr<FCExprAST>(std::make_unique<FCNumberExprAST>(1))));
                return std::make_unique<FCFunctionAST>(
                    std::make_unique<FCPrototypeAST>("outer", std::vector<VarDeclPtr>{}),
                    std::make_unique<FCBlockExprAST>(std::move(body)));
            };

            if constexpr (FCMarks::kNestedFunctionDefinitions)
            {
                FCSemanticContext ctx;
                FC::FCDiagnostics diagnostics;
                FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

                auto outerFn = buildNested(false);
                expect(analyzer.analyze(outerFn.get()),
                       "semantic-analyzer - nested definition allowed when enabled");

                auto *outerRecord = ctx.getCompiledFunction("outer");
                auto *innerRecord = ctx.getCompiledFunction("inner");
                expect(outerRecord != nullptr, "semantic-analyzer - outer function registered");
                expect(innerRecord != nullptr, "semantic-analyzer - nested function registered");
                expect(innerRecord != outerRecord,
                       "semantic-analyzer - nested function has distinct record");

                // 内层帧独立：外层帧只含 outerVar
                expect(outerRecord->frameSize == 1,
                       "semantic-analyzer - nested frame does not inflate outer frame");

                // 捕获外层局部：名字解析照常放行（作用域栈连续），
                // 内层记录独立登记，执行期由求值器与 codegen 拒绝
                {
                    FCSemanticContext captureCtx;
                    FC::FCDiagnostics diagnostics;
                    FCSemanticContext::SemanticAnalyzer captureAnalyzer(captureCtx, diagnostics);
                    auto captureFn = buildNested(true);
                    expect(captureAnalyzer.analyze(captureFn.get()),
                           "semantic-analyzer - nested capture resolved when enabled");
                    expect(captureCtx.getCompiledFunction("inner") != nullptr,
                           "semantic-analyzer - captured nested definition registered");
                }

                // 同帧跨块仍可见：帧归属判定只在跨帧时生效
                {
                    FCSemanticContext sameCtx;
                    FC::FCDiagnostics diagnostics;
                    FCSemanticContext::SemanticAnalyzer sameAnalyzer(sameCtx, diagnostics);
                    std::vector<std::unique_ptr<FCExprAST>> outerBlock;
                    outerBlock.emplace_back(std::make_unique<FCVarDeclExprAST>(
                        std::make_shared<VarDecl>("blockVar", "int"), nullptr));
                    std::vector<std::unique_ptr<FCExprAST>> innerBlock;
                    innerBlock.emplace_back(
                        std::make_unique<FCVariableExprAST>(std::string("blockVar")));
                    outerBlock.emplace_back(
                        std::make_unique<FCBlockExprAST>(std::move(innerBlock)));
                    auto sameFn = std::make_unique<FCFunctionAST>(
                        std::make_unique<FCPrototypeAST>("sameFrameBlock",
                                                         std::vector<VarDeclPtr>{}),
                        std::make_unique<FCBlockExprAST>(std::move(outerBlock)));
                    expect(sameAnalyzer.analyze(sameFn.get()),
                           "semantic-analyzer - same-frame block visibility kept");
                }
            }
            else
            {
                FCSemanticContext ctx;
                FC::FCDiagnostics diagnostics;
                FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);

                auto outerFn = buildNested(false);
                expect(!analyzer.analyze(outerFn.get()),
                       "semantic-analyzer - nested definition rejected when disabled");
                expect(ctx.getCompiledFunction("inner") == nullptr,
                       "semantic-analyzer - rejected nested definition leaves no record");
                // 外层函数体分析同样失败：其记录一并撤销，函数表里不留半成品
                expect(ctx.getCompiledFunction("outer") == nullptr,
                       "semantic-analyzer - failed enclosing function leaves no record");

                // 守卫在压帧之前生效：拒绝后帧栈已回退，后续顶层定义仍可分析
                auto afterFn = std::make_unique<FCFunctionAST>(
                    std::make_unique<FCPrototypeAST>("afterNested", std::vector<VarDeclPtr>{}),
                    std::make_unique<FCNumberExprAST>(2));
                expect(analyzer.analyze(afterFn.get()),
                       "semantic-analyzer - frames restored after nested rejection");
            }
        }

        // ============================================================
        // 边界分支：null 根、同名参数重定义在参数声明期被拒
        // ============================================================
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);
            expect(!analyzer.analyze(nullptr), "semantic-analyzer - null root rejected");

            std::vector<VarDeclPtr> dupArgs;
            dupArgs.push_back(std::make_shared<VarDecl>("a", "int"));
            dupArgs.push_back(std::make_shared<VarDecl>("a", "int"));
            auto dupFn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("dup", std::move(dupArgs)),
                std::make_unique<FCNumberExprAST>(1));
            expect(!analyzer.analyze(dupFn.get()),
                   "semantic-analyzer - duplicate parameter rejected at declaration");

            // bindVariable 的防御分支：null 节点或 null 符号一律拒绝
            auto useNode = std::make_unique<FCVariableExprAST>(std::string("gb"));
            expect(!ctx.bindVariable(nullptr, nullptr),
                   "semantic-analyzer - bindVariable rejects null node and symbol");
            expect(!ctx.bindVariable(useNode.get(), nullptr),
                   "semantic-analyzer - bindVariable rejects null symbol");
        }
    }
    // ==================== Semantic Type Error Tests ====================
    void ComprehensiveTestSuite::runSemanticTypeErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SEMANTIC TYPE ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::string> errorCases = {
            "var x:int = \"s\"",
            "var x:int = 1.5",
            "var x:double = 1",
            "def f() { 1 + 2.0 }",
            "def f() { 1.0 + 2 }",
            "def f() { 1 = 2 }",
            "def f() { \"a\" - \"b\" }",
            "def f() { \"a\" * 2 }",
            "def f(a:string,b:string) { a < b }",
            "def f() { if \"a\" then 1 else 2 }",
            "def f() { if 1 then 1 else 2.0 }",
            "def f() { for i = 0.0, i < 3, 1 in i }",
            "def f() { for i = 0, \"a\", 1 in i }",
            "def f() { for i = 0, i < 3, \"s\" in i }",
            "var x:int = { }",
            "var g1:int = 1; var g2:double = 2.5; def f() { g1 + g2 }",
            "var g:int = 1; var h:double = 2.0; var s:string = \"x\"; g + h",
            "def f(a:double) { a } def g() { f(1) }",
            "def mixed(a:int, b:double) { a + b }",
            "var gd:double = 1.5; var gi:int = 2; def mix2() { gd + gi }",
            "def f(a:double) { a } def g() { f(1) }",
        };

        for (const auto& code : errorCases)
        {
            std::cout << "  Testing error: " << code << "\n";
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            bool isErr = (ast == nullptr);
            bool stageOk = false;
            if (isErr && !scanner.diagnostics().empty()) {
                stageOk = scanner.diagnostics()[0].stage == "semantic";
            }
            expect(isErr && stageOk, "semantic-type-error - " + code);
        }

        std::vector<std::string> successCases = {
            "def concat(a:string,b:string) { a + b }",
            "def f(n:double) { if n < 2.0 then n else n }",
            "if 1.5 then 1 else 2",
        };
        for (const auto& code : successCases)
        {
            std::cout << "  Testing success: " << code << "\n";
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            expect(ast != nullptr, "semantic-type-success - " + code);
        }
    }
    void ComprehensiveTestSuite::runSymbolTableTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "SYMBOL TABLE TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        SymbolTable table;
        auto d1 = std::make_shared<VarDecl>("alpha", "int");
        VariableSymbol s1(d1, 0, 0);

        expect(table.addSymbol("alpha", std::make_shared<VariableSymbol>(s1)), "symbol-table - add new symbol");
        expect(!table.addSymbol("alpha", std::make_shared<VariableSymbol>(s1)), "symbol-table - duplicate add rejected");
        expect(table.size() == 1, "symbol-table - size after add");

        const SymbolTable& ctable = table;
        expect(ctable.lookupShared("alpha") != nullptr, "symbol-table - const lookup hit");
        expect(ctable.lookupShared("alpha")->declaration == d1, "symbol-table - const lookup declaration");
        expect(ctable.lookupShared("alpha")->slot == 0, "symbol-table - const lookup slot");
        expect(ctable.lookupShared("missing") == nullptr, "symbol-table - const lookup miss");

        expect(table.lookupShared("alpha") != nullptr, "symbol-table - mutable lookup hit");
        expect(table.lookupShared("missing") == nullptr, "symbol-table - mutable lookup miss");

        expect(table.getAllSymbols().size() == 1, "symbol-table - getAllSymbols size");
        expect(table.getAllSymbols().count("alpha") == 1, "symbol-table - getAllSymbols content");

        expect(table.removeSymbol("alpha"), "symbol-table - remove hit");
        expect(!table.removeSymbol("alpha"), "symbol-table - remove miss");
        expect(table.size() == 0, "symbol-table - empty after remove");

        auto d2 = std::make_shared<VarDecl>("beta", "double");
        VariableSymbol s2(d2, 3, 1);
        expect(table.addSymbol("beta", std::make_shared<VariableSymbol>(s2)), "symbol-table - add second symbol");
        table.dump();
        expect(table.size() == 1, "symbol-table - size after second add");

        table.clear();
        expect(table.size() == 0, "symbol-table - clear");
        expect(table.lookupShared("beta") == nullptr, "symbol-table - lookup after clear");

        // 通过 FCSemanticContext 间接驱动 SymbolTable（persistent 表）
        {
            FCSemanticContext ctx;
            ctx.declareVariable(std::make_shared<VarDecl>("p1", "int"));
            ctx.declareVariable(std::make_shared<VarDecl>("p2", "double"));
            // persistent 表与 compiledProgram 无关，这里只验证编译输出
            const auto& prog = ctx.getCompiledProgram();
            expect(prog.globalSymbols.size() == 2, "symbol-table - semantic globals recorded");
        }
    }
    // ==================== Compiled Program Tests ====================
    // 验证语义输出契约：CompiledProgram 记录全局变量、函数参数、局部变量的存储布局。
    void ComprehensiveTestSuite::runCompiledProgramTests()
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
            expect(prog.globalSymbols.size() + symbolCnt == 4, "compiled - all symbols recorded");

            auto lookupVar = [&](const std::string& varName)
            {
                std::shared_ptr<VariableSymbol> sym = nullptr;
                for (const auto& gs : prog.globalSymbols)
                {
                    if (gs->declaration && gs->declaration->name == varName)
                    {
                        sym = gs;
                        break;
                    }
                }
                if (!sym)
                {
                    for (auto& func : prog.functions)
                    {
                        sym = func.second->symbols.lookupShared(varName);
                        if (sym) break;
                    }
                }

                return sym;
            };

            auto gsym = lookupVar("g");
            expect(gsym != nullptr && gsym->runtime.frameIndex == -1 &&
                   gsym->slot == 0, "compiled - global symbol layout");

            auto asym = lookupVar("a");
            expect(asym != nullptr && asym->runtime.frameIndex != -1 &&
                   asym->slot == 0, "compiled - param a layout");

            auto bsym = lookupVar("b");
            expect(bsym != nullptr && bsym->runtime.frameIndex != -1 &&
                   bsym->slot == 1, "compiled - param b layout");

            auto tsym = lookupVar("t");
            expect(tsym != nullptr && tsym->runtime.frameIndex != -1 &&
                   tsym->slot == 2, "compiled - local t layout");

            const auto* add = prog.getFunction("add");
            expect(add != nullptr, "compiled - getFunction add");
            expect(add->frameSize == 3, "compiled - frame size includes params and locals");
            expect(add->symbols.lookupShared("a") != nullptr && add->symbols.lookupShared("b") != nullptr &&
                   add->symbols.lookupShared("t") != nullptr, "compiled - function symbol table");
            expect(add->symbols.lookupShared("g") == nullptr, "compiled - globals not in function symbols");

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
            expect(prog.globalSymbols.size() == 2, "compiled - only globals recorded");
            expect(prog.globalSymbols[0]->slot == 0, "compiled - global a slot");
            expect(prog.globalSymbols[1]->slot == 1, "compiled - global b slot");
            expect(sem.currentGlobalLayoutSize() == 2, "compiled - global layout size");
            // <global> 帧记录常驻 functions 表；无用户函数时表长恒为 1
            expect(prog.functions.size() == 1 && prog.getFunction("<global>") != nullptr,
                   "compiled - no functions");
        }

        // 顶层块中的 var 必须注册为全局（README:291）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("{ var g:int = 42; g }");
            expect(ast != nullptr, "compiled - top-level block scan ok");
            const auto& sem = scanner.semanticContext();
            const auto& prog = sem.getCompiledProgram();
            expect(prog.globalSymbols.size() == 1, "compiled - top-level block global count");
            expect(prog.globalSymbols[0]->runtime.frameIndex == -1,
                   "compiled - top-level block var is global");
            expect(prog.globalSymbols[0]->slot == 0, "compiled - top-level block global slot");
        }

        // 顶层括号序列内的 var 同样注册为全局（README:291，顶层任意位置）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("if 1 then (var g:int = 2; g) else 3; def f() { g }");
            expect(ast != nullptr, "compiled - top-level paren-seq var scan ok");
            const auto& sem = scanner.semanticContext();
            const auto& prog = sem.getCompiledProgram();
            expect(prog.globalSymbols.size() == 1, "compiled - top-level paren-seq global count");
            expect(prog.globalSymbols[0]->runtime.frameIndex == -1,
                   "compiled - top-level paren-seq var is global");
            expect(prog.getFunction("f") != nullptr, "compiled - function referencing global g compiled");
        }

        // 无局部变量的函数
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { 1 }");
            const auto& prog = scanner.semanticContext().getCompiledProgram();
            const auto* f = prog.getFunction("f");
            expect(f != nullptr && f->frameSize == 0,
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
                std::make_unique<FCNumberExprAST>(1));
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
            expect(ctx.currentGlobalLayoutSize() == 0, "compiled - reset clears global layout");
            expect(ctx.currentFrameLayoutSize() == 0, "compiled - reset clears frame layout");
        }
    }
    // ==================== Semantic Dump Tests ====================
    void ComprehensiveTestSuite::runSemanticDumpTests()
    {
        auto dumpOf = [](const std::string &code) -> std::string {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
                return "<error>";
            return FCMarks::describeCompiledProgram(
                scanner.semanticContext().getCompiledProgram());
        };

        std::string globalOnly = dumpOf("1");
        expect(globalOnly.find("frame <global> (global) frameSize=0") != std::string::npos,
               "semantic-dump - empty global frame");
        expect(globalOnly.find("bindings=0") != std::string::npos,
               "semantic-dump - no bindings for a literal");

        std::string globalVar = dumpOf("var a:int = 1; a");
        expect(globalVar.find("a:int slot=0 depth=0 frameIndex=-1") != std::string::npos,
               "semantic-dump - global variable keeps slot and frame index");
        expect(globalVar.find("bindings=2") != std::string::npos,
               "semantic-dump - declaration and reference both bound");

        std::string add = dumpOf("def add(a:int, b:int) { a + b }");
        expect(add.find("frame <global> (global)") != std::string::npos,
               "semantic-dump - global frame printed first");
        expect(add.find("frame add (function) frameSize=2 params=[0, 1]") !=
                   std::string::npos,
               "semantic-dump - parameter slots follow prototype order");
        expect(add.find("a:int slot=0 depth=0 frameIndex=0") != std::string::npos,
               "semantic-dump - parameter is frame-local");
        expect(add.find("b:int slot=1 depth=0 frameIndex=0") != std::string::npos,
               "semantic-dump - second parameter takes next slot");

        std::string locals = dumpOf("def f(n:int) { var x:int = n; x }");
        expect(locals.find("var") == std::string::npos &&
                   locals.find("x:int slot=1") != std::string::npos,
               "semantic-dump - local takes a slot after parameters");

        std::string shadowed = dumpOf("def g() { var x:int = 1; { var x:int = 2; x } x }");
        expect(shadowed.find("frame g (function) frameSize=2") != std::string::npos,
               "semantic-dump - nested block shadowing takes a new frame slot");
        expect(shadowed.find("bindings=4") != std::string::npos,
               "semantic-dump - references bind to their own shadowed symbol");
        expect(dumpOf("1 +") == "<error>",
               "semantic-dump - parse failure yields no program");

        FC::setTraceStagesForTesting("semantic");
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def h(a:int) { a }");
            expect(ast != nullptr, "semantic-dump - FC_TRACE=semantic keeps analysis working");
        }
        FC::setTraceStagesForTesting("");
    }

    // ==================== Type Mapping Tests (VarDecl::type 派生) ====================
    void ComprehensiveTestSuite::runTypeMappingTests()
    {
        expect(VarDecl("a", "int").type == FCTypeId::Int, "type-map - int");
        expect(VarDecl("a", "double").type == FCTypeId::Double, "type-map - double");
        expect(VarDecl("a", "string").type == FCTypeId::String, "type-map - string");
        expect(VarDecl("a", "bogus").type == FCTypeId::Error, "type-map - unknown maps to Error");
    }

    // ==================== Return Type Tests (Phase 2 不动点产物) ====================
    void ComprehensiveTestSuite::runReturnTypeTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "RETURN TYPE TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        struct ReturnTypeCase
        {
            std::string code;
            std::string functionName;
            FCTypeId expected;
        };

        std::vector<ReturnTypeCase> cases = {
            {"def f() { 1 }", "f", FCTypeId::Int},
            {"def f() { 1.5 }", "f", FCTypeId::Double},
            {"def f() { \"s\" }", "f", FCTypeId::String},
            {"def f() { }", "f", FCTypeId::Error},                       // 空函数体契约：analysis 成功 + Error
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "fib", FCTypeId::Int},
            {"def a() { b() } def b() { 2.0 }", "a", FCTypeId::Double},   // 前向引用
            {"def d(n:double) { if n < 2.0 then n else d(n - 1.0) }", "d", FCTypeId::Double},  // 不动点必要性
        };

        for (const auto& testCase : cases)
        {
            FCScanner scanner;
            auto ast = scanner.analysis(testCase.code);
            expect(ast != nullptr, "return-type - analysis ok: " + testCase.code);

            const auto* record =
                scanner.semanticContext().getCompiledFunction(testCase.functionName);
            expect(record != nullptr, "return-type - record exists: " + testCase.functionName);
            expect(record->ast != nullptr, "return-type - ast linked: " + testCase.functionName);
            expect(record->returnType == testCase.expected,
                   "return-type - " + testCase.functionName + " has expected type");
        }

        // 空函数体契约：analysis 不失败（ast 非空）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { }");
            expect(ast != nullptr, "return-type - empty body analysis succeeds");
        }
    }
