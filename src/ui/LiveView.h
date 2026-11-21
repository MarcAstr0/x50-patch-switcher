#pragma once

#include <optional>

#include <QBrush>
#include <QFont>
#include <QLineF>
#include <QPen>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QStaticText>
#include <QString>
#include <QTimer>
#include <QTransform>
#include <QVector>
#include <QWidget>

#include "core/SetModel.h"
#include "core/SoundAddress.h"
#include "ui/NameFit.h"
#include "ui/PixelRun.h"

class Catalogue;
class QPainter;

// The live view (SPEC 4.2, DESIGN 5 and 6): a top bar, a 2 x 3 or 2 x 4 grid of large buttons
// and the Esc hint, all painted by this one widget so that nothing allocates while painting.
//
// Everything that depends on size, device pixel ratio, set, sound or connection (the unit u,
// every rectangle, font, fitted pixel-font text, static text, polygon, pen and brush) is
// computed when that input changes, in layoutAll() / layoutBar() / layoutHint(); paintEvent
// only draws. update(rect) is called only for what a setter actually changed, so an idle view
// never repaints. There are no timers except the 2 s single-shot that ends the red "failed
// send" chip (DESIGN 5.5), which is a state change, not an animation.
//
// The view sends nothing. A click or touch released inside a non-empty button emits
// slotClicked(); what to do with it is the caller's (T4.2). The widget takes no focus ring
// and shows no dialogs.
class LiveView : public QWidget
{
    Q_OBJECT
public:
    // DESIGN 5.4: derived from (empty?, lit?, pressed?), never stored by hand.
    enum class ButtonState { Normal, Lit, Empty, Pressed, PressedLit };

    // `catalogue` gives the sound names (not owned; may be null).
    explicit LiveView(const Catalogue *catalogue, QWidget *parent = nullptr);

    // The set to show (a copy). Identical content is a no-op.
    void setSet(const SetModel &set);
    // The sound the X50 is on. nullopt: unknown (the readout shows "--", no button is lit).
    void setCurrentSound(const std::optional<SoundAddress> &sound);
    void clearCurrentSound() { setCurrentSound(std::nullopt); }
    // `message` replaces the default text ("X50 CONNECTED" / "NO X50"); a message with
    // connected == false is an error (red, up to two lines), DESIGN 5.5.
    void setConnection(bool connected, const QString &message = QString());
    // SPEC 4.3: a send that could not happen. Red chip on the right zone for 2 s; a non-empty
    // `reason` replaces the status text inside the chip meanwhile (errors show in the top bar only).
    void flashSendFailure(const QString &reason = QString());
    // DESIGN 5.6 armed text (first Esc pressed or Esc held). Used by T4.2.
    void setEscArmed(bool armed);
    // Names in the catalogue changed (unit names read): lay out again.
    void refresh();

