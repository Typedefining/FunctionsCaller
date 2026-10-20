#ifndef FC_AST_DUMP_H
#define FC_AST_DUMP_H

#include <string>

namespace FCExprClass {
struct FCExprAST;
} // namespace FCExprClass

namespace FCMarks {

// 纯语法 AST 的文本形式：缩进树，每层两个空格，行尾带换行。
// 只读取 AST，不访问符号表、作用域或帧布局。
std::string describeAst(const FCExprClass::FCExprAST &node);

} // namespace FCMarks

#endif // FC_AST_DUMP_H
