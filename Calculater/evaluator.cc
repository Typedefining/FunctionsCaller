#include "evaluator.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace FCExprClass;
using namespace FCMarks;

namespace
{
FCValue makeDangleValue()
{
	FCValue value;
	value.type = FCValueCategory::Dangle;
	value.evaluteVal.danglingVal = nullptr;
	return value;
}

FCValue makeStringValue(const std::string& text)
{
	FCValue value;
	value.type = FCValueCategory::String;
	std::memset(value.evaluteVal.charVal, 0, sizeof(value.evaluteVal.charVal));
	const auto length = std::min(text.size(), sizeof(value.evaluteVal.charVal) - 1);
	std::memcpy(value.evaluteVal.charVal, text.data(), length);
	return value;
}

FCValue evaluateExpression(const FCExprAST* expression,
	FCEvaluationContext& context);

FCValue evaluateBinary(const FCBinaryExprAST* expression,
	FCEvaluationContext& context)
{
	const auto op = expression->getOperator();

	if (op == '=')
	{
		auto* variable = dynamic_cast<const FCVariableExprAST*>(expression->getLHS());
		if (variable == nullptr || variable->decl == nullptr)
		{
			std::fprintf(stderr, "LogError: LHS of assignment must be a variable\n");
			return makeDangleValue();
		}

		const auto value = evaluateExpression(expression->getRHS(), context);
		if (value.type == FCValueCategory::Dangle)
			return value;

		Frame& frame = context.currentFrame();
		const int slot = variable->decl->slot;
		if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
		{
			std::fprintf(stderr, "LogError: Invalid slot for %s\n",
				variable->decl->name.c_str());
			return makeDangleValue();
		}

		frame.locals[slot] = value;
		return value;
	}

	const auto lhs = evaluateExpression(expression->getLHS(), context);
	const auto rhs = evaluateExpression(expression->getRHS(), context);
	if (lhs.type == FCValueCategory::Integer && rhs.type == FCValueCategory::Integer)
	{
		FCValue result;
		result.type = FCValueCategory::Integer;
		switch (op)
		{
		case '+': result.evaluteVal.intVal = lhs.evaluteVal.intVal + rhs.evaluteVal.intVal; break;
		case '-': result.evaluteVal.intVal = lhs.evaluteVal.intVal - rhs.evaluteVal.intVal; break;
		case '*': result.evaluteVal.intVal = lhs.evaluteVal.intVal * rhs.evaluteVal.intVal; break;
		case '/': result.evaluteVal.intVal = lhs.evaluteVal.intVal / rhs.evaluteVal.intVal; break;
		case '<': result.evaluteVal.intVal = lhs.evaluteVal.intVal < rhs.evaluteVal.intVal; break;
		default:
			std::fprintf(stderr, "LogError: %c\n", op);
			return makeDangleValue();
		}
		return result;
	}

	if (lhs.type == FCValueCategory::Floating && rhs.type == FCValueCategory::Floating)
	{
		FCValue result;
		result.type = FCValueCategory::Floating;
		switch (op)
		{
		case '+': result.evaluteVal.doubleVal = lhs.evaluteVal.doubleVal + rhs.evaluteVal.doubleVal; break;
		case '-': result.evaluteVal.doubleVal = lhs.evaluteVal.doubleVal - rhs.evaluteVal.doubleVal; break;
		case '*': result.evaluteVal.doubleVal = lhs.evaluteVal.doubleVal * rhs.evaluteVal.doubleVal; break;
		case '/': result.evaluteVal.doubleVal = lhs.evaluteVal.doubleVal / rhs.evaluteVal.doubleVal; break;
		default:
			std::fprintf(stderr, "LogError: %c\n", op);
			return makeDangleValue();
		}
		return result;
	}

	if (lhs.type == FCValueCategory::String && rhs.type == FCValueCategory::String && op == '+')
		return makeStringValue(std::string(lhs.evaluteVal.charVal) + rhs.evaluteVal.charVal);

	return makeDangleValue();
}

FCValue evaluateCall(const FCCallExprAST* expression,
	FCEvaluationContext& context)
{
	auto* function = context.functions.findFunction(expression->getName());
	if (function == nullptr)
	{
		std::fprintf(stderr, "Function not found: %s\n", expression->getName().c_str());
		return makeDangleValue();
	}

	const auto& prototype = function->getProto();
	if (expression->getArgs().size() != prototype->getArgs().size())
	{
		std::fprintf(stderr, "Argument count mismatch in %s\n", expression->getName().c_str());
		return makeDangleValue();
	}

	std::vector<FCValue> argumentValues;
	argumentValues.reserve(expression->getArgs().size());
	for (const auto& argument : expression->getArgs())
	{
		const auto value = evaluateExpression(argument.get(), context);
		if (value.type == FCValueCategory::Dangle)
			return value;
		argumentValues.push_back(value);
	}

	context.pushFrame(expression->getName());
	Frame& frame = context.currentFrame();
	for (size_t i = 0; i < argumentValues.size(); ++i)
	{
		const auto& parameter = prototype->getArgs()[i];
		if (parameter.decl == nullptr)
		{
			context.popFrame();
			return makeDangleValue();
		}

		const int slot = parameter.decl->slot;
		if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
		{
			std::fprintf(stderr, "Invalid parameter slot in %s\n", expression->getName().c_str());
			context.popFrame();
			return makeDangleValue();
		}
		frame.locals[slot] = argumentValues[i];
	}

	const auto result = evaluateExpression(function->getBody(), context);
	context.popFrame();
	return result;
}

FCValue evaluateExpression(const FCExprAST* expression,
	FCEvaluationContext& context)
{
	if (expression == nullptr)
		return makeDangleValue();

	if (const auto* number = dynamic_cast<const FCNumberExprAST*>(expression))
	{
		FCValue result;
		if (number->isFloating())
		{
			result.type = FCValueCategory::Floating;
			result.evaluteVal.doubleVal = number->m_doubleVal;
		}
		else
		{
			result.type = FCValueCategory::Integer;
			result.evaluteVal.intVal = number->m_intVal;
		}
		return result;
	}

	if (const auto* string = dynamic_cast<const FCStringExprAST*>(expression))
		return makeStringValue(string->m_stringVal);

	if (const auto* variable = dynamic_cast<const FCVariableExprAST*>(expression))
	{
		if (variable->decl == nullptr || variable->decl->slot < 0 || context.callStack.empty())
			return makeDangleValue();
		const auto& frame = context.currentFrame();
		if (variable->decl->slot >= static_cast<int>(frame.locals.size()))
			return makeDangleValue();
		return frame.locals[variable->decl->slot];
	}

	if (const auto* binary = dynamic_cast<const FCBinaryExprAST*>(expression))
		return evaluateBinary(binary, context);

	if (const auto* call = dynamic_cast<const FCCallExprAST*>(expression))
		return evaluateCall(call, context);

	if (dynamic_cast<const FCPrototypeAST*>(expression) != nullptr)
		return makeDangleValue();

	if (const auto* function = dynamic_cast<const FCFunctionAST*>(expression))
	{
		context.functions.registerFunction(const_cast<FCFunctionAST*>(function));
		return makeDangleValue();
	}

	if (const auto* conditional = dynamic_cast<const FCIfExprAST*>(expression))
	{
		const auto condition = evaluateExpression(conditional->getCondition(), context);
		bool truthy = false;
		if (condition.type == FCValueCategory::Integer)
			truthy = condition.evaluteVal.intVal != 0;
		else if (condition.type == FCValueCategory::Floating)
			truthy = condition.evaluteVal.doubleVal != 0.0;
		else
			return makeDangleValue();
		return evaluateExpression(truthy ? conditional->getThen() : conditional->getElse(), context);
	}

	if (const auto* loop = dynamic_cast<const FCForExprAST*>(expression))
	{
		if (loop->getDecl() == nullptr || context.callStack.empty())
			return makeDangleValue();
		const auto start = evaluateExpression(loop->getStart(), context);
		const auto end = evaluateExpression(loop->getEnd(), context);
		FCValue step;
		if (loop->getStep() == nullptr)
		{
			step.type = FCValueCategory::Integer;
			step.evaluteVal.intVal = 1;
		}
		else
			step = evaluateExpression(loop->getStep(), context);
		if (start.type != FCValueCategory::Integer || end.type != FCValueCategory::Integer ||
			step.type != FCValueCategory::Integer)
			return makeDangleValue();

		Frame& frame = context.currentFrame();
		const int slot = loop->getDecl()->slot;
		if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
			return makeDangleValue();
		frame.locals[slot] = start;
		while (evaluateExpression(loop->getEnd(), context).evaluteVal.intVal != 0)
		{
			evaluateExpression(loop->getBody(), context);
			frame.locals[slot].evaluteVal.intVal += step.evaluteVal.intVal;
		}
		return makeDangleValue();
	}

	if (const auto* sequence = dynamic_cast<const FCSeqExprAST*>(expression))
	{
		FCValue result = makeDangleValue();
		for (const auto& item : sequence->getExpressions())
			result = evaluateExpression(item.get(), context);
		return result;
	}

	if (const auto* declaration = dynamic_cast<const FCVarDeclExprAST*>(expression))
	{
		if (declaration->decl == nullptr || declaration->initExpr == nullptr || context.callStack.empty())
			return makeDangleValue();
		const auto value = evaluateExpression(declaration->initExpr.get(), context);
		if (value.type == FCValueCategory::Dangle)
			return value;
		Frame& frame = context.currentFrame();
		const int slot = declaration->decl->slot;
		if (slot < 0 || slot >= static_cast<int>(frame.locals.size()))
			return makeDangleValue();
		frame.locals[slot] = value;
		return value;
	}

	if (const auto* program = dynamic_cast<const FCProgramAST*>(expression))
	{
		context.functions.index(const_cast<FCProgramAST*>(program));
		FCValue result = makeDangleValue();
		for (const auto& statement : program->getStatements())
		{
			if (dynamic_cast<const FCFunctionAST*>(statement.get()) != nullptr)
				continue;
			result = evaluateExpression(statement.get(), context);
		}
		return result;
	}

	return makeDangleValue();
}
}

