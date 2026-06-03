#pragma once
#include <vector>
#include <string>
#include "SAL_OBJECTS.h"

// This just needs to work for now. Really wish I could use python or smthn.
inline std::string acceptable_suffix = ".sal";

void lexer(SAL_FILE& input_file);