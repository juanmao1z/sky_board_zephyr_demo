/**
 * @file main.cpp
 * @brief 应用主入口：把控制权交给 app 初始化流程。
 */

#include "app/app_Init.hpp"

/**
 * @brief 应用入口函数。
 * @return app 层初始化结果码，0 表示成功，负值表示失败。
 */
int main(void) { return app::app_Init(); }
