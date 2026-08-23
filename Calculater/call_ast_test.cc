#include "scanner.h"

#include <iostream>

int main()
{
	FCScanner scanner;
	auto program = scanner.analysis("def add(a:int b:int) a+b\nadd(2,3);");
	if (!program)
	{
		std::cerr << "failed to parse function definition and call\n";
		return 1;
	}

	FCExprClass::FCEvaluationContext context;
	const auto result = program->evaluate(context);
	if (result.type != FCMarks::FCValueCategory::Integer || result.evaluteVal.intVal != 5)
	{
		std::cerr << "unexpected result from function call\n";
		return 2;
	}

	if (context.functions.findFunction("add") == nullptr)
	{
		std::cerr << "function was not indexed\n";
		return 3;
	}

	return 0;
}
