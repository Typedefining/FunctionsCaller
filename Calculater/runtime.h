#pragma once

// 解释器运行时值表示。不依赖 token/ast/symbol（分层最底层）。
// FCValue 为值语义：判别式（std::variant 下标）是类别的唯一存放地，
// 字符串按值持有，无堆指针别名、无手工生命周期。

#include <string>
#include <variant>
#include <vector>

namespace FCMarks
{
	enum struct FCValueCategory
	{
		Integer,
		Floating,
		String,
		Dangle
	};

	// monostate = Dangle（未定义/错误哨兵，语义与原实现一致）
	struct FCValue
	{
		std::variant<std::monostate, int, double, std::string> storage;

		FCValueCategory category() const {
			if (std::holds_alternative<int>(storage))
				return FCValueCategory::Integer;
			if (std::holds_alternative<double>(storage))
				return FCValueCategory::Floating;
			if (std::holds_alternative<std::string>(storage))
				return FCValueCategory::String;
			return FCValueCategory::Dangle;
		}

		// 类型安全读取：类别不匹配返回 nullptr（不抛异常）
		const int* asInteger() const { return std::get_if<int>(&storage); }
		const double* asFloating() const { return std::get_if<double>(&storage); }
		const std::string* asString() const { return std::get_if<std::string>(&storage); }
	};

	struct Frame
	{
		std::string funcName;
		std::vector<FCValue> locals;  // 按槽位存放值；全局帧与调用帧统一按 slot 下标访问
	};
}
