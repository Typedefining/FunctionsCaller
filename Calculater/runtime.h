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

	union FCValueUnion
	{
		int intVal;
		double doubleVal;
		char charVal[2048];
		void* danglingVal;
	};

	struct FCValue
	{
		FCValueCategory type = FCValueCategory::Dangle;
		FCValueUnion evaluteVal{};
	};

	struct Frame
	{
		std::string funcName;
		std::vector<FCValue> locals;
	};
}
