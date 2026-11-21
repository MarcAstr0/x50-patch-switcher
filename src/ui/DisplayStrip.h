#pragma once

#include <optional>

#include <QBrush>
#include <QFont>
#include <QPen>
#include <QRect>
#include <QRectF>
#include <QStaticText>
#include <QString>
#include <QVector>
#include <QWidget>

#include "core/SoundAddress.h"

class Catalogue;
class QPainter;

// Browse view display strip (DESIGN 7.2): an LCD panel echoing the X50's display on the
// left, the connection LED and status on the dark background to its right (outside the
// panel). Painted with QPainter only; the panel in device-pixel space (DESIGN 2.2).
//
// Layout (boxes, fonts, elided text, static text) is computed when the state changes
// and when the size or device pixel ratio changes; paintEvent only draws. update() is
// called only when a setter actually changes something.
//
// Category text follows the Decisions log (2026-11-21): the two-digit number is shown
// only once the unit's category list has been read (Catalogue::hasUnitCategories());
// before that the line is "Category:<name>".
class DisplayStrip : public QWidget
{
    Q_OBJECT
public:
    // Every string the panel shows, for layout and for tests (DESIGN 7.2 "Exact strings").
    struct Texts
    {
        bool known = false;     // false: unknown-sound state (nothing sent, nothing followed)
        QString mode;           // "PROG 0:Play", "COMBI 0:Play", or "--"
        QString pageBold;       // "Program" or "Combination"; empty when unknown
        QString pageRegular;    // ":Program Select" or ":Combi Select"
        QString bank;           // "Bank C"; empty when unknown
        QString category;       // "Category 00:Keyboard" / "Category:Keyboard"; empty when unknown
        QString bar;            // "096: Piano &Strings", or "--" when unknown
        bool operator==(const Texts &o) const;
    };

    explicit DisplayStrip(QWidget *parent = nullptr);

    // Where names and categories come from (not owned; may be null). Call refresh()
    // after the catalogue's unit names or categories change.
    void setCatalogue(const Catalogue *catalogue);
    // The sound the X50 is on. Names and categories come from the catalogue; an
    // address the catalogue does not know shows "---" as its name and "--" as category.
    void setSound(const SoundAddress &address);
    // Nothing sent, nothing followed: the unknown-sound state.
    void clearSound();
    void refresh();
    // Connection zone on the right. `message` replaces the default text ("X50 CONNECTED"
    // or "NO X50"); a message with connected == false is an error (red, up to two lines).
    void setConnection(bool connected, const QString &message = QString());

    // Pure text composition (DESIGN 7.2).
    static Texts compose(const std::optional<SoundAddress> &sound, const Catalogue *catalogue);
    Texts texts() const { return m_texts; }
    QString connectionText() const { return m_connText; }
    bool isConnected() const { return m_connected; }

    // Strip height in logical px for a device pixel ratio: ceil(74k / dpr), k = max(1, round(dpr)).
    static int heightForScale(qreal dpr);
    int pixelScaleK() const { return m_k; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    // The height changed because k or the DPR changed (MainWindow recomputes its minimum).
    void heightChanged(int height);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;

private:
    // One character at an integer device position. Qt 6.4's FreeType engine keeps unrounded
    // advances, so a whole string drifts off the pixel grid after a few glyphs and grey
    // fringes appear; placing every glyph at the sum of whole-pixel advances keeps the face
    // crisp (DESIGN 2.2 rule 2 asks for integer positions).
    struct Glyph
    {
        QPoint pos;
        QString text;
    };
    using Run = QVector<Glyph>;

    struct Layout
    {
        // Device pixels, relative to the widget's top-left device pixel.
        QRect panel;          // bezel box
        QRect field;
        QRect modeSeg, pageSeg, bar;
        Run modeText, pageBoldText, pageRegText, bankText, categoryText, barText;
        bool clipped = false;
        // Logical px.
        QPointF connTextPos;
        // Device px (DESIGN 3.5: ring and LED snapped to whole device pixels).
        QRect ledDev;
    };

    static void drawRun(QPainter &p, const Run &run);
    void setTexts(const Texts &t);
    void relayout();
    // Re-lays out when the device pixel ratio changed; queued from paintEvent, never run there.
    void syncScale();

    const Catalogue *m_catalogue = nullptr;
    std::optional<SoundAddress> m_sound;
    Texts m_texts;
    bool m_connected = false;
    QString m_connText = QStringLiteral("NO X50");
    QString m_connMessage;

    qreal m_dpr = 0;
    int m_k = 0;
    int m_lastHeight = -1;
    bool m_syncQueued = false;
    // Pens and brushes built in relayout(), so paintEvent constructs none.
    QPen m_penPaper, m_penInk, m_penStatus, m_penRing, m_penNone;
    QBrush m_brushBezel, m_brushField, m_brushCyan, m_brushNone;
    qreal m_ringW = 3;  // device px
    QFont m_regular16, m_bold16, m_regular32, m_label;
    QStaticText m_connStatic;
    Layout m_l;
};
