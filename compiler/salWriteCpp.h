/* =================================================================================================== */
/*                                                                                                     */
/*  Module: salWriteCpp.h                                                                              */
/*  Description: Takes the given AST and transposes it to C++, then compiles out to g++.               */
/*  Date Last Updated: 9/23/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "SALItms.h"
#include "astObjs.h"
#include "fileScope.h"
#include <memory>
#include <unordered_map>

// For now, we will run and interpret directly in here.

class OutCPPFile {
    public:

    void makeVars(ScopeLayer mainScope) {
        // Looks through the entire file and creates an instance of SalVariable from this list.
        // First we need to figure out what the type is.
        for (const auto& itm : mainScope.variables) {
            if (itm.second->type.originalTxt == "bool") {

            }
        }
    }


};


