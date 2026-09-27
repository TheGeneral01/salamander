#pragma once

#include "../var.h"
#include <string>

struct SalBool : public SalVariable {
    bool value;

    SalBool(bool val = false) : SalVariable(SalType::BOOL), value(val) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};