namespace FCExprClass
{
bool FCFunctionRegistry::registerFunction(FCFunctionAST* function)
{
	if (function == nullptr)
		return false;

	const auto name = function->getProtoName();
	auto it = m_functions.find(name);
	if (it != m_functions.end())
		return it->second == function;

	m_functions.emplace(name, function);
	return true;
}

FCFunctionAST* FCFunctionRegistry::findFunction(const std::string& name) const
{
	auto it = m_functions.find(name);
	return it == m_functions.end() ? nullptr : it->second;
}

void FCFunctionRegistry::clear()
{
	m_functions.clear();
}

bool FCFunctionRegistry::index(FCExprAST* root)
{
	if (root == nullptr)
		return false;

	if (auto* function = dynamic_cast<FCFunctionAST*>(root))
		return registerFunction(function);

	if (auto* program = dynamic_cast<FCProgramAST*>(root))
	{
		bool success = true;
		for (const auto& statement : program->getStatements())
		{
			if (auto* function = dynamic_cast<FCFunctionAST*>(statement.get()))
				success = registerFunction(function) && success;
		}
		return success;
	}

	return false;
}

void FCEvaluationContext::pushFrame(const std::string& functionName)
{
	Frame frame;
	frame.funcName = functionName;
	if (const auto* function = functions.findFunction(functionName))
		frame.locals.resize(function->getLocalCount());
	callStack.push_back(std::move(frame));
}

void FCEvaluationContext::popFrame()
{
	assert(!callStack.empty());
	callStack.pop_back();
}

Frame& FCEvaluationContext::currentFrame()
{
	assert(!callStack.empty());
	return callStack.back();
}

FCValue evaluate(const FCExprAST* expression, FCEvaluationContext& context)
{
	return evaluateExpression(expression, context);
}

FCValue evaluate(FCExprAST* expression, FCEvaluationContext& context)
{
	return evaluateExpression(expression, context);
}
}
