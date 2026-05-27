#include <common/display.hpp>
#include <debug/debug.hpp>
#include <iomanip>
#include <iostream>

namespace Debug {

void print_tokens(const std::vector<Token> &tokens) {
  std::cout << std::left << std::setw(24) << "kind" << std::setw(16) << "value"
            << std::setw(10) << "span" << "\n";
  std::cout << std::string(50, '-') << "\n";

  for (const auto &t : tokens) {
    std::string val;
    switch (t.kind) {
    case TokenKind::Literal:
      switch (t.literal_kind) {
      case LiteralKind::Integer:
        val = std::to_string(std::get<long long>(t.value));
        break;
      case LiteralKind::Float:
        val = std::to_string(std::get<double>(t.value));
        break;
      case LiteralKind::Char:
        val = std::string(1, std::get<char>(t.value));
        break;
      case LiteralKind::String:
        val = std::get<std::string>(t.value);
        break;
      }
      break;
    case TokenKind::Identifier:
      val = std::holds_alternative<std::string>(t.value)
                ? std::get<std::string>(t.value)
                : t.text;
      break;
    case TokenKind::Eof:
      val = "<eof>";
      break;
    default:
      val = t.text.empty() ? "<empty>" : t.text;
      break;
    }

    std::string span = "[" + std::to_string(t.span.start) + ", " +
                       std::to_string(t.span.end) + ")";
    std::cout << std::left << std::setw(24) << to_string(t.kind, t.literal_kind)
              << std::setw(16) << val << std::setw(10) << span << "\n";
  }

  std::cout << std::string(50, '-') << "\n";
  std::cout << tokens.size() << " tokens\n\n";
}

static std::string conn(bool last) { return last ? "└── " : "├── "; }
static std::string ext(bool last) { return last ? "    " : "│   "; }

static void print_expr(const Expr &expr, const std::string &prefix, bool last);

static void print_children(const std::vector<ExprPtr> &exprs,
                           const std::string &pfx) {
  for (size_t i = 0; i < exprs.size(); i++)
    print_expr(*exprs[i], pfx, i == exprs.size() - 1);
}

static void print_expr(const Expr &expr, const std::string &prefix, bool last) {
  std::string my = prefix + conn(last);
  std::string ch = prefix + ext(last);

  visitExpr(
      [&](const auto &n) {
        using T = std::decay_t<decltype(n)>;

        if constexpr (std::is_same_v<T, IntLiteral>)
          std::cout << my << "IntLiteral(" << n.value << ")\n";
        else if constexpr (std::is_same_v<T, FloatLiteral>)
          std::cout << my << "FloatLiteral(" << n.value << ")\n";
        else if constexpr (std::is_same_v<T, CharLiteral>)
          std::cout << my << "CharLiteral('" << n.value << "')\n";
        else if constexpr (std::is_same_v<T, CharList>)
          std::cout << my << "CharList(\""
                    << std::string(n.chars.begin(), n.chars.end()) << "\")\n";
        else if constexpr (std::is_same_v<T, Identifier>)
          std::cout << my << "Identifier(" << n.name << ")\n";
        else if constexpr (std::is_same_v<T, This>)
          std::cout << my << "This\n";
        else if constexpr (std::is_same_v<T, Package>)
          std::cout << my << "Package(" << n.name << ")\n";
        else if constexpr (std::is_same_v<T, Import>)
          std::cout << my << "Import(\"" << n.path << "\")\n";
        else if constexpr (std::is_same_v<T, Unary>) {
          std::cout << my << "Unary(" << to_string(n.op) << ")\n";
          print_expr(*n.rhs, ch, true);
        } else if constexpr (std::is_same_v<T, Binary>) {
          std::cout << my << "Binary(" << to_string(n.op) << ")\n";
          print_expr(*n.lhs, ch, false);
          print_expr(*n.rhs, ch, true);
        } else if constexpr (std::is_same_v<T, FieldAccess>) {
          std::cout << my << "FieldAccess(." << n.field << ")\n";
          print_expr(*n.object, ch, true);
        } else if constexpr (std::is_same_v<T, Block>) {
          std::cout << my << "Block\n";
          print_children(n.exprs, ch);
        } else if constexpr (std::is_same_v<T, Lambda>) {
          std::cout << my << "Lambda(";
          for (size_t i = 0; i < n.params.size(); i++) {
            if (i)
              std::cout << ", ";
            std::cout << n.params[i].name;
          }
          std::cout << ")\n";
          print_children(n.body.exprs, ch);
        } else if constexpr (std::is_same_v<T, If>) {
          std::cout << my << "If\n";
          bool has_else = n.els.has_value();
          std::cout << ch << conn(!has_else) << "Cond\n";
          print_expr(*n.cond, ch + ext(!has_else), true);
          if (has_else) {
            std::cout << ch << "├── Then\n";
            print_children(n.then.exprs, ch + "│   ");
            std::cout << ch << "└── Else\n";
            print_children(n.els->exprs, ch + "    ");
          } else {
            std::cout << ch << "└── Then\n";
            print_children(n.then.exprs, ch + "    ");
          }
        } else if constexpr (std::is_same_v<T, Call>) {
          std::cout << my << "Call\n";
          bool has_args = !n.args.empty();
          std::cout << ch << conn(!has_args) << "Callee\n";
          print_expr(*n.callee, ch + ext(!has_args), true);
          if (has_args) {
            std::cout << ch << "└── Args\n";
            print_children(n.args, ch + "    ");
          }
        } else if constexpr (std::is_same_v<T, Let>) {
          bool is_func = n.value && std::holds_alternative<Block>(n.value->val);
          std::cout << my << "Let(" << (n.is_public ? "public" : "private")
                    << " " << n.name;
          if (is_func) {
            std::cout << "(";
            for (size_t i = 0; i < n.params.size(); i++) {
              if (i)
                std::cout << ", ";
              std::cout << n.params[i].name;
            }
            std::cout << ")";
          }
          std::cout << ")\n";
          if (n.value)
            print_expr(*n.value, ch, true);
        } else if constexpr (std::is_same_v<T, Match>) {
          std::cout << my << "Match\n";
          std::cout << ch << "├── Subject\n";
          print_expr(*n.subject, ch + "│   ", true);
          std::cout << ch << "└── Arms\n";
          std::string ap = ch + "    ";
          for (size_t i = 0; i < n.arms.size(); i++) {
            const auto &arm = n.arms[i];
            bool arm_last = i == n.arms.size() - 1;
            std::cout << ap << conn(arm_last) << "Arm(" << arm.constructor;
            if (!arm.bindings.empty()) {
              std::cout << "(";
              for (size_t j = 0; j < arm.bindings.size(); j++) {
                if (j)
                  std::cout << ", ";
                std::cout << arm.bindings[j];
              }
              std::cout << ")";
            }
            std::cout << ") =>\n";
            print_expr(*arm.body, ap + ext(arm_last), true);
          }
        } else if constexpr (std::is_same_v<T, New>) {
          std::cout << my << "New(" << n.type_name << ")\n";
          print_children(n.args, ch);
        } else if constexpr (std::is_same_v<T, TypeDecl>) {
          std::cout << my << "TypeDecl(" << n.name << ")\n";
          for (size_t i = 0; i < n.variants.size(); i++) {
            const auto &v = n.variants[i];
            bool v_last = i == n.variants.size() - 1;
            std::cout << ch << conn(v_last) << "Variant(" << v.name;
            if (!v.fields.empty()) {
              std::cout << "(";
              for (size_t j = 0; j < v.fields.size(); j++) {
                if (j)
                  std::cout << ", ";
                std::cout << v.fields[j];
              }
              std::cout << ")";
            }
            std::cout << ")\n";
          }
        } else if constexpr (std::is_same_v<T, StructDecl>) {
          std::cout << my << "StructDecl(" << n.name << ")\n";
          size_t total = n.fields.size() + n.methods.size(), idx = 0;
          for (const auto &f : n.fields) {
            bool f_last = ++idx == total;
            std::cout << ch << conn(f_last) << "Field("
                      << (f.is_public ? "public" : "private") << " " << f.name
                      << ")\n";
          }
          for (const auto &m : n.methods) {
            bool m_last = ++idx == total;
            std::cout << ch << conn(m_last) << "Method("
                      << (m.is_public ? "public" : "private");
            std::cout << (m.is_constructor ? " new(" : " let " + m.name + "(");
            for (size_t j = 0; j < m.params.size(); j++) {
              if (j)
                std::cout << ", ";
              std::cout << m.params[j].name;
            }
            std::cout << "))\n";
            print_children(m.body.exprs, ch + ext(m_last));
          }
        }
      },
      expr);
}

void print_ast(const std::vector<ExprPtr> &program) {
  for (size_t i = 0; i < program.size(); i++)
    print_expr(*program[i], "", i == program.size() - 1);
  std::cout << "\n";
}

} // namespace Debug