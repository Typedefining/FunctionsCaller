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
	m_globalScope.clear();
	m_globalDeclList.clear();
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
	if (auto declaration = lookupVariableInCurrentScope(functionName, name))
		return declaration;

	const auto functionIt = m_varTableInFunc.find(functionName);
	if (functionIt != m_varTableInFunc.end())
	{
		const auto& stack = functionIt->second;
		for (auto scopeIt = stack.rbegin(); scopeIt != stack.rend(); ++scopeIt)
		{
			const auto declarationIt = scopeIt->find(name);
			if (declarationIt != scopeIt->end())
				return declarationIt->second;
		}
	}
	return lookupGlobalVariable(name);
}

VarDeclPtr FCSemanticContext::lookupVariableInCurrentScope(
	const std::string& functionName, const std::string& name) const
{
	const auto functionIt = m_varTableInFunc.find(functionName);
	if (functionIt == m_varTableInFunc.end() || functionIt->second.empty())
		return nullptr;

	const auto& currentScope = functionIt->second.back();
	const auto declarationIt = currentScope.find(name);
	return declarationIt == currentScope.end() ? nullptr : declarationIt->second;
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

VarDeclPtr FCSemanticContext::lookupGlobalVariable(const std::string& name) const
{
	const auto it = m_globalScope.find(name);
	return it == m_globalScope.end() ? nullptr : it->second;
}

void FCSemanticContext::insertGlobalVariable(
	const std::string& name, VarDeclPtr declaration)
{
	assert(declaration != nullptr && "global declaration must not be null");
	declaration->isGlobal = true;
	declaration->slot = static_cast<int>(m_globalDeclList.size());
	m_globalScope[name] = declaration;
	m_globalDeclList.push_back(std::move(declaration));
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

const std::vector<VarDeclPtr>& FCSemanticContext::globalDeclarations() const
{
	return m_globalDeclList;
}
}
