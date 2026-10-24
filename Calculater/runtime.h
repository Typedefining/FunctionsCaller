#pragma once

// 解释器运行时值表示。不依赖 token/ast/symbol（分层最底层）。
// FCValue 为值语义：判别式（std::variant 下标）是类别的唯一存放地，
// 字符串按值持有，无堆指针别名、无手工生命周期。
// 类别语义：monostate = Void（无值：空块/空序列/定义语句/默认构造），
// FCErrorValue = Error（求值失败哨兵，与诊断通道耦合）。

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
		Void,
		Error
	};

	// Error 类别的空载荷标签类型（variant 需要一个可判别的独立备选）
	struct FCErrorValue {};

	struct FCValue
	{
		std::variant<std::monostate, int, double, std::string, FCErrorValue> storage;

		FCValueCategory category() const {
			if (std::holds_alternative<int>(storage))
				return FCValueCategory::Integer;
			if (std::holds_alternative<double>(storage))
				return FCValueCategory::Floating;
			if (std::holds_alternative<std::string>(storage))
				return FCValueCategory::String;
			if (std::holds_alternative<FCErrorValue>(storage))
				return FCValueCategory::Error;
			return FCValueCategory::Void;
		}

		bool isVoid() const { return category() == FCValueCategory::Void; }
		bool isError() const { return category() == FCValueCategory::Error; }
		bool isValue() const {
			return category() != FCValueCategory::Void && category() != FCValueCategory::Error;
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
