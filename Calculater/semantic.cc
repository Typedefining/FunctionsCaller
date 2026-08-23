#include "semantic.h"

#include <cassert>

namespace FCMarks
{
FCSemanticContext::FCSemanticContext()
{
	reset();
}

void FCSemanticContext::reset()
{
	m_binopPrecedence = {
		{'=', 5},
		{'<', 9},
		{'+', 10},
		{'-', 10},
		{'*', 20},
		{'/', 20}
	};
	m_varTableInFunc.clear();
	m_funcDeclList.clear();
}

int FCSemanticContext::getOperatorPrecedence(char op) const
{
	const auto it = m_binopPrecedence.find(op);
	return it == m_binopPrecedence.end() ? 0 : it->second;
}

void FCSemanticContext::pushScopeForFunc(const std::string& functionName)
{
	m_varTableInFunc[functionName].emplace_back();
}

void FCSemanticContext::popScopeForFunc(const std::string& functionName)
{
	auto& stack = m_varTableInFunc[functionName];
	if (!stack.empty())
		stack.pop_back();
}

VarDeclPtr FCSemanticContext::lookupVariableDecl(
	const std::string& functionName, const std::string& name) const
{
	const auto functionIt = m_varTableInFunc.find(functionName);
	if (functionIt == m_varTableInFunc.end())
		return nullptr;

	const auto& stack = functionIt->second;
	for (auto scopeIt = stack.rbegin(); scopeIt != stack.rend(); ++scopeIt)
	{
		const auto declarationIt = scopeIt->find(name);
		if (declarationIt != scopeIt->end())
			return declarationIt->second;
	}
	return nullptr;
}

void FCSemanticContext::insertVariableInCurrentScope(
	const std::string& functionName,
	const std::string& name, VarDeclPtr declaration)
{
	auto& stack = m_varTableInFunc[functionName];
	assert(!stack.empty() && "must have a scope before insert");
	stack.back()[name] = declaration;
	m_funcDeclList[functionName].push_back(std::move(declaration));
}

std::vector<VarDeclPtr>& FCSemanticContext::functionDeclarations(
	const std::string& functionName)
{
	return m_funcDeclList[functionName];
}

const std::vector<VarDeclPtr>& FCSemanticContext::functionDeclarations(
	const std::string& functionName) const
{
	static const std::vector<VarDeclPtr> empty;
	const auto it = m_funcDeclList.find(functionName);
	return it == m_funcDeclList.end() ? empty : it->second;
}
}
