#pragma once

#include "../var.h"
#include <string>
#include <utility>

struct SalString : public SalVariable {
    std::string value;

    SalString(std::string val = {}) : SalVariable(SalType::STRING), value(std::move(val)) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};