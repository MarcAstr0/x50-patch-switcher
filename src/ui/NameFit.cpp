#include "ui/NameFit.h"

#include <QFontMetrics>
#include <QtMath>

namespace NameFit {

namespace {
constexpr int kFloor = 2;      // DESIGN 5.3: 32 px font, cap height 18 px
constexpr int kInk = 12;       // ink rows of one line
constexpr int kPitch = 14;     // line pitch in font pixels

struct Metrics
{
    QVector<int> adv;  // per character of the text, font pixels
    int span(int from, int to) const
    {
        int w = 0;
        for (int i = from; i < to; ++i)
            w += adv[i];
        return w;
    }
};

Result make(int k, const QString &text, const Metrics &m, int from, int to)
{
    Result r;
    r.k = k;
    r.lines << text.mid(from, to - from);
    r.advances << k * m.span(from, to);
    return r;
}

// One break at the space that gives the smallest widest line; ties go to the earlier space.
bool balanced(const QString &text, const Metrics &m, int k, Result *out)
{
    int bestWidest = -1, bestAt = -1;
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] != QLatin1Char(' ') || i == 0 || i == text.size() - 1)
            continue;
        const int widest = qMax(m.span(0, i), m.span(i + 1, text.size()));
        if (bestAt < 0 || widest < bestWidest) {
            bestWidest = widest;
            bestAt = i;
        }
    }
    if (bestAt < 0)
        return false;
    out->k = k;
    out->lines = QStringList{text.left(bestAt), text.mid(bestAt + 1)};
    out->advances = QVector<int>{k * m.span(0, bestAt), k * m.span(bestAt + 1, text.size())};
    return true;
}

// Break anywhere (DESIGN 5.3 step 4), character by character; a character wider than the box
// still gets a line of its own so the loop ends.
Result wrapAnywhere(const QString &text, const Metrics &m, int k, int wBox)
{
    Result r;
    r.k = k;
    int start = 0, w = 0;
    for (int i = 0; i < text.size(); ++i) {
        const int a = k * m.adv[i];
        if (i > start && w + a > wBox) {
            r.lines << text.mid(start, i - start);
            r.advances << w;
            start = i;
            w = 0;
        }
        w += a;
    }
    r.lines << text.mid(start);
    r.advances << w;
    return r;
}
} // namespace

int kCap(double U) { return qMax(kFloor, qRound(4.5 * U)); }

int fontPixelAdvance(const QFontMetrics &fm16, const QString &text)
{
    int w = 0;
    for (const QChar c : text)
        w += qRound(double(fm16.horizontalAdvance(c)));
    return w;
}

Result fit(const QString &text, int wBox, int hBox, double U, const QFontMetrics &fm16)
{
    Metrics m;
    m.adv.reserve(text.size());
    for (const QChar c : text)
        m.adv << qRound(double(fm16.horizontalAdvance(c)));

    const int cap = kCap(U);
    const bool spaced = text.contains(QLatin1Char(' '));
    int s1 = 0, s2 = 0;
    Result two;
    for (int k = cap; k >= kFloor; --k) {
        if (!s1 && kInk * k <= hBox && k * m.span(0, text.size()) <= wBox)
            s1 = k;
        if (!s2 && spaced && (kInk + kPitch) * k <= hBox) {
            Result r;
            if (balanced(text, m, k, &r) && r.advances[0] <= wBox && r.advances[1] <= wBox) {
                s2 = k;
                two = r;
            }
        }
    }
    if (s2 && (!s1 || s2 > s1))
        return two;
    if (s1)
        return make(s1, text, m, 0, text.size());
    for (int k = kFloor; k >= 1; --k) {
        const Result r = wrapAnywhere(text, m, k, wBox);
        if (k == 1 || kInk * k + kPitch * k * (int(r.lines.size()) - 1) <= hBox)
            return r;
    }
    return make(1, text, m, 0, text.size());  // unreachable
}

} // namespace NameFit
