#include "test_harness.h"

    // ==================== Diagnostic Tests ====================
    // 结构化诊断通道：FCDiagnostics 记录带 stage/位置/消息的诊断，
    // FCScanner 每轮 analysis() 前清空，供后续 semantic/evaluator/codegen 阶段共用。
    void ComprehensiveTestSuite::runDiagnosticTests()
    {
        // 1. 解析错误产生带位置的诊断
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def 123() { 1 }");
            expect(ast == nullptr, "diag: parse error returns null");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty(), "diag: parse error recorded");
            if (!diags.empty())
            {
                expect(diags[0].stage == "parser", "diag: stage is parser");
                expect(diags[0].location.line == 1 && diags[0].location.column == 5,
                       "diag: location points at offending token");
                expect(diags[0].message.find("Expected function name") != std::string::npos,
                       "diag: message preserved");
            }
        }
        // 2. 成功时无诊断
        {
            FCScanner scanner;
            auto ast = scanner.analysis("1 + 2");
            expect(ast != nullptr && scanner.diagnostics().empty(),
                   "diag: clean input yields no diagnostics");
        }
        // 3. 复用清理
        {
            FCScanner scanner;
            scanner.analysis("def 123() { 1 }");       // 失败
            auto ok = scanner.analysis("1 + 2");        // 成功
            expect(ok != nullptr && scanner.diagnostics().empty(),
                   "diag: scannerDiagnosticsResetOnReuse");
        }
        // 4. EOF 位置
        {
            FCScanner scanner;   // "if 1 then 2" 缺 else，错误在 EOF 处
            auto ast = scanner.analysis("if 1 then 2");
            expect(ast == nullptr, "diag: missing else fails");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty() && diags[0].location.line >= 1,
                   "diag: parserDiagnosticAtEofHasPosition");
        }
        // 5. 词法错误穿透（未闭合字符串）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("\"abc");
            expect(ast == nullptr, "diag: unterminated string fails");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty()
                   && diags[0].message.find("unterminated string literal") != std::string::npos
                   && diags[0].location.line == 1 && diags[0].location.column == 1,
                   "diag: lexerErrorSurfacesWithPosition");
        }
        // 6. formatDiagnostic 精确格式
        {
            FCDiagnostic d{"parser", {1, 5}, "msg"};
            expect(FC::formatDiagnostic(d) == "error[parser] 1:5: msg",
                   "diag: formatDiagnostic with location");
            FCDiagnostic d2{"semantic", {}, "msg"};
            expect(FC::formatDiagnostic(d2) == "error[semantic]: msg",
                   "diag: formatDiagnostic without location");
        }
        // 7. 语义错误：重复声明
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { var x:int = 1; var x:int = 2; x }");
            expect(ast == nullptr, "diag: duplicate var fails");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty() && diags[0].stage == "semantic"
                   && diags[0].message.find("'x'") != std::string::npos,
                   "diag: duplicate var recorded with name");
        }
        // 8. 语义错误：未声明变量
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { y }");
            expect(ast == nullptr, "diag: undeclared variable fails");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty() && diags[0].message.find("'y'") != std::string::npos,
                   "diag: undeclared variable recorded");
        }
        // 9. 语义错误：重复函数
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { 1 } def f() { 2 }");
            expect(ast == nullptr, "diag: duplicate function fails");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty() && diags[0].message.find("'f'") != std::string::npos,
                   "diag: duplicate function recorded");
        }
        // 10. 语义错误：嵌套 def（默认配置）
        //     经 scanner 的路径：parser 无 tok_def 表达式产生式，块内 def 在
        //     解析期即被拒（语义守卫不可达，诊断 stage 为 parser）；
        //     语义守卫本身用手工 AST 验证（下一块）。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { def g() { 1 } g() }");
            expect(ast == nullptr, "diag: nested def rejected");
            const auto& diags = scanner.diagnostics();
            expect(!diags.empty() && diags[0].stage == "parser",
                   "diag: in-block def rejected at parser stage");
        }
        // 10b. 语义守卫（mayDefineFunctionHere）：手工 AST 触发嵌套拒绝，
        //      诊断进通道且 stage 为 semantic（仅默认配置；开启嵌套时守卫放行）
        if constexpr (!FCMarks::kNestedFunctionDefinitions)
        {
            FCSemanticContext ctx;
            FC::FCDiagnostics diagnostics;
            FCSemanticContext::SemanticAnalyzer analyzer(ctx, diagnostics);
            auto outerFn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("outer", std::vector<VarDeclPtr>{}),
                std::make_unique<FCBlockExprAST>([] {
                    std::vector<std::unique_ptr<FCExprAST>> body;
                    body.emplace_back(std::make_unique<FCFunctionAST>(
                        std::make_unique<FCPrototypeAST>("inner", std::vector<VarDeclPtr>{}),
                        std::make_unique<FCNumberExprAST>(1)));
                    return body;
                }()));
            expect(!analyzer.analyze(outerFn.get()), "diag: nested def rejected by semantic guard");
            expect(!diagnostics.empty()
                   && diagnostics[0].stage == "semantic"
                   && diagnostics[0].message.find("nested function definition") != std::string::npos,
                   "diag: nested def recorded");
        }
        // 11. for 循环变量：analyzeFor 有独立作用域（ScopeGuard），同名外层变量被遮蔽而非冲突
        //     （brief 原文假设 var i 后 for i 报重复，与 analyzeFor 的作用域语义不符，见 ledger ruling）
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { var i:int = 0; for i = 0, i < 1 in i }");
            expect(ast != nullptr && scanner.diagnostics().empty(),
                   "diag: for-var shadows outer, no conflict");
        }
        // 12. 求值错误：函数不存在（诊断与 Error 并存）
        //     语义期不校验被调用函数是否存在（analyzeCall 仅查实参），源文本可通过分析。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("undefinedfn(1)");
            expect(ast != nullptr, "diag: eval case parses");
            FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
            FCValue v = evaluate(ast.get(), context);
            expect(v.isError(), "diag: eval error yields Error");
            expect(!context.diagnostics.empty()
                   && context.diagnostics.items()[0].stage == "evaluator"
                   && context.diagnostics.items()[0].message.find("undefinedfn") != std::string::npos,
                   "diag: unknown function recorded");
        }
        // 13. 求值错误：实参数不匹配
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def add(a:int, b:int) { a + b } add(2, 3, 4)");
            expect(ast != nullptr, "diag: arity case parses");
            FCEvaluationContext context(scanner.semanticContext().getCompiledProgram());
            FCValue v = evaluate(ast.get(), context);
            expect(v.isError(), "diag: arity error yields Error");
            expect(!context.diagnostics.empty()
                   && context.diagnostics.items()[0].message.find("Argument count mismatch") != std::string::npos,
                   "diag: arity mismatch recorded");
        }
        // 14. 求值错误：赋值左侧非变量
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { 1 = 2 } f()");
            expect(ast == nullptr, "diag: bad assign yields Error (semantic stage)");
            expect(!scanner.diagnostics().empty()
                   && scanner.diagnostics().items()[0].stage == "semantic"
                   && scanner.diagnostics().items()[0].message.find("assignment") != std::string::npos,
                   "diag: bad assignment recorded");
        }
        // 15. codegen 错误：赋值左侧非变量
        {
            // 手工 AST（沿用 runCodegenErrorTests 的既有构造方式）：
            // Binary(Assign, Number(1), Number(2))
            FCCodegenContext context("diagModule", CompiledProgram{});
            auto assign = std::make_unique<FCBinaryExprAST>(
                FCBinaryOp::Assign, std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCNumberExprAST>(2));
            expect(codegenTopLevel(assign.get(), context) == nullptr,
                   "diag: codegen assign fails");
            expect(!context.diagnostics.empty()
                   && context.diagnostics.items()[0].stage == "codegen"
                   && context.diagnostics.items()[0].message.find("assignment") != std::string::npos,
                   "diag: codegen assign error recorded");
        }
        // 16. codegen 错误：调用不存在的函数
        {
            // Call("nosuchfn", []) 手工 AST（沿用既有 FCCallExprAST 构造）
            FCCodegenContext context("diagModule", CompiledProgram{});
            auto call = std::make_unique<FCCallExprAST>(
                "nosuchfn", std::vector<std::unique_ptr<FCExprAST>>{});
            expect(codegenTopLevel(call.get(), context) == nullptr,
                   "diag: codegen call fails");
            expect(!context.diagnostics.empty()
                   && context.diagnostics.items()[0].message.find("nosuchfn") != std::string::npos,
                   "diag: codegen unknown function recorded");
        }
        // 17. 空函数体 codegen 失败（README:265 已知行为：语法可解析，LLVM codegen 失败）
        //     analysis 本身成功（semantic 对空块放行），失败发生在 codegenBlock。
        {
            FCScanner scanner;
            auto ast = scanner.analysis("def f() { }");
            expect(ast != nullptr, "diag: empty body parses");
            FCCodegenContext context("diagModule", scanner.semanticContext().getCompiledProgram());
            context.semantic = &scanner.semanticContextMutable();
            expect(codegenTopLevel(ast.get(), context) == nullptr, "diag: empty body fails");
            expect(!context.diagnostics.empty(), "diag: empty body error recorded");
        }
    }
