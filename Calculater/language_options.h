#pragma once

namespace FCMarks {

// 语言特性开关：是否允许在函数体内定义函数。
//
// 关闭（默认）：语义期拒绝嵌套 def，保持“同时至多一个函数帧”的不变式。
// 打开：语义期允许并登记嵌套 def，但不提供执行能力——被定义函数不得捕获
//       外层局部，求值器与 codegen 一律拒绝。
constexpr bool kNestedFunctionDefinitions =
#if defined(FC_ENABLE_NESTED_FUNCTIONS)
    true;
#else
    false;
#endif

} // namespace FCMarks
