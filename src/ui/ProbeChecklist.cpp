#include "ui/ProbeChecklist.h"

#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QResizeEvent>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextOption>

namespace {
// DESIGN 8.1 numbers.
constexpr int kRowMin = 32, kRowGap = 4, kGlyphBox = 24, kGlyphSize = 20, kTextIndent = 36;
constexpr int kHintTop = 4, kHintBottom = 8, kRulePx = 2, kRuleX = 12, kPlaceholderHeight = 96;
constexpr int kHintLineHeightPercent = 135;

QColor c(QRgb rgb) { return QColor::fromRgba(rgb); }

QColor glyphColor(ProbeChecklist::State s)
{
    switch (s) {
    case ProbeChecklist::State::Pass: return c(Theme::green);
    case ProbeChecklist::State::Fail: return c(Theme::red);
    case ProbeChecklist::State::Unknown: return c(Theme::amber);
    case ProbeChecklist::State::Running: return c(Theme::cyanText);
    }
    return c(Theme::text);
}

// The four shapes, centred in the 24 x 24 box `box`.
void paintGlyph(QPainter &p, const QRectF &box, ProbeChecklist::State s)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPointF m = box.center();
    const QColor col = glyphColor(s);
    const QRectF shape(m.x() - kGlyphSize / 2.0, m.y() - kGlyphSize / 2.0, kGlyphSize, kGlyphSize);
    switch (s) {
    case ProbeChecklist::State::Pass:
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawEllipse(shape);
        Glyphs::checkMark(p, shape.adjusted(4, 4, -4, -4), c(Theme::onCyan), 2.0);
        break;
    case ProbeChecklist::State::Fail:
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        p.drawRoundedRect(shape, Theme::radiusPill, Theme::radiusPill);
        Glyphs::cross(p, shape.adjusted(5, 5, -5, -5), c(Theme::onRed), 2.0);
        break;
    case ProbeChecklist::State::Unknown: {
        const qreal h = 10.5;  // half diagonal: 21 px across, inside the 24 px box with the stroke
        QPolygonF d;
        d << QPointF(m.x(), m.y() - h) << QPointF(m.x() + h, m.y()) << QPointF(m.x(), m.y() + h)
          << QPointF(m.x() - h, m.y());
        QPen pen(col, 2.0);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(d);
        QPen dash(col, 2.0);
        dash.setCapStyle(Qt::RoundCap);
        p.setPen(dash);
        p.drawLine(QPointF(m.x() - 3, m.y()), QPointF(m.x() + 3, m.y()));
        break;
    }
    case ProbeChecklist::State::Running: {
        p.setPen(QPen(col, 2.0));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(shape.adjusted(1, 1, -1, -1));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        for (int i = -1; i <= 1; ++i)
            p.drawEllipse(QPointF(m.x() + i * 5.0, m.y()), 1.5, 1.5);
        break;
    }
    }
    p.restore();
}
} // namespace

QString ProbeChecklist::placeholderText()
{
    return QStringLiteral("Run Check X50 to see whether the X50 is set up for this app.");
}

ProbeChecklist::ProbeChecklist(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("probeChecklist"));
    setFont(Theme::roleFont(Theme::Role::Body));
    QSizePolicy sp(QSizePolicy::Preferred, QSizePolicy::Minimum);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setFocusPolicy(Qt::NoFocus);
    relayout();
}

void ProbeChecklist::setRows(const QVector<Row> &rows)
{
    if (rows == m_rows)
        return;  // no repaint without a state change
    m_rows = rows;
    m_layoutWidth = -1;
    relayout();
    updateGeometry();
    update();
}

// Builds the hint documents for `width` and the row geometry. Called from setRows and
// resizeEvent, never from paintEvent.
void ProbeChecklist::buildHintDocuments(int width) const
{
    m_hints.clear();
    m_hints.reserve(m_rows.size());
    const QFont small = Theme::roleFont(Theme::Role::Small);
    const QFont strong = Theme::roleFont(Theme::Role::BodyStrong);
    QFont strongSmall = strong;
    strongSmall.setPixelSize(small.pixelSize());
    const int textWidth = qMax(40, width - kTextIndent);
    for (const Row &r : m_rows) {
        if (r.hint.isEmpty()) {
            m_hints.emplace_back();
            continue;
        }
        auto doc = std::make_unique<QTextDocument>();
        doc->setDocumentMargin(0);
        doc->setDefaultFont(small);
        QTextOption opt;
        opt.setWrapMode(QTextOption::WordWrap);
        doc->setDefaultTextOption(opt);
        QTextCursor cur(doc.get());
        QTextBlockFormat bf;
        bf.setLineHeight(kHintLineHeightPercent, QTextBlockFormat::ProportionalHeight);
        cur.setBlockFormat(bf);
        QTextCharFormat normal;
        normal.setFont(small);
        normal.setForeground(c(Theme::textDim));
        QTextCharFormat bold;
        bold.setFont(strongSmall);
        bold.setForeground(c(Theme::textDim));
        // Words between double quotes are the X50's own labels: strong weight, quotes included.
        const QStringList parts = r.hint.split(QLatin1Char('"'));
        for (int i = 0; i < parts.size(); ++i) {
            const bool quoted = (i % 2) == 1 && i < parts.size() - 1;
            if (quoted) {
                cur.insertText(QStringLiteral("\""), normal);
                cur.insertText(parts.at(i), bold);
                cur.insertText(QStringLiteral("\""), normal);
            } else {
                // An unpaired trailing quote part is plain text, with its opening quote back.
                if ((i % 2) == 1)
                    cur.insertText(QStringLiteral("\""), normal);
                cur.insertText(parts.at(i), normal);
            }
        }
        doc->setTextWidth(textWidth);
        m_hints.push_back(std::move(doc));
    }
}

