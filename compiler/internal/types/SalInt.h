/* =================================================================================================== */
/*                                                                                                     */
/*  Module: SalInt.h                                                                                   */
/*  Description: Defines the SAL integer runtime value.                                                 */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "../var.h"

struct SalInt : public SalVariable {
    int64_t value = 0;

    SalInt(int64_t val = 0) : SalVariable(SalType::INT), value(val) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};