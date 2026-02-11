#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/graphql.h — Built-in GraphQL server capability
// ═══════════════════════════════════════════════════════════════════
//
//  Usage:
//    auto schema = std::make_shared<graphql::Schema>();
//    schema->query("user", [](auto args, auto ctx) {
//        return JsonValue({{"name", "John"}, {"id", args["id"].get<int>()}});
//    });
//    app.post("/graphql", graphql::createHandler(schema));
//
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <variant>
#include <optional>
#include <stdexcept>
#include <sstream>

namespace nodepp::graphql {

// ── Resolver function type ──
using Resolver = std::function<JsonValue(JsonValue args, JsonValue context)>;

// ═══════════════════════════════════════════
//  Simple GraphQL Query Parser
// ═══════════════════════════════════════════

struct FieldSelection {
    std::string name;
    std::string alias;
    nlohmann::json arguments;
    std::vector<FieldSelection> selections;
};

struct ParsedQuery {
    std::string operationType; // "query" or "mutation"
    std::string operationName;
    std::vector<FieldSelection> selections;
};

namespace detail {

class GraphQLParser {
public:
    explicit GraphQLParser(const std::string& source)
        : source_(source), pos_(0) {}

    ParsedQuery parse() {
        ParsedQuery result;
        skipWhitespace();

        // Parse operation type
        if (peek() == '{') {
            result.operationType = "query";
        } else {
            auto keyword = parseIdentifier();
            if (keyword == "query") {
                result.operationType = "query";
            } else if (keyword == "mutation") {
                result.operationType = "mutation";
            } else {
                throw std::runtime_error("Expected 'query' or 'mutation', got '" + keyword + "'");
            }

            skipWhitespace();

            // Optional operation name
            if (peek() != '{' && peek() != '(') {
                result.operationName = parseIdentifier();
                skipWhitespace();
            }

            // Skip variable definitions
            if (peek() == '(') {
                skipBalanced('(', ')');
                skipWhitespace();
            }
        }

        result.selections = parseSelectionSet();
        return result;
    }

private:
    std::string source_;
    std::size_t pos_;

    char peek() const {
        return pos_ < source_.size() ? source_[pos_] : '\0';
    }

    char advance() {
        return pos_ < source_.size() ? source_[pos_++] : '\0';
    }

    void skipWhitespace() {
        while (pos_ < source_.size() &&
               (source_[pos_] == ' ' || source_[pos_] == '\t' ||
                source_[pos_] == '\n' || source_[pos_] == '\r' ||
                source_[pos_] == ',')) {
            pos_++;
        }
    }

    void expect(char c) {
        skipWhitespace();
        if (advance() != c) {
            throw std::runtime_error(
                std::string("Expected '") + c + "' at position " + std::to_string(pos_));
        }
    }

    void skipBalanced(char open, char close) {
        expect(open);
        int depth = 1;
        while (depth > 0 && pos_ < source_.size()) {
            char c = advance();
            if (c == open) depth++;
            if (c == close) depth--;
        }
    }

    bool isIdentChar(char c) const {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    }

    std::string parseIdentifier() {
        skipWhitespace();
        std::string result;
        while (pos_ < source_.size() && isIdentChar(source_[pos_])) {
            result += source_[pos_++];
        }
        if (result.empty()) {
            throw std::runtime_error("Expected identifier at position " + std::to_string(pos_));
        }
        return result;
    }

    nlohmann::json parseValue() {
        skipWhitespace();
        char c = peek();

        if (c == '"') {
            return parseString();
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            return parseNumber();
        } else if (c == 't' || c == 'f') {
            return parseBool();
        } else if (c == 'n') {
            return parseNull();
        } else if (c == '{') {
            return parseObjectValue();
        } else if (c == '[') {
            return parseArrayValue();
        } else {
            // Enum value or variable reference
            auto ident = parseIdentifier();
            return nlohmann::json(ident);
        }
    }

    std::string parseString() {
        expect('"');
        std::string result;
        while (peek() != '"' && peek() != '\0') {
            if (peek() == '\\') {
                advance();
                char esc = advance();
                switch (esc) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += esc; break;
                }
            } else {
                result += advance();
            }
        }
        expect('"');
        return result;
    }

