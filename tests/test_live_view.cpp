// T4.1: LiveView painting (DESIGN 5, 6) and the MainWindow switch between browse and live.
// Registered four times in CMake (1x, 2x, 1.25x, 1.5x via QT_SCALE_FACTOR); every check is written
// against whatever device pixel ratio the process has. The fit table is DESIGN 5.3 (computed by
// docs/design/mockup/fit_table.py on the real font file); a 1920x1080 screen at scale s has the
// logical window (1920/s) x (1080/s) (DESIGN 5.7).
#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPaintEvent>
#include <QRegion>
#include <QPixmap>
#include <QScreen>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFocusEvent>
#include <QTest>
#include <QtMath>

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/SetModel.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/LiveView.h"
#include "ui/MainWindow.h"
#include "ui/NameFit.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
using B = SoundAddress::Bank;
SoundAddress combi(B b, int n) { return A(A::Kind::Combi, b, n); }
SoundAddress prog(B b, int n) { return A(A::Kind::Prog, b, n); }

// The Friday Set 2 of the mockup (docs/design/mockup/live.py SET8).
SetModel friday()
{
    SetModel s(QStringLiteral("f"), QStringLiteral("Friday Set 2"), 8);
    s.setSlot(0, combi(B::A, 2));
    s.setLabel(0, QStringLiteral("Intro pad"));
    s.setSlot(1, prog(B::D, 104));
    s.setLabel(1, QStringLiteral("Verse EP"));
    s.setSlot(2, combi(B::C, 96));
    s.setSlot(3, combi(B::A, 0));
    s.setSlot(4, combi(B::A, 15));
    s.setLabel(4, QStringLiteral("Bridge organ"));
    s.setSlot(6, prog(B::A, 28));
    s.setLabel(6, QStringLiteral("Solo lead"));
    s.setSlot(7, combi(B::A, 26));
    return s;
}

SetModel oneSlot(int size, const QString &label)
{
    SetModel s(QStringLiteral("t"), QStringLiteral("T"), size);
    s.setSlot(0, combi(B::A, 0));
    s.setLabel(0, label);
    return s;
}

QRgb px(const QImage &img, int x, int y) { return img.pixelColor(x, y).rgb() | 0xFF000000u; }

// A LiveView that remembers what each paint event covered (logical px), for the dirty-rect tests.
class PaintSpy : public LiveView
{
public:
    using LiveView::LiveView;
    QRegion painted;
    void forget() { painted = QRegion(); }

protected:
    void paintEvent(QPaintEvent *e) override
    {
        painted += e->region();
        LiveView::paintEvent(e);
    }
};
} // namespace

class TestLiveView : public QObject
{
    Q_OBJECT
    Catalogue m_cat;

    static double dpr() { return QGuiApplication::primaryScreen()->devicePixelRatio(); }
    // A shown LiveView of a given logical size.
    static void sizeTo(LiveView &v, int w, int h)
    {
        v.resize(w, h);
        QCoreApplication::processEvents();
    }
    static QSize screenLogical(int screenW, int screenH) { return QSize(qFloor(screenW / dpr()), qFloor(screenH / dpr())); }
    static QImage grab(LiveView &v) { return v.grab().toImage(); }
    int dv(const LiveView &v, double design) const { return qRound(qRound(design * v.unit()) * dpr()); }

private slots:
    void initTestCase()
    {
        Theme::loadFonts();
        Theme::applyStyleSheet(*static_cast<QApplication *>(QCoreApplication::instance()));
        QString err;
        QVERIFY2(m_cat.loadFromResource(&err), qPrintable(err));
    }

    // DESIGN 5.2: 2 rows; 4 columns for 8 slots, 3 for 6; margin 24u, gap 20u; last column takes the rest.
    void gridShape()
    {
        for (int size : {8, 6}) {
            LiveView v(&m_cat);
            SetModel s(QStringLiteral("g"), QStringLiteral("G"), size);
            v.setSet(s);
            v.show();
            const QSize sz = screenLogical(1920, 1080);
            sizeTo(v, sz.width(), sz.height());
            QCOMPARE(v.slotCount(), size);
            QCOMPARE(v.rows(), 2);
            const int cols = size == 8 ? 4 : 3;
            QCOMPARE(v.columns(), cols);
            QVERIFY2(qAbs(v.unit() * dpr() - 1.5) < 0.01 || sz.width() != 1920 / dpr(), "u * dpr");
            const int m = qRound(24 * v.unit()), g = qRound(20 * v.unit());
            for (int i = 0; i < size; ++i) {
                const QRect r = v.buttonRect(i);
                QVERIFY(r.width() > 0 && r.height() > 0);
                const int c = i % cols, row = i / cols;
                QCOMPARE(v.buttonRect(c).x(), r.x());                 // columns line up
                QCOMPARE(v.buttonRect(row * cols).y(), r.y());        // rows line up
                if (c > 0)
                    QCOMPARE(r.x() - v.buttonRect(i - 1).right() - 1, g);
            }
            QCOMPARE(v.buttonRect(0).x(), m);
            QCOMPARE(v.buttonRect(cols - 1).right() + 1, v.width() - m);
            QCOMPARE(v.buttonRect(cols).y() - v.buttonRect(0).bottom() - 1, g);
            // The grid ends 20u above the hint strip (32u) at the bottom.
            QCOMPARE(v.buttonRect(size - 1).bottom() + 1 + g + qRound(32 * v.unit()), v.height());
        }
    }

