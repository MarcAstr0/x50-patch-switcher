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
#include <QStringList>
#include <QTimer>
#include <QTransform>
#include <QVector>
#include <QWidget>

#include "core/SetModel.h"
#include "core/SoundAddress.h"
#include "ui/NameFit.h"
#include "ui/PixelRun.h"

class Catalogue;
class QFocusEvent;
class QKeyEvent;
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
// The view sends nothing. It turns input into signals and the caller acts (T4.2):
//  - a click or touch released inside a non-empty button, keys 1-8 and next/previous (which skip
//    empty slots, do not wrap, and start from the first or last slot when the current sound is not
//    in the set) emit slotClicked(index) for a non-empty slot; the current slot's own key or button
//    emits it again (re-send);
//  - Ctrl+Left/Right and [ / ] emit setStepRequested(-1/+1);
//  - Esc held 1 s or pressed twice within 1 s emits leaveRequested(); the first press arms the
//    hint. The two Esc timers are the one allowed timer under 2 s.
// Auto-repeat of any key does nothing. Every other key is swallowed. The widget takes no focus
// ring, never moves focus and shows no dialogs.
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
    // Names in the catalogue may have changed (unit names read): lays out again only if a name
    // that is shown differs from the one the layout was built with.
    void refresh();
    // The outcome of a send this view asked for (MainWindow connects X50Link::sent / sendFailed).
    // Ends the "target slot" if `address` is the sound it was waiting for. On success the lit slot
    // follows setCurrentSound; on failure the lit slot stays.
    void sendResolved(const SoundAddress &address);
    // A requested send that never reached the link (no link): forget the target.
    void clearTarget() { m_target = -1; }

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
    int targetSlot() const { return m_target; }     // -1: no send pending
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
    // A non-empty slot was chosen by click, touch or key (index is 0-based). Sends nothing itself.
    void slotClicked(int index);
    // Previous (-1) or next (+1) set was asked for. Nothing is sent for it.
    void setStepRequested(int direction);
    // Esc was held 1 s or pressed twice within 1 s.
    void leaveRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    bool focusNextPrevChild(bool next) override;
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
    int sd(double designPx) const;            // stroke width: max(1, round(v * u * dpr))
    int fontPx(double designPx) const;        // device px of a live text size, 11 px floor
    QRect toDevice(const QRect &logical) const;
    void prep(QStaticText &st, const QString &text, const QFont &font, int wrapWidth = -1) const;
    QFont textFont(int face, double designPx, double trackingDesign) const;
    int findLit() const;
    ButtonState stateOf(int i) const;
    int hit(const QPoint &pos) const;
    void setPressed(int idx);
    QStringList nameSignature() const;
    int stepTarget(int direction) const;      // next/previous non-empty slot from the target (else lit) one, or -1
    void choose(int slot);                    // remember the choice, then emit slotClicked
    void escPressed();
    void escReleased();
    void escTimeout();
    void resetEsc();

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
    int m_target = -1;            // slot of the newest send still in flight (Mode Change wait), -1: none
    int m_chosen = -1;            // slot last chosen here: lights it when a sound sits in two slots
    int m_pressed = -1;
    int m_cols = 4;
    QTimer m_flashTimer;
    QTimer m_escTimer;            // 1 s single-shot: the Esc hold, and the window for a second press
    bool m_escDown = false;
    QStringList m_nameSig;        // the names the layout was built with (refresh() compares)

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
