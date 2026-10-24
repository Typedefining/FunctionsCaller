#include "test_harness.h"

    // ==================== Codegen Tests ====================
    void ComprehensiveTestSuite::runCodegenTests()
    {
        std::vector<std::tuple<std::string, std::string>> codegenCases = {
            {"def simple() { 42 }", "simple function"},
            {"def add(a:int, b:int) { a + b }", "integer addition"},
            {"def mul(a:double, b:double) { a * b }", "floating multiplication"},
            {"def concat(a:string, b:string) { a + b }", "string concatenation"},
            {"def iftest(a:int) { if a < 1 then 1 else 2 }", "if expression"},
            {"def loop(n:int) { for i = 0, i < n, 1 in i }", "for loop"},
            {"def local(a:int) { var b:int = a + 1; b }", "local variable"},
            {"var g:int = 10; def read() { g } def write() { g = 20 }", "global variables"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }", "recursive function"},
            {"def power(a:int, n:int) { if n < 1 then 1 else a * power(a, n - 1) }", "power function"},
            {"def fact(n:int) { if n < 2 then 1 else n * fact(n - 1) }", "factorial recursion"},
            {"def max(a:int, b:int) { if a < b then b else a }", "max function"},
            {"def sum(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + i; s }", "loop with accumulation"},
            {"def countdown(n:int) { if n < 1 then 0 else (var x:int = n; x - countdown(n - 1)) }", "complex recursion"},
            {"var g:double = 3.14; def readDouble() { g }", "global double"},
            {"def id(x:int) { x } def caller() { id(42) }", "nested function call"},
            {"var s:string = \"hello\"; def get() { s } def set(x:string) { s = x }", "string global"},
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
            {"var g:int = 0; def inc() { g = g + 1 } def run() { inc(); inc(); g }", "global mutation via functions"},
            {"def seqfn() { (var x:int = 1; x + 1; x + 2) }", "sequence function"},
            {"def div2(a:double, b:double) { a / b } def caller() { div2(7.5, 2.5) }", "nested float call"},
            {"var gd:double = 1.5; var gi:int = 2; def mix2() { gd + gi }", "mixed globals"},
            {"def strcat(a:string, b:string) { a + b } def use() { strcat(\"a\", \"b\") }", "string concat via call"},
            {"def many(n:int) { var a:int = 1; var b:int = 2; var c:int = 3; var d:int = 4; a + b + c + d + n }", "many locals"},
            {"def branch(a:int) { if a < 1 then 1 else if a < 2 then 2 else 3 }", "nested if else"},
            {"def gauss(n:int) { var s:int = 0; for i = 1, i < n, 1 in s = s + i; s }", "gauss loop"},
            {"(var g:int = 1; g + 1); def f() { g }", "global in top-level sequence"},
            {"{ 1 + 2 } def f() { 3 }", "top-level block in program"},
            {"if 1 then 2 else 3; def f() { 4 }", "top-level if in program"},
            {"def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) } fib(5)", "recursive function in program"},
            {"def fmax(a:double, b:double) { if a < b then b else a }", "double if expression"},
            {"def fcond(a:double) { if a then 1 else 2 }", "floating if condition"},
            {"var s:string = \"hi\"", "standalone string global"},
            // ---- 新增 ----
            {"def caller() { later(5) } def later(x:int) { x + 1 }", "forward reference call"},
            {"def isEven(n:int) { if n < 1 then 1 else isOdd(n - 1) } def isOdd(n:int) { if n < 1 then 0 else isEven(n - 1) }", "mutual recursion codegen"},
            {"def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in { s = s + i } s }", "for with block body"},
            {"def f() { 1 + 2 * 3 - 4 / 2 + (5 * 6) }", "deep arithmetic"},
            {"def f(a:double) { a } def g() { f(1) }", "int arg to double param"},
            {"var g:double = 1.5; def f() { g = 2.5 }", "double global assignment"},
            {"def f(a:int) { a } def g() { f(3) } def h() { g() + f(4) }", "call chain across functions"},
            {"def f() { if 1 then 2 else 3 }", "constant condition if"},
            {"var x:int = 1; x + 2", "top-level expression with global"},
            {"def f(n:int) { var s:int = 0; for i = 0, i < n, 1 in s = s + (i * 2); s }", "loop with arithmetic body"},
            {"def f(a:int) { var r:int = a; r = r * 2; r = r + 1; r }", "sequential local assignments"},
            {"def f(s:string) { s + s }", "string param self concat"},
            {"def f(a:double, b:double) { if a < b then a else b } def g() { f(1.0, 2.0) }", "double call with comparison"},
            {"def outer(n:int) { var s:int = 0; for i = 0, i < n, 1 in { var t:int = i * 2; s = s + t } s }", "loop with inner declaration"},
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
        // {"{ var g:int = 1; g } def f() { g }", "top-level block with scoped declaration"},
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

        // 嵌套定义：codegen 拒绝生成半成品 IR（与语言开关无关，后端一律不提供该能力）
        {
            std::vector<std::unique_ptr<FCExprAST>> nestedBody;
            nestedBody.emplace_back(std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("inner", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1)));
            nestedBody.emplace_back(std::make_unique<FCNumberExprAST>(2));
            auto outerFn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("nestedOuter", std::vector<VarDeclPtr>{}),
                std::make_unique<FCBlockExprAST>(std::move(nestedBody)));

            // 手工提供编译记录：本用例只验证 codegen 的拒绝分支
            CompiledProgram nestedProgram;
            auto nestedRecord = std::make_shared<CompiledFunction>();
            nestedRecord->frameSize = 0;
            nestedProgram.functions["nestedOuter"] = nestedRecord;

            FCCodegenContext nestedContext("NestedTest", nestedProgram);
            auto* generated = codegen(outerFn.get(), nestedContext);
            expect(generated == nullptr, "codegen - nested definition rejected");
        }
    }
    // ==================== Codegen Error Tests ====================
    void ComprehensiveTestSuite::runCodegenErrorTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "CODEGEN ERROR TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        std::vector<std::pair<std::string, std::string>> errorCases = {
            {"1 = 2", "assignment to non-variable"},
            {"def f() { unknownfn() }", "function body call to unknown function"},
            {"def f(a:int) { a } f(1, 2)", "argument count mismatch"},
            {"def f(a:int) { a } f(\"s\")", "call argument type mismatch"},
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
            {"def f(a:int) { a } f(unknownfn())", "call argument codegen error"},
            {"def f() { if 1 then unknownfn() else 2 }", "if then branch error"},
            {"def f() { if 1 then 2 else unknownfn() }", "if else branch error"},
            {"def f() { for i = 0, i < 3, 1 in unknownfn() }", "for body error"},
            {"def f() { unknownfn() } 1", "program function codegen error"},
            {"def empty() {}", "empty function body"},
            // ---- 新增 ----
            {"def f(a:string, b:string) { a < b }", "string comparison"},
            {"def f() { 1.0 = 2.0 }", "assignment to float literal"},
            {"def f() { var x:int = 1; for i = 0, i < 3, 1 in x = \"s\" }", "assignment mismatch in loop body"},
            {"def f(a:int) { a } f(1, 2, 3)", "too many arguments"},
            {"def f() { g(1) } g(1)", "call to undefined top-level function"},
            {"def f() { if 1.5 then 1 else \"x\" }", "float condition with string branch"},
            {"def f() { var x:int = 1; x = x + \"s\"; x }", "assignment with mismatched rhs"},
            {"def f() { for i = 0, i < 3, 1 in 1.0 = 2.0 }", "assignment to literal in loop body"},
            {"def f() { undeclared }", "function references undeclared variable"}
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
            expect(codegen(proto.get(), cc) == nullptr,
                   "codegen-error - codegen standalone prototype returns null");

            auto emptyBody = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("empty", std::vector<VarDeclPtr>{}),
                nullptr);
            expect(codegen(emptyBody.get(), cc) == nullptr,
                   "codegen-error - codegen function with null body");

            auto nullBody2 = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("empty2", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(1));
            (void)nullBody2;
        }
    }
    // ==================== Codegen API Tests ====================
    void ComprehensiveTestSuite::runCodegenApiTests()
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
                FCBinaryOp::Add, std::make_unique<FCNumberExprAST>(1), std::make_unique<FCNumberExprAST>(2));
            auto* bv = codegen(bin.get(), cc);
            expect(bv != nullptr, "codegen-api - binary add");

            auto cmp = std::make_unique<FCBinaryExprAST>(
                FCBinaryOp::Less, std::make_unique<FCNumberExprAST>(1), std::make_unique<FCNumberExprAST>(2));
            auto* cv = codegen(cmp.get(), cc);
            expect(cv != nullptr, "codegen-api - binary compare");

            // createEntryBlockAlloca
            auto* alloca = cc.createEntryBlockAlloca(fn, "tmp", cc.getType("int"));
            expect(alloca != nullptr && alloca->getAllocatedType()->isIntegerTy(),
                "codegen-api - createEntryBlockAlloca");

            // 局部变量分配 + 读取 + 赋值
            auto decl = std::make_shared<VarDecl>("loc", "int");
            auto sym = std::make_shared<VariableSymbol>(decl, 0, 1);
            auto locSymbol = sym;
            auto* locAlloca = cc.createEntryBlockAlloca(fn, "loc", cc.getType("int"));
            cc.namedValues[locSymbol] = locAlloca;

            auto var = std::make_unique<FCVariableExprAST>(std::string("loc"));
            cc.semanticBinding().bind(var.get(), sym);
            auto* vv = codegen(var.get(), cc);
            expect(vv != nullptr && vv->getType()->isIntegerTy(), "codegen-api - local variable load");

            auto assignLHS = std::make_unique<FCVariableExprAST>(std::string("loc"));
            cc.semanticBinding().bind(assignLHS.get(), sym);
            auto assign = std::make_unique<FCBinaryExprAST>(
                FCBinaryOp::Assign, std::move(assignLHS), std::make_unique<FCNumberExprAST>(5));
            auto* av = codegen(assign.get(), cc);
            expect(av != nullptr, "codegen-api - local assignment");

            cc.builder.CreateRet(av ? av : bv);
            expect(!llvm::verifyFunction(*fn, &llvm::errs()), "codegen-api - function verifies");
        }

        // 程序级入口：顶层裸表达式也生成 __fc_main
        {
            FCCodegenContext cc("TopLevelApi", {});
            auto number = std::make_unique<FCNumberExprAST>(7);
            auto* entry = codegenTopLevel(number.get(), cc);
            auto* mainFn = cc.module->getFunction("__fc_main");
            expect(entry != nullptr && entry == mainFn,
                "codegen-api - top-level expression creates __fc_main");

            const bool wellFormed = mainFn != nullptr && mainFn->size() == 1 &&
                mainFn->getEntryBlock().getTerminator() != nullptr;
            expect(wellFormed, "codegen-api - top-level entry block terminated");
            if (wellFormed)
            {
                auto* ret = llvm::dyn_cast<llvm::ReturnInst>(mainFn->getEntryBlock().getTerminator());
                auto* value = ret != nullptr ? llvm::dyn_cast<llvm::ConstantInt>(ret->getReturnValue()) : nullptr;
                expect(value != nullptr && value->getValue() == 7,
                    "codegen-api - top-level constant returned");
            }
        }

        // 直接 codegen 一个函数（原型不再独立代码生成）
        {
            FCCodegenContext cc("ReuseApi", {});
            std::vector<VarDeclPtr> fnArgs;
            fnArgs.push_back(std::make_shared<VarDecl>("x", "int"));
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("reuse", std::move(fnArgs)),
                std::make_unique<FCNumberExprAST>(1));
            expect(codegen(fn.get(), cc) != nullptr, "codegen-api - function declares and defines");
            expect(cc.module->getFunction("reuse") != nullptr, "codegen-api - declared function present");

            // 同一个函数对象重复 codegen：直接复用已有实现
            auto fn2 = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("twice", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(2));
            expect(codegen(fn2.get(), cc) != nullptr, "codegen-api - first codegen of function");
            expect(codegen(fn2.get(), cc) != nullptr, "codegen-api - second codegen reuses existing");
            expect(cc.module->getFunction("twice") != nullptr, "codegen-api - twice function present");
        }
    }