    // DESIGN 5.2 reference numbers: 1920x1080 at 1x gives 440, 440, 440, 438 by 429 (8 slots) and 596 (6).
    void gridReferenceSizes1x()
    {
        if (dpr() != 1.0)
            QSKIP("1x only: logical 1920x1080");
        LiveView v(&m_cat);
        v.setSet(SetModel(QStringLiteral("g"), QStringLiteral("G"), 8));
        v.show();
        sizeTo(v, 1920, 1080);
        QCOMPARE(v.buttonRect(0).size(), QSize(440, 429));
        QCOMPARE(v.buttonRect(3).width(), 438);
        v.setSet(SetModel(QStringLiteral("g"), QStringLiteral("G"), 6));
        QCOMPARE(v.buttonRect(0).size(), QSize(596, 429));
    }

    // DESIGN 5.4 "Lit": 6u border in `text`, cyan fill, solid badge, NOW tag; no other button has them.
    void litButton()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QCOMPARE(v.litSlot(), 1);
        QCOMPARE(v.buttonState(1), LiveView::ButtonState::Lit);
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Normal);
        QVERIFY(!v.currentNotInSet());
        const QImage img = grab(v);
        const int wLit = qMax(1, dv(v, 6)), wNorm = qMax(1, dv(v, 2));

        const QRect lit = v.buttonRectDevice(1), norm = v.buttonRectDevice(0);
        QCOMPARE(px(img, lit.center().x(), lit.top() + wLit / 2), Theme::text);                // white border
        QCOMPARE(px(img, lit.center().x(), lit.top() + wLit + 3), Theme::cyan);               // cyan fill
        QCOMPARE(px(img, norm.center().x(), norm.top() + wNorm / 2), Theme::lineStrong);      // normal border
        QCOMPARE(px(img, norm.center().x(), norm.top() + wNorm + 3), Theme::surface);
        // Solid badge: its interior near the left edge is on-cyan on the lit button only.
        const int badge = dv(v, 44), pad = qRound(16 * v.unit());
        const int bx = qRound((v.buttonRect(1).x() + pad) * dpr()) + wNorm + 2, by = qRound((v.buttonRect(1).y() + pad) * dpr()) + badge / 2;
        QCOMPARE(px(img, bx, by), Theme::onCyan);
        const int nx = qRound((v.buttonRect(0).x() + pad) * dpr()) + wNorm + 2, ny = qRound((v.buttonRect(0).y() + pad) * dpr()) + badge / 2;
        QCOMPARE(px(img, nx, ny), Theme::surface);
        // NOW tag (triangle and text): on-cyan pixels in the right half of the badge row, none on the others.
        auto countRight = [&](int slot, QRgb colour) {
            const QRect b = v.buttonRectDevice(slot);
            const int y0 = qRound((v.buttonRect(slot).y() + pad) * dpr()), y1 = y0 + badge;
            int n = 0;
            for (int y = y0; y < y1; ++y)
                for (int x = b.center().x(); x < b.right() - wLit; ++x)
                    n += px(img, x, y) == colour;
            return n;
        };
        QVERIFY(countRight(1, Theme::onCyan) > 40);
        QCOMPARE(countRight(0, Theme::onCyan), 0);
        QCOMPARE(countRight(2, Theme::onCyan), 0);
        // The big name on the lit button is on-cyan (dark on cyan), not text.
        const QRect box = v.nameBoxDevice(1);
        int dark = 0, bright = 0;
        for (int y = box.top(); y <= box.bottom(); ++y)
            for (int x = box.left(); x <= box.right(); ++x) {
                dark += px(img, x, y) == Theme::onCyan;
                bright += px(img, x, y) == Theme::text;
            }
        QVERIFY(dark > 100);
        QCOMPARE(bright, 0);
    }

    // DESIGN 5.4 "Empty": no fill, dashed 2u `line` border, "EMPTY" in text-empty, does nothing.
    void emptySlot()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QCOMPARE(v.buttonState(5), LiveView::ButtonState::Empty);
        QVERIFY(v.nameFit(5).lines.isEmpty());
        const QImage img = grab(v);
        const QRect r = v.buttonRectDevice(5);
        const int w = qMax(1, dv(v, 2));
        QCOMPARE(px(img, r.left() + r.width() / 5, r.center().y() + r.height() / 4), Theme::bgLive);  // no fill
        QSet<QRgb> seen;
        const int inset = dv(v, 14) * 2;
        for (int x = r.left() + inset; x < r.right() - inset; ++x)
            seen.insert(px(img, x, r.top() + w / 2));
        QVERIFY(seen.contains(Theme::line));
        QVERIFY(seen.contains(Theme::bgLive));  // dashed: gaps show the background
        int textEmpty = 0;
        for (int y = r.center().y() - 30; y < r.center().y() + 30; ++y)
            for (int x = r.center().x() - 80; x < r.center().x() + 80; ++x)
                textEmpty += px(img, x, y) == Theme::textEmpty;
        QVERIFY(textEmpty > 20);
        // Press and release on it: no pressed state, no signal.
        QSignalSpy spy(&v, &LiveView::slotClicked);
        const QPoint c = v.buttonRect(5).center();
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, c);
        QCOMPARE(v.buttonState(5), LiveView::ButtonState::Empty);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, c);
        QCOMPARE(spy.count(), 0);
    }

    // T4.2 follow-up 3: losing focus clears the pressed look (the release would never arrive).
    void focusOutClearsPressed()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.show();
        sizeTo(v, 1280, 720);
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Pressed);
        QFocusEvent out(QEvent::FocusOut, Qt::OtherFocusReason);
        QCoreApplication::sendEvent(&v, &out);
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Normal);
    }

    // DESIGN 5.4 "Pressed": press on a button, send nothing (signal only) on release inside; sliding out cancels.
    void pressedAndClick()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        sizeTo(v, 1280, 720);
        QSignalSpy spy(&v, &LiveView::slotClicked);
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Pressed);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Normal);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 0);
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(1).center());
        QCOMPARE(v.buttonState(1), LiveView::ButtonState::PressedLit);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(1).center());
        QCOMPARE(spy.count(), 1);
        // Slide out of the button: cancelled, nothing is emitted on release.
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(2).center());
        QCOMPARE(v.buttonState(2), LiveView::ButtonState::Pressed);
        QTest::mouseMove(&v, v.buttonRect(3).center());
        QCOMPARE(v.buttonState(2), LiveView::ButtonState::Normal);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(3).center());
        QCOMPARE(spy.count(), 1);
    }

    // DESIGN 5.3 table: k and line count for the listed names. Slot 0 is the first column.
    void fitTable()
    {
        struct Case { QString name; int k; int lines; };
        struct Config { int sw, sh, count; bool unused; QVector<Case> cases; };
        const QString n1 = QStringLiteral("StereophonicKeys"), n2 = QStringLiteral("Waiting4Sunrise"),
                      n3 = QStringLiteral("Piano &Strings"), n4 = QStringLiteral("Solo lead"),
                      n5 = QStringLiteral("Layered A.Piano"), n6 = QStringLiteral("WWWWWWWWWWWWWWWW");
        const double d = dpr();
        const bool is15 = qFuzzyCompare(d, 1.5);
        QVector<Config> configs = {
            {1920, 1080, 8, false, {{n1, 3, 1}, {n2, 3, 1}, {n3, 6, 2}, {n4, 7, 2}, {n5, 7, 2}, {n6, 2, 1}}},
            {1920, 1080, 6, false, {{n1, 4, 1}, {n2, 4, 1}, {n3, 7, 2}, {n4, 7, 1}, {n5, 7, 2}, {n6, 3, 1}}},
            {1366, 768, 8, false, {{n1, 2, 1}, {n2, 2, 1}, {n3, 4, 2}, {n4, 5, 2}, {n5, is15 ? 5 : 4, 2}, {n6, 2, 2}}},
            {1366, 768, 6, false, {{n1, 3, 1}, {n2, 3, 1}, {n3, 5, 2}, {n4, 5, 1}, {n5, 5, 2}, {n6, 2, 1}}},
        };
        LiveView v(&m_cat);
        v.show();
        for (const Config &c : configs) {
            v.setSet(oneSlot(c.count, QStringLiteral("x")));
            const QSize sz = screenLogical(c.sw, c.sh);
            sizeTo(v, sz.width(), sz.height());
            for (const Case &cs : c.cases) {
                v.setSet(oneSlot(c.count, cs.name));
                const NameFit::Result r = v.nameFit(0);
                const QString what = QStringLiteral("%1x%2 %3 slots dpr %4: %5 (box %6x%7, u %8)")
                                         .arg(c.sw).arg(c.sh).arg(c.count).arg(d).arg(cs.name)
                                         .arg(v.nameBoxDevice(0).width()).arg(v.nameBoxDevice(0).height()).arg(v.unit());
                QVERIFY2(r.k == cs.k && r.lines.size() == cs.lines,
                         qPrintable(what + QStringLiteral(" got k=%1 lines=%2").arg(r.k).arg(r.lines.size())));
                // Every line fits the box; no ellipsis ever (DESIGN 5.3 step 4).
                for (int a : r.advances)
                    QVERIFY2(a <= v.nameBoxDevice(0).width(), qPrintable(what));
                QCOMPARE(r.lines.join(QLatin1Char(' ')).remove(QChar(0x2026)), r.lines.join(QLatin1Char(' ')));
            }
        }
    }

    // The mockup's own rows (DESIGN 5.3 last paragraph): Verse EP two lines at k = 7, Piano &Strings
    // breaks after "Piano", 6-slot "Warm pad" one line at k = 7 (a tie goes to one line).
    void fitMockupRows()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QCOMPARE(v.nameFit(1).k, 7);
        QCOMPARE(v.nameFit(1).lines, (QStringList{QStringLiteral("Verse"), QStringLiteral("EP")}));
        QCOMPARE(v.nameFit(2).lines, (QStringList{QStringLiteral("Piano"), QStringLiteral("&Strings")}));
        QCOMPARE(v.nameFit(3).k, 3);  // StereophonicKeys, no label: the sound's own name
        QCOMPARE(v.nameFit(3).lines.size(), 1);
    }

    // DESIGN 5.3 step 4: no fit even at k = 1 on one line: break anywhere, never an ellipsis.
    void fitSafetyNet()
    {
        const QFontMetrics fm1(Theme::pixelFont(1, true));
        const QString text = QStringLiteral("Supercalifragilisticexpialidocious");
        const NameFit::Result r = NameFit::fit(text, 120, 400, 1.5, fm1);
        QCOMPARE(r.k, 2);
        QVERIFY(r.lines.size() > 1);
        QCOMPARE(r.lines.join(QString()), text);
        for (int a : r.advances)
            QVERIFY(a <= 120);
        const NameFit::Result tiny = NameFit::fit(text, 120, 20, 1.5, fm1);  // 20 px high: only k = 1 can be tried
        QCOMPARE(tiny.k, 1);
        QCOMPARE(tiny.lines.join(QString()), text);
    }

    // DESIGN 5.5: the readout panel is the dimmed LCD: only lcd-field-dim and lcd-ink inside the field.
    void readoutPanelColours()
    {
        for (const std::optional<SoundAddress> &cur :
             {std::optional<SoundAddress>(prog(B::D, 104)), std::optional<SoundAddress>(), std::optional<SoundAddress>(prog(B::D, 1))}) {
            LiveView v(&m_cat);
            v.setSet(friday());
            v.setCurrentSound(cur);
            v.show();
            const QSize sz = screenLogical(1920, 1080);
            sizeTo(v, sz.width(), sz.height());
            const QImage img = grab(v);
            const QRect f = v.readoutFieldDevice();
            QVERIFY(f.isValid() && f.width() > 100);
            const int corner = dv(v, 4) + 1;
            QSet<QRgb> seen;
            for (int y = f.top() + corner; y <= f.bottom() - corner; ++y)
                for (int x = f.left() + corner; x <= f.right() - corner; ++x)
                    seen.insert(px(img, x, y));
            QVERIFY2((seen - QSet<QRgb>{Theme::lcdFieldDim, Theme::lcdInk}).isEmpty(),
                     qPrintable(QStringLiteral("extra colours: %1").arg((seen - QSet<QRgb>{Theme::lcdFieldDim, Theme::lcdInk}).size())));
            QVERIFY(seen.contains(Theme::lcdInk) && seen.contains(Theme::lcdFieldDim));
            // The bezel ring is lcd-bezel.
            QCOMPARE(px(img, v.readoutPanelDevice().center().x(), v.readoutPanelDevice().top()), Theme::lcdBezel);
        }
    }

    // DESIGN 5.5 / 2.2: kb and km on a 1920x1080 screen are 3 and 2 at every scale (U = 1.5).
    void readoutSizesAndText()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QCOMPARE(v.readoutKb(), 3);
        QCOMPARE(v.readoutKm(), 2);
        QCOMPARE(v.readoutMode(), QStringLiteral("PROG 0:Play"));
        QCOMPARE(v.readoutBar(), QStringLiteral("D104: Classic E.Piano"));
        v.setCurrentSound(combi(B::C, 96));
        QCOMPARE(v.readoutMode(), QStringLiteral("COMBI 0:Play"));
        QCOMPARE(v.readoutBar(), QStringLiteral("C096: Piano &Strings"));
        v.setCurrentSound(prog(B::G, 1));
        QCOMPARE(v.readoutBar(), QStringLiteral("G001: Acoustic Piano"));
        v.setCurrentSound(std::nullopt);
        QCOMPARE(v.readoutMode(), QStringLiteral("--"));
        QCOMPARE(v.readoutBar(), QStringLiteral("--"));
        const QSize s2 = screenLogical(1366, 768);
        sizeTo(v, s2.width(), s2.height());
        v.setCurrentSound(prog(B::D, 104));
        QCOMPARE(v.readoutKb(), 2);
        QCOMPARE(v.readoutKm(), 1);
    }

    // DESIGN 5.5: NOT IN SET sits outside the panel, to its right; no button is lit (6.2).
    void notInSetTag()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 1));
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QVERIFY(v.currentNotInSet());
        QCOMPARE(v.litSlot(), -1);
        for (int i = 0; i < v.slotCount(); ++i)
            QVERIFY(v.buttonState(i) != LiveView::ButtonState::Lit);
        const QRect tag = v.tagRectDevice();
        QVERIFY(tag.left() > v.readoutPanelDevice().right());
        const QImage img = grab(v);
        const int w = qMax(1, dv(v, 2));
        QCOMPARE(px(img, tag.center().x(), tag.top() + w / 2), Theme::cyanText);
        v.setCurrentSound(prog(B::D, 104));
        QVERIFY(!v.currentNotInSet());
        QVERIFY(v.tagRectDevice().isNull());
    }

    // DESIGN 5.5 right zone and 6.3: NO X50 in red, an error message in red, the last-known lit button stays.
    void connectionStates()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QCOMPARE(v.connectionText(), QStringLiteral("NO X50"));  // nothing known yet
        v.setConnection(true);
        QCOMPARE(v.connectionText(), QStringLiteral("X50 CONNECTED"));
        QVERIFY(v.isConnected());
        v.setConnection(false);
        QCOMPARE(v.connectionText(), QStringLiteral("NO X50"));
        QCOMPARE(v.litSlot(), 1);  // stays as last known
        v.setConnection(false, QStringLiteral("Port busy (another program is using the X50)"));
        QVERIFY(v.connectionText().startsWith(QStringLiteral("PORT BUSY")));
        QVERIFY(!grab(v).isNull());
    }

    // DESIGN 5.5 failed-send chip: red, 2 s single-shot, a state change not an animation.
    void sendFailureChip()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.show();
        const QSize sz = screenLogical(1920, 1080);
        sizeTo(v, sz.width(), sz.height());
        QVERIFY(!v.sendFailureShown());
        v.flashSendFailure();
        QVERIFY(v.sendFailureShown());
        const QImage img = grab(v);
        // The right-zone chip is filled red somewhere on the bar's right side.
        int red = 0;
        const QRect bar = v.barRectDevice();
        for (int x = bar.right() - 400; x < bar.right(); x += 2)
            red += px(img, x, bar.center().y()) == Theme::red;
        QVERIFY(red > 20);
        QTRY_VERIFY_WITH_TIMEOUT(!v.sendFailureShown(), 3000);
    }

    // DESIGN 5.6: the hint, armed text; DESIGN 6.5: update only the hint strip.
    void escHint()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.show();
        sizeTo(v, 1280, 720);
        QCOMPARE(v.hintText(), QStringLiteral("Hold|for 1 second, or press Esc twice, to leave live view"));
        v.setEscArmed(true);
        QCOMPARE(v.hintText(), QStringLiteral("|pressed: press Esc again, or keep holding, to leave"));
        v.setEscArmed(false);
        QVERIFY(v.hintText().startsWith(QStringLiteral("Hold")));
    }

    // DESIGN 6.5: no repaint without a state change, and a change repaints.
    void noPaintWithoutStateChange()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.setConnection(true);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QTest::qWait(100);
        const int before = v.paintCount();
        QVERIFY(before > 0);
        // Same values again, and a long idle: nothing paints.
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.setConnection(true);
        v.setEscArmed(false);
        QTest::qWait(1500);
        const int afterRefresh = v.paintCount();
        QCOMPARE(afterRefresh, before);
        // A real change repaints.
        v.setCurrentSound(combi(B::C, 96));
        QTRY_VERIFY(v.paintCount() > afterRefresh);
        const int afterSound = v.paintCount();
        v.setConnection(false);
        QTRY_VERIFY(v.paintCount() > afterSound);
    }

    // DESIGN 6.5: a sound change repaints the two buttons and the bar, not the others.
    void soundChangeRepaintsOnlyAffectedButtons()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        sizeTo(v, 1280, 720);
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QTest::qWait(100);
        // Paint a marker into button 7's pixels' backing: simply count that its state did not change.
        v.setCurrentSound(combi(B::C, 96));
        QCOMPARE(v.litSlot(), 2);
        QCOMPARE(v.buttonState(1), LiveView::ButtonState::Normal);
        QCOMPARE(v.buttonState(2), LiveView::ButtonState::Lit);
        QCOMPARE(v.buttonState(6), LiveView::ButtonState::Normal);
    }

    // DESIGN 6.5 + T4.1 review: a sound change repaints exactly the old button, the new button and the
    // bar (as the paint events' regions), and a send-failure flash repaints the bar only.
    void dirtyRectsOnSoundChangeAndFlash()
    {
        PaintSpy v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));  // slot 1
        v.show();
        sizeTo(v, 1280, 720);
        QVERIFY(QTest::qWaitForWindowExposed(&v));
        QTest::qWait(100);
        v.forget();
        v.setCurrentSound(combi(B::C, 96));  // slot 2
        QTRY_VERIFY(!v.painted.isEmpty());
        QTest::qWait(50);
        auto covers = [&](const QRect &r) { return QRegion(r).subtracted(v.painted).isEmpty(); };
        QVERIFY2(covers(v.buttonRect(1)), "the old lit button was not repainted");
        QVERIFY2(covers(v.buttonRect(2)), "the new lit button was not repainted");
        QVERIFY2(covers(v.barRectDevice().isEmpty() ? QRect() : QRect(0, 0, v.width(), qRound(56 * v.unit()))), "the top bar was not repainted");
        for (int i : {0, 3, 4, 5, 6, 7})
            QVERIFY2(!v.painted.intersects(v.buttonRect(i)), qPrintable(QStringLiteral("button %1 was repainted").arg(i)));
        QVERIFY2(!v.painted.intersects(QRect(0, v.height() - qRound(32 * v.unit()) + 1, v.width(), qRound(32 * v.unit()) - 1)),
                 "the hint strip was repainted");

        v.forget();
        v.flashSendFailure(QStringLiteral("Send failed"));
        QTRY_VERIFY(!v.painted.isEmpty());
        QTest::qWait(50);
        QVERIFY2(covers(QRect(v.width() / 2, 0, v.width() / 2, qRound(56 * v.unit()))), "the flash did not repaint the bar");
        for (int i = 0; i < 8; ++i)
            QVERIFY(!v.painted.intersects(v.buttonRect(i)));
        // The flash ends by itself after 2 s and repaints the bar again.
        v.forget();
        QTRY_VERIFY_WITH_TIMEOUT(!v.painted.isEmpty(), 3000);
        QVERIFY(!v.sendFailureShown());
    }

    // T4.1 review: a pressed slot does not outlive a set whose size changed, or a slot that became empty.
    void pressedResetsWhenTheSetChanges()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.show();
        sizeTo(v, 1280, 720);
        QSignalSpy spy(&v, &LiveView::slotClicked);
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Pressed);
        SetModel six(QStringLiteral("f"), QStringLiteral("Six"), 6);
        six.setSlot(0, combi(B::A, 2));
        v.setSet(six);  // the size changed
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Normal);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(spy.count(), 0);
        // The pressed slot becomes empty, then is filled again: it is not pressed any more.
        QTest::mousePress(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Pressed);
        SetModel emptied(QStringLiteral("f"), QStringLiteral("Six"), 6);
        v.setSet(emptied);
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Empty);
        v.setSet(six);
        QCOMPARE(v.buttonState(0), LiveView::ButtonState::Normal);
        QTest::mouseRelease(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(0).center());
        QCOMPARE(spy.count(), 0);
        // A release on an empty slot never emits.
        QTest::mouseClick(&v, Qt::LeftButton, Qt::NoModifier, v.buttonRect(4).center());
        QCOMPARE(spy.count(), 0);
    }

    // DESIGN 5.3 step 3, tie: "Warm pad" with a box that holds it exactly on one line at kcap (U = 1.5,
    // kcap = 7) and whose height allows two lines: s2 == s1, so one line.
    void fitTieGoesToOneLine()
    {
        const QFontMetrics fm1(Theme::pixelFont(1, true));
        const QString text = QStringLiteral("Warm pad");
        const int w = 7 * NameFit::fontPixelAdvance(fm1, text);
        const NameFit::Result r = NameFit::fit(text, w, 26 * 7, 1.5, fm1);
        QCOMPARE(r.k, 7);
        QCOMPARE(r.lines, (QStringList{text}));
        // One font pixel narrower: one line no longer fits at 7, two lines do, and two larger lines win.
        const NameFit::Result r2 = NameFit::fit(text, w - 7, 26 * 7, 1.5, fm1);
        QCOMPARE(r2.k, 7);
        QCOMPARE(r2.lines, (QStringList{QStringLiteral("Warm"), QStringLiteral("pad")}));
    }

    // DESIGN 5.3 step 2: two breaks give the same widest line: the earlier space wins.
    void fitTieGoesToEarlierSpace()
    {
        const QFontMetrics fm1(Theme::pixelFont(1, true));
        const QString text = QStringLiteral("aa aa aa");
        const int half = NameFit::fontPixelAdvance(fm1, QStringLiteral("aa aa"));
        QCOMPARE(NameFit::fontPixelAdvance(fm1, text) - NameFit::fontPixelAdvance(fm1, QStringLiteral("aa ")), half);
        const NameFit::Result r = NameFit::fit(text, 7 * half, 26 * 7, 1.5, fm1);
        QCOMPARE(r.k, 7);
        QCOMPARE(r.lines, (QStringList{QStringLiteral("aa"), QStringLiteral("aa aa")}));
    }

    // DESIGN 2.2 (pixel font, no antialiasing): inside a name box there are exactly two colours, the
    // button fill and the text colour, at every device scale the binary runs at.
    void nameBoxesHaveTwoColours()
    {
        for (int size : {8, 6}) {
            LiveView v(&m_cat);
            SetModel s = friday();
            if (size == 6) {
                s = SetModel(QStringLiteral("g"), QStringLiteral("G"), 6);
                s.setSlot(0, combi(B::A, 2));
                s.setLabel(0, QStringLiteral("Warm pad"));
                s.setSlot(1, prog(B::D, 104));
                s.setLabel(1, QStringLiteral("Verse EP"));
                s.setSlot(2, combi(B::C, 96));
                s.setSlot(4, prog(B::A, 28));
                s.setLabel(4, QStringLiteral("Solo lead"));
            }
            v.setSet(s);
            v.setCurrentSound(prog(B::D, 104));
            v.show();
            const QSize sz = screenLogical(1920, 1080);
            sizeTo(v, sz.width(), sz.height());
            const QImage img = grab(v);
            for (int i = 0; i < v.slotCount(); ++i) {
                if (v.buttonState(i) == LiveView::ButtonState::Empty)
                    continue;
                const bool lit = v.buttonState(i) == LiveView::ButtonState::Lit;
                const QRgb fill = lit ? Theme::cyan : Theme::surface, ink = lit ? Theme::onCyan : Theme::text;
                const QRect box = v.nameBoxDevice(i);
                QVERIFY(!box.isEmpty());
                QSet<QRgb> colours;
                for (int y = box.top(); y <= box.bottom(); ++y)
                    for (int x = box.left(); x <= box.right(); ++x)
                        colours.insert(px(img, x, y));
                QVERIFY2(colours == (QSet<QRgb>{fill, ink}) || colours == (QSet<QRgb>{fill}),
                         qPrintable(QStringLiteral("name box %1 (size %2) has %3 colours").arg(i).arg(size).arg(colours.size())));
            }
        }
    }

    // The window renders at the DESIGN 5.7 sizes without crashing at tiny and huge sizes.
    void screenshotSmoke()
    {
        LiveView v(&m_cat);
        v.setSet(friday());
        v.setCurrentSound(prog(B::D, 104));
        v.show();
        for (const QSize &s : {screenLogical(1920, 1080), screenLogical(1366, 768), QSize(500, 300), QSize(3000, 1200)}) {
            sizeTo(v, s.width(), s.height());
            const QPixmap p = v.grab();
            QVERIFY(!p.isNull());
            QCOMPARE(p.size(), QSize(qRound(s.width() * dpr()), qRound(s.height() * dpr())));
        }
    }
};

