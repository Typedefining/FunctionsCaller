#include "ast_dump.h"

#include "ast.h"

#include <ostream>
#include <sstream>
#include <string>

namespace FCMarks {
using namespace FCExprClass;

namespace {

void indentTo(std::ostream &out, int depth) {
  out << std::string(static_cast<size_t>(depth) * 2, ' ');
}

const char *opSymbol(FCBinaryOp op) {
  switch (op) {
  case FCBinaryOp::Assign:
    return "=";
  case FCBinaryOp::Less:
    return "<";
  case FCBinaryOp::Add:
    return "+";
  case FCBinaryOp::Sub:
    return "-";
  case FCBinaryOp::Mul:
    return "*";
  case FCBinaryOp::Div:
    return "/";
  default:
    return "?";
  }
}

void dumpNode(const FCExprAST &node, std::ostream &out, int depth);

void dumpParams(const FCPrototypeAST &proto, std::ostream &out) {
  out << "(";
  const auto &args = proto.getArgs();
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0)
      out << ", ";
    out << args[i]->name << ":" << args[i]->typeName;
  }
  out << ")";
}

void dumpChildren(const std::vector<std::unique_ptr<FCExprAST>> &children,
                  std::ostream &out, int depth) {
  for (const auto &child : children) {
    if (child)
      dumpNode(*child, out, depth + 1);
  }
}

struct DumperVisitor {
  std::ostream& out;
  int depth;
  explicit DumperVisitor(std::ostream& o, int d) : out(o), depth(d) {}

  void visit(const FCPrototypeAST& n) {
    indentTo(out, depth);
    out << "Prototype " << n.getProtoName();
    dumpParams(n, out);
    out << "\n";
  }

  void visit(const FCProgramAST& n) {
    indentTo(out, depth);
    out << "Program (" << n.getStatements().size() << " statements)\n";
    dumpChildren(n.getStatements(), out, depth);
  }

  void visit(const FCBlockExprAST& n) {
    indentTo(out, depth);
    out << "Block (" << n.getExpressions().size() << " expressions)\n";
    for (const auto &e : n.getExpressions())
      dumpNode(*e, out, depth + 1);
  }

  void visit(const FCSeqExprAST& n) {
    indentTo(out, depth);
    out << "Seq (" << n.getExpressions().size() << " expressions)\n";
    for (const auto &e : n.getExpressions())
      dumpNode(*e, out, depth + 1);
  }

  void visit(const FCFunctionAST& n) {
    indentTo(out, depth);
    out << "Function " << n.getProtoName();
    if (const auto &proto = n.getProto())
      dumpParams(*proto, out);
    out << "\n";
    if (const auto *body = n.getBody())
      dumpNode(*body, out, depth + 1);
  }

  void visit(const FCVarDeclExprAST& n) {
    indentTo(out, depth);
    out << "VarDecl " << n.decl->name << ":" << n.decl->typeName << "\n";
    if (n.initExpr)
      dumpNode(*n.initExpr, out, depth + 1);
  }

  void visit(const FCForExprAST& n) {
    indentTo(out, depth);
    out << "For " << n.getDecl()->name << ":" << n.getDecl()->typeName << "\n";
    if (const auto *start = n.getStart())
      dumpNode(*start, out, depth + 1);
    if (const auto *end = n.getEnd())
      dumpNode(*end, out, depth + 1);
    if (const auto *step = n.getStep())
      dumpNode(*step, out, depth + 1);
    if (const auto *body = n.getBody())
      dumpNode(*body, out, depth + 1);
  }

  void visit(const FCIfExprAST& n) {
    indentTo(out, depth);
    out << "If\n";
    dumpNode(*n.getCondition(), out, depth + 1);
    dumpNode(*n.getThen(), out, depth + 1);
    dumpNode(*n.getElse(), out, depth + 1);
  }

  void visit(const FCCallExprAST& n) {
    indentTo(out, depth);
    out << "Call " << n.getName() << "\n";
    for (const auto &e : n.getArgs())
      dumpNode(*e, out, depth + 1);
  }

  void visit(const FCBinaryExprAST& n) {
    indentTo(out, depth);
    out << "Binary " << opSymbol(n.getOperator()) << "\n";
    dumpNode(*n.getLHS(), out, depth + 1);
    dumpNode(*n.getRHS(), out, depth + 1);
  }

  void visit(const FCVariableExprAST& n) {
    indentTo(out, depth);
    out << "Variable " << n.name << "\n";
  }

  void visit(const FCStringExprAST& n) {
    indentTo(out, depth);
    out << "String \"" << n.m_stringVal << "\"\n";
  }

  void visit(const FCNumberExprAST& n) {
    indentTo(out, depth);
    out << "Number ";
    if (n.isFloating())
      out << n.m_doubleVal;
    else
      out << n.m_intVal;
    out << "\n";
  }
};

void dumpNode(const FCExprAST &node, std::ostream &out, int depth) {
  DumperVisitor v{ out, depth };
  try {
    FCExprClass::dispatchAST(node, v);
  } catch (const std::runtime_error&) {
    // 未知节点类型保留旧行为：打印 "Expr\n"
    indentTo(out, depth);
    out << "Expr\n";
  }
}

} // namespace

std::string describeAst(const FCExprAST &node) {
  std::ostringstream out;
  dumpNode(node, out, 0);
  return out.str();
}

} // namespace FCMarks
