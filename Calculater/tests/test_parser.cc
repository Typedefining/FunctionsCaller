#include "test_harness.h"

    // ==================== Parser Tests ====================
    // 独立语法单元测试：只组合 Lexer -> Parser，产出纯语法 AST，
    // 不触及 FCSemanticContext / SemanticBinding / frameIndex
    void ComprehensiveTestSuite::runParserTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "PARSER TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 算术优先级构建正确的子树：x + 1 * 2 -> x + (1 * 2)
        {
            FC::Lexer lexer("x + 1 * 2");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - precedence expression parses");
            auto* bin = dynamic_cast<FCBinaryExprAST*>(ast.get());
            expect(bin != nullptr && bin->getOperator() == FCBinaryOp::Add,
                   "parser - root is add");
            auto* rhs = dynamic_cast<const FCBinaryExprAST*>(bin->getRHS());
            expect(rhs != nullptr && rhs->getOperator() == FCBinaryOp::Mul,
                   "parser - mul binds tighter than add");
            auto* lhs = dynamic_cast<const FCVariableExprAST*>(bin->getLHS());
            expect(lhs != nullptr && lhs->name == "x",
                   "parser - variable node keeps name only");
        }

        // 变量引用是纯语法节点：不含符号，语义绑定留给 SemanticAnalyzer
        {
            FC::Lexer lexer("y");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - bare variable parses");
            auto* var = dynamic_cast<FCVariableExprAST*>(ast.get());
            expect(var != nullptr && var->name == "y",
                   "parser - variable node is pure syntax");
        }

        // 构造不读取 token：解析阶段开始前不持有 token，首 token 不被吞掉
        {
            FC::Lexer lexer("1");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            expect(!parser.started(), "parser - constructor stays outside parse phase");
            auto ast = parser.parseProgram();
            expect(dynamic_cast<FCNumberExprAST*>(ast.get()) != nullptr,
                   "parser - constructor does not consume the first token");
            expect(parser.started(), "parser - parseProgram enters parse phase");
        }

        // reset() 复位解析阶段与错误标记，同一实例可解析第二段输入
        {
            FC::Lexer lexer("1");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            expect(parser.parseProgram() != nullptr, "parser - first program parses");

            lexer.reset("2");
            parser.reset();
            expect(!parser.started() && !parser.hadError(),
                   "parser - reset clears phase and error flag");
            auto second = parser.parseProgram();
            auto* number = dynamic_cast<FCNumberExprAST*>(second.get());
            expect(number != nullptr, "parser - second program parses after reset");
        }

        // 函数定义：原型 + 块体，无语义副作用
        {
            FC::Lexer lexer("def add(a:int, b:int) { a + b }");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - function definition parses");
            auto* fn = dynamic_cast<FCFunctionAST*>(ast.get());
            expect(fn != nullptr && fn->getProtoName() == "add",
                   "parser - function proto name");
            expect(fn->getProto()->getArgs().size() == 2,
                   "parser - proto arg count");
            auto* body = dynamic_cast<const FCBlockExprAST*>(fn->getBody());
            expect(body != nullptr && body->getExpressions().size() == 1,
                   "parser - block body parsed");
        }

        // for 语法结构完整（start/end/step/body）
        {
            FC::Lexer lexer("for i = 0, i < 10, 1 in i");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - for expression parses");
            auto* loop = dynamic_cast<FCForExprAST*>(ast.get());
            expect(loop != nullptr && loop->getDecl() && loop->getDecl()->name == "i",
                   "parser - for variable decl");
            expect(loop->getStart() && loop->getEnd() && loop->getStep() && loop->getBody(),
                   "parser - for parts present");
        }

        // 语法错误路径：hadError 标记 + 返回 null（不接触语义）
        {
            FC::Lexer lexer("1 +");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr || parser.hadError(),
                   "parser - dangling operator flags error");
        }

        {
            FC::Lexer lexer("if 1 then 2");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr || parser.hadError(),
                   "parser - if missing else flags error");
        }

        // 多语句输入：顶层分号折叠为一个 Seq（单条时不额外包 Program）
        {
            FC::Lexer lexer("var a:int = 1; a + 1");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - multi statement program parses");
            auto* seq = dynamic_cast<FCSeqExprAST*>(ast.get());
            expect(seq != nullptr && seq->getExpressions().size() == 2,
                   "parser - top-level folds into sequence");
        }

        // 多条独立语句（含函数定义）才包装 FCProgramAST
        {
            FC::Lexer lexer("def f() { 1 } 2");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - program with function parses");
            auto* program = dynamic_cast<FCProgramAST*>(ast.get());
            expect(program != nullptr && program->getStatements().size() == 2,
                   "parser - program wraps function and expr");
        }

        // ---- 分隔符统一规则（startsExpression 终止 + 结构性 token 归属制）----
        // else 终止括号序列，由 if 消费：if c then (a;b) else d 正确拆分
        {
            FC::Lexer lexer("if 1 then (2; 3) else 4");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - else terminates paren seq");
            auto* cond = dynamic_cast<FCIfExprAST*>(ast.get());
            expect(cond != nullptr && cond->getElse() != nullptr,
                   "parser - if consumes else after paren seq");
            auto* seq = dynamic_cast<const FCSeqExprAST*>(cond->getThen());
            expect(seq != nullptr && seq->getExpressions().size() == 2,
                   "parser - paren seq holds two exprs before else");
        }

        // '}' 终止顶层序列，块由自己消费：{ a; b } 作为 if 分支
        {
            FC::Lexer lexer("if 1 then { 2; 3 } else 4");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - brace terminates seq");
            auto* cond = dynamic_cast<FCIfExprAST*>(ast.get());
            expect(cond != nullptr, "parser - if consumes branches around block");
            auto* blk = dynamic_cast<const FCBlockExprAST*>(cond->getThen());
            expect(blk != nullptr && blk->getExpressions().size() == 2,
                   "parser - block consumes its brace-terminated items");
        }

        // ')' 终止括号序列，由括号自己消费
        {
            FC::Lexer lexer("(1; 2) + 3");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - paren close terminates seq");
            auto* add = dynamic_cast<FCBinaryExprAST*>(ast.get());
            expect(add != nullptr && add->getOperator() == FCBinaryOp::Add,
                   "parser - seq in parens binds as add operand");
        }

        // def 终止顶层序列：表达式 def 表达式 三语句程序
        {
            FC::Lexer lexer("1; def f() { 2 } 3");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - def terminates top-level seq");
            auto* program = dynamic_cast<FCProgramAST*>(ast.get());
            expect(program != nullptr && program->getStatements().size() == 3,
                   "parser - program alternates exprs and defs");
        }

        // 顶层裸 ';' 非法：空语句一律报错
        {
            FC::Lexer lexer("; 1");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr && parser.hadError(),
                   "parser - top-level bare semicolon is an error");
        }

        // 块内 "x;" 收尾合法；"(a;)" 括号内末尾分号同样合法
        {
            FC::Lexer lexer("def f() { 1; }");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast != nullptr, "parser - block trailing semicolon legal");
        }

        // 新增错误路径：块内裸 ';' / 未闭合块
        {
            FC::Lexer lexer("def f() { 1; ; 2 }");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr && parser.hadError(),
                   "parser - bare semicolon inside block is an error");
        }
        {
            FC::Lexer lexer("def f() { 1");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr && parser.hadError(),
                   "parser - unterminated block is an error");
        }

        // 畸形数字字面量不得抛异常逃出解析阶段：应产出诊断并返回 null
        {
            FC::Lexer lexer(".");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr && parser.hadError(),
                   "parser - dot literal flags error instead of throwing");
            expect(!diagnostics.empty(),
                   "parser - dot literal records a diagnostic");
            expect(diagnostics[0].stage == "parser",
                   "parser - diagnostic stage is parser");
        }
        {
            FC::Lexer lexer("99999999999999999999");
            FC::FCDiagnostics diagnostics;
            FC::Parser parser(lexer, diagnostics);
            auto ast = parser.parseProgram();
            expect(ast == nullptr && parser.hadError(),
                   "parser - overflow integer literal flags error instead of throwing");
            expect(!diagnostics.empty(),
                   "parser - overflow integer records a diagnostic");
        }
    }
