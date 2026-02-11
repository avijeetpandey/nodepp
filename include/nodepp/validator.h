#pragma once
// ═══════════════════════════════════════════════════════════════════
//  nodepp/validator.h — Request validation with schema-based rules
// ═══════════════════════════════════════════════════════════════════

#include "http.h"
#include "json_utils.h"
#include <string>
#include <vector>
#include <functional>
#include <regex>
#include <optional>

namespace nodepp::validator {

// ── Validation error ──
struct ValidationError {
    std::string field;
    std::string message;
    std::string rule;
};

// ── Field rule builder (fluent API) ──
class FieldRule {
public:
    explicit FieldRule(const std::string& name) : name_(name) {}

    FieldRule& required() { required_ = true; return *this; }
    FieldRule& optional() { required_ = false; return *this; }
    FieldRule& isString() { type_ = "string"; return *this; }
    FieldRule& isNumber() { type_ = "number"; return *this; }
    FieldRule& isInt() { type_ = "integer"; return *this; }
    FieldRule& isBool() { type_ = "boolean"; return *this; }
    FieldRule& isArray() { type_ = "array"; return *this; }
    FieldRule& isObject() { type_ = "object"; return *this; }

    FieldRule& minLength(int n) { minLen_ = n; return *this; }
    FieldRule& maxLength(int n) { maxLen_ = n; return *this; }
    FieldRule& min(double n) { min_ = n; return *this; }
    FieldRule& max(double n) { max_ = n; return *this; }
    FieldRule& pattern(const std::string& regex) { pattern_ = regex; return *this; }

    FieldRule& email() {
        pattern_ = R"(^[a-zA-Z0-9._%+\-]+@[a-zA-Z0-9.\-]+\.[a-zA-Z]{2,}$)";
        patternName_ = "email";
        return *this;
    }

    FieldRule& url() {
        pattern_ = R"(^https?://[^\s/$.?#].[^\s]*$)";
        patternName_ = "url";
        return *this;
    }

    FieldRule& oneOf(std::vector<std::string> values) {
        enum_ = std::move(values);
        return *this;
    }

    FieldRule& custom(std::function<std::optional<std::string>(const JsonValue&)> fn) {
        customValidators_.push_back(std::move(fn));
        return *this;
    }

    // ── Validate a value against this rule ──
    std::vector<ValidationError> validate(const JsonValue& body) const {
        std::vector<ValidationError> errors;
        bool exists = body.has(name_);
        auto val = body[name_];

        if (required_ && (!exists || val.isNull())) {
            errors.push_back({name_, name_ + " is required", "required"});
            return errors;
        }

        if (!exists || val.isNull()) return errors; // optional and missing

        // Type checking
        if (!type_.empty()) {
            bool typeOk = false;
            if (type_ == "string")  typeOk = val.isString();
            else if (type_ == "number")  typeOk = val.isNumber();
            else if (type_ == "integer") typeOk = val.isNumber();
            else if (type_ == "boolean") typeOk = val.raw().is_boolean();
            else if (type_ == "array")   typeOk = val.isArray();
            else if (type_ == "object")  typeOk = val.isObject();

            if (!typeOk) {
                errors.push_back({name_, name_ + " must be " + type_, "type"});
                return errors;
            }
        }

        // String validations
        if (val.isString()) {
            auto str = val.get<std::string>();
            if (minLen_ >= 0 && static_cast<int>(str.size()) < minLen_) {
                errors.push_back({name_, name_ + " must be at least " + std::to_string(minLen_) + " characters", "minLength"});
            }
            if (maxLen_ >= 0 && static_cast<int>(str.size()) > maxLen_) {
                errors.push_back({name_, name_ + " must be at most " + std::to_string(maxLen_) + " characters", "maxLength"});
            }
            if (!pattern_.empty()) {
                std::regex re(pattern_);
                if (!std::regex_match(str, re)) {
                    auto ruleName = patternName_.empty() ? "pattern" : patternName_;
                    errors.push_back({name_, name_ + " is not a valid " + ruleName, ruleName});
                }
            }
            if (!enum_.empty()) {
                bool found = false;
                for (auto& v : enum_) if (v == str) { found = true; break; }
                if (!found) {
                    errors.push_back({name_, name_ + " must be one of the allowed values", "oneOf"});
                }
            }
        }

        // Number validations
        if (val.isNumber()) {
            double num = val.get<double>();
            if (min_.has_value() && num < *min_) {
                errors.push_back({name_, name_ + " must be >= " + std::to_string(*min_), "min"});
            }
            if (max_.has_value() && num > *max_) {
                errors.push_back({name_, name_ + " must be <= " + std::to_string(*max_), "max"});
            }
        }

        // Custom validators
        for (auto& fn : customValidators_) {
            auto err = fn(val);
            if (err.has_value()) {
                errors.push_back({name_, *err, "custom"});
            }
        }

        return errors;
    }

    const std::string& fieldName() const { return name_; }

private:
    std::string name_;
    bool required_ = false;
    std::string type_;
    int minLen_ = -1;
    int maxLen_ = -1;
    std::optional<double> min_;
    std::optional<double> max_;
    std::string pattern_;
    std::string patternName_;
    std::vector<std::string> enum_;
    std::vector<std::function<std::optional<std::string>(const JsonValue&)>> customValidators_;
};

// ── Schema — a collection of field rules ──
class Schema {
public:
    FieldRule& field(const std::string& name) {
        rules_.emplace_back(name);
        return rules_.back();
    }

    std::vector<ValidationError> validate(const JsonValue& body) const {
        std::vector<ValidationError> allErrors;
        for (auto& rule : rules_) {
            auto errs = rule.validate(body);
            allErrors.insert(allErrors.end(), errs.begin(), errs.end());
        }
        return allErrors;
    }

    bool isValid(const JsonValue& body) const {
        return validate(body).empty();
    }

private:
    std::vector<FieldRule> rules_;
};

// ── Convenience: create a schema inline ──
inline Schema schema() { return Schema(); }

// ── Validation middleware ──
inline http::MiddlewareFunction validate(const Schema& s) {
    return [s](http::Request& req, http::Response& res, http::NextFunction next) {
        auto errors = s.validate(req.body);
        if (!errors.empty()) {
            nlohmann::json errArr = nlohmann::json::array();
            for (auto& e : errors) {
                errArr.push_back({{"field", e.field}, {"message", e.message}, {"rule", e.rule}});
            }
            res.status(400).json(nlohmann::json{
                {"error", "Validation Failed"},
                {"errors", errArr}
            });
            return;
        }
        next();
    };
}

} // namespace nodepp::validator
