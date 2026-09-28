/* =================================================================================================== */
/*                                                                                                     */
/*  Module: SalBool.h                                                                                  */
/*  Description: Defines the SAL boolean runtime value.                                                  */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "../var.h"
#include <string>

struct SalBool : public SalVariable {
    bool value;

    SalBool(bool val = false) : SalVariable(SalType::BOOL), value(val) {}

    std::unique_ptr<SalVariable> castTo(SalType targetType) const override;
};