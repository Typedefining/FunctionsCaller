#include "test_harness.h"
#include <string_view>

    // ==================== AST Info Tests ====================
    void ComprehensiveTestSuite::runAstInfoTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "AST INFO TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        try
        {
            expect(std::string_view(nodeKindName(FCNumberExprAST(42))) == "Number",
                   "ast-info - number node kind");
            expect(std::string_view(nodeKindName(FCNumberExprAST(3.14))) == "Number",
                   "ast-info - float number node kind");
            expect(std::string_view(nodeKindName(FCStringExprAST("hello"))) == "String",
                   "ast-info - string node kind");

            expect(std::string_view(nodeKindName(FCVariableExprAST(std::string("v")))) == "Variable",
                   "ast-info - variable node kind");
            expect(std::string_view(nodeKindName(*std::make_unique<FCBinaryExprAST>(
                FCBinaryOp::Add, std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCNumberExprAST>(2)))) == "Binary",
                   "ast-info - binary node kind");

            std::vector<std::unique_ptr<FCExprAST>> args;
            args.push_back(std::make_unique<FCNumberExprAST>(1));
            expect(std::string_view(nodeKindName(*std::make_unique<FCCallExprAST>("add", std::move(args)))) == "Call",
                   "ast-info - call node kind");

            std::vector<VarDeclPtr> protoArgs;
            protoArgs.push_back(std::make_shared<VarDecl>("a", "int"));
            auto proto = std::make_unique<FCPrototypeAST>("f", std::move(protoArgs));
            expect(std::string_view(nodeKindName(*proto)) == "Prototype",
                   "ast-info - prototype node kind");

            auto fn = std::make_unique<FCFunctionAST>(
                std::move(proto), std::make_unique<FCNumberExprAST>(1));
            expect(std::string_view(nodeKindName(*fn)) == "Function",
                   "ast-info - function node kind");

            expect(std::string_view(nodeKindName(*std::make_unique<FCIfExprAST>(
                std::make_unique<FCNumberExprAST>(1),
                std::make_unique<FCNumberExprAST>(10),
                std::make_unique<FCNumberExprAST>(20)))) == "If",
                   "ast-info - if node kind");

            expect(std::string_view(nodeKindName(*std::make_unique<FCForExprAST>(
                std::make_shared<VarDecl>("i", "int"),
                std::make_unique<FCNumberExprAST>(0),
                std::make_unique<FCNumberExprAST>(10),
                nullptr,
                std::make_unique<FCNumberExprAST>(1)))) == "For",
                   "ast-info - for node kind");

            expect(std::string_view(nodeKindName(*std::make_unique<FCVarDeclExprAST>(
                std::make_shared<VarDecl>("x", "int"),
                std::make_unique<FCNumberExprAST>(5)))) == "VarDecl",
                   "ast-info - vardecl node kind");

            std::vector<std::unique_ptr<FCExprAST>> seq;
            seq.push_back(std::make_unique<FCNumberExprAST>(1));
            seq.push_back(std::make_unique<FCNumberExprAST>(2));
            expect(std::string_view(nodeKindName(*std::make_unique<FCSeqExprAST>(std::move(seq)))) == "Seq",
                   "ast-info - seq node kind");

            std::vector<std::unique_ptr<FCExprAST>> blk;
            blk.push_back(std::make_unique<FCNumberExprAST>(1));
            expect(std::string_view(nodeKindName(*std::make_unique<FCBlockExprAST>(std::move(blk)))) == "Block",
                   "ast-info - block node kind");

            std::vector<std::unique_ptr<FCExprAST>> progStmts;
            progStmts.push_back(std::make_unique<FCNumberExprAST>(1));
            expect(std::string_view(nodeKindName(*std::make_unique<FCProgramAST>(std::move(progStmts)))) == "Program",
                   "ast-info - program node kind");

            expect(true, "ast-info - construct and check node kind for all node types");
        }
        catch (...)
        {
            expect(false, "ast-info - exception while checking AST node kinds");
        }
    }
    // ==================== AST Dump Tests ====================
    void ComprehensiveTestSuite::runAstDumpTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "AST DUMP TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        auto dumpOf = [](const std::string &code) {
            FCScanner scanner;
            auto ast = scanner.analysis(code);
            return ast ? describeAst(*ast) : std::string("<error>");
        };

        expect(dumpOf("1 + 2 * 3") == "Binary +\n"
                                      "  Number 1\n"
                                      "  Binary *\n"
                                      "    Number 2\n"
                                      "    Number 3\n",
               "ast-dump - precedence tree");

        expect(dumpOf("(1 + 2) * 3") == "Binary *\n"
                                        "  Binary +\n"
                                        "    Number 1\n"
                                        "    Number 2\n"
                                        "  Number 3\n",
               "ast-dump - parentheses override precedence");

        expect(dumpOf("var a:int = 1; a") == "Seq (2 expressions)\n"
                                             "  VarDecl a:int\n"
                                             "    Number 1\n"
                                             "  Variable a\n",
               "ast-dump - declaration and reference");

        expect(dumpOf("def add(a:int, b:int) { a + b }") ==
                   "Function add(a:int, b:int)\n"
                   "  Block (1 expressions)\n"
                   "    Binary +\n"
                   "      Variable a\n"
                   "      Variable b\n",
               "ast-dump - function with parameters");

        expect(dumpOf("def pick(a:int, b:int) { if a < b then a else b }") ==
                   "Function pick(a:int, b:int)\n"
                   "  Block (1 expressions)\n"
                   "    If\n"
                   "      Binary <\n"
                   "        Variable a\n"
                   "        Variable b\n"
                   "      Variable a\n"
                   "      Variable b\n",
               "ast-dump - if branches in order");
        expect(dumpOf("f(1, 2)") == "Call f\n  Number 1\n  Number 2\n",
               "ast-dump - call arguments");

        expect(dumpOf("missing(") == "<error>",
               "ast-dump - parse failure yields no tree");

        expect(dumpOf("1; 2") ==
                   "Seq (2 expressions)\n"
                   "  Number 1\n"
                   "  Number 2\n",
               "ast-dump - sequence for ';'");

        expect(dumpOf("def fib(n:int) { if n < 2 then n else fib(n - 1) + fib(n - 2) }") ==
                   "Function fib(n:int)\n"
                   "  Block (1 expressions)\n"
                   "    If\n"
                   "      Binary <\n"
                   "        Variable n\n"
                   "        Number 2\n"
                   "      Variable n\n"
                   "      Binary +\n"
                   "        Call fib\n"
                   "          Binary -\n"
                   "            Variable n\n"
                   "            Number 1\n"
                   "        Call fib\n"
                   "          Binary -\n"
                   "            Variable n\n"
                   "            Number 2\n",
               "ast-dump - nested calls compose with if");

        expect(dumpOf("for i = 0, i < 3, 1 in i") ==
                   "For i:int\n"
                   "  Number 0\n"
                   "  Binary <\n"
                   "    Variable i\n"
                   "    Number 3\n"
                   "  Number 1\n"
                   "  Variable i\n",
               "ast-dump - for with step");

        expect(dumpOf("for i = 0, i < 3 in i") ==
                   "For i:int\n"
                   "  Number 0\n"
                   "  Binary <\n"
                   "    Variable i\n"
                   "    Number 3\n"
                   "  Variable i\n",
               "ast-dump - for without step");

        expect(dumpOf("\"hi\"") == "String \"hi\"\n",
               "ast-dump - string literal");

        expect(dumpOf("1.5") == "Number 1.5\n",
               "ast-dump - floating literal");

        expect(dumpOf("def f() { 1 } def g() { 2 }") ==
                   "Program (2 statements)\n"
                   "  Function f()\n"
                   "    Block (1 expressions)\n"
                   "      Number 1\n"
                   "  Function g()\n"
                   "    Block (1 expressions)\n"
                   "      Number 2\n",
               "ast-dump - program with two definitions");

        {
            FCPrototypeAST proto("g", {std::make_shared<VarDecl>("x", "int")});
            expect(describeAst(proto) == "Prototype g(x:int)\n",
                   "ast-dump - prototype");
        }
        {
            struct BareExprAST : FCExprAST {
            };
            BareExprAST bare;
            expect(describeAst(bare) == "Expr\n",
                   "ast-dump - unspecialized node");
        }

        expect(dumpOf("def half(x:int) { x = x / 2 }") ==
                   "Function half(x:int)\n"
                   "  Block (1 expressions)\n"
                   "    Binary =\n"
                   "      Variable x\n"
                   "      Binary /\n"
                   "        Variable x\n"
                   "        Number 2\n",
               "ast-dump - assignment and division operators");

        {
            FCBinaryExprAST unknown(FCBinaryOp::Unknown,
                                    std::make_unique<FCNumberExprAST>(1),
                                    std::make_unique<FCNumberExprAST>(2));
            expect(describeAst(unknown) == "Binary ?\n  Number 1\n  Number 2\n",
                   "ast-dump - unknown operator falls back to '?'");
        }

        FC::setTraceStagesForTesting("ast");
        {
            FCScanner scanner;
            auto ast = scanner.analysis("1 + 2 * 3");
            expect(ast != nullptr, "ast-dump - FC_TRACE=ast keeps analysis working");
        }
        FC::setTraceStagesForTesting("");
    }
    // ==================== Token / AST API Tests ====================
    void ComprehensiveTestSuite::runTokenApiTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "TOKEN / AST API TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 数字字面量
        {
            FCNumberExprAST ni(42);
            expect(ni.m_intVal == 42 && !ni.isFloating(), "token-api - int literal value");
            expect(std::string_view(nodeKindName(ni)) == "Number", "token-api - int literal type");
            FCNumberExprAST nd(3.5);
            expect(nd.m_doubleVal == 3.5 && nd.isFloating(), "token-api - double literal value");
        }

        // 字符串字面量
        {
            FCStringExprAST s("hello");
            expect(s.m_stringVal == "hello", "token-api - string value");
            expect(std::string_view(nodeKindName(s)) == "String", "token-api - string type");
        }

        // 变量表达式
        {
            auto decl = std::make_shared<VarDecl>("v", "double");
            FCVariableExprAST v(std::string("v"));
            expect(v.name == decl->name, "token-api - variable name");
            expect(std::string_view(nodeKindName(v)) == "Variable", "token-api - variable type");
            expect(decl->name == "v" && decl->typeName == "double", "token-api - VarDecl fields");
        }

        // 二元表达式
        {
            auto bin = std::make_unique<FCBinaryExprAST>(
                FCBinaryOp::Mul, std::make_unique<FCNumberExprAST>(3), std::make_unique<FCNumberExprAST>(4));
            expect(bin->getOperator() == FCBinaryOp::Mul, "token-api - binary operator");
            expect(bin->getLHS() != nullptr && bin->getRHS() != nullptr, "token-api - binary operands");
            expect(std::string_view(nodeKindName(*bin)) == "Binary", "token-api - binary type");
        }

        // 调用表达式
        {
            std::vector<std::unique_ptr<FCExprAST>> args;
            args.push_back(std::make_unique<FCNumberExprAST>(1));
            args.push_back(std::make_unique<FCNumberExprAST>(2));
            auto call = std::make_unique<FCCallExprAST>("foo", std::move(args));
            expect(call->getName() == "foo", "token-api - call callee");
            expect(call->getArgs().size() == 2, "token-api - call args count");
            expect(std::string_view(nodeKindName(*call)) == "Call", "token-api - call type");
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
            expect(std::string_view(nodeKindName(*proto)) == "Prototype", "token-api - prototype type");
        }

        // 函数
        {
            auto fn = std::make_unique<FCFunctionAST>(
                std::make_unique<FCPrototypeAST>("fn", std::vector<VarDeclPtr>{}),
                std::make_unique<FCNumberExprAST>(9));
            expect(fn->getProtoName() == "fn", "token-api - function proto name");
            expect(fn->getBody() != nullptr, "token-api - function body");
            expect(fn->getProto()->getProtoName() == "fn", "token-api - function getProto");
            expect(std::string_view(nodeKindName(*fn)) == "Function", "token-api - function type");
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

        // VariableSymbol / VariableRuntimeInfo 默认值
        {
            VariableSymbol sym;
            expect(sym.declaration == nullptr, "token-api - default symbol null decl");
            expect(sym.slot == -1, "token-api - default symbol slot -1");
            expect(sym.scopeDepth == 0 && sym.runtime.frameIndex == -1,
                "token-api - default symbol flags and runtime");

            auto decl = std::make_shared<VarDecl>("g", "int");
            VariableSymbol custom(decl, 4, 2);
            expect(custom.declaration == decl && custom.slot == 4 && custom.scopeDepth == 2,
                "token-api - custom symbol fields");
        }

        // CompiledProgram / CompiledFunction 默认值
        {
            CompiledProgram prog;
            expect(prog.globalSymbols.empty(), "token-api - compiled program empty globals");
            expect(prog.getFunction("none") == nullptr, "token-api - compiled program missing function");

            CompiledFunction cf;
            expect(cf.ast == nullptr && cf.frameSize == 0,
                "token-api - compiled function defaults");
            expect(cf.parametersIdx.empty(), "token-api - compiled function empty params");
        }

        // SemanticBinding 按节点指针查找
        {
            auto binding = std::make_shared<SemanticBinding>();
            auto node = std::make_unique<FCVariableExprAST>(std::string("x"));
            auto sym = std::make_shared<VariableSymbol>(std::make_shared<VarDecl>("x", "int"), 0, 0);

            expect(binding->find(node.get()) == nullptr, "token-api - binding miss before bind");
            expect(binding->bind(node.get(), sym), "token-api - binding bind");
            expect(binding->find(node.get()) == sym, "token-api - binding find by node");
            expect(binding->find(static_cast<const void*>(node.get())) == sym,
                "token-api - binding find by void pointer");
        }
    }