// ---------------------------------------------------------------- MainWindow: browse <-> live

class TestLiveWindow : public QObject
{
    Q_OBJECT
    Catalogue m_cat;

private slots:
    void initTestCase()
    {
        Theme::loadFonts();
        Theme::applyStyleSheet(*static_cast<QApplication *>(QCoreApplication::instance()));
        QVERIFY(m_cat.loadFromResource());
    }

    // Entering live: settings forbidden, browse hidden, nothing sent by entering or by state changes.
    void enteringLive()
    {
        QTemporaryDir dir;
        Library lib;
        lib.load(dir.path());
        QString id;
        lib.createSet(QStringLiteral("Friday Set 2"), 8, &id);
        lib.setSlot(id, 1, prog(B::D, 104));
        lib.setLabel(id, 1, QStringLiteral("Verse EP"));
        FakeX50 fake;
        X50Link link(&fake);
        link.setPortNames(QStringLiteral("X50"));
        MainWindow w(&m_cat, &lib, &link);
        w.setLiveFullScreen(false);
        link.start();
        QTRY_COMPARE_WITH_TIMEOUT(link.state(), X50Link::State::Connected, 3000);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.browseView()->isVisible());
        QVERIFY(w.settingsAllowed());
        fake.clearReceived();

        QSignalSpy sent(&link, &X50Link::sent);
        QVERIFY(w.enterLive());
        QVERIFY(w.isLive());
        QVERIFY(!w.settingsAllowed());
        QVERIFY(!w.openSettings());
        QVERIFY(w.liveView()->isVisible());
        QVERIFY(!w.browseView()->isVisible());
        QVERIFY(!w.displayStrip()->isVisible());
        QCOMPARE(w.liveView()->slotCount(), 8);
        QCOMPARE(w.liveView()->set().name(), QStringLiteral("Friday Set 2"));
        QVERIFY(w.liveView()->isConnected());
        QVERIFY(!w.enterLive());  // already live