    nlohmann::json parseNumber() {
        skipWhitespace();
        std::string numStr;
        bool isFloat = false;
        if (peek() == '-') numStr += advance();
        while (peek() >= '0' && peek() <= '9') numStr += advance();
        if (peek() == '.') {
            isFloat = true;
            numStr += advance();
            while (peek() >= '0' && peek() <= '9') numStr += advance();
        }
        return isFloat ? nlohmann::json(std::stod(numStr))
                       : nlohmann::json(std::stoi(numStr));
    }

    nlohmann::json parseBool() {
        auto word = parseIdentifier();
        return nlohmann::json(word == "true");
    }

    nlohmann::json parseNull() {
        parseIdentifier(); // consume "null"
        return nlohmann::json(nullptr);
    }

    nlohmann::json parseObjectValue() {
        expect('{');
        nlohmann::json obj = nlohmann::json::object();
        skipWhitespace();
        while (peek() != '}') {
            auto key = parseIdentifier();
            expect(':');
            obj[key] = parseValue();
            skipWhitespace();
        }
        expect('}');
        return obj;
    }

    nlohmann::json parseArrayValue() {
        expect('[');
        nlohmann::json arr = nlohmann::json::array();
        skipWhitespace();
        while (peek() != ']') {
            arr.push_back(parseValue());
            skipWhitespace();
        }
        expect(']');
        return arr;
    }

    nlohmann::json parseArguments() {
        expect('(');
        nlohmann::json args = nlohmann::json::object();
        skipWhitespace();
        while (peek() != ')') {
            auto name = parseIdentifier();
            expect(':');
            args[name] = parseValue();
            skipWhitespace();
        }
        expect(')');
        return args;
    }

    FieldSelection parseField() {
        FieldSelection field;
        auto nameOrAlias = parseIdentifier();
        skipWhitespace();

        // Check for alias (alias: fieldName)
        if (peek() == ':') {
            advance();
            field.alias = nameOrAlias;
            field.name = parseIdentifier();
            skipWhitespace();
        } else {
            field.name = nameOrAlias;
        }

        // Parse arguments
        if (peek() == '(') {
            field.arguments = parseArguments();
        } else {
            field.arguments = nlohmann::json::object();
        }

        skipWhitespace();

        // Parse nested selection set
        if (peek() == '{') {
            field.selections = parseSelectionSet();
        }

        return field;
    }

    std::vector<FieldSelection> parseSelectionSet() {
        expect('{');
        std::vector<FieldSelection> selections;
        skipWhitespace();

        while (peek() != '}' && peek() != '\0') {
            selections.push_back(parseField());
            skipWhitespace();
        }

        expect('}');
        return selections;
    }
};

} // namespace detail

// ═══════════════════════════════════════════
//  class Schema
//  Define GraphQL types, queries, and mutations.
// ═══════════════════════════════════════════
class Schema {
public:
    // ── Register a query resolver ──
    Schema& query(const std::string& name, Resolver resolver) {
        queryResolvers_[name] = std::move(resolver);
        return *this;
    }

    // ── Register a mutation resolver ──
    Schema& mutation(const std::string& name, Resolver resolver) {
        mutationResolvers_[name] = std::move(resolver);
        return *this;
    }

