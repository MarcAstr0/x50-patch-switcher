// T3.1: DisplayStrip. Strings are DESIGN 7.2 "Exact strings"; the unit-category rule is
// the Decisions log entry of 2026-11-21 (number only after the unit's list was read).
// Registered twice in CMake: at 1x and with QT_SCALE_FACTOR=2, so the checks below are
// written against whatever device pixel ratio the process has.
#include <QApplication>
#include <QFontMetricsF>
#include <QImage>
#include <QSet>
#include <QPixmap>
#include <QTest>

#include "core/Catalogue.h"
#include "ui/DisplayStrip.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
SoundAddress combiC096() { return A(A::Kind::Combi, A::Bank::C, 96); }
SoundAddress progG001() { return A(A::Kind::Prog, A::Bank::G, 1); }
SoundAddress progA000() { return A(A::Kind::Prog, A::Bank::A, 0); }

QStringList sixteen(const QString &first, int at = 0) {
    QStringList l;
    for (int i = 0; i < 16; ++i)
        l << QStringLiteral("cat%1").arg(i);
    l[at] = first;
    return l;
}
} // namespace

class TestDisplayStrip : public QObject {
    Q_OBJECT
    Catalogue m_cat;

private slots:
    void initTestCase() {
        Theme::loadFonts();
        QString err;
        QVERIFY2(m_cat.loadFromResource(&err), qPrintable(err));
    }

    // DESIGN 7.2: COMBI C096 "Piano &Strings", category Keyboard (data/x50-factory.tsv).
    void stringsCombiC096() {
        const DisplayStrip::Texts t = DisplayStrip::compose(combiC096(), &m_cat);
        QVERIFY(t.known);
        QCOMPARE(t.mode, QStringLiteral("COMBI 0:Play"));
        QCOMPARE(t.pageBold, QStringLiteral("Combination"));
        QCOMPARE(t.pageRegular, QStringLiteral(":Combi Select"));
        QCOMPARE(t.bank, QStringLiteral("Bank C"));
        QCOMPARE(t.bar, QStringLiteral("096: Piano &Strings"));
        QCOMPARE(t.category, QStringLiteral("Category:Keyboard"));  // unit list not read yet
    }

    // DESIGN 7.2: GM numbers 001..128, bank letter kept in the Bank row only.
    void stringsProgG001() {
        const DisplayStrip::Texts t = DisplayStrip::compose(progG001(), &m_cat);
        QCOMPARE(t.mode, QStringLiteral("PROG 0:Play"));
        QCOMPARE(t.pageBold, QStringLiteral("Program"));
        QCOMPARE(t.pageRegular, QStringLiteral(":Program Select"));
        QCOMPARE(t.bank, QStringLiteral("Bank G"));
        QCOMPARE(t.bar, QStringLiteral("001: Acoustic Piano"));
        QCOMPARE(DisplayStrip::compose(A(A::Kind::Prog, A::Bank::Gd, 1), &m_cat).bank, QStringLiteral("Bank g(d)"));
    }

    // Decisions log: with the unit's category list read, the zero-based index is shown.
    // DESIGN 7.2 data points: COMBI Keyboard = 00, PROG SlowSynth = 09 (Operation Guide).
    void categoryNumberOnlyAfterUnitRead() {
        Catalogue c;
        QVERIFY(c.loadFromResource());
        QCOMPARE(DisplayStrip::compose(progA000(), &c).category, QStringLiteral("Category:SlowSynth"));
        QVERIFY(c.setUnitCategories(sixteen(QStringLiteral("SlowSynth"), 9), sixteen(QStringLiteral("Keyboard"), 0)));
        QCOMPARE(DisplayStrip::compose(progA000(), &c).category, QStringLiteral("Category 09:SlowSynth"));
        QCOMPARE(DisplayStrip::compose(combiC096(), &c).category, QStringLiteral("Category 00:Keyboard"));
        // GM banks are outside the unit's 16 names: no index.
        QCOMPARE(DisplayStrip::compose(progG001(), &c).category, QStringLiteral("Category --:Keyboard"));
        // A category the unit's list does not contain.
        QVERIFY(c.setUnitCategories(sixteen(QStringLiteral("x"), 1), sixteen(QStringLiteral("y"), 1)));
        QCOMPARE(DisplayStrip::compose(combiC096(), &c).category, QStringLiteral("Category --:Keyboard"));
        c.clearUnitCategories();
        QCOMPARE(DisplayStrip::compose(combiC096(), &c).category, QStringLiteral("Category:Keyboard"));
    }