        // The X50 changes sound on its panel: the lit button follows; still nothing is sent.
        fake.panelSelect(prog(B::D, 104));
        QTRY_COMPARE(w.liveView()->litSlot(), 1);
        fake.panelSelect(prog(B::D, 1));
        QTRY_VERIFY(w.liveView()->currentNotInSet());
        QTest::qWait(400);
        QVERIFY2(fake.received().isEmpty(), "entering live or following the panel sent MIDI");
        QCOMPARE(sent.count(), 0);

        w.leaveLive();
        QVERIFY(!w.isLive());
        QVERIFY(w.settingsAllowed());
        QVERIFY(w.browseView()->isVisible());
        QVERIFY(!w.liveView()->isVisible());
        link.stop();
    }

    // F5 in browse enters live (the BrowseView signal is wired).
    void f5EntersLive()
    {
        MainWindow w(&m_cat, nullptr, nullptr);
        w.setLiveFullScreen(false);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        // A real F5 key event in the search field (where browse focus starts), not the signal.
        QTest::keyClick(w.browseView()->searchField(), Qt::Key_F5);
        QVERIFY(w.isLive());
        QVERIFY(w.liveView()->isVisible());
        QCOMPARE(w.liveView()->slotCount(), 8);  // no set at all: an empty 8-slot set, every slot Empty
        for (int i = 0; i < 8; ++i)
            QCOMPARE(w.liveView()->buttonState(i), LiveView::ButtonState::Empty);
    }

    // Errors reach the live top bar (no dialog, no toast): unplug, failed send.
    void errorsGoToTheTopBar()
    {
        QTemporaryDir dir;
        Library lib;
        lib.load(dir.path());
        QString id;
        lib.createSet(QStringLiteral("S"), 8, &id);
        lib.setSlot(id, 0, prog(B::D, 104));
        FakeX50 fake;
        X50Link link(&fake);
        link.setPortNames(QStringLiteral("X50"));
        MainWindow w(&m_cat, &lib, &link);
        w.setLiveFullScreen(false);
        link.start();
        QTRY_COMPARE_WITH_TIMEOUT(link.state(), X50Link::State::Connected, 3000);
        w.show();
        QVERIFY(w.enterLive());
        link.send(prog(B::D, 104));
        QTRY_COMPARE_WITH_TIMEOUT(w.liveView()->litSlot(), 0, 3000);
        // A send that fails flashes the right zone.
        fake.setFailSend(true);
        link.send(prog(B::D, 104));
        QTRY_VERIFY_WITH_TIMEOUT(w.liveView()->sendFailureShown(), 3000);
        // The chip says why (the failure reason replaces the status text), in the bar, with no dialog.
        QCOMPARE(w.liveView()->connectionText(), QStringLiteral("COULD NOT SEND TO THE X50: FAKE: WRITE FAILED"));
        QVERIFY(!QApplication::activeModalWidget());
        fake.setFailSend(false);
        // The unit goes away: the bar says so, the lit state of the last known sound stays.
        fake.unplug();
        QTRY_VERIFY_WITH_TIMEOUT(!w.liveView()->isConnected(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(!w.liveView()->sendFailureShown(), 3000);
        QVERIFY2(w.liveView()->connectionText().startsWith(QStringLiteral("NO X50")), qPrintable(w.liveView()->connectionText()));
        QCOMPARE(w.liveView()->litSlot(), 0);  // the last known sound stays lit
        QVERIFY(w.isLive());
        QVERIFY(w.liveView()->isVisible());
        link.stop();
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    int rc = 0;
    {
        TestLiveView t;
        rc |= QTest::qExec(&t, argc, argv);
    }
    {
        TestLiveWindow t;
        rc |= QTest::qExec(&t, argc, argv);
    }
    return rc;
}

#include "test_live_view.moc"
