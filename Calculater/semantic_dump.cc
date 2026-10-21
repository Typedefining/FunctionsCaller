#include "semantic_dump.h"

#include "token.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <vector>

namespace FCMarks {

namespace {

// <global> 记录与函数记录同构存放，输出时需要区分其身份
const char *frameKind(const std::string &name) {
	return name == "<global>" ? "global" : "function";
}

} // namespace

std::string describeCompiledProgram(const CompiledProgram &program) {
	std::vector<std::string> names;
	names.reserve(program.functions.size());
	for (const auto &entry : program.functions)
		names.push_back(entry.first);

	// 固定输出顺序：<global> 在最前，其余按函数名排序
	std::sort(names.begin(), names.end(),
	          [](const std::string &lhs, const std::string &rhs) {
		          if ((lhs == "<global>") != (rhs == "<global>"))
			          return lhs == "<global>";
		          return lhs < rhs;
	          });

	std::ostringstream out;
	out << "CompiledProgram\n";

	for (const auto &name : names) {
		const auto &record = *program.functions.at(name);
		out << "  frame " << name << " (" << frameKind(name) << ")"
		    << " frameSize=" << record.frameSize
		    << " maxTempSlots=" << record.maxTempSlots;
		if (!record.parametersIdx.empty()) {
			out << " params=[";
			for (size_t i = 0; i < record.parametersIdx.size(); ++i) {
				if (i != 0)
					out << ", ";
				out << record.parametersIdx[i];
			}
			out << "]";
		}
		out << "\n";

		std::vector<std::shared_ptr<VariableSymbol>> symbols;
		symbols.reserve(record.symbols.size());
		// 符号表只登记已构造完成的符号；空项与缺声明的项不参与输出
		for (const auto &entry : record.symbols.getAllSymbols()) {
			if (entry.second && entry.second->declaration)
				symbols.push_back(entry.second);
		}
		std::sort(symbols.begin(), symbols.end(),
		          [](const std::shared_ptr<VariableSymbol> &lhs,
		             const std::shared_ptr<VariableSymbol> &rhs) {
			          return lhs->slot < rhs->slot;
		          });

		for (const auto &symbol : symbols) {
			out << "    " << symbol->declaration->name << ":"
			    << symbol->declaration->typeName << " slot=" << symbol->slot
			    << " depth=" << symbol->scopeDepth
			    << " frameIndex=" << symbol->runtime.frameIndex << "\n";
		}
	}

	out << "  bindings=" << program.m_binding->size() << "\n";
	return out.str();
}

} // namespace FCMarks