    // ── Execute a GraphQL query ──
    nlohmann::json execute(const std::string& queryStr,
                           const nlohmann::json& variables = nlohmann::json::object(),
                           const nlohmann::json& context = nlohmann::json::object()) {
        try {
            detail::GraphQLParser parser(queryStr);
            auto parsed = parser.parse();

            nlohmann::json data = nlohmann::json::object();
            nlohmann::json errors = nlohmann::json::array();

            auto& resolvers = (parsed.operationType == "mutation")
                ? mutationResolvers_
                : queryResolvers_;

            for (auto& field : parsed.selections) {
                auto it = resolvers.find(field.name);
                if (it == resolvers.end()) {
                    errors.push_back(nlohmann::json{
                        {"message", "Cannot query field '" + field.name + "' on type '"
                                    + parsed.operationType + "'"},
                        {"locations", nlohmann::json::array()},
                        {"path", nlohmann::json::array({field.name})}
                    });
                    continue;
                }

                try {
                    // Merge arguments with variables
                    nlohmann::json args = field.arguments;
                    for (auto& [key, value] : variables.items()) {
                        if (!args.contains(key)) {
                            args[key] = value;
                        }
                    }

                    JsonValue result = it->second(
                        JsonValue(args),
                        JsonValue(context)
                    );

                    nlohmann::json resultJson = result.raw();

                    // Filter result fields based on selection set
                    if (!field.selections.empty() && resultJson.is_object()) {
                        nlohmann::json filtered = nlohmann::json::object();
                        filterFields(resultJson, field.selections, filtered);
                        resultJson = filtered;
                    }

                    std::string key = field.alias.empty() ? field.name : field.alias;
                    data[key] = resultJson;

                } catch (const std::exception& e) {
                    errors.push_back(nlohmann::json{
                        {"message", e.what()},
                        {"path", nlohmann::json::array({field.name})}
                    });
                    data[field.name] = nullptr;
                }
            }

            nlohmann::json response = {{"data", data}};
            if (!errors.empty()) {
                response["errors"] = errors;
            }
            return response;

        } catch (const std::exception& e) {
            return nlohmann::json{
                {"data", nullptr},
                {"errors", nlohmann::json::array({
                    nlohmann::json{{"message", std::string("Parse error: ") + e.what()}}
                })}
            };
        }
    }

private:
    std::unordered_map<std::string, Resolver> queryResolvers_;
    std::unordered_map<std::string, Resolver> mutationResolvers_;

    void filterFields(const nlohmann::json& source,
                      const std::vector<FieldSelection>& selections,
                      nlohmann::json& target) {
        for (auto& sel : selections) {
            std::string key = sel.alias.empty() ? sel.name : sel.alias;
            if (source.contains(sel.name)) {
                if (!sel.selections.empty() && source[sel.name].is_object()) {
                    nlohmann::json nested = nlohmann::json::object();
                    filterFields(source[sel.name], sel.selections, nested);
                    target[key] = nested;
                } else if (!sel.selections.empty() && source[sel.name].is_array()) {
                    nlohmann::json arr = nlohmann::json::array();
                    for (auto& item : source[sel.name]) {
                        if (item.is_object()) {
                            nlohmann::json filtered = nlohmann::json::object();
                            filterFields(item, sel.selections, filtered);
                            arr.push_back(filtered);
                        } else {
                            arr.push_back(item);
                        }
                    }
                    target[key] = arr;
                } else {
                    target[key] = source[sel.name];
                }
            }
        }
    }
};

// ═══════════════════════════════════════════
//  createHandler — Express-compatible route handler
// ═══════════════════════════════════════════

inline http::RouteHandler createHandler(std::shared_ptr<Schema> schema) {
    return [schema](http::Request& req, http::Response& res) {
        nlohmann::json requestBody;

        // Parse GraphQL request from body
        if (!req.body.isNull()) {
            requestBody = req.body.raw();
        } else if (!req.rawBody.empty()) {
            try {
                requestBody = nlohmann::json::parse(req.rawBody);
            } catch (...) {
                res.status(400).json(nlohmann::json{
                    {"errors", nlohmann::json::array({
                        nlohmann::json{{"message", "Invalid JSON in request body"}}
                    })}
                });
                return;
            }
        } else {
            // Check query parameter for GET requests
            auto queryParam = req.query.find("query");
            if (queryParam != req.query.end()) {
                requestBody = {{"query", queryParam->second}};
            } else {
                res.status(400).json(nlohmann::json{
                    {"errors", nlohmann::json::array({
                        nlohmann::json{{"message", "Missing GraphQL query"}}
                    })}
                });
                return;
            }
        }

        std::string query = requestBody.value("query", "");
        auto variables = requestBody.value("variables", nlohmann::json::object());
        auto context = nlohmann::json::object();

        if (query.empty()) {
            res.status(400).json(nlohmann::json{
                {"errors", nlohmann::json::array({
                    nlohmann::json{{"message", "Missing GraphQL query"}}
                })}
            });
            return;
        }

        auto result = schema->execute(query, variables, context);
        res.json(result);
    };
}

} // namespace nodepp::graphql