    // ---- state and layout, for the screenshot harness and the tests ----
    const SetModel &set() const { return m_set; }
    int slotCount() const { return m_buttons.size(); }
    int columns() const { return m_cols; }
    int rows() const { return 2; }
    double unit() const { return m_u; }
    QRect buttonRect(int i) const;                  // logical px
    QRect buttonRectDevice(int i) const;            // device px, relative to the widget
    QRect nameBoxDevice(int i) const;               // the box the fit ran in
    NameFit::Result nameFit(int i) const;
    ButtonState buttonState(int i) const;
    int litSlot() const { return m_lit; }           // -1: none
    bool currentNotInSet() const { return m_notInSet; }
    std::optional<SoundAddress> currentSound() const { return m_current; }
    QString readoutMode() const { return m_readout.modeText; }   // "PROG 0:Play" / "--"
    QString readoutBar() const { return m_readout.barText; }     // as shown (maybe elided)
    QRect readoutPanelDevice() const { return m_readout.panel; }
    QRect readoutFieldDevice() const { return m_readout.field; }
    QRect barRectDevice() const { return m_bar.dev; }
    QRect hintRectDevice() const { return m_hint.dev; }
    QRect tagRectDevice() const { return m_notInSetTag.rect.toAlignedRect(); }
    int readoutKb() const { return m_readout.kb; }
    int readoutKm() const { return m_readout.km; }
    QString setNameShown() const { return m_bar.setText; }
    bool isConnected() const { return m_connected; }
    QString connectionText() const { return m_conn.shown; }
    bool sendFailureShown() const { return m_flash; }
    bool escArmed() const { return m_escArmed; }
    QString hintText() const { return m_hint.pre + QLatin1Char('|') + m_hint.post; }
    int paintCount() const { return m_paintCount; }

signals:
    // Released inside a non-empty button (index is 0-based). Sends nothing itself.
    void slotClicked(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct Button
    {
        bool empty = true;
        QRect rect;                  // logical
        QRect dev;                   // device px
        QRect nameBox;               // device px
        NameFit::Result fit;
        PixelRun::Run nameRun;
        QFont nameFont;
        QRectF badge;                // device px
        QStaticText badgeDigit;
        QPointF badgeDigitPos;
        QStaticText emptyText;
        QPointF emptyPos;
        QPolygonF triangle;
        QStaticText nowText;
        QPointF nowPos;
        QStaticText addrText;
        QPointF addrPos;
        bool hasSound = false;
        QStaticText soundText;
        QPointF soundPos;
        QFont soundFont;
    };
    struct Chip
    {
        QRectF rect;
        QStaticText text;
        QPointF textPos;
    };
    struct Bar
    {
        QRect rect;      // logical
        QRect dev, ruleDev;
        QString setText;
        QStaticText setStatic;
        QPointF setPos;
        QFont setFont;
    };
    struct Readout
    {
        bool known = false;
        QString modeText, barText;
        QRect panel, field, modeSeg, barSeg;   // device px
        PixelRun::Run modeRun, barRun;
        QFont modeFont, barFont;
        int kb = 1, km = 1;
        int outline = 1;
        bool visible = false;
    };
    struct Conn
    {
        QString shown;
        QStaticText text;
        QPointF textPos;
        QFont font;
        QRectF led;      // device px
        QRectF ring;     // the ring drawn inside the LED box
        QLineF diag;     // the "no" bar across the ring
        QRectF chip;
    };
    struct Hint
    {
        QRect rect;      // logical
        QRect dev;
        QString pre, post;
        QStaticText preText, postText, capText;
        QPointF prePos, postPos, capTextPos;
        QRectF cap;
    };

    // ---- layout (never called from paintEvent) ----
    void layoutAll();
    void layoutButtons();
    void layoutBar();
    void layoutHint();
    void buildTools();
    void syncScale();
    int dv(double designPx) const;            // round(round(v * u) * dpr)
    int fontPx(double designPx) const;        // device px of a live text size, 11 px floor
    QRect toDevice(const QRect &logical) const;
    void prep(QStaticText &st, const QString &text, const QFont &font, int wrapWidth = -1) const;
    QFont textFont(int face, double designPx, double trackingDesign) const;
    int findLit() const;
    ButtonState stateOf(int i) const;
    int hit(const QPoint &pos) const;
    void setPressed(int idx);

    // ---- painting ----
    void paintBar(QPainter &p);
    void paintButton(QPainter &p, int i);
    void paintHint(QPainter &p);

    const Catalogue *m_catalogue = nullptr;
    SetModel m_set;
    std::optional<SoundAddress> m_current;
    bool m_connected = false;
    QString m_connMessage;
    bool m_flash = false;
    QString m_flashText;
    bool m_escArmed = false;
    bool m_notInSet = false;
    int m_lit = -1;
    int m_pressed = -1;
    int m_cols = 4;
    QTimer m_flashTimer;

    double m_u = 1.0;
    qreal m_dpr = 0;
    bool m_syncQueued = false;
    int m_paintCount = 0;
    QTransform m_prepTransform;

    QVector<Button> m_buttons;
    Bar m_bar;
    Readout m_readout;
    Chip m_notInSetTag;
    Conn m_conn;
    Hint m_hint;

    // Fonts of the small texts.
    QFont m_fBadge, m_fTag, m_fAddr, m_fHint, m_fNotInSet;
    int m_radiusBezel = 0, m_radiusField = 0, m_radiusChip = 0;
    // Geometry in device px shared by every button.
    int m_radius = 0, m_badgeRadius = 0;
    qreal m_wNormal = 1, m_wLit = 1, m_wPressed = 1, m_wEmpty = 1, m_wBadge = 1;
    // Pens and brushes, so paintEvent constructs none.
    QPen m_penNone, m_penNormal, m_penLit, m_penPressed, m_penEmpty, m_penBadge, m_penBadgeEmpty;
    QPen m_penText, m_penTextDim, m_penTextEmpty, m_penOnCyan, m_penCyanText, m_penRed, m_penOnRed;
    QPen m_penLcdInk, m_penLcdField, m_penTag, m_penCap, m_penCapArmed, m_penRing, m_penRingOnRed;
    QBrush m_brNone, m_brSurface, m_brPressed, m_brCyan, m_brCyanText, m_brOnCyan, m_brBar, m_brLine;
    QBrush m_brBezel, m_brField, m_brInk, m_brRed, m_brBg;
};
