#pragma once

#include <string>

namespace FCMarks {

struct CompiledProgram;

// 语义分析结果的文本形式：每个函数帧的帧大小、参数槽位，以及符号的
// 槽位、声明深度与运行时帧下标。用于代码阅读时的中间状态观察。
std::string describeCompiledProgram(const CompiledProgram &program);

} // namespace FCMarks
