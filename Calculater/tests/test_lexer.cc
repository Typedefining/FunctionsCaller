#include "test_harness.h"

    // ==================== Lexer Tests ====================
    // 独立词法单元测试：只实例化 FC::Lexer，不依赖 Parser/Semantic
    void ComprehensiveTestSuite::runLexerTests()
    {
        std::cout << "\n" << std::string(60, '=') << "\n";
        std::cout << "LEXER TESTS\n";
        std::cout << std::string(60, '=') << "\n";

        // 关键字与标识符
        {
            FC::Lexer lexer("def if then else for in var abc123");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_def), "lexer - def keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_if), "lexer - if keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_then), "lexer - then keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_else), "lexer - else keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_for), "lexer - for keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_in), "lexer - in keyword");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_var), "lexer - var keyword");

            auto id = lexer.nextToken();
            expect(id.kind == static_cast<int>(FCToken::tok_identifier) && id.lexeme == "abc123",
                   "lexer - identifier lexeme");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_eof),
                   "lexer - reaches eof after tokens");
            expect(lexer.isAtEnd(), "lexer - isAtEnd at eof");
        }

        // 数字：整数与浮点数 lexeme
        {
            FC::Lexer lexer("123 3.14");
            auto i = lexer.nextToken();
            expect(i.kind == static_cast<int>(FCToken::tok_number) && i.lexeme == "123", "lexer - integer lexeme");
            auto f = lexer.nextToken();
            expect(f.kind == static_cast<int>(FCToken::tok_number) && f.lexeme == "3.14", "lexer - float lexeme");
        }

        // 字符串
        {
            FC::Lexer lexer("\"hello world\"");
            auto s = lexer.nextToken();
            expect(s.kind == static_cast<int>(FCToken::tok_string) && s.lexeme == "hello world",
                   "lexer - string lexeme");
        }

        // 未闭合字符串是词法错误：返回 tok_error，lexeme 携带错误说明
        {
            FC::Lexer lexer("\"unterminated");
            auto err = lexer.nextToken();
            expect(err.kind == static_cast<int>(FCToken::tok_error),
                   "lexer - unterminated string yields tok_error");
            expect(err.lexeme == "unterminated string literal",
                   "lexer - unterminated string carries message");
            expect(!lexer.isAtEnd(), "lexer - lexer error is not eof");
        }

        // 未识别字符同样是词法错误，lexeme 指出具体字符
        {
            FC::Lexer lexer("@");
            auto err = lexer.nextToken();
            expect(err.kind == static_cast<int>(FCToken::tok_error),
                   "lexer - unexpected character yields tok_error");
            expect(err.lexeme == "unexpected character '@'",
                   "lexer - unexpected character carries message");
        }

        // 注释被跳过，注释后继续返回 token
        {
            FC::Lexer lexer("# comment only\n42");
            auto t = lexer.nextToken();
            expect(t.kind == static_cast<int>(FCToken::tok_number) && t.lexeme == "42",
                   "lexer - comment skipped");
        }

        // 行号/列号追踪：位置是 Token 第一个字符所在的行列（从 1 开始）
        {
            FC::Lexer lexer("a\n  bc");
            auto first = lexer.nextToken();
            expect(first.line == 1 && first.column == 1 && first.lexeme == "a",
                   "lexer - first token at line 1 column 1");
            auto second = lexer.nextToken();
            expect(second.line == 2 && second.column == 3 && second.lexeme == "bc",
                   "lexer - line advances and column counts leading spaces");

            FC::Lexer spaced("1 + 2 * 3");
            expect(spaced.nextToken().column == 1, "lexer - column of first operand");
            expect(spaced.nextToken().column == 3, "lexer - column of plus");
            expect(spaced.nextToken().column == 5, "lexer - column of second operand");
            expect(spaced.nextToken().column == 7, "lexer - column of star");
            expect(spaced.nextToken().column == 9, "lexer - column of third operand");
            expect(spaced.nextToken().column == 10, "lexer - eof sits after last character");
        }

        // 标点与运算符：Token 只携带词法类别，运算符含义由 binaryOpOf(kind) 在解析层推得
        {
            FC::Lexer lexer("( ) { } , ; : = + - * / <");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_parenthes_open), "lexer - paren open");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_parenthes_close), "lexer - paren close");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_brace_open), "lexer - brace open");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_brace_close), "lexer - brace close");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_comma), "lexer - comma");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_semicolon), "lexer - semicolon");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_colon), "lexer - colon");

            auto assign = lexer.nextToken();
            expect(assign.kind == static_cast<int>(FCToken::tok_assign), "lexer - assign kind");
            expect(binaryOpOf(FCToken::tok_assign) == FCBinaryOp::Assign, "lexer - assign maps to Assign");

            auto plus = lexer.nextToken();
            expect(plus.kind == static_cast<int>(FCToken::tok_plus), "lexer - plus kind is enum");
            expect(binaryOpOf(FCToken::tok_plus) == FCBinaryOp::Add, "lexer - plus maps to Add");

            auto minus = lexer.nextToken();
            expect(minus.kind == static_cast<int>(FCToken::tok_minus), "lexer - minus kind is enum");
            expect(binaryOpOf(FCToken::tok_minus) == FCBinaryOp::Sub, "lexer - minus maps to Sub");

            auto star = lexer.nextToken();
            expect(star.kind == static_cast<int>(FCToken::tok_star), "lexer - star kind is enum");
            expect(binaryOpOf(FCToken::tok_star) == FCBinaryOp::Mul, "lexer - star maps to Mul");

            auto slash = lexer.nextToken();
            expect(slash.kind == static_cast<int>(FCToken::tok_slash), "lexer - slash kind is enum");
            expect(binaryOpOf(FCToken::tok_slash) == FCBinaryOp::Div, "lexer - slash maps to Div");

            auto less = lexer.nextToken();
            expect(less.kind == static_cast<int>(FCToken::tok_less), "lexer - less kind is enum");
            expect(binaryOpOf(FCToken::tok_less) == FCBinaryOp::Less, "lexer - less maps to Less");
        }

        // 诊断输出开关：按阶段匹配逗号分隔列表
        {
            FC::setTraceStagesForTesting("");
            expect(!FC::traceEnabled("lexer"), "trace - disabled when stages empty");

            FC::setTraceStagesForTesting("all");
            expect(FC::traceEnabled("lexer") && FC::traceEnabled("parser"),
                   "trace - all enables every stage");

            FC::setTraceStagesForTesting("lexer, parser ");
            expect(FC::traceEnabled("lexer"), "trace - first stage matched");
            expect(FC::traceEnabled("parser"), "trace - stage after comma matched despite spaces");
            expect(!FC::traceEnabled("semantic"), "trace - unlisted stage disabled");

            FC::setTraceStagesForTesting(",lexer,");
            expect(FC::traceEnabled("lexer"), "trace - empty list items are skipped");
            expect(!FC::traceEnabled("codegen"), "trace - empty list items do not match everything");

            // 打开 lexer 诊断输出：执行一次词法分析，输出内容不作为断言对象
            FC::setTraceStagesForTesting("lexer");
            FC::Lexer traced("1 + 2");
            expect(traced.nextToken().kind == static_cast<int>(FCToken::tok_number),
                   "trace - tracing does not change tokens");
            FC::setTraceStagesForTesting("");
        }

        // kind → 运算符映射：非运算符 kind 一律 Unknown
        {
            expect(binaryOpOf(FCToken::tok_identifier) == FCBinaryOp::Unknown, "lexer - identifier has no binary op");
            expect(binaryOpOf(FCToken::tok_eof) == FCBinaryOp::Unknown, "lexer - eof has no binary op");
            expect(binaryOpOf(FCToken::tok_semicolon) == FCBinaryOp::Unknown, "lexer - semicolon has no binary op");
        }

        // peek 不推进，current 返回最近消费的 token
        {
            FC::Lexer lexer("aa bb");
            auto first = lexer.nextToken();
            auto peeked = lexer.peekToken();
            expect(peeked.lexeme == "bb", "lexer - peek returns next token");
            expect(lexer.currentToken().lexeme == "aa", "lexer - current returns consumed token");
            auto consumed = lexer.nextToken();
            expect(consumed.lexeme == "bb" && consumed.kind == peeked.kind, "lexer - consume returns peeked token");
        }

        // peek 在游标副本上识别：重复 peek 稳定，且不移动主游标（跨注释、跨行）
        {
            FC::Lexer lexer("# comment\n42");
            auto peeked1 = lexer.peekToken();
            auto peeked2 = lexer.peekToken();
            expect(peeked1.kind == static_cast<int>(FCToken::tok_number) && peeked1.lexeme == "42",
                   "lexer - peek scans past comment without consuming");
            expect(peeked2.kind == peeked1.kind && peeked2.lexeme == peeked1.lexeme &&
                       peeked2.line == peeked1.line && peeked2.column == peeked1.column,
                   "lexer - repeated peek is stable");
            auto consumed = lexer.nextToken();
            expect(consumed.lexeme == "42" && consumed.line == 2 && consumed.column == 1,
                   "lexer - peek leaves cursor at same position");
        }

        // reset 复用实例
        {
            FC::Lexer lexer("xyz");
            lexer.nextToken();
            lexer.reset("q1");
            auto t = lexer.nextToken();
            expect(t.kind == static_cast<int>(FCToken::tok_identifier) && t.lexeme == "q1",
                   "lexer - reset reuses instance");
        }

        // 空输入立即 eof
        {
            FC::Lexer lexer("");
            expect(lexer.nextToken().kind == static_cast<int>(FCToken::tok_eof), "lexer - empty input eof");
            expect(lexer.isAtEnd(), "lexer - isAtEnd on empty");
        }
    }
