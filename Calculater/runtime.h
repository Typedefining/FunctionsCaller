#pragma once

#include <string>
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

	struct FCStringValue
	{
		std::string str;
	};

	union FCValueUnion
	{
		int intVal;
		double doubleVal;
		FCStringValue* charVal;
		void* danglingVal;
	};

	struct VariableRuntimeInfo {
		int frameIndex = -1;
	};

	struct FCValue
	{
		FCValueCategory type = FCValueCategory::Dangle;
		FCValueUnion evaluteVal{};
	};

	struct Frame
	{
		std::string funcName;
		std::vector<FCValue> locals;  // 按槽位存放值；全局帧与调用帧统一按 slot 下标访问
	};
}
