#pragma once
#include "data/Quote.h"

namespace QuoteParser {
QVector<Quote> parseText(const QString& text, const QuoteFormatOptions& opt);
}
