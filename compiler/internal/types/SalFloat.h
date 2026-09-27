#pragma once

#include "../var.h"

struct SalFloat : public SalVariable {
    float value = 0;

    SalFloat(float val = 0) : SalVariable(SalType::FLOAT), value(val) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};