#pragma once

#include <memory>
#include <string>
#include <cstdint>

// Type tag identifier
enum class SalType : uint8_t {
    INT,
    FLOAT,
    BOOL,
    STRING,
    CUSTOM_CLASS
};

// Abstract Base Class for all SAL Variables
struct SalVariable {
    SalType type;
    SalType lastType; // Tracks type history for Newt runtime/JIT stability checks
    uint16_t flags = 0;

    SalVariable(SalType initialType) 
        : type(initialType), lastType(initialType) {}

    virtual ~SalVariable() = default;

    // Pure virtual interface: Each derived variable class implements its own conversion logic
    virtual std::unique_ptr<SalVariable> castTo(SalType targetType) const = 0;

    // Helper for quick inspection
    SalType getType() const { return type; }
    SalType getLastType() const { return lastType; }
};

#include "types/SalBool.h"
#include "types/SalFloat.h"
#include "types/SalInt.h"
#include "types/SalString.h"
#include "types/Casting.h"