    // DESIGN 7.2: unknown sound: mode "--", no page, bank or category, bar "--".
    void stringsUnknownSound() {
        const DisplayStrip::Texts t = DisplayStrip::compose(std::nullopt, &m_cat);
        QVERIFY(!t.known);
        QCOMPARE(t.mode, QStringLiteral("--"));
        QCOMPARE(t.bar, QStringLiteral("--"));
        QVERIFY(t.pageBold.isEmpty() && t.pageRegular.isEmpty() && t.bank.isEmpty() && t.category.isEmpty());
    }

    // DESIGN 7.2: an address read from the unit before names were read shows "096: ---".
    void unknownNameShowsDashes() {
        Catalogue empty;
        QCOMPARE(DisplayStrip::compose(combiC096(), &empty).bar, QStringLiteral("096: ---"));
        QCOMPARE(DisplayStrip::compose(combiC096(), nullptr).category, QStringLiteral("Category:--"));
    }

    void setterRepaintsOnlyOnChange() {
        DisplayStrip s;
        s.setCatalogue(&m_cat);
        s.resize(900, s.height());
        s.show();
        QVERIFY(QTest::qWaitForWindowExposed(&s));
        QApplication::processEvents();
        // grab() does not reset the dirty state; count through an event filter instead.
        struct Counter : QObject {
            int paints = 0;
            bool eventFilter(QObject *, QEvent *e) override {
                if (e->type() == QEvent::Paint) ++paints;
                return false;
            }
        } counter;
        s.installEventFilter(&counter);
        s.setSound(combiC096());
        QTest::qWait(50);
        const int afterFirst = counter.paints;
        QVERIFY(afterFirst >= 1);
        s.setSound(combiC096());
        s.setConnection(true);
        QTest::qWait(50);
        const int afterConn = counter.paints;
        QVERIFY(afterConn > afterFirst);
        s.setSound(combiC096());
        s.setConnection(true);
        s.refresh();
        QTest::qWait(50);
        QCOMPARE(counter.paints, afterConn);  // nothing changed: nothing repainted
    }

    // Smoke: construct, grab, and check a few device pixels against DESIGN 7.2 boxes.
    void grabKnownSound() {
        DisplayStrip s;
        s.setCatalogue(&m_cat);
        s.setSound(combiC096());
        s.setConnection(true);
        s.resize(900, s.height());
        const QPixmap pm = s.grab();
        const QImage img = pm.toImage();
        const qreal dpr = s.devicePixelRatioF();
        const int k = Theme::pixelScale(dpr);
        QCOMPARE(s.pixelScaleK(), k);
        QCOMPARE(img.width(), qRound(900 * dpr));
        QCOMPARE(img.height(), qRound(s.height() * dpr));
        QVERIFY(img.height() >= 74 * k);
        QVERIFY(s.height() == DisplayStrip::heightForScale(dpr));
        // Panel field origin is (2k, 2k); mode segment starts at field (4k, 3k).
        QCOMPARE(img.pixelColor(50 * k, 3 * k).rgb(), Theme::lcdField);   // field, above the segments
        QCOMPARE(img.pixelColor(7 * k, 6 * k).rgb(), Theme::lcdInk);      // inside the mode segment
        QCOMPARE(img.pixelColor(7 * k, 2 * k + 37 * k + k).rgb(), Theme::lcdInk);  // sound bar fill
        QCOMPARE(img.pixelColor(0, 74 * k - 1).rgb(), Theme::bgBrowse);   // outside the rounded bezel corner
        QCOMPARE(img.pixelColor(300 * k + 20, 2 * k).rgb() == Theme::bgBrowse, false);  // still inside the panel
        // Connection zone is outside the panel on bg-browse, with a cyan LED when connected.
        bool cyan = false;
        for (int x = img.width() - 400; x < img.width() && !cyan; ++x)
            cyan = img.pixelColor(x, int(37.0 * k)).rgb() == Theme::cyan;
        QVERIFY(cyan);
    }

    void grabUnknownSoundDisconnected() {
        DisplayStrip s;
        s.setCatalogue(&m_cat);
        s.clearSound();
        s.setConnection(false);
        s.resize(900, s.height());
        const QImage img = s.grab().toImage();
        const int k = s.pixelScaleK();
        QVERIFY(!img.isNull());
        // Unknown: the sound bar is an outline of k px (ink), with the field colour inside.
        QCOMPARE(img.pixelColor(2 * k + 4 * k, 2 * k + 37 * k).rgb(), Theme::lcdInk);
        QCOMPARE(img.pixelColor(2 * k + 4 * k + 2 * k, 2 * k + 37 * k + 2 * k).rgb(), Theme::lcdField);
        QCOMPARE(s.connectionText(), QStringLiteral("NO X50"));
        bool red = false;
        for (int x = img.width() - 400; x < img.width() && !red; ++x)
            red = img.pixelColor(x, int(37.0 * k)).rgb() == Theme::red;
        QVERIFY(red);
    }

