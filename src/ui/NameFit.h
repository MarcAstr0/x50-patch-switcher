#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class QFontMetrics;

// The DESIGN 5.3 fit-to-width algorithm for the big text of a live button, in Pixel
// Operator X50 Bold at whole `k` device pixels per font pixel (font size 16k). Pure: it
// measures with a QFontMetrics of the same face at k = 1 (16 px, no hinting), whose advance
// of a character is whole font pixels, so a width at k is k times that sum (DESIGN 2.2).
// All lengths here are device pixels.
namespace NameFit {

struct Result
{
    int k = 1;                // device px per font pixel
    QStringList lines;        // one or two lines normally; any number in the safety net
    QVector<int> advances;    // per line, device px at k
    int height() const { return lines.isEmpty() ? 0 : 12 * k + 14 * k * (int(lines.size()) - 1); }
};

// Largest k: max(2, round(4.5 * U)), U = u * devicePixelRatio. Same for 6 and 8 slots.
int kCap(double U);
// Sum of the whole-font-pixel advances of `text`, in font pixels.
int fontPixelAdvance(const QFontMetrics &fm16, const QString &text);
// `wBox` x `hBox`: the name box in device px. Never returns an empty line list for a
// non-empty text.
Result fit(const QString &text, int wBox, int hBox, double U, const QFontMetrics &fm16);

} // namespace NameFit
