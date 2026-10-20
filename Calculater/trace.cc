#include "trace.h"

#include <cstdlib>
#include <string>
#include <string_view>

namespace FC {
namespace {

// 首次查询时读取环境变量并缓存，避免每个 Token 都扫描环境块。
std::string& traceStages() {
	static std::string stages = [] {
		const char* value = std::getenv("FC_TRACE");
		return value == nullptr ? std::string{} : std::string{ value };
	}();
	return stages;
}

// stage 是否出现在逗号分隔的阶段列表中（允许空白与空项）
bool containsStage(const std::string& stages, const char* stage) {
	if (stages == "all")
		return true;

	const std::string_view target{ stage };
	size_t begin = 0;
	while (begin <= stages.size()) {
		size_t end = stages.find(',', begin);
		if (end == std::string::npos)
			end = stages.size();

		std::string_view item{ stages.data() + begin, end - begin };
		while (!item.empty() && (item.front() == ' ' || item.front() == '\t'))
			item.remove_prefix(1);
		while (!item.empty() && (item.back() == ' ' || item.back() == '\t'))
			item.remove_suffix(1);

		if (item == target)
			return true;
		if (end == stages.size())
			break;
		begin = end + 1;
	}
	return false;
}

} // namespace

bool traceEnabled(const char* stage) {
	return containsStage(traceStages(), stage);
}

void setTraceStagesForTesting(const std::string& stages) {
	traceStages() = stages;
}

} // namespace FC
