// T3.1: Theme. Token values are typed here by hand from docs/design/DESIGN.md 1.1;
// font facts (advances, sizes) from DESIGN 2.2.
#include <QApplication>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QTest>

#include "ui/Theme.h"

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QVERIFY(Theme::loadFonts()); }

    void allSixFontFilesLoadAndReportAFamily() {
        const QStringList report = Theme::loadedFamilyReport();
        QCOMPARE(report.size(), 6);
        for (const QString &line : report)
            QVERIFY2(!line.contains(QStringLiteral("failed")), qPrintable(line));
        QVERIFY(Theme::fontsLoaded());
        const QStringList known = QFontDatabase::families();
        for (Theme::Face f : {Theme::Face::AtkinsonRegular, Theme::Face::AtkinsonBold, Theme::Face::BarlowMedium,
                              Theme::Face::BarlowSemiBold, Theme::Face::PixelRegular, Theme::Face::PixelBold}) {
            QVERIFY(!Theme::family(f).isEmpty());
            QVERIFY2(known.contains(Theme::family(f)), qPrintable(Theme::family(f)));
        }
        // DESIGN 2.2 rule 5: one family string for both Pixel Operator files.
        QCOMPARE(Theme::family(Theme::Face::PixelRegular), QStringLiteral("Pixel Operator X50"));
        QCOMPARE(Theme::family(Theme::Face::PixelBold), QStringLiteral("Pixel Operator X50"));
        QCOMPARE(Theme::family(Theme::Face::AtkinsonRegular), QStringLiteral("Atkinson Hyperlegible"));
    }

    void keyTokensMatchDesign() {
        QCOMPARE(QColor(Theme::bgLive).name(), QStringLiteral("#0b0d0f"));
        QCOMPARE(QColor(Theme::cyan).name(), QStringLiteral("#5db0d0"));
        QCOMPARE(QColor(Theme::cyanText).name(), QStringLiteral("#7cc8e4"));
        QCOMPARE(QColor(Theme::onCyan).name(), QStringLiteral("#04141b"));
        QCOMPARE(QColor(Theme::text).name(), QStringLiteral("#f2f5f7"));
        QCOMPARE(QColor(Theme::red).name(), QStringLiteral("#ff8080"));
        QCOMPARE(QColor(Theme::bgBrowse).name(), QStringLiteral("#121518"));
        QCOMPARE(QColor(Theme::lcdField).name(), QStringLiteral("#e6e8e1"));
        QCOMPARE(QColor(Theme::lcdFieldDim).name(), QStringLiteral("#bfc3ba"));
        QCOMPARE(QColor(Theme::lcdInk).name(), QStringLiteral("#111411"));
        QCOMPARE(QColor(Theme::lcdBezel).name(), QStringLiteral("#3a413b"));
        QCOMPARE(QColor(Theme::selRow).name(), QStringLiteral("#1e4a5c"));
        QCOMPARE(QColor(Theme::placeholder).name(), QStringLiteral("#8a97a2"));
        QCOMPARE(QColor(Theme::cyan).alpha(), 255);
    }

    void sizesMatchDesign() {
        QCOMPARE(Theme::stripHeightK, 74);
        QCOMPARE(Theme::stripPanelWidthK, 480);
        QCOMPARE(Theme::browseMinWidth, 1000);
        QCOMPARE(Theme::browseMinHeight, 700);
        QCOMPARE(Theme::browseDefaultWidth, 1180);
        QCOMPARE(Theme::browseDefaultHeight, 760);
        QCOMPARE(Theme::s4, 16);
        QCOMPARE(Theme::liveTopBar, 56);
    }

    void pixelScaleRule() {
        QCOMPARE(Theme::pixelScale(1.0), 1);
        QCOMPARE(Theme::pixelScale(1.25), 1);
        QCOMPARE(Theme::pixelScale(1.5), 2);
        QCOMPARE(Theme::pixelScale(2.0), 2);
        QCOMPARE(Theme::pixelScale(0.5), 1);
    }

    void pixelFontFollowsRendering22() {
        for (int k : {1, 2, 3}) {
            const QFont f = Theme::pixelFont(k, false);
            QCOMPARE(f.pixelSize(), 16 * k);
            QCOMPARE(int(f.hintingPreference()), int(QFont::PreferNoHinting));
            QVERIFY(!f.kerning());
            QVERIFY(f.styleStrategy() & QFont::NoSubpixelAntialias);
            QCOMPARE(f.letterSpacing(), 0.0);
        }
        // Advances are whole font pixels times k: digit 7 regular / 8 bold, space 4 (DESIGN 2.2).
        for (int k : {1, 2}) {
            QCOMPARE(QFontMetrics(Theme::pixelFont(k, false)).horizontalAdvance(QStringLiteral("0")), 7 * k);
            QCOMPARE(QFontMetrics(Theme::pixelFont(k, true)).horizontalAdvance(QStringLiteral("0")), 8 * k);
            QCOMPARE(QFontMetrics(Theme::pixelFont(k, false)).horizontalAdvance(QStringLiteral(" ")), 4 * k);
            QCOMPARE(QFontMetrics(Theme::pixelFont(k, true)).horizontalAdvance(QStringLiteral("M")), 9 * k);
        }
    }

    // DESIGN 2.2 rule 4 reproduces the 7.2 offsets: mode segment (box y 3k, h 16k) baseline 14k;
    // sound bar (y 37k, h 30k at 2k) baseline 58k.
    void baselineFormula() {
        QCOMPARE(Theme::pixelBaseline(3, 16, 1), 14);
        QCOMPARE(Theme::pixelBaseline(6, 32, 2), 28);
        QCOMPARE(Theme::pixelBaseline(37, 30, 2), 58);
    }

    void roleFontsUseDesignSizes() {
        QCOMPARE(Theme::roleFont(Theme::Role::Body).pixelSize(), 15);
        QCOMPARE(Theme::roleFont(Theme::Role::Small).pixelSize(), 13);
        QCOMPARE(Theme::roleFont(Theme::Role::Label).pixelSize(), 12);
        QCOMPARE(Theme::roleFont(Theme::Role::Label).letterSpacing(), 1.5);
        QCOMPARE(Theme::roleFont(Theme::Role::Title).pixelSize(), 20);
    }

    void styleSheetApplies() {
        Theme::applyStyleSheet(*qApp);
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("#121518")));
    }
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
