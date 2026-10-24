#include "test_harness.h"

    // ==================== Evaluator Tests ====================
    void ComprehensiveTestSuite::runEvaluatorTests()
    {
        std::vector<std::tuple<std::string, std::string, std::string>> evaluatorCases = {
            {"var x:int = 5; x + 3", "variable declaration", "8"},
            {"def square(x:int) { x * x } square(4)", "function call", "16"},
            {"def add(a:int, b:int) { a + b } add(3, 7)", "two-argument function", "10"},
            {"if 1 then 10 else 20", "if true branch", "10"},
            {"if 0 then 10 else 20", "if false branch", "20"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) } fib(5)", "fibonacci", "5"},
            {"var x:int = 1; x = x + 1; x", "assignment", "2"},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) } power(2, 4)", "power function", "16"},
            {"var counter:int = 0; def inc() { counter = counter + 1 } inc(); inc(); counter", "global counter", "2"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) } fact(6)", "factorial", "720"},
            {"def max(a:int, b:int) { if a < b then b else a } max(10, max(5, 20))", "nested call", "20"},
            {"var g:int = 1; def f() { g = g + 1 } f(); f(); g", "global mutation", "3"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } sum(10)", "loop sum", "45"},
            {"def concat(a:string, b:string) { a + b } concat(\"ab\", \"cd\")", "string function", "\"abcd\""},
            {"var x:int = 1; (x = x + 2; x * 3)", "assignment in seq", "9"},
            {"def id(x:int) { x } id(id(5))", "nested id", "5"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) } countdown(5)", "alternating sum", "3"},
            {"var s:string = \"a\"; s = s + \"b\"; s + \"c\"", "string variable", "\"abc\""},
            {"def loopReturn(n:int) { for i = 0, i < n, 1 in i } loopReturn(3)", "loop return value", "2"},
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
            {"def sumDefault(n:int) { var s:int = 0; for i = 0, i < n in s = s + i; s } sumDefault(5)", "for default step", "10"},
            {"def dbl(x:double) { x * 2.0 } dbl(3.5)", "double function", "7.0"},
            {"def bang(s:string) { s + \"!\" } bang(\"hi\")", "string append", "\"hi!\""},
            {"var a:string = \"x\"; var b:string = \"y\"; a + b", "string globals", "\"xy\""},
            {"def sub(a:int, b:int) { a - b } sub(10, 4)", "function subtraction", "6"},
            {"def div(a:int, b:int) { a / b } div(20, 4)", "function division", "5"},
            {"def fsub(a:double, b:double) { a - b } fsub(5.5, 2.0)", "float function subtraction", "3.5"},
            {"def fdiv(a:double, b:double) { a / b } fdiv(7.5, 2.5)", "float function division", "3.0"},
            {"var x:int = 10; def f() { var x:int = 2; x } f()", "local shadows global", "2"},
            // 块级遮蔽：同函数内层块重新声明同名变量，内层声明必须覆盖外层符号
            {"def f() { var x:int = 1; { var x:int = 2; x } } f()", "block shadowing inner wins", "2"},
            {"def f() { var x:int = 1; { var x:int = 2; } x } f()", "block shadowing outer restored", "1"},
            {"def nested2() { var x:int = 1; var y:int = 2; x + y } nested2()", "multiple locals", "3"},
            {"def nf(n:int) { var s:int = 0; for i = 0, i < n, 1 in for j = 0, j < n, 1 in s = s + i; s } nf(3)", "nested for loop", "9"},
            {"def accum(n:int) { var r:int = 1; for i = 1, i < n, 1 in r = r * 2; r } accum(5)", "loop accumulation multiply", "16"},
            {"def choose(n:int) { if n < 3 then 1 else (choose(n - 1) + choose(n - 2)) } choose(6)", "tree recursion", "8"},
            // ---- 新增 ----
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) } def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) } isEven(10)", "mutual recursion even", "1"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) } def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) } isOdd(6)", "mutual recursion odd false", "0"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) } def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) } isOdd(7)", "mutual recursion odd true", "1"},
            {"def sumdown(n:int) { var s:int = 0; for i = n, 0 < i, 0 - 1 in s = s + i; s } sumdown(5)", "negative step loop", "15"},
            {"def f() { 1 } def g() { 2 } f() + g()", "multiple functions combined", "3"},
            {"var g:int = 0; def bump() { g = g + 2 } bump(); bump(); g", "global accumulation", "4"},
            {"def dec(n:int) { n - 1 } dec(dec(dec(5)))", "chained calls", "2"},
            {"def add3(a:int, b:int, c:int) { a + b + c } add3(1, 2, 3)", "three-argument function", "6"},
            {"var s:string = \"ab\"; def app(x:string) { s = s + x; s } app(\"c\")", "string param mutation", "\"abc\""},
            {"def dbl2(x:double) { x * 2.0 } dbl2(dbl2(2.0))", "double chained", "8.0"},
            {"def pick(a:int) { if a then 1 else 0 } pick(0) + pick(5)", "if as value", "1"},
            {"var a:int = 1; var b:int = 2; def swap() { var t:int = a; a = b; b = t } swap(); a * 10 + b", "swap via temp", "21"},
            {"def fib10(n:int) { if n < 2 then n else fib10(n - 1) + fib10(n - 2) } fib10(10)", "fibonacci 10", "55"},
            {"def loop1(n:int) { for i = 0, i < n, 1 in i } loop1(1)", "single iteration loop", "0"},
            {"def loop0(n:int) { for i = 0, i < n, 1 in i } loop0(0)", "zero iteration loop", "0"},
            {"\"a\" + \"b\" + \"c\"", "string chain concat top-level", "\"abc\""},
            {"def mul2(a:int, b:int) { a * b } def twice(x:int) { mul2(x, 2) } twice(21)", "call through helper", "42"},
            {"var g:double = 1.0; def up() { g = g + 0.5 } up(); up(); g", "double global mutation", "2.0"},
            {"def f() { var x:int = 1; { var y:int = 2; x + y } } f()", "block locals", "3"},
            {"def cl(n:int) { if n < 1 then 0 else (cl(n - 1) + 1) } cl(25)", "linear recursion 25", "25"},
            {"def gauss(n:int) { var s:int = 0; for i = 1, i < n, 1 in s = s + i; s } gauss(100)", "gauss sum", "4950"},
            {"0 - 7", "negative result", "-7"},
            {"7 * 0", "multiply by zero", "0"},
            {"var s:string = \"x\"; s + \"\"", "string plus empty", "\"x\""},
            // 触发 evaluator.cc 赋值时符号未绑定的错误分支（line 174-176）
            {"var x:int = 1; (x = 2)", "assignment value check", "2"},
            // 触发 evaluator.cc 无效 slot 错误分支（line 179-184）
            {"def f() { var x:int = 1; x } f()", "local variable in function", "1"},
            // 触发 evaluateBinary 默认操作符分支（line 213-215）
            {"def f(a:int, b:int) { a + b } f(3, 7)", "binary add function call", "10"},
            // 触发 evaluateFor slot 越界（line 88-89）
            {"def f(n:int) { for i = 0, i < n, 1 in i } f(5)", "for loop return last", "4"},
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

                bool ok = result.isValue();
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
                    std::cout << "      FAILED - Evaluator returned Error\n";
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
    // ==================== Evaluator Edge Case Tests ====================
    // 直接构造 AST 节点并绑定符号，以触发 evaluator.cc 中的错误分支
    void ComprehensiveTestSuite::runEvaluatorEdgeCaseTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "EVALUATOR EDGE CASE TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 1. 赋值时符号未绑定（触发 line 174-176：LHS of assignment is not bound to a symbol）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var x:int = 1; x");
            if (ast)
            {
                CompiledProgram program = scanner.semanticContext().getCompiledProgram();
                FCEvaluationContext ctx(program);
                // 构造一个未绑定符号的变量表达式作为赋值 LHS
                auto unboundVar = std::make_unique<FCVariableExprAST>(std::string("unbound"));
                auto rhs = std::make_unique<FCNumberExprAST>(42);
                auto assign = std::make_unique<FCBinaryExprAST>(FCBinaryOp::Assign, std::move(unboundVar), std::move(rhs));
                auto result = evaluate(assign.get(), ctx);
                expect(result.isError(), "evaluator - assignment unbound symbol returns error");
            }
            else
            {
                expect(false, "evaluator - assignment unbound symbol (scanner failed)");
            }
        }

        // 2. 赋值时 slot 无效（触发 line 179-184：Invalid slot for ...）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var x:int = 1; x = 2");
            if (ast)
            {
                CompiledProgram program = scanner.semanticContext().getCompiledProgram();
                FCEvaluationContext ctx(program);
                // 构造一个绑定了符号但 slot 越界的变量
                auto varDecl = std::make_shared<VarDecl>("badslot", "int");
                auto badSymbol = std::make_shared<VariableSymbol>();
                badSymbol->declaration = varDecl;
                badSymbol->slot = 999; // 越界
                badSymbol->runtime.frameIndex = -1; // 全局
                auto badVar = std::make_unique<FCVariableExprAST>(std::string("badslot"));
                ctx.bindNode(badVar.get(), badSymbol);
                auto rhs = std::make_unique<FCNumberExprAST>(42);
                auto assign = std::make_unique<FCBinaryExprAST>(FCBinaryOp::Assign, std::move(badVar), std::move(rhs));
                auto result = evaluate(assign.get(), ctx);
                expect(result.isError(), "evaluator - assignment invalid slot returns error");
            }
            else
            {
                expect(false, "evaluator - assignment invalid slot (scanner failed)");
            }
        }

        // 3. 二元表达式未知操作符（触发 line 213-215：LogError: %c）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("1 + 2");
            if (ast)
            {
                CompiledProgram program = scanner.semanticContext().getCompiledProgram();
                FCEvaluationContext ctx(program);
                // 构造一个带未知操作符的二元表达式
                auto lhs = std::make_unique<FCNumberExprAST>(1);
                auto rhs = std::make_unique<FCNumberExprAST>(2);
                auto binop = std::make_unique<FCBinaryExprAST>(FCBinaryOp::Unknown, std::move(lhs), std::move(rhs));
                auto result = evaluate(binop.get(), ctx);
                expect(result.isError(), "evaluator - unknown operator returns error");
            }
            else
            {
                expect(false, "evaluator - unknown operator (scanner failed)");
            }
        }

        // 4. for 循环变量 slot 越界（触发 line 88-89）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f(n:int) { for i = 0, i < n, 1 in i } f(5)");
            if (ast)
            {
                CompiledProgram program = scanner.semanticContext().getCompiledProgram();
                FCEvaluationContext ctx(program);
                // 构造一个 for 表达式，其符号 slot 越界
                auto varDecl = std::make_shared<VarDecl>("badfor", "int");
                auto badSymbol = std::make_shared<VariableSymbol>();
                badSymbol->declaration = varDecl;
                badSymbol->slot = 999; // 越界
                badSymbol->runtime.frameIndex = 0; // 局部
                auto badVar = std::make_unique<FCVariableExprAST>(std::string("badfor"));
                auto start = std::make_unique<FCNumberExprAST>(0);
                auto end = std::make_unique<FCNumberExprAST>(5);
                auto step = std::make_unique<FCNumberExprAST>(1);
                auto body = std::make_unique<FCVariableExprAST>(std::string("badfor"));
                ctx.bindNode(badVar.get(), badSymbol);
                // 为 body 变量也绑定符号（使其不返回错误）
                auto bodySymbol = std::make_shared<VariableSymbol>();
                bodySymbol->declaration = std::make_shared<VarDecl>("badfor", "int");
                bodySymbol->slot = 0;
                bodySymbol->runtime.frameIndex = 0;
                ctx.bindNode(body.get(), bodySymbol);
                auto forExpr = std::make_unique<FCForExprAST>(
                    std::move(varDecl), std::move(start), std::move(end), std::move(step), std::move(body));
                auto result = evaluate(forExpr.get(), ctx);
                expect(result.isError(), "evaluator - for invalid slot returns error");
            }
            else
            {
                expect(false, "evaluator - for invalid slot (scanner failed)");
            }
        }

        // 5. 调用注册在 AST registry 但无语义期 CompiledProgram 记录的函数
        //    （触发 evaluateCall 的 no-compiled-record 防御分支）
        {
            FCEvaluationContext ctx({});
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("ghost", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1));
            ctx.functions.registerFunction(fn.get());
            std::vector<std::unique_ptr<FCExprAST>> args;
            auto call = std::make_unique<FCCallExprAST>("ghost", std::move(args));
            auto result = evaluate(call.get(), ctx);
            expect(result.isError(),
                   "evaluator - call without compiled record returns error");
        }

        // 6. 调用的函数其形参未登记进 CompiledFunction 符号表
        //    （触发 unresolved-parameter 防御分支）
        {
            FCSemanticContext sem;
            auto fnAst = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("hollow",
                    [] {
                        std::vector<VarDeclPtr> a;
                        a.push_back(std::make_shared<VarDecl>("p", "int"));
                        return a;
                    }()),
                std::make_unique<FCVariableExprAST>(std::string("p")));
            {
                FC::FCDiagnostics diagnostics;
                FCSemanticContext::SemanticAnalyzer analyzer(sem, diagnostics);
                expect(analyzer.analyze(fnAst.get()),
                       "evaluator - hollow function analyzed");
            }
            // 正常记录存在；篡改 prototypes 指向不存在符号的形参以触发分支：
            // 直接构造一个调用并从 CompiledProgram 拷贝中删除参数符号即可
            CompiledProgram program = sem.getCompiledProgram();
            program.functions["hollow"]->symbols = SymbolTable{};  // 清空符号表
            FCEvaluationContext ctx(program);
            ctx.functions.registerFunction(fnAst.get());
            std::vector<std::unique_ptr<FCExprAST>> callArgs;
            callArgs.push_back(std::make_unique<FCNumberExprAST>(1));
            auto call = std::make_unique<FCCallExprAST>("hollow", std::move(callArgs));
            auto result = evaluate(call.get(), ctx);
            expect(result.isError(),
                   "evaluator - unresolved parameter symbol returns error");
        }

        // 7. 嵌套定义：求值器不提供执行能力（与语言开关无关，后端一律拒绝）
        {
            std::vector<std::unique_ptr<FCExprAST>> nestedBody;
            nestedBody.emplace_back(std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("inner", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1)));
            nestedBody.emplace_back(std::make_unique<FCNumberExprAST>(2));
            auto nestedOuter = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("nestedOuter", std::vector<VarDeclPtr>{}),
                std::make_unique<FCBlockExprAST>(std::move(nestedBody)));

            // 手工提供编译记录：本用例只验证求值器的拒绝分支
            CompiledProgram nestedProgram;
            auto nestedRecord = std::make_shared<CompiledFunction>();
            nestedRecord->frameSize = 0;
            nestedProgram.functions["nestedOuter"] = nestedRecord;

            FCEvaluationContext nestedCtx(nestedProgram);
            nestedCtx.functions.registerFunction(nestedOuter.get());

            // 顶层（无活动函数帧）仍走登记路径
            expect(evaluate(nestedOuter.get(), nestedCtx).isVoid(),
                   "evaluator - top-level definition still registers");

            // 进入函数体后遇到嵌套定义：明确拒绝
            std::vector<std::unique_ptr<FCExprAST>> nestedArgs;
            auto nestedCall =
                std::make_unique<FCCallExprAST>("nestedOuter", std::move(nestedArgs));
            expect(evaluate(nestedCall.get(), nestedCtx).isError(),
                   "evaluator - nested definition rejected inside a frame");
        }

        // 8. 混合绑定模式：语义绑定非空时，手工本地绑定仍可见
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var x:int = 1; x");   // 语义绑定表非空（x 已绑定）
            if (ast)
            {
                FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
                // 先执行程序：var x:int = 1 把值 1 写入全局帧 slot 0
                (void)evaluate(ast.get(), ctx);
                // 手工绑定一个与语义绑定无关的变量节点
                auto varDecl = std::make_shared<VarDecl>("manual", "int");
                auto manualSymbol = std::make_shared<VariableSymbol>();
                manualSymbol->declaration = varDecl;
                manualSymbol->slot = 0;
                manualSymbol->runtime.frameIndex = -1;   // 全局帧 slot 0，即上面 var x:int = 1 的槽位
                auto manualVar = std::make_unique<FCVariableExprAST>(std::string("manual"));
                ctx.bindNode(manualVar.get(), manualSymbol);
                // 求值手工节点：应读到全局帧 slot 0 的值 1
                FCValue v = evaluate(manualVar.get(), ctx);
                expect(v.category() == FCValueCategory::Integer && *v.asInteger() == 1,
                       "evaluator - local binding visible when semantic binding nonempty");
            }
            else
            {
                expect(false, "evaluator - local binding visible (scanner failed)");
            }
        }

        // 9. 调用解析优先级：registry 与 compiledProgram 同名且原型不一致时，
        //    原型与槽位必须同取 compiledProgram 记录（单一数据源）
        {
            FCSemanticContext sem;
            FC::FCDiagnostics diagnostics;
            auto fnAst = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("conflict", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(7));
            {
                FCSemanticContext::SemanticAnalyzer analyzer(sem, diagnostics);
                expect(analyzer.analyze(fnAst.get()),
                       "evaluator - conflict function analyzed");
            }
            CompiledProgram program = sem.getCompiledProgram();
            // 构造一个不同原型的 AST（单参数）注册进 registry：两表不一致
            std::vector<VarDeclPtr> fakeArgs;
            fakeArgs.push_back(std::make_shared<VarDecl>("p", "int"));
            auto fakeAst = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("conflict", std::move(fakeArgs)),
                std::make_unique<FCNumberExprAST>(99));
            FCEvaluationContext ctx(program);
            ctx.functions.registerFunction(fakeAst.get());
            // 零实参调用：compiledProgram 原型（零参数）应执行成功返回 7；
            // 旧实现按 registry 单参数原型报参数个数不匹配，返回 Error
            std::vector<std::unique_ptr<FCExprAST>> args;
            auto call = std::make_unique<FCCallExprAST>("conflict", std::move(args));
            FCValue v = evaluate(call.get(), ctx);
            expect(v.category() == FCValueCategory::Integer && *v.asInteger() == 7,
                   "evaluator - call resolves by compiled program record");
        }
    }
    // ==================== Evaluator Error Tests ====================
    void ComprehensiveTestSuite::runEvaluatorErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "EVALUATOR ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::pair<std::string, std::string>> errorCases = {
            {"undefinedfn(1)", "call to undefined function"},
            {"def f(a:int) { a } f(1, 2)", "argument count mismatch"},
            {"1 + \"a\"", "int plus string mismatch"},
            {"def fl(n:int) { for i = 0.0, i < n, 1 in i } fl(3)", "floating loop variable"},
            {"\"a\" - \"b\"", "string subtraction"},
            {"if \"a\" then 1 else 2", "non-numeric if condition"},
            {"for i = 0, i < 3, 1 in i", "top-level for loop"},
            {"def f(n:int) { for i = 0, i < n, 1 in undefinedfn() } f(3)", "loop body error"},
            {"(1; \"a\" - \"b\"; 3)", "sequence error propagation"},
            {"def f() { var x:int = undefinedfn(); x } f()", "declaration init error"},
            {"1 = 2", "assignment to non-variable"},
            {"var x:int = 1; x = undefinedfn(); x", "assignment rhs error"},
            {"def f(a:int) { a } f(undefinedfn())", "call argument error"},
            {"1.0 < 2.0", "float comparison"},
            // ---- 新增 ----
            {"1 + 2.0", "int plus double mismatch"},
            {"2.0 + 1", "double plus int mismatch"},
            {"\"a\" + 1", "string plus int"},
            {"if undefinedfn() then 1 else 2", "error if condition"},
            {"def f() { 1.0 + 2 } f()", "mixed arithmetic inside function"},
            {"def f() { \"a\" * 2 } f()", "string multiplication"},
            {"undefinedfn() + 1", "error lhs binary"},
            {"def f(n:int) { for i = 0, i < n, undefinedfn() in i } f(3)", "error for step"},
            {"def f() { if 1 then undefinedfn() else 2 } f()", "error then branch"},
            {"def f() { var x:int = 1; x = x + \"s\"; x } f()", "mixed assignment rhs"},
            {"var g:int = 1; g + \"s\"", "global plus string"}
        };

        for (const auto& [code, name] : errorCases)
        {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            FCEvaluationContext ctx = makeEvalContext(scanner);
            bool isError = true;
            if (ast)
            {
                try
                {
                    FCValue result = evaluate(ast.get(), ctx);
                    isError = result.isError();
                }
                catch (...)
                {
                    isError = true;
                }
            }
            expect(isError, "evaluator-error - " + name);
        }

        // 直接驱动公共 API，覆盖 dispatch 的空指针 / 未知节点 / 帧查找错误分支
        {
            FCEvaluationContext ctx({});
            expect(evaluate(static_cast<FCExprAST*>(nullptr), ctx).isError(),
                   "evaluator-error - evaluate null");

            const FCExprAST* constNull = nullptr;
            expect(evaluate(constNull, ctx).isError(),
                   "evaluator-error - const evaluate null");

            std::vector<VarDeclPtr> protoArgs;
            auto proto = std::make_unique<FCPrototypeAST>("p", std::move(protoArgs));
            expect(evaluate(proto.get(), ctx).isError(),
                   "evaluator-error - evaluate prototype node");

            // 空序列 / 空块 → Void
            std::vector<std::unique_ptr<FCExprAST>> emptyItems;
            auto emptySeq = std::make_unique<FCSeqExprAST>(std::move(emptyItems));
            expect(evaluate(emptySeq.get(), ctx).isVoid(),
                   "evaluator-error - empty sequence");

            std::vector<std::unique_ptr<FCExprAST>> emptyBlk;
            auto emptyBlock = std::make_unique<FCBlockExprAST>(std::move(emptyBlk));
            expect(evaluate(emptyBlock.get(), ctx).isVoid(),
                   "evaluator-error - empty block");

            // for 的 decl 为空
            auto nullFor = std::make_unique<FCForExprAST>(
                nullptr, std::make_unique<FCNumberExprAST>(0),
                std::make_unique<FCNumberExprAST>(10), nullptr,
                std::make_unique<FCNumberExprAST>(1));
            expect(evaluate(nullFor.get(), ctx).isError(),
                   "evaluator-error - for with null decl");

            // 局部变量但无调用帧
            auto localDecl = std::make_shared<VarDecl>("v", "int");
            VariableSymbol localSym(localDecl, 0, 1);
            localSym.runtime.frameIndex = 0;
            ctx.compiledProgram.globalSymbols.push_back(std::make_shared<VariableSymbol>(localSym));
            auto localVar = std::make_unique<FCVariableExprAST>(std::string("v"));
            ctx.bindNode(localVar.get(), std::make_shared<VariableSymbol>(localSym));
            expect(evaluate(localVar.get(), ctx).isError(),
                   "evaluator-error - local variable without frame");

            // 全局变量但槽位非法
            auto negDecl = std::make_shared<VarDecl>("g", "int");
            VariableSymbol negSym(negDecl, -1, 0);
            negSym.runtime.frameIndex = -1;
            ctx.compiledProgram.globalSymbols.push_back(std::make_shared<VariableSymbol>(negSym));
            auto negVar = std::make_unique<FCVariableExprAST>(std::string("g"));
            ctx.bindNode(negVar.get(), std::make_shared<VariableSymbol>(negSym));
            expect(evaluate(negVar.get(), ctx).isError(),
                   "evaluator-error - global variable with negative slot");

            // 局部变量槽位超出帧大小
            auto oobDecl = std::make_shared<VarDecl>("o", "int");
            VariableSymbol oobSym(oobDecl, 100, 1);
            oobSym.runtime.frameIndex = 0;
            ctx.compiledProgram.globalSymbols.push_back(std::make_shared<VariableSymbol>(oobSym));
            auto oobVar = std::make_unique<FCVariableExprAST>(std::string("o"));
            ctx.bindNode(oobVar.get(), std::make_shared<VariableSymbol>(oobSym));
            ctx.pushFrame("oob");
            expect(evaluate(oobVar.get(), ctx).isError(),
                   "evaluator-error - local variable slot out of range");
            ctx.popFrame();

            // 全局帧按构造时的 globalSymbols 大小分配，之后不随 globalSymbols 增长自动扩展
            expect(ctx.callStack[0].locals.size() == 0, "evaluator-error - global frame initial size matches empty globalSymbols");
        }

        // 帧 API 直接测试
        {
            // 构造时会自动压入顶层全局帧
            FCEvaluationContext ctx({});
            expect(ctx.callStack.size() == 1, "evaluator-error - initial call stack has global frame");
            ctx.pushFrame("unknown");
            expect(ctx.currentFrame().funcName == "unknown", "evaluator-error - push frame name");
            expect(ctx.currentFrame().locals.empty(), "evaluator-error - unregistered frame has no locals");
            expect(ctx.callStack.size() == 2, "evaluator-error - call stack size after push");
            ctx.popFrame();
            expect(ctx.callStack.size() == 1, "evaluator-error - call stack size after pop");
            expect(!ctx.callStack.empty(), "evaluator-error - global frame retained after pop");

            // 注册一个带局部变量的函数后 pushFrame 会按语义期 frameSize 分配帧
            {
                // 语义期路径：declareVariable 分配 3 个帧槽 → registerFunction
                // 记录 frameSize=3 → evaluator pushFrame 读取该值
                FCSemanticContext semantic;
                {
                    FCSemanticContext::FunctionScopeGuard guard(semantic);
                    semantic.declareVariable(std::make_shared<VarDecl>("fa", "int"));
                    semantic.declareVariable(std::make_shared<VarDecl>("fb", "int"));
                    semantic.declareVariable(std::make_shared<VarDecl>("fc", "int"));

                    auto framed = std::make_unique<FCFunctionAST>(
                        std::make_unique<FCPrototypeAST>("framed2", std::vector<VarDeclPtr>{}),
                        std::make_unique<FCNumberExprAST>(1));
                    semantic.registerFunction("framed2", framed.get());
                }
                const auto* compiled = semantic.getCompiledProgram().getFunction("framed2");
                expect(compiled != nullptr && compiled->frameSize == 3,
                       "evaluator-error - semantic frameSize recorded");

                FCEvaluationContext sizedCtx(semantic.getCompiledProgram());
                sizedCtx.pushFrame("framed2");
                expect(sizedCtx.currentFrame().locals.size() == 3,
                       "evaluator-error - frame sized by semantic frameSize");
                sizedCtx.popFrame();
            }
        }
    }
    // ==================== Evaluator Value Semantics Tests ====================
    // 钉住 FCValue 变体迁移（92aef02）后的值语义行为，防止未来重构静默回退：
    // - 终止条件非数值 / 循环变量被赋成字符串 → Error + 结构化诊断
    //   （旧实现读 union 位模式做整数运算）
    // - 字符串跨帧传递与赋值独立 → variant 按值深拷贝（旧实现为共享堆指针）
    void ComprehensiveTestSuite::runEvaluatorValueSemanticsTests()
    {
        // 测试 A：for 终止条件求值结果非数值 → Error + 诊断。
        // 注意：顶层 for 的循环变量落在全局帧（frameIndex == -1），
        // evaluateFor 在帧守卫处提前返回，终止条件不求值；
        // 须置于函数内才能命中被钉住的分支。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { for i = 0, \"a\", 1 in i } f()");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isError(),
                   "value-semantics - for end condition must be numeric");
            bool hasDiagnostic = false;
            for (const auto& d : ctx.diagnostics.items())
                if (d.message.find("for end condition must be numeric") != std::string::npos)
                    hasDiagnostic = true;
            expect(hasDiagnostic,
                   "value-semantics - for end condition must be numeric - diagnostic");
        }

        // 测试 B：循环体把循环变量赋成字符串 → Error + 诊断，不做位模式加法。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { for i = 0, i < 3, 1 in i = \"s\" } f()");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isError(),
                   "value-semantics - loop variable must stay integer");
            bool hasDiagnostic = false;
            for (const auto& d : ctx.diagnostics.items())
                if (d.message.find("for loop variable must stay integer") != std::string::npos)
                    hasDiagnostic = true;
            expect(hasDiagnostic,
                   "value-semantics - loop variable must stay integer - diagnostic");
        }

        // 测试 C：字符串跨帧值语义——返回值与参数在帧弹出后仍有效（深拷贝）。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def make() { \"abc\" } def id(s:string) { s } id(make())");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.category() == FCValueCategory::String && v.asString() != nullptr &&
                       *v.asString() == "abc",
                   "value-semantics - string survives frame pop");
        }

        // 测试 D：赋值独立性——b 的重绑不共享 a 的存储（防退回共享缓冲实现）。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("var a:string = \"x\"; var b:string = a; b = b + \"!\"; a");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.category() == FCValueCategory::String && v.asString() != nullptr &&
                       *v.asString() == "x",
                   "value-semantics - assignment copies strings");
        }
    }
    // ==================== Evaluator Void Tests ====================
    // 钉住 Dangle→Void/Error 拆分后的传播规则（2026-10-24 计划）：
    // - Void 流经：序列/块继续、可存入槽位、循环体全 Void 返回 Void
    // - Error 穿透：for 终止条件错误不再被 union 误读吞成常量
    void ComprehensiveTestSuite::runEvaluatorVoidTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "EVALUATOR VOID TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 1. 空块 → Void
        {
            FCEvaluationContext ctx({});
            std::vector<std::unique_ptr<FCExprAST>> emptyBlk;
            auto emptyBlock = std::make_unique<FCBlockExprAST>(std::move(emptyBlk));
            expect(evaluate(emptyBlock.get(), ctx).isVoid(),
                   "void - empty block yields Void");
        }

        // 2. 空序列 → Void
        {
            FCEvaluationContext ctx({});
            std::vector<std::unique_ptr<FCExprAST>> emptyItems;
            auto emptySeq = std::make_unique<FCSeqExprAST>(std::move(emptyItems));
            expect(evaluate(emptySeq.get(), ctx).isVoid(),
                   "void - empty sequence yields Void");
        }

        // 3. 仅含 def 的程序 → Void（原 errorCases 条目迁移）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { 1 }");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            expect(ast && evaluate(ast.get(), ctx).isVoid(),
                   "void - definitions-only program yields Void");
        }

        // 4. 序列遇 Void 继续：(1; v(); 2) → 2（行为变化点：旧实现 Dangle 中止）
        //    计划原文 "(1; { }; 2)" 不合当前语法（parser 禁止 ';' 紧跟块终结符），
        //    以返回空块（Void）的函数调用作序列中项，钉住同一传播行为。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def v() { } (1; v(); 2)");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.category() == FCValueCategory::Integer &&
                       v.asInteger() != nullptr && *v.asInteger() == 2,
                   "void - sequence continues past empty block");
        }

        // 5. for 体为空块：循环走完，结果 Void
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { for i = 0, i < 2 in { } } f()");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isVoid(),
                   "void - for with empty-block body yields Void");
        }

        // 6. for 终止条件为错误 → Error 传播（行为变化点：旧实现 union 误读返回 0）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { for i = 0, undefinedfn(), 1 in i } f()");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isError(),
                   "error - for end condition error propagates");
        }

        // 7. if 条件为 Void → Error（条件必须数值）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("if { } then 1 else 2");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isError(),
                   "error - void if condition is an error");
        }

        // 8. Void 存入槽位继续：var x:int = { } x → Void（行为变化点：旧实现中止）
        //    计划原文 "var x:int = { }; x" 不合当前语法（';' 禁止紧跟块终结符），
        //    块后直接跟表达式同语义：声明 init 为空块 → Void 存入 x，x 读出 Void。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { var x:int = { } x } f()");
            FCEvaluationContext ctx = makeEvalContext(scanner);
            FCValue v = ast ? evaluate(ast.get(), ctx) : FCValue{};
            expect(v.isVoid(),
                   "void - void initializer stores and flows");
        }
    }
    // 求值阶段的诊断输出：只在 FC_TRACE 含 evaluator 时打印调用与返回
    void ComprehensiveTestSuite::runEvaluatorTraceTests()
    {
        auto evaluateOf = [](const std::string &code) -> FCMarks::FCValue {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            if (!ast)
                return FCMarks::FCValue{};
            FCEvaluationContext ctx(scanner.semanticContext().getCompiledProgram());
            return evaluate(ast.get(), ctx);
        };

        FC::setTraceStagesForTesting("evaluator");

        auto add = evaluateOf("def add(a:int, b:int) { a + b } add(2, 3)");
        expect(add.category() == FCValueCategory::Integer && *add.asInteger() == 5,
               "evaluator-trace - integer call result");

        auto zero = evaluateOf("def two() { 2 } two()");
        expect(zero.category() == FCValueCategory::Integer && *zero.asInteger() == 2,
               "evaluator-trace - zero-argument call");

        auto floating = evaluateOf("def half(x:double) { x / 2.0 } half(5.0)");
        expect(floating.category() == FCValueCategory::Floating &&
                   *floating.asFloating() == 2.5,
               "evaluator-trace - floating result");

        auto text = evaluateOf("def join(a:string, b:string) { a + b } join(\"x\", \"y\")");
        expect(text.category() == FCValueCategory::String && text.asString() != nullptr &&
                   *text.asString() == "xy",
               "evaluator-trace - string result");

        auto failed = evaluateOf("def bad() { missing() } bad()");
        expect(failed.isError(),
               "evaluator-trace - error result");

        FC::setTraceStagesForTesting("");
    }
    // ==================== Stress Tests ====================
    void ComprehensiveTestSuite::runStressTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "STRESS TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::tuple<std::string, std::string, std::string>> stressCases = {
            {"def count(n:int) { if n < 1 then 0 else count(n - 1) + 1 } count(100)", "deep recursion 100", "100"},
            {"def sumN(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } sumN(1000)", "large loop 1000", "499500"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) } fib(15)", "fibonacci 15", "610"},
            {"def pow2(n:int) { var r:int = 1; for i = 0, i < n, 1 in r = r * 2; r } pow2(20)", "exponential loop", "1048576"},
            {"def ack(m:int) { if m < 1 then 1 else (ack(m - 1) + ack(m - 1)) } ack(12)", "binary tree recursion", "4096"},
            // ---- 新增 ----
            // {"def count(n:int) { if n < 1 then 0 else count(n - 1) + 1 } count(250)", "deep recursion 250", "250"},
            {"def sumN(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } sumN(10000)", "large loop 10000", "49995000"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) } fib(20)", "fibonacci 20", "6765"},
            {"def pow2(n:int) { var r:int = 1; for i = 0, i < n, 1 in r = r * 2; r } pow2(25)", "exponential loop 25", "33554432"},
            {"def ack(m:int) { if m < 1 then 1 else (ack(m - 1) + ack(m - 1)) } ack(15)", "binary tree recursion 15", "32768"},
            {"var a:int = 2; def f(x:int) { x * 2 } def g(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + f(i); s } g(10)", "mixed program with call in loop", "90"}
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
                expect(result.isValue() && actual == expected,
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
            auto ast1 = s1.analysis("def f1(x:int) { x + 1 } f1(1)");
            FCEvaluationContext ctx = makeEvalContext(s1);
            expect(evaluate(ast1.get(), ctx).category() == FCValueCategory::Integer,
                   "stress - context reuse program 1");

            FCScanner s2;
            auto ast2 = s2.analysis("def g1(x:int) { x * 2 } g1(21)");
            ctx.compiledProgram = s2.semanticContext().getCompiledProgram();
            FCValue r2 = evaluate(ast2.get(), ctx);
            expect(r2.category() == FCValueCategory::Integer && *r2.asInteger() == 42,
                   "stress - context reuse program 2");
        }

        // codegen 压力：多函数、多全局、复杂表达式
        {
            std::string code =
                "var a:int = 1; var b:double = 2.5; "
                "def f1(x:int) { x + 1 } "
                "def f2(x:double) { x * 2.0 } "
                "def f3(x:int) { if x < 10 then x else x - 10 } "
                "def f4(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s } "
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
            std::string code = "def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in { var t:int = i; if t < 10 then s = s + t else s = s - t } s }";
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
    // ==================== Registry Tests ====================
    void ComprehensiveTestSuite::runRegistryTests()
    {
        std::vector<std::tuple<std::string, std::string, int>> registryCases = {
            {"def one() { 1 }", "single function", 1},
            {"def add(a:int, b:int) { a + b }", "function with params", 1},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "recursive function", 1},
            {"def square(x:int) { x * x } def cube(x:int) { x * x * x }", "multiple functions", 2},
            {"var g:int = 1; def read() { g } def write() { g = 2 }", "functions with globals", 2},
            {"def empty() { 42 }", "zero-argument function", 1},
            {"def a() { 1 } def b() { 2 } def c() { 3 }", "three functions", 3},
            {"def f() { var x:int = 1; x }", "function with local", 1},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "recursive factorial", 1},
            {"def a() { 1 } def b() { 2 } def c() { 3 } def d() { 4 }", "four functions", 4},
            {"def a() { 1 } def b() { 2 } def c() { 3 } def d() { 4 } def e() { 5 }", "five functions", 5},
            {"var g:int = 1; def f() { g }", "global plus function", 1},
            {"def mix(a:int, b:double, c:string) { a }", "mixed-parameter function", 1},
            {"def f() { var x:int = 1; x } def g() { f() }", "local and call", 2},
            {"def r(n:int) { if n < 1 then 0 else r(n - 1) }", "recursive", 1},
            {"def f1() { 1 } def f2() { 2 } def f3() { 3 } def f4() { 4 } def f5() { 5 } def f6() { 6 }", "six functions", 6},
            // ---- 新增 ----
            {"def a() { 1 } var g:int = 1; def b() { g }", "functions interleaved with global", 2}
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
            std::make_unique<FCNumberExprAST>(1));
        auto func2 = std::make_unique<FCFunctionAST>(
            std::make_unique<FCPrototypeAST>("duplicate", std::vector<VarDeclPtr>{}),
            std::make_unique<FCNumberExprAST>(2));

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
                std::make_unique<FCNumberExprAST>(1));
            expect(reg.index(fn.get()), "registry - index single function");
            expect(reg.findFunction("solo") != nullptr, "registry - find after index");

            auto num = std::make_unique<FCNumberExprAST>(42);
            expect(!reg.index(num.get()), "registry - index non-function non-program");

            // index 一个只含函数的 program
            std::vector<std::unique_ptr<FCExprAST>> stmts;
            stmts.push_back(std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("progfn", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1)));
            auto prog = std::make_unique<FCProgramAST>(std::move(stmts));
            expect(reg.index(prog.get()), "registry - index program with functions");
            expect(reg.findFunction("progfn") != nullptr, "registry - program function found");

        // FrameGuard 回归保护：手工构造 AST 函数（无语义期记录）触发
        // evaluateCall 的错误路径并调用后继续求值，调用栈必须恢复为 1 帧。
        {
            FCSemanticContext sem;
            FC::FCDiagnostics diagnostics;
            auto fnAst = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("frameguard", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1));
            CompiledProgram program;
            auto rec = std::make_shared<CompiledFunction>();
            program.functions["frameguard"] = rec;
            rec->symbols.addSymbol("x", std::make_shared<VariableSymbol>(
                std::make_shared<VarDecl>("x", "int"), 0, 0));
            FCEvaluationContext ctx(program);
            ctx.functions.registerFunction(fnAst.get());
            // 零参数但记录要求 1 个形参 => 参数个数不匹配，错误路径弹帧
            std::vector<std::unique_ptr<FCExprAST>> args;
            args.push_back(std::make_unique<FCNumberExprAST>(1));
            auto call = std::make_unique<FCCallExprAST>("frameguard", std::move(args));
            FCValue v = evaluate(call.get(), ctx);
            expect(v.isError(),
                   "evaluator - frameguard error path returns error");
            // 后续求值若帧栈泄漏，frame.locals size 不匹配；断言栈深恢复
            expect(ctx.callStack.size() == 1,
                   "evaluator - frameguard frame depth restored after error path");
        }            reg.clear();
            expect(reg.findFunction("solo") == nullptr, "registry - clear");
            expect(reg.findFunction("progfn") == nullptr, "registry - clear removes all");
        }
    }

