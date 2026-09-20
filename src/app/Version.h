#pragma once
#include <QString>

// 版本号由 CMake 注入（add_compile_definitions(SW_VERSION="x.y.z")，见 CMakeLists.txt）。
// 直接用其它方式编译时回落到 0.0.0，保证始终可编译。
#ifndef SW_VERSION
#define SW_VERSION "0.0.0"
#endif

namespace Version {
inline QString text() { return QStringLiteral(SW_VERSION); }              // 1.6.2
inline QString label() { return QStringLiteral("v") + text(); }           // v1.6.2
}  // namespace Version
