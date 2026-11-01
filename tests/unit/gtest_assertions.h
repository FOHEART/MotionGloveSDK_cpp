/**
 * @file  gtest_assertions.h
 * @brief 单元测试共享断言工具：浮点近似比较宏。
 *
 * @details
 * 依据《TDD与GoogleTest实践指南》的断言约定：
 *  - 浮点一律禁止 EXPECT_EQ(float, float)，统一使用 EXPECT_NEAR_FLOAT(a, b, eps)；
 *  - 失败时必须打印期望值与实际值，保证失败信息自解释；
 *  - 采用自定义宏而非 EXPECT_PRED_FORMAT3，避免框架额外传入 3 个字符串化
 *    表达式导致 "function does not take 6 arguments" 一类的编译错误。
 */
#pragma once

#include <cmath>
#include <gtest/gtest.h>

/**
 * @brief 浮点近似断言：|actual - expected| <= epsilon 即通过。
 * @param actual   实际值（允许 float/double/int 表达式）。
 * @param expected 期望值（解析解或手算金标准）。
 * @param epsilon  容差，取值 (0, +inf)；由数据通路最窄环节的精度决定，
 *                 float 往返用 1e-6，普通三角函数计算用 1e-4。
 * @note 失败时输出 expected / epsilon / actual 以及两侧表达式原文。
 */
#define EXPECT_NEAR_FLOAT(actual, expected, epsilon)                                 \
    do {                                                                             \
        const float mg_actual_   = static_cast<float>(actual);                       \
        const float mg_expected_ = static_cast<float>(expected);                     \
        if (!(std::fabs(mg_actual_ - mg_expected_) <= static_cast<float>(epsilon)))  \
        {                                                                            \
            ADD_FAILURE() << "expected " << mg_expected_                             \
                          << " (+/-" << epsilon << "), got " << mg_actual_           \
                          << "  (" #actual " vs " #expected ")";                     \
        }                                                                            \
    } while (0)