int ProbeChecklist::layoutFor(int width, bool apply) const
{
    if (apply || m_layoutWidth != width) {
        buildHintDocuments(width);
        m_layoutWidth = width;
    }
    m_tops.resize(m_rows.size());
    m_heights.resize(m_rows.size());
    m_hintHeights.resize(m_rows.size());
    if (m_rows.isEmpty()) {
        m_totalHeight = kPlaceholderHeight;
        return m_totalHeight;
    }
    int y = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        int h = kRowMin;
        int hintH = 0;
        if (m_hints[static_cast<size_t>(i)]) {
            hintH = qCeil(m_hints[static_cast<size_t>(i)]->size().height());
            h += kHintTop + hintH + kHintBottom;
        }
        m_tops[i] = y;
        m_heights[i] = h;
        m_hintHeights[i] = hintH;
        y += h + (i + 1 < m_rows.size() ? kRowGap : 0);
    }
    m_totalHeight = y;
    return y;
}

void ProbeChecklist::relayout()
{
    layoutFor(qMax(width(), 100), true);
}

int ProbeChecklist::heightForWidth(int w) const
{
    // Measure with the width asked for, then restore the cache for the real width.
    const int h = layoutFor(qMax(w, 100), true);
    layoutFor(qMax(width(), 100), true);
    return h;
}

QSize ProbeChecklist::sizeHint() const
{
    return QSize(400, heightForWidth(400));
}

QSize ProbeChecklist::minimumSizeHint() const
{
    return QSize(200, m_rows.isEmpty() ? kPlaceholderHeight : kRowMin);
}

ProbeChecklist::RowGeometry ProbeChecklist::rowGeometry(int index) const
{
    if (index < 0 || index >= m_tops.size())
        return {};
    return {m_tops.at(index), m_heights.at(index)};
}

void ProbeChecklist::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (event->size().width() != m_layoutWidth) {
        relayout();
        updateGeometry();
    }
}

void ProbeChecklist::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    if (m_rows.isEmpty()) {
        const QRectF box(0, 0, width(), kPlaceholderHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(c(Theme::field));
        p.drawRoundedRect(box, Theme::radiusControl, Theme::radiusControl);
        p.setPen(c(Theme::textDim));
        p.setFont(Theme::roleFont(Theme::Role::Small));
        p.drawText(box.adjusted(Theme::s4, 0, -Theme::s4, 0), Qt::AlignCenter | Qt::TextWordWrap, placeholderText());
        return;
    }
    const QFont body = Theme::roleFont(Theme::Role::Body);
    p.setFont(body);
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row &r = m_rows.at(i);
        const int top = m_tops.at(i);
        const QRectF glyphBox(0, top + (kRowMin - kGlyphBox) / 2.0, kGlyphBox, kGlyphBox);
        paintGlyph(p, glyphBox, r.state);
        p.setPen(r.state == State::Running ? c(Theme::textDim) : c(Theme::text));
        p.setFont(body);
        p.drawText(QRect(kTextIndent, top, width() - kTextIndent, kRowMin), Qt::AlignVCenter | Qt::AlignLeft, r.text);
        const auto &doc = m_hints[static_cast<size_t>(i)];
        if (!doc)
            continue;
        const int hintTop = top + kRowMin + kHintTop;
        const int hintH = m_hintHeights.at(i);
        // The rule starts under the glyph and ends with the hint text.
        const qreal ruleTop = glyphBox.bottom() - 2;
        const QColor rule = r.state == State::Fail ? c(Theme::red) : c(Theme::amber);
        p.fillRect(QRectF(kRuleX - kRulePx / 2.0, ruleTop, kRulePx, hintTop + hintH - ruleTop), rule);
        p.save();
        p.translate(kTextIndent, hintTop);
        doc->drawContents(&p);
        p.restore();
    }
}
