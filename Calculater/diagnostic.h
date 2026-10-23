#pragma once

#include <string>
#include <vector>

namespace FC {

/// 源位置：0 = 未知（诊断未携带位置时的占位值）
struct FCSourceLocation {
	int line = 0;
	int column = 0;
};

/// 单条结构化诊断：阶段 + 位置 + 消息文本
struct FCDiagnostic {
	std::string stage;    // "parser" / "semantic" / "evaluator" / "codegen"
	FCSourceLocation location;
	std::string message;
};

/// 诊断收集通道：各编译阶段写入，调用方读取。
/// 头文件内联实现，供 Parser/Scanner 及后续阶段共用。
class FCDiagnostics {
public:
	void add(std::string stage, FCSourceLocation location, std::string message) {
		m_items.push_back(FCDiagnostic{std::move(stage), location, std::move(message)});
	}

	bool empty() const {
		return m_items.empty();
	}

	void clear() {
		m_items.clear();
	}

	const std::vector<FCDiagnostic>& items() const {
		return m_items;
	}

	// 下标访问：测试与调用方按序读取诊断（等价于 items()[index]）。
	const FCDiagnostic& operator[](std::size_t index) const {
		return m_items[index];
	}

private:
	std::vector<FCDiagnostic> m_items;
};

/// 确切输出格式（测试以此固定）：
/// 有位置：  "error[parser] 1:5: <message>"
/// 无位置：  "error[parser]: <message>"
inline std::string formatDiagnostic(const FCDiagnostic& diagnostic) {
	std::string result = "error[" + diagnostic.stage + "]";
	if (diagnostic.location.line != 0 || diagnostic.location.column != 0) {
		result += " " + std::to_string(diagnostic.location.line)
			+ ":" + std::to_string(diagnostic.location.column);
	}
	result += ": " + diagnostic.message;
	return result;
}

} // namespace FC
