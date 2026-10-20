#pragma once

#include <string>

namespace FC {

// 诊断输出开关，用于代码阅读与排障，不改变任何语义。
// 通过环境变量 FC_TRACE 打开：取值为阶段名（lexer/parser/semantic/
// evaluator/codegen），多个阶段用逗号分隔，all 表示全部打开。
// 例：$env:FC_TRACE = "lexer"；$env:FC_TRACE = "lexer,parser"
bool traceEnabled(const char* stage);

// 直接覆盖启用阶段，忽略环境变量。测试与需要在进程内切换阶段的工具使用。
void setTraceStagesForTesting(const std::string& stages);

} // namespace FC
