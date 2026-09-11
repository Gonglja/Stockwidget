#pragma once
#include <QString>

namespace Schedule {

// "HH:MM" -> 自午夜起的分钟数；非法返回 -1。
int minutesFromHhMm(const QString& hhmm);

// 判断当前是否处于允许时段。
// mode: "always" / "market" / "custom"
// start/end: 仅 custom 时使用（"HH:MM"）
// nowMinutes: 当前时刻（自午夜起的分钟数）
// weekday: 1=周一 .. 7=周日（QDate::dayOfWeek）
bool inWindow(const QString& mode, const QString& start, const QString& end, int nowMinutes,
              int weekday);

}  // namespace Schedule
