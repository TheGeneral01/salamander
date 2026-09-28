/* =================================================================================================== */
/*                                                                                                     */
/*  Module: SalFloat.h                                                                                 */
/*  Description: Defines the SAL floating-point runtime value.                                         */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "../var.h"

struct SalFloat : public SalVariable {
    float value = 0;

    SalFloat(float val = 0) : SalVariable(SalType::FLOAT), value(val) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};