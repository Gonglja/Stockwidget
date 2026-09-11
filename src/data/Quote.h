#pragma once
#include <QMetaType>
#include <QString>
#include <QVector>

struct Quote {
    QString code;
    QString name;
    double open = 0, prevClose = 0, price = 0, high = 0, low = 0;
    double buy1 = 0, sell1 = 0, volume = 0, amount = 0;
    double avg = 0, committee = 0;
    bool etf = false;

    QString priceText, changeText, changePctText;
    QString b1Text, s1Text, committeeText, volumeText, amountText, avgText;
    int deltaSign = 0, committeeSign = 0, avgSign = 0, b1Sign = 0, s1Sign = 0;

    double kOpen = 0, kClose = 0, kHigh = 0, kLow = 0, kPrev = 0;
};

struct QuoteFormatOptions {
    enum class B1S1Display { Qty, Price, Both };
    bool shortCode = false;
    int nameLength = 0;
    B1S1Display b1s1 = B1S1Display::Qty;
};

Q_DECLARE_METATYPE(Quote)
