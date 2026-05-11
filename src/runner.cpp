#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "planner/query_planner.hpp"

namespace {

std::string Trim(const std::string& s) {
  size_t l = 0;
  while (l < s.size() && std::isspace(static_cast<unsigned char>(s[l]))) {
    ++l;
  }

  size_t r = s.size();
  while (r > l && std::isspace(static_cast<unsigned char>(s[r - 1]))) {
    --r;
  }

  return s.substr(l, r - l);
}

std::string StorageValueToString(const storage::Value& val) {
  const auto visitor = graph::PlannerUtils::overloads {
    [](int64_t cur) { return std::to_string(cur); },
    [](double cur) { return std::to_string(cur); },
    [](const std::string& cur) {
      std::string quoted = "\"";
      for (char ch : cur) {
        if (ch == '\\' || ch == '"') {
          quoted += '\\';
        }
        quoted += ch;
      }
      quoted += '"';
      return quoted;
    },
    [](bool cur) { return std::string(cur ? "true" : "false"); }
  };
  return std::visit(visitor, val);
}

std::string PropertiesToString(const storage::Properties& properties) {
  if (properties.empty()) {
    return {};
  }

  std::vector<std::pair<std::string, const storage::Value*>> props;
  props.reserve(properties.size());
  for (const auto& [key, value] : properties) {
    props.emplace_back(key, &value);
  }

  std::sort(props.begin(), props.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.first < rhs.first;
  });

  std::string result = " {";
  for (size_t i = 0; i < props.size(); ++i) {
    if (i) {
      result += ", ";
    }
    result += props[i].first;
    result += ": ";
    result += StorageValueToString(*props[i].second);
  }
  result += '}';
  return result;
}

std::string NodeToString(storage::Node* node) {
  if (!node) {
    return "null";
  }

  std::string result = "(";
  for (const auto& label : node->labels) {
    result += ':';
    result += label;
  }
  result += PropertiesToString(node->properties);
  result += ')';
  return result;
}

std::string EdgeToString(storage::Edge* edge) {
  if (!edge) {
    return "null";
  }

  std::string result = "-[:";
  result += edge->type;
  result += PropertiesToString(edge->properties);
  result += "]->";
  return result;
}

std::string SlotToString(const graph::exec::RowSlot& slot) {
  static const auto visitor = graph::PlannerUtils::overloads {
    [](const graph::Value& val) { return graph::PlannerUtils::ValueToString(val); },
    [](storage::Node* node) { return NodeToString(node); },
    [](storage::Edge* edge) { return EdgeToString(edge); }
  };
  return std::visit(visitor, slot.value);
}

void PrintRow(const graph::exec::Row& row) {
  for (size_t i = 0; i < row.slots.size(); ++i) {
    if (i) {
      std::cout << " | ";
    }
    std::cout << row.slots_names[i] << ": " << SlotToString(row.slots[i]);
  }
  std::cout << '\n';
}

}  // namespace

int main() {
  auto current_db_path = std::filesystem::current_path() / "graphdb";
  storage::GraphDB db;

  while (true) {
    std::string input;

    while (true) {
      std::string line;
      if (!std::getline(std::cin, line)) {
        return 0;
      }

      input += line + '\n';

      if (line.find(';') != std::string::npos) {
        break;
      }
    }

    std::string trimmed_input = Trim(input);
    if (trimmed_input == "exit;") {
      break;
    }

    if (trimmed_input == "flush;") {
      db.flush();
      continue;
    }

    if (trimmed_input == "save;") {
      if (!db.save_to_file(current_db_path.string())) {
        std::cerr << "Failed to save database to: " << current_db_path << "\n";
      }
      continue;
    }

    if (trimmed_input.rfind("save ", 0) == 0) {
      std::string path = Trim(trimmed_input.substr(5));
      path.pop_back();
      if (path.empty()) {
        std::cerr << "Usage: save <path>;\n";
        continue;
      }

      current_db_path = std::filesystem::path(path);
      if (!db.save_to_file(current_db_path.string())) {
        std::cerr << "Failed to save database to: " << current_db_path << '\n';
      }
      continue;
    }

    if (trimmed_input.rfind("load ", 0) == 0) {
      std::string path = Trim(trimmed_input.substr(5));
      path.pop_back();

      if (path.empty()) {
        std::cerr << "Usage: load <path>;\n";
        continue;
      }

      current_db_path = std::filesystem::path(path);
      if (!db.load_from_file(current_db_path.string())) {
        std::cerr << "Failed to load database from: " << current_db_path << '\n';
      }
      continue;
    }


    parser::Parser parser(lexer::Lex(input));
    ast::QueryAST ast = parser.ParseSingle();

    graph::exec::ExecContext ctx(&db);
    graph::planner::Planner planner(ctx, &db, std::move(ast));

    planner.build_logical_plan();
    planner.build_physical_plan();

    graph::exec::RowCursorPtr cursor = planner.getPhysicalPlan().root->open(ctx);

    graph::exec::Row row;
    while (cursor->next(row)) {
      PrintRow(row);
      row.clear();
    }
  }

  return 0;
}