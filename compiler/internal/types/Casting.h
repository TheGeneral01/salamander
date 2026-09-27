#pragma once

#include "SalBool.h"
#include "SalFloat.h"
#include "SalInt.h"
#include "SalString.h"
#include <stdexcept>
#include <string>

namespace sal_internal {
template <typename T>
std::unique_ptr<SalVariable> finishCast(std::unique_ptr<T> value, SalType sourceType) {
    value->lastType = sourceType;
    return value;
}
}

inline std::unique_ptr<SalVariable> SalInt::castTo(SalType targetType) const {
    switch (targetType) {
        case SalType::INT: return sal_internal::finishCast(std::make_unique<SalInt>(value), type);
        case SalType::FLOAT: return sal_internal::finishCast(std::make_unique<SalFloat>(static_cast<float>(value)), type);
        case SalType::BOOL: return sal_internal::finishCast(std::make_unique<SalBool>(value != 0), type);
        case SalType::STRING: return sal_internal::finishCast(std::make_unique<SalString>(std::to_string(value)), type);
        default: throw std::runtime_error("Unsupported cast from int");
    }
}

inline std::unique_ptr<SalVariable> SalFloat::castTo(SalType targetType) const {
    switch (targetType) {
        case SalType::FLOAT: return sal_internal::finishCast(std::make_unique<SalFloat>(value), type);
        case SalType::INT: return sal_internal::finishCast(std::make_unique<SalInt>(static_cast<int64_t>(value)), type);
        case SalType::BOOL: return sal_internal::finishCast(std::make_unique<SalBool>(value != 0), type);
        case SalType::STRING: return sal_internal::finishCast(std::make_unique<SalString>(std::to_string(value)), type);
        default: throw std::runtime_error("Unsupported cast from float");
    }
}

inline std::unique_ptr<SalVariable> SalBool::castTo(SalType targetType) const {
    switch (targetType) {
        case SalType::BOOL: return sal_internal::finishCast(std::make_unique<SalBool>(value), type);
        case SalType::INT: return sal_internal::finishCast(std::make_unique<SalInt>(value ? 1 : 0), type);
        case SalType::FLOAT: return sal_internal::finishCast(std::make_unique<SalFloat>(value ? 1.0f : 0.0f), type);
        case SalType::STRING: return sal_internal::finishCast(std::make_unique<SalString>(value ? "True" : "False"), type);
        default: throw std::runtime_error("Unsupported cast from bool");
    }
}

inline std::unique_ptr<SalVariable> SalString::castTo(SalType targetType) const {
    switch (targetType) {
        case SalType::STRING: return sal_internal::finishCast(std::make_unique<SalString>(value), type);
        case SalType::BOOL:
            if (value == "True" || value == "true") return sal_internal::finishCast(std::make_unique<SalBool>(true), type);
            if (value == "False" || value == "false") return sal_internal::finishCast(std::make_unique<SalBool>(false), type);
            throw std::runtime_error("Cannot cast string to bool: expected True or False");
        case SalType::FLOAT:
            try {
                std::size_t consumed = 0;
                float parsed = std::stof(value, &consumed);
                if (consumed != value.size()) throw std::invalid_argument("trailing characters");
                return sal_internal::finishCast(std::make_unique<SalFloat>(parsed), type);
            } catch (const std::exception&) {
                throw std::runtime_error("Cannot cast string to float: " + value);
            }
        case SalType::INT:
            try {
                std::size_t consumed = 0;
                long long parsed = std::stoll(value, &consumed);
                if (consumed != value.size()) throw std::invalid_argument("trailing characters");
                return sal_internal::finishCast(std::make_unique<SalInt>(static_cast<int64_t>(parsed)), type);
            } catch (const std::exception&) {
                throw std::runtime_error("Cannot cast string to int: " + value);
            }
        default: throw std::runtime_error("Unsupported cast from string");
    }
}