    void narrowWindowClipsPanelNotZone() {
        DisplayStrip s;
        s.setCatalogue(&m_cat);
        s.setSound(combiC096());
        s.setConnection(false, QStringLiteral("Port busy (another program is using the X50)"));
        s.resize(520, s.height());
        const QImage img = s.grab().toImage();
        QVERIFY(!img.isNull());
        QCOMPARE(img.width(), qRound(520 * s.devicePixelRatioF()));
    }

    // R-T3.1 / DESIGN 7.2: the connection message stays in its zone (the right 40% of the
    // window, up to two lines) even for one long token with no break opportunity.
    void connectionMessageStaysInZone_data() {
        QTest::addColumn<QString>("message");
        QTest::newRow("long token") << QString(60, QLatin1Char('W'));
        QTest::newRow("long path") << QStringLiteral("C:/Users/keys/AppData/Roaming/x50-patch-switcher/library-with-a-very-long-name.json");
        QTest::newRow("normal") << QStringLiteral("Port busy (another program is using the X50)");
        QTest::newRow("short") << QStringLiteral("NO X50");
    }
    void connectionMessageStaysInZone() {
        QFETCH(QString, message);
        const int W = 900;
        DisplayStrip s;
        s.setCatalogue(&m_cat);
        s.setSound(combiC096());
        s.resize(W, s.height());
        s.setConnection(false, message);
        const QImage img = s.grab().toImage();
        const qreal dpr = s.devicePixelRatioF();
        const int k = Theme::pixelScale(dpr);
        const int zoneStart = int(0.6 * W * dpr) - 1;      // right 40%, one device pixel of rounding
        const int panelRight = 480 * k + 2;
        auto isBg = [&](int x, int y) { return img.pixelColor(x, y).rgb() == Theme::bgBrowse; };
        // Nothing between the panel and the zone: the message did not grow leftwards.
        for (int y = 0; y < img.height(); ++y)
            for (int x = panelRight; x < zoneStart; ++x)
                QVERIFY2(isBg(x, y), qPrintable(QStringLiteral("ink at %1,%2").arg(x).arg(y)));
        // Text rows: right of the LED block, so only the message is in the band.
        const int textStart = qMax(zoneStart + int(28 * dpr), panelRight);
        int top = -1, bottom = -1;
        for (int y = 0; y < img.height(); ++y)
            for (int x = textStart; x < img.width(); ++x)
                if (!isBg(x, y)) {
                    if (top < 0) top = y;
                    bottom = y;
                    break;
                }
        QVERIFY(top >= 0);
        QFont label = Theme::roleFont(Theme::Role::Label);
        label.setPixelSize(Theme::stripLabelPx);
        const double lineH = QFontMetricsF(label).lineSpacing() * dpr;
        QVERIFY2((bottom - top + 1) <= 2 * lineH, qPrintable(QStringLiteral("%1 rows, line %2").arg(bottom - top + 1).arg(lineH)));
    }

    // R-T3.1: inside the LCD field (inset by the bezel's corner radius) only the LCD colours
    // appear: ink, paper, the dim field colour. The anti-aliasing belongs to the bezel only.
    void panelInteriorOnlyLcdColours() {
        const QSet<QRgb> allowed = {Theme::lcdInk, Theme::lcdField, Theme::lcdFieldDim};
        for (int mode = 0; mode < 2; ++mode) {
            DisplayStrip s;
            s.setCatalogue(&m_cat);
            if (mode == 0)
                s.setSound(combiC096());
            else
                s.clearSound();
            s.setConnection(true);
            s.resize(900, s.height());
            const QImage img = s.grab().toImage();
            const int k = s.pixelScaleK();
            const QRect interior(4 * k, 4 * k, 472 * k, 66 * k);  // field inset by 2k from the field edge
            for (int y = interior.top(); y <= interior.bottom(); ++y)
                for (int x = interior.left(); x <= interior.right(); ++x)
                    QVERIFY2(allowed.contains(img.pixelColor(x, y).rgb()),
                             qPrintable(QStringLiteral("mode %1: stray colour %2 at %3,%4")
                                            .arg(mode).arg(img.pixelColor(x, y).name()).arg(x).arg(y)));
        }
    }

    void heightFormula() {
        QCOMPARE(DisplayStrip::heightForScale(1.0), 74);
        QCOMPARE(DisplayStrip::heightForScale(1.25), 60);   // ceil(74 / 1.25) = 59.2
        QCOMPARE(DisplayStrip::heightForScale(1.5), 99);    // k = 2: ceil(148 / 1.5) = 98.67
        QCOMPARE(DisplayStrip::heightForScale(2.0), 74);    // k = 2: 148 / 2
    }
};

QTEST_MAIN(TestDisplayStrip)
#include "test_display_strip.moc"
