/* =================================================================================================== */
/*                                                                                                     */
/*  Module: GpuScheduler.h                                                                             */
/*  Description: Analyzes parallel SAL loops and dispatches eligible work through Vulkan.              */
/*  Date Last Updated: 9/27/26                                                                         */
/*                                                                                                     */
/*  Author: Alexander Tuten - TheGeneral01                                                             */
/*  Github Repo: https://github.com/TheGeneral01/salamander                                            */
/*                                                                                                     */
/* =================================================================================================== */

#pragma once

#include "../astObjs.h"
#include <shaderc/shaderc.hpp>
#include <vulkan/vulkan.h>

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class GpuScheduler {
    struct ArrayInfo {
        std::vector<int32_t> values;
        uint32_t offset = 0;
    };

    struct Plan {
        WhileStmt* loop = nullptr;
        std::string induction;
        int32_t begin = 0;
        int32_t bound = 0;
        std::unordered_map<std::string, int32_t> scalars;
        std::unordered_map<std::string, std::string> strings;
        std::map<std::string, ArrayInfo> arrays;
        std::unordered_set<std::string> outputs;
        std::vector<std::string> inputs;
        std::vector<int32_t> packed;
        std::vector<std::string> arrayNames;
        uint32_t ignoredPrintCalls = 0;
    };

    struct GpuResult {
        std::vector<uint32_t> spirv;
        double interfaceMilliseconds = 0.0;
        std::string deviceName;
    };

    static void checkVk(VkResult result, const char* operation) {
        if (result != VK_SUCCESS) {
            throw std::runtime_error(std::string(operation) + " failed with VkResult " +
                                     std::to_string(result));
        }
    }

    static const VarExpr* variable(const Expr* expression) {
        return dynamic_cast<const VarExpr*>(expression);
    }

    static const LiteralExpr* integerLiteral(const Expr* expression) {
        auto literal = dynamic_cast<const LiteralExpr*>(expression);
        return literal && literal->value.type == INTLIT ? literal : nullptr;
    }

    static bool isVariable(const Expr* expression, const std::string& name) {
        auto item = variable(expression);
        return item && item->name.originalTxt == name;
    }

    static int32_t parseInteger(const Expr* expression) {
        auto literal = integerLiteral(expression);
        if (!literal) throw std::runtime_error("GPU loop values must be int literals");
        const int64_t value = std::stoll(literal->value.originalTxt);
        if (value < std::numeric_limits<int32_t>::min() || value > std::numeric_limits<int32_t>::max()) {
            throw std::runtime_error("GPU loop value exceeds int32 range");
        }
        return static_cast<int32_t>(value);
    }

    static bool parseIntegerList(const Expr* expression, std::vector<int32_t>& values) {
        auto list = dynamic_cast<const ListExpr*>(expression);
        if (!list) return false;
        for (const auto& item : list->itms) {
            auto literal = integerLiteral(item.get());
            if (!literal) return false;
            const int64_t value = std::stoll(literal->value.originalTxt);
            if (value < std::numeric_limits<int32_t>::min() ||
                value > std::numeric_limits<int32_t>::max()) return false;
            values.push_back(static_cast<int32_t>(value));
        }
        return true;
    }

    static bool safeIntegerExpression(const Expr* expression, const Plan& plan,
                                      const std::string& induction) {
        if (!expression) return false;
        if (integerLiteral(expression)) return true;
        if (auto item = variable(expression)) {
            return item->name.originalTxt == induction ||
                   plan.scalars.find(item->name.originalTxt) != plan.scalars.end();
        }
        if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
            auto collection = variable(index->target.get());
            return collection && isVariable(index->index.get(), induction) &&
                   plan.arrays.find(collection->name.originalTxt) != plan.arrays.end();
        }
        if (auto binary = dynamic_cast<const BinOpExpr*>(expression)) {
            const auto op = binary->op.type;
            if (op != ADD && op != SUB && op != MULT) return false;
            return safeIntegerExpression(binary->left.get(), plan, induction) &&
                   safeIntegerExpression(binary->right.get(), plan, induction);
        }
        if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
            return unary->op.type == SUB && safeIntegerExpression(unary->value.get(), plan, induction);
        }
        return false;
    }

    static bool int32Range(const Expr* expression, const Plan& plan, int64_t begin,
                           int64_t end, int64_t& minimum, int64_t& maximum) {
        if (auto literal = integerLiteral(expression)) {
            minimum = maximum = std::stoll(literal->value.originalTxt);
            return minimum >= std::numeric_limits<int32_t>::min() &&
                   maximum <= std::numeric_limits<int32_t>::max();
        }
        if (auto item = variable(expression)) {
            if (item->name.originalTxt == plan.induction) {
                minimum = begin;
                maximum = end - 1;
            } else {
                auto value = plan.scalars.find(item->name.originalTxt);
                if (value == plan.scalars.end()) return false;
                minimum = maximum = value->second;
            }
            return minimum >= std::numeric_limits<int32_t>::min() &&
                   maximum <= std::numeric_limits<int32_t>::max();
        }
        if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
            auto collection = variable(index->target.get());
            if (!collection) return false;
            auto found = plan.arrays.find(collection->name.originalTxt);
            if (found == plan.arrays.end() || begin < 0 ||
                end > static_cast<int64_t>(found->second.values.size())) return false;
            const auto first = found->second.values.begin() + begin;
            const auto last = found->second.values.begin() + end;
            if (first == last) return false;
            const auto bounds = std::minmax_element(first, last);
            minimum = *bounds.first;
            maximum = *bounds.second;
            return true;
        }
        if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
            if (unary->op.type != SUB) return false;
            int64_t childMinimum = 0;
            int64_t childMaximum = 0;
            if (!int32Range(unary->value.get(), plan, begin, end, childMinimum, childMaximum)) return false;
            minimum = -childMaximum;
            maximum = -childMinimum;
            return minimum >= std::numeric_limits<int32_t>::min() &&
                   maximum <= std::numeric_limits<int32_t>::max();
        }
        auto binary = dynamic_cast<const BinOpExpr*>(expression);
        if (!binary) return false;
        int64_t leftMinimum = 0;
        int64_t leftMaximum = 0;
        int64_t rightMinimum = 0;
        int64_t rightMaximum = 0;
        if (!int32Range(binary->left.get(), plan, begin, end, leftMinimum, leftMaximum) ||
            !int32Range(binary->right.get(), plan, begin, end, rightMinimum, rightMaximum)) return false;
        if (binary->op.type == ADD) {
            minimum = leftMinimum + rightMinimum;
            maximum = leftMaximum + rightMaximum;
        } else if (binary->op.type == SUB) {
            minimum = leftMinimum - rightMaximum;
            maximum = leftMaximum - rightMinimum;
        } else if (binary->op.type == MULT) {
            const int64_t candidates[] = {leftMinimum * rightMinimum, leftMinimum * rightMaximum,
                                          leftMaximum * rightMinimum, leftMaximum * rightMaximum};
            minimum = *std::min_element(std::begin(candidates), std::end(candidates));
            maximum = *std::max_element(std::begin(candidates), std::end(candidates));
        } else {
            return false;
        }
        return minimum >= std::numeric_limits<int32_t>::min() &&
               maximum <= std::numeric_limits<int32_t>::max();
    }

    static std::string glslExpression(const Expr* expression, const Plan& plan,
                                      const std::string& induction) {
        if (auto literal = integerLiteral(expression)) return literal->value.originalTxt;
        if (auto item = variable(expression)) {
            if (item->name.originalTxt == induction) return "iterationIndex";
            return item->name.originalTxt;
        }
        if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
            auto collection = variable(index->target.get());
            if (!collection) throw std::runtime_error("GPU index target must be a list variable");
            return "data[" + std::to_string(plan.arrays.at(collection->name.originalTxt).offset) +
                   "u + uint(iterationIndex)]";
        }
        if (auto binary = dynamic_cast<const BinOpExpr*>(expression)) {
            std::string op;
            switch (binary->op.type) {
                case ADD: op = "+"; break;
                case SUB: op = "-"; break;
                case MULT: op = "*"; break;
                case DIVD: op = "/"; break;
                case POW: return "int(pow(float(" + glslExpression(binary->left.get(), plan, induction) +
                                  "), float(" + glslExpression(binary->right.get(), plan, induction) + ")))";
                default: throw std::runtime_error("Unsupported GPU arithmetic operator");
            }
            return "(" + glslExpression(binary->left.get(), plan, induction) + " " + op + " " +
                   glslExpression(binary->right.get(), plan, induction) + ")";
        }
        if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
            return "(-" + glslExpression(unary->value.get(), plan, induction) + ")";
        }
        throw std::runtime_error("Unsupported GPU expression");
    }

    static int32_t evaluateInteger(const Expr* expression, const Plan& plan, int32_t iteration) {
        if (auto literal = integerLiteral(expression)) return parseInteger(literal);
        if (auto item = variable(expression)) {
            if (item->name.originalTxt == plan.induction) return iteration;
            return plan.scalars.at(item->name.originalTxt);
        }
        if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
            auto collection = variable(index->target.get());
            const auto& values = plan.arrays.at(collection->name.originalTxt).values;
            return values.at(static_cast<std::size_t>(iteration));
        }
        if (auto binary = dynamic_cast<const BinOpExpr*>(expression)) {
            const int32_t left = evaluateInteger(binary->left.get(), plan, iteration);
            const int32_t right = evaluateInteger(binary->right.get(), plan, iteration);
            switch (binary->op.type) {
                case ADD: return left + right;
                case SUB: return left - right;
                case MULT: return left * right;
                case DIVD:
                    if (right == 0) throw std::runtime_error("Division by zero in GPU loop");
                    return left / right;
                case POW: {
                    int32_t result = 1;
                    for (int32_t count = 0; count < right; ++count) result *= left;
                    return result;
                }
                default: throw std::runtime_error("Unsupported GPU arithmetic operator");
            }
        }
        if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
            return -evaluateInteger(unary->value.get(), plan, iteration);
        }
        throw std::runtime_error("Unsupported GPU expression");
    }

    static bool buildPlan(std::vector<std::unique_ptr<Stmt>>& statements, WhileStmt& loop, Plan& plan) {
        plan.loop = &loop;
        std::unordered_map<std::string, VarDeclStmt*> declarations;
        std::size_t loopPosition = statements.size();
        for (std::size_t index = 0; index < statements.size(); ++index) {
            if (statements[index].get() == &loop) loopPosition = index;
            if (auto declaration = dynamic_cast<VarDeclStmt*>(statements[index].get())) {
                declarations[declaration->name.originalTxt] = declaration;
                if (declaration->type.originalTxt == "int") {
                    if (auto literal = integerLiteral(declaration->init.get())) {
                        const int64_t value = std::stoll(literal->value.originalTxt);
                        if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max()) {
                            plan.scalars[declaration->name.originalTxt] = static_cast<int32_t>(value);
                        }
                    }
                } else if (declaration->type.originalTxt == "string" ||
                           declaration->type.originalTxt == "str") {
                    auto literal = dynamic_cast<LiteralExpr*>(declaration->init.get());
                    if (literal && literal->value.type == STRLIT) {
                        plan.strings[declaration->name.originalTxt] = literal->value.originalTxt;
                    }
                } else if (declaration->type.originalTxt == "list") {
                    ArrayInfo array;
                    if (!parseIntegerList(declaration->init.get(), array.values)) return false;
                    plan.arrays[declaration->name.originalTxt] = std::move(array);
                }
            }
        }
        if (loopPosition == statements.size()) return false;
        for (std::size_t index = 0; index < loopPosition; ++index) {
            if (!dynamic_cast<VarDeclStmt*>(statements[index].get()) &&
                !dynamic_cast<FuncDefStmt*>(statements[index].get())) {
                auto previousLoop = dynamic_cast<WhileStmt*>(statements[index].get());
                if (!previousLoop || !previousLoop->shader.gpuExecuted) return false;
            }
        }

        auto condition = dynamic_cast<BinOpExpr*>(loop.condition.get());
        auto induction = condition && condition->op.type == LESS
            ? variable(condition->left.get()) : nullptr;
        auto boundVariable = condition ? variable(condition->right.get()) : nullptr;
        auto boundCall = condition ? dynamic_cast<CallExpr*>(condition->right.get()) : nullptr;
        if (!induction || (!boundVariable && !boundCall)) return false;
        plan.induction = induction->name.originalTxt;
        auto initial = plan.scalars.find(plan.induction);
        if (initial == plan.scalars.end()) return false;
        plan.begin = initial->second;
        if (boundVariable) {
            auto bound = plan.scalars.find(boundVariable->name.originalTxt);
            if (bound == plan.scalars.end()) return false;
            plan.bound = bound->second;
        } else {
            const std::string suffix = ".length";
            if (!boundCall->args.empty() || !boundCall->funcName.originalTxt.ends_with(suffix)) return false;
            const std::string stringName = boundCall->funcName.originalTxt.substr(
                0, boundCall->funcName.originalTxt.size() - suffix.size());
            auto text = plan.strings.find(stringName);
            if (text == plan.strings.end() ||
                text->second.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) return false;
            plan.bound = static_cast<int32_t>(text->second.size());
        }
        if (plan.begin < 0 || plan.bound <= plan.begin || plan.bound - plan.begin > 1000000) return false;

        const BinOpExpr* counterUpdate = nullptr;
        for (const auto& statement : loop.body) {
            auto expression = dynamic_cast<ExprStmt*>(statement.get());
            if (!expression) return false;
            auto printCall = dynamic_cast<CallExpr*>(expression->expr.get());
            if (printCall && printCall->funcName.originalTxt == "print" && printCall->args.size() == 1) {
                ++plan.ignoredPrintCalls;
                continue;
            }
            auto assignment = dynamic_cast<BinOpExpr*>(expression->expr.get());
            if (!assignment || assignment->op.type != ASSIGN) return false;
            if (auto target = variable(assignment->left.get())) {
                auto increment = dynamic_cast<BinOpExpr*>(assignment->right.get());
                auto incrementTarget = increment ? variable(increment->left.get()) : nullptr;
                auto amount = increment ? integerLiteral(increment->right.get()) : nullptr;
                if (target->name.originalTxt != plan.induction || counterUpdate || !increment ||
                    increment->op.type != ADD || !incrementTarget ||
                    incrementTarget->name.originalTxt != plan.induction || !amount ||
                    amount->value.originalTxt != "1") return false;
                counterUpdate = assignment;
                continue;
            }
            auto target = dynamic_cast<IndexExpr*>(assignment->left.get());
            auto output = target ? variable(target->target.get()) : nullptr;
            if (!output || !isVariable(target->index.get(), plan.induction) ||
                plan.arrays.find(output->name.originalTxt) == plan.arrays.end() ||
                static_cast<std::size_t>(plan.bound) > plan.arrays.at(output->name.originalTxt).values.size() ||
                !safeIntegerExpression(assignment->right.get(), plan, plan.induction)) return false;
            plan.outputs.insert(output->name.originalTxt);
        }
        if (!counterUpdate || plan.outputs.empty()) return false;

        std::unordered_set<std::string> readArrays;
        std::unordered_set<std::string> usedArrays;
        std::unordered_set<std::string> usedScalars;
        auto collect = [&](auto&& self, const Expr* expression) -> void {
            if (!expression) return;
            if (auto item = variable(expression)) {
                if (item->name.originalTxt != plan.induction &&
                    plan.scalars.count(item->name.originalTxt)) usedScalars.insert(item->name.originalTxt);
            } else if (auto index = dynamic_cast<const IndexExpr*>(expression)) {
                if (auto array = variable(index->target.get())) readArrays.insert(array->name.originalTxt);
                self(self, index->index.get());
            } else if (auto binary = dynamic_cast<const BinOpExpr*>(expression)) {
                self(self, binary->left.get());
                self(self, binary->right.get());
            } else if (auto unary = dynamic_cast<const UnOpExpr*>(expression)) {
                self(self, unary->value.get());
            }
        };
        for (const auto& statement : loop.body) {
            auto expression = dynamic_cast<ExprStmt*>(statement.get());
            if (!expression || dynamic_cast<CallExpr*>(expression->expr.get())) continue;
            auto assignment = dynamic_cast<BinOpExpr*>(expression->expr.get());
            collect(collect, assignment->right.get());
        }
        for (const auto& output : plan.outputs) {
            if (readArrays.count(output)) return false;
            usedArrays.insert(output);
        }
        usedArrays.insert(readArrays.begin(), readArrays.end());
        plan.inputs.assign(readArrays.begin(), readArrays.end());
        for (const auto& name : usedArrays) {
            auto declaration = declarations.find(name);
            if (declaration == declarations.end() || declaration->second->type.originalTxt != "list") return false;
            if (static_cast<std::size_t>(plan.bound) > plan.arrays.at(name).values.size()) return false;
        }
        for (const auto& name : usedScalars) {
            if ((!boundVariable || name != boundVariable->name.originalTxt) &&
                plan.scalars.find(name) == plan.scalars.end()) return false;
        }
        for (const auto& statement : loop.body) {
            auto expression = dynamic_cast<const ExprStmt*>(statement.get());
            if (expression && dynamic_cast<const CallExpr*>(expression->expr.get())) continue;
            auto assignment = expression ? dynamic_cast<const BinOpExpr*>(expression->expr.get()) : nullptr;
            if (!assignment || variable(assignment->left.get())) continue;
            int64_t minimum = 0;
            int64_t maximum = 0;
            if (!int32Range(assignment->right.get(), plan, plan.begin, plan.bound, minimum, maximum)) return false;
        }

        uint32_t offset = 0;
        for (auto& [name, array] : plan.arrays) {
            if (!usedArrays.count(name)) continue;
            array.offset = offset;
            plan.arrayNames.push_back(name);
            offset += static_cast<uint32_t>(array.values.size());
            plan.packed.insert(plan.packed.end(), array.values.begin(), array.values.end());
        }
        return true;
    }

    static std::string generateShader(const Plan& plan) {
        std::string source =
            "#version 450\n"
            "layout(local_size_x = 64) in;\n"
            "layout(set = 0, binding = 0, std430) buffer State { int data[]; };\n"
            "void main() {\n"
            "    uint lane = gl_GlobalInvocationID.x;\n"
            "    int iterationIndex = " + std::to_string(plan.begin) + " + int(lane);\n"
            "    if (iterationIndex >= " + std::to_string(plan.bound) + ") return;\n";
        for (const auto& [name, value] : plan.scalars) {
            if (name != plan.induction) source += "    const int " + name + " = " + std::to_string(value) + ";\n";
        }
        for (const auto& statement : plan.loop->body) {
            auto expression = dynamic_cast<ExprStmt*>(statement.get());
            if (expression && dynamic_cast<CallExpr*>(expression->expr.get())) continue;
            auto assignment = expression ? dynamic_cast<BinOpExpr*>(expression->expr.get()) : nullptr;
            auto target = assignment ? dynamic_cast<IndexExpr*>(assignment->left.get()) : nullptr;
            if (!target) continue;
            auto output = variable(target->target.get());
            const auto base = plan.arrays.at(output->name.originalTxt).offset;
            source += "    data[" + std::to_string(base) + "u + uint(iterationIndex)] = " +
                      glslExpression(assignment->right.get(), plan, plan.induction) + ";\n";
        }
        source += "}\n";
        return source;
    }

    class VulkanDispatch {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        VkQueue queue = VK_NULL_HANDLE;
        uint32_t queueFamily = 0;
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkCommandPool commandPool = VK_NULL_HANDLE;
        std::string deviceName;

        uint32_t memoryType(uint32_t mask, VkMemoryPropertyFlags required) {
            VkPhysicalDeviceMemoryProperties properties{};
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
            for (uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
                if ((mask & (1u << index)) &&
                    (properties.memoryTypes[index].propertyFlags & required) == required) return index;
            }
            throw std::runtime_error("No host-visible coherent Vulkan memory type found");
        }

        void createDevice() {
            VkApplicationInfo app{};
            app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            app.pApplicationName = "SAL compiler";
            app.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo create{};
            create.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create.pApplicationInfo = &app;
            checkVk(vkCreateInstance(&create, nullptr, &instance), "vkCreateInstance");
            uint32_t count = 0;
            checkVk(vkEnumeratePhysicalDevices(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            if (!count) throw std::runtime_error("No Vulkan device available");
            std::vector<VkPhysicalDevice> devices(count);
            checkVk(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            for (auto candidate : devices) {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(candidate, &properties);
                uint32_t familyCount = 0;
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
                std::vector<VkQueueFamilyProperties> families(familyCount);
                vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
                for (uint32_t index = 0; index < familyCount; ++index) {
                    if (families[index].queueCount && (families[index].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                        physicalDevice = candidate;
                        queueFamily = index;
                        deviceName = properties.deviceName;
                        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
                    }
                }
                if (physicalDevice != VK_NULL_HANDLE) {
                    VkPhysicalDeviceProperties selected{};
                    vkGetPhysicalDeviceProperties(physicalDevice, &selected);
                    if (selected.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
                }
            }
            if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No Vulkan compute queue found");
            const float priority = 1.0f;
            VkDeviceQueueCreateInfo queueCreate{};
            queueCreate.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreate.queueFamilyIndex = queueFamily;
            queueCreate.queueCount = 1;
            queueCreate.pQueuePriorities = &priority;
            VkDeviceCreateInfo deviceCreate{};
            deviceCreate.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreate.queueCreateInfoCount = 1;
            deviceCreate.pQueueCreateInfos = &queueCreate;
            checkVk(vkCreateDevice(physicalDevice, &deviceCreate, nullptr, &device), "vkCreateDevice");
            vkGetDeviceQueue(device, queueFamily, 0, &queue);
        }

        void createBuffer(std::vector<int32_t>& packed) {
            VkBufferCreateInfo create{};
            create.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            create.size = packed.size() * sizeof(int32_t);
            create.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            create.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            checkVk(vkCreateBuffer(device, &create, nullptr, &buffer), "vkCreateBuffer");
            VkMemoryRequirements requirements{};
            vkGetBufferMemoryRequirements(device, buffer, &requirements);
            VkMemoryAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocate.allocationSize = requirements.size;
            allocate.memoryTypeIndex = memoryType(requirements.memoryTypeBits,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            checkVk(vkAllocateMemory(device, &allocate, nullptr, &memory), "vkAllocateMemory");
            checkVk(vkBindBufferMemory(device, buffer, memory, 0), "vkBindBufferMemory");
            void* mapped = nullptr;
            checkVk(vkMapMemory(device, memory, 0, create.size, 0, &mapped), "vkMapMemory(upload)");
            std::memcpy(mapped, packed.data(), static_cast<std::size_t>(create.size));
            vkUnmapMemory(device, memory);
        }

        void createPipeline(const std::vector<uint32_t>& spirv) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 0;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            VkDescriptorSetLayoutCreateInfo descriptorCreate{};
            descriptorCreate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descriptorCreate.bindingCount = 1;
            descriptorCreate.pBindings = &binding;
            checkVk(vkCreateDescriptorSetLayout(device, &descriptorCreate, nullptr, &descriptorLayout),
                    "vkCreateDescriptorSetLayout");
            VkPipelineLayoutCreateInfo pipelineLayoutCreate{};
            pipelineLayoutCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreate.setLayoutCount = 1;
            pipelineLayoutCreate.pSetLayouts = &descriptorLayout;
            checkVk(vkCreatePipelineLayout(device, &pipelineLayoutCreate, nullptr, &pipelineLayout),
                    "vkCreatePipelineLayout");
            VkShaderModuleCreateInfo moduleCreate{};
            moduleCreate.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            moduleCreate.codeSize = spirv.size() * sizeof(uint32_t);
            moduleCreate.pCode = spirv.data();
            VkShaderModule module = VK_NULL_HANDLE;
            checkVk(vkCreateShaderModule(device, &moduleCreate, nullptr, &module), "vkCreateShaderModule");
            VkPipelineShaderStageCreateInfo stage{};
            stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            stage.module = module;
            stage.pName = "main";
            VkComputePipelineCreateInfo pipelineCreate{};
            pipelineCreate.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineCreate.stage = stage;
            pipelineCreate.layout = pipelineLayout;
            const VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                                              &pipelineCreate, nullptr, &pipeline);
            vkDestroyShaderModule(device, module, nullptr);
            checkVk(result, "vkCreateComputePipelines");
            VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
            VkDescriptorPoolCreateInfo poolCreate{};
            poolCreate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolCreate.maxSets = 1;
            poolCreate.poolSizeCount = 1;
            poolCreate.pPoolSizes = &poolSize;
            checkVk(vkCreateDescriptorPool(device, &poolCreate, nullptr, &descriptorPool),
                    "vkCreateDescriptorPool");
            VkDescriptorSetAllocateInfo setAllocate{};
            setAllocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            setAllocate.descriptorPool = descriptorPool;
            setAllocate.descriptorSetCount = 1;
            setAllocate.pSetLayouts = &descriptorLayout;
            VkDescriptorSet set = VK_NULL_HANDLE;
            checkVk(vkAllocateDescriptorSets(device, &setAllocate, &set), "vkAllocateDescriptorSets");
            VkDescriptorBufferInfo bufferInfo{buffer, 0, VK_WHOLE_SIZE};
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = 0;
            write.descriptorCount = 1;
            write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            write.pBufferInfo = &bufferInfo;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
            descriptorSet = set;
        }

        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        void dispatch(std::size_t iterations) {
            VkCommandPoolCreateInfo poolCreate{};
            poolCreate.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolCreate.queueFamilyIndex = queueFamily;
            checkVk(vkCreateCommandPool(device, &poolCreate, nullptr, &commandPool), "vkCreateCommandPool");
            VkCommandBufferAllocateInfo allocate{};
            allocate.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate.commandPool = commandPool;
            allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate.commandBufferCount = 1;
            VkCommandBuffer command = VK_NULL_HANDLE;
            checkVk(vkAllocateCommandBuffers(device, &allocate, &command), "vkAllocateCommandBuffers");
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            checkVk(vkBeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
            VkBufferMemoryBarrier uploadBarrier{};
            uploadBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            uploadBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            uploadBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            uploadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            uploadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            uploadBarrier.buffer = buffer;
            uploadBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                 0, 0, nullptr, 1, &uploadBarrier, 0, nullptr);
            vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout,
                                    0, 1, &descriptorSet, 0, nullptr);
            vkCmdDispatch(command, static_cast<uint32_t>((iterations + 63) / 64), 1, 1);
            VkBufferMemoryBarrier readbackBarrier{};
            readbackBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            readbackBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            readbackBarrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
            readbackBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            readbackBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            readbackBarrier.buffer = buffer;
            readbackBarrier.size = VK_WHOLE_SIZE;
            vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                                 0, 0, nullptr, 1, &readbackBarrier, 0, nullptr);
            checkVk(vkEndCommandBuffer(command), "vkEndCommandBuffer");
            VkFenceCreateInfo fenceCreate{};
            fenceCreate.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence = VK_NULL_HANDLE;
            checkVk(vkCreateFence(device, &fenceCreate, nullptr, &fence), "vkCreateFence");
            VkSubmitInfo submit{};
            submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command;
            checkVk(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit");
            checkVk(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
            vkDestroyFence(device, fence, nullptr);
        }

    public:
        ~VulkanDispatch() { cleanup(); }

        GpuResult prepare(const std::string& source, const std::vector<int32_t>& initial) {
            shaderc::Compiler compiler;
            shaderc::CompileOptions options;
            options.SetSourceLanguage(shaderc_source_language_glsl);
            options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
            options.SetOptimizationLevel(shaderc_optimization_level_performance);
            auto binary = compiler.CompileGlslToSpv(source, shaderc_compute_shader, "sal_parallel.comp", options);
            if (binary.GetCompilationStatus() != shaderc_compilation_status_success) {
                throw std::runtime_error("ShaderC compilation failed: " + binary.GetErrorMessage());
            }
            GpuResult result;
            result.spirv.assign(binary.cbegin(), binary.cend());
            const auto started = std::chrono::steady_clock::now();
            createDevice();
            auto packed = initial;
            createBuffer(packed);
            createPipeline(result.spirv);
            result.interfaceMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            result.deviceName = deviceName;
            result.spirv.shrink_to_fit();
            resultWords = std::move(packed);
            return result;
        }

        std::vector<int32_t> dispatchAndReadback(std::size_t iterations) {
            dispatch(iterations);
            void* mapped = nullptr;
            checkVk(vkMapMemory(device, memory, 0, resultWords.size() * sizeof(int32_t), 0, &mapped),
                    "vkMapMemory(readback)");
            std::memcpy(resultWords.data(), mapped, resultWords.size() * sizeof(int32_t));
            vkUnmapMemory(device, memory);
            return resultWords;
        }

        std::vector<int32_t> resultWords;

        void cleanup() {
            if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
            if (device != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
            if (device != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            if (device != VK_NULL_HANDLE && pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
            if (device != VK_NULL_HANDLE && pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (device != VK_NULL_HANDLE && descriptorLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
            if (device != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, buffer, nullptr);
            if (device != VK_NULL_HANDLE && memory != VK_NULL_HANDLE) vkFreeMemory(device, memory, nullptr);
            if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
            if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
            device = VK_NULL_HANDLE;
            instance = VK_NULL_HANDLE;
        }
    };

    static void replaceListInitializer(VarDeclStmt& declaration, const std::vector<int32_t>& values) {
        auto list = std::make_unique<ListExpr>();
        list->type = makeToken(TYPE, "int", declaration.name.line, declaration.name.col);
        for (int32_t value : values) {
            auto literal = std::make_unique<LiteralExpr>();
            literal->value = makeToken(INTLIT, std::to_string(value), declaration.name.line, declaration.name.col);
            list->itms.push_back(std::move(literal));
        }
        declaration.init = std::move(list);
    }

    bool runOne(std::vector<std::unique_ptr<Stmt>>& statements, WhileStmt* candidate) {
        if (!candidate) return false;
        Plan plan;
        try {
            if (!buildPlan(statements, *candidate, plan)) return false;
            GLSL_shader artifact;
            artifact.source = generateShader(plan);
            artifact.inductionVariable = plan.induction;
            artifact.invariantBound = std::to_string(plan.bound);
            artifact.stateVariables = plan.arrayNames;
            artifact.inputBuffers = plan.inputs;
            artifact.outputBuffers.assign(plan.outputs.begin(), plan.outputs.end());
            artifact.descriptorSetCount = 1;
            artifact.workgroupSize = 64;
            artifact.ignoredPrintCalls = plan.ignoredPrintCalls;
            artifact.estimatedIterations = static_cast<uint64_t>(plan.bound - plan.begin);
            artifact.parallelEligible = true;
            artifact.gpuEligible = true;
            artifact.invariantBoundHoisted = true;
            artifact.counterStrengthReduced = true;

            volatile int32_t sample = 0;
            const auto sampleStart = std::chrono::steady_clock::now();
            const std::vector<int32_t> workerStateSample = plan.packed;
            if (!workerStateSample.empty()) sample = workerStateSample.front();
            for (const auto& statement : candidate->body) {
                auto expression = dynamic_cast<ExprStmt*>(statement.get());
                if (!expression || dynamic_cast<CallExpr*>(expression->expr.get())) continue;
                auto assignment = dynamic_cast<BinOpExpr*>(expression->expr.get());
                auto target = dynamic_cast<IndexExpr*>(assignment->left.get());
                if (!target) continue;
                auto output = variable(target->target.get());
                sample = evaluateInteger(assignment->right.get(), plan, plan.begin);
                (void)output;
            }
            (void)sample;
            const double sampleMs = std::max(0.000001, std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - sampleStart).count());
            const double estimatedCpuMs = sampleMs * static_cast<double>(plan.bound - plan.begin);
            constexpr double conservativeVulkanStartupFloorMs = 2.0;
            if (estimatedCpuMs <= conservativeVulkanStartupFloorMs * 1.25) {
                std::cout << "GPU loop candidate stayed on CPU: estimated " << estimatedCpuMs
                          << " ms is below the conservative Vulkan startup floor.\n";
                return false;
            }

            VulkanDispatch dispatch;
            GpuResult result = dispatch.prepare(artifact.source, plan.packed);
            if (estimatedCpuMs <= result.interfaceMilliseconds * 1.25) {
                std::cout << "GPU loop candidate stayed on CPU: estimated " << estimatedCpuMs
                          << " ms versus Vulkan interface " << result.interfaceMilliseconds
                          << " ms; 25% crossover margin not met.\n";
                return false;
            }
            auto outputWords = dispatch.dispatchAndReadback(
                static_cast<std::size_t>(plan.bound - plan.begin));

            artifact.SPIRV_SHADER = std::move(result.spirv);
            artifact.gpuExecuted = true;
            candidate->shader.SPIRV_SHADER = artifact.SPIRV_SHADER;
            candidate->shader.source = artifact.source;
            candidate->shader.stateVariables = artifact.stateVariables;
            candidate->shader.inputBuffers = artifact.inputBuffers;
            candidate->shader.outputBuffers = artifact.outputBuffers;
            candidate->shader.inductionVariable = artifact.inductionVariable;
            candidate->shader.invariantBound = artifact.invariantBound;
            candidate->shader.descriptorSetCount = artifact.descriptorSetCount;
            candidate->shader.workgroupSize = artifact.workgroupSize;
            candidate->shader.ignoredPrintCalls = artifact.ignoredPrintCalls;
            candidate->shader.estimatedIterations = artifact.estimatedIterations;
            candidate->shader.parallelEligible = artifact.parallelEligible;
            candidate->shader.gpuEligible = artifact.gpuEligible;
            candidate->shader.gpuExecuted = artifact.gpuExecuted;
            candidate->shader.invariantBoundHoisted = artifact.invariantBoundHoisted;
            candidate->shader.counterStrengthReduced = artifact.counterStrengthReduced;

            uint32_t offset = 0;
            for (const auto& name : plan.arrayNames) {
                auto declaration = std::find_if(statements.begin(), statements.end(), [&](const auto& statement) {
                    auto variableDeclaration = dynamic_cast<VarDeclStmt*>(statement.get());
                    return variableDeclaration && variableDeclaration->name.originalTxt == name;
                });
                if (declaration != statements.end() && plan.outputs.count(name)) {
                    auto& values = plan.arrays.at(name).values;
                    std::copy_n(outputWords.begin() + offset, values.size(), values.begin());
                    replaceListInitializer(*static_cast<VarDeclStmt*>(declaration->get()), values);
                }
                offset += static_cast<uint32_t>(plan.arrays.at(name).values.size());
            }
            auto inductionDeclaration = std::find_if(statements.begin(), statements.end(), [&](const auto& statement) {
                auto variableDeclaration = dynamic_cast<VarDeclStmt*>(statement.get());
                return variableDeclaration && variableDeclaration->name.originalTxt == plan.induction;
            });
            if (inductionDeclaration != statements.end()) {
                auto literal = std::make_unique<LiteralExpr>();
                literal->value = makeToken(INTLIT, std::to_string(plan.bound), 0, 0);
                static_cast<VarDeclStmt*>(inductionDeclaration->get())->init = std::move(literal);
            }
            std::cout << "GPU parallel while on " << result.deviceName << ": "
                      << artifact.estimatedIterations << " iterations, CPU estimate "
                      << estimatedCpuMs << " ms, Vulkan interface " << result.interfaceMilliseconds
                      << " ms (ShaderC optimized).\n";
            if (artifact.ignoredPrintCalls != 0) {
                std::cout << "GPU while ignored " << artifact.ignoredPrintCalls
                          << " print call(s).\n";
            }
            return true;
        } catch (const std::exception& error) {
            std::cerr << "GPU loop fallback: " << error.what() << '\n';
            return false;
        }
    }

public:
    bool run(std::vector<std::unique_ptr<Stmt>>& statements) {
        bool anyExecuted = false;
        for (const auto& statement : statements) {
            if (auto loop = dynamic_cast<WhileStmt*>(statement.get())) {
                anyExecuted = runOne(statements, loop) || anyExecuted;
            }
        }
        return anyExecuted;
    }
};
