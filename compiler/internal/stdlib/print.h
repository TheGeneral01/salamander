/* =================================================================================================== */
/*                                                                                                     */
/*  Module: print.h                                                                                    */
/*  Description: Provides generic printing helpers for SAL standard-library values.                    */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// Helper to detect std::vector (or any contiguous container)
template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename Alloc>
struct is_vector<std::vector<T, Alloc>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// Primary print function
template <typename T>
void print(const T& arg) {
    // 1. Floating-point types (float, double)
    if constexpr (std::is_floating_point_v<T>) {
        std::cout << std::to_string(arg) << std::endl;
    } 
    // 2. Booleans (printed as "True" / "False")
    else if constexpr (std::is_same_v<T, bool>) {
        std::cout << (arg ? "True" : "False") << std::endl;
    } 
    // 3. std::string / string literals / char pointers
    else if constexpr (std::is_convertible_v<T, std::string_view>) {
        std::cout << arg << std::endl;
    } 
    // 4. std::vector types (prints formatted: [item1, item2, ...])
    else if constexpr (is_vector_v<T>) {
        std::cout << "[";
        for (size_t i = 0; i < arg.size(); ++i) {
            // Recursively handle vector elements (supports nested vectors!)
            if constexpr (std::is_same_v<typename T::value_type, std::string> || 
                          std::is_convertible_v<typename T::value_type, std::string_view>) {
                std::cout << "\"" << arg[i] << "\"";
            } else {
                // Non-newline print for sub-elements
                if constexpr (is_vector_v<typename T::value_type>) {
                    // For nested containers, inline printing without std::endl
                    print_inline(arg[i]);
                } else if constexpr (std::is_same_v<typename T::value_type, bool>) {
                    std::cout << (arg[i] ? "True" : "False");
                } else {
                    std::cout << arg[i];
                }
            }
            if (i + 1 < arg.size()) {
                std::cout << ", ";
            }
        }
        std::cout << "]" << std::endl;
    } 
    // 5. Types printable directly via operator<< (ints, custom printable classes)
    else if constexpr (requires(std::ostream& os, const T& a) { os << a; }) {
        std::cout << arg << std::endl;
    }
    // 6. Pointers (prints memory address)
    else if constexpr (std::is_pointer_v<T>) {
        std::cout << static_cast<const void*>(arg) << std::endl;
    }
    // 7. Fallback for unprintable types (prints typename / object address)
    else {
        std::cout << "<object at " << static_cast<const void*>(&arg) << ">" << std::endl;
    }
}