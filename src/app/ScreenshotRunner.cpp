#include "app/ScreenshotRunner.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/SoundAddress.h"
#include "app/CliRunner.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/MainWindow.h"
#include "ui/ProbeChecklist.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QPixmap>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <functional>
#include <memory>

namespace {
constexpr int kLogicalWidth = 1920;
constexpr int kLogicalHeight = 1080;

SoundAddress combi(SoundAddress::Bank b, int n) { return SoundAddress(SoundAddress::Kind::Combi, b, n); }
SoundAddress prog(SoundAddress::Bank b, int n) { return SoundAddress(SoundAddress::Kind::Prog, b, n); }

// The two sets of the mockups (docs/design/mockup/live.py SET8, SET6).
void buildCannedLibrary(Library &lib)
{
    using B = SoundAddress::Bank;
    struct Slot { int idx; SoundAddress addr; const char *label; };
    QString id;
    lib.createSet(QStringLiteral("Friday Set 2"), 8, &id);
    const Slot set8[] = {
        {0, combi(B::A, 2), "Intro pad"},      {1, prog(B::D, 104), "Verse EP"},
        {2, combi(B::C, 96), nullptr},         {3, combi(B::A, 0), nullptr},
        {4, combi(B::A, 15), "Bridge organ"},  {6, prog(B::A, 28), "Solo lead"},
        {7, combi(B::A, 26), nullptr},
    };
    for (const Slot &s : set8) {
        lib.setSlot(id, s.idx, s.addr);
        if (s.label)
            lib.setLabel(id, s.idx, QString::fromLatin1(s.label));
    }
    lib.createSet(QStringLiteral("Acoustic Night"), 6, &id);
    const Slot set6[] = {
        {0, combi(B::B, 64), nullptr},        {1, prog(B::A, 25), "Warm pad"},
        {2, prog(B::B, 1), "Ballad piano"},   {3, combi(B::A, 17), nullptr},
        {4, prog(B::A, 92), "Harp intro"},    {5, combi(B::C, 51), nullptr},
    };
    for (const Slot &s : set6) {
        lib.setSlot(id, s.idx, s.addr);
        if (s.label)
            lib.setLabel(id, s.idx, QString::fromLatin1(s.label));
    }
}

// Favourites, use counts and one edited name, so the result list shows every marker of the
// browse mockup (docs/design/mockup-browse.png).
void buildCannedMarks(Library &lib, Catalogue &cat)
{
    using B = SoundAddress::Bank;
    lib.setFavourite(combi(B::B, 64), true);
    lib.setFavourite(combi(B::C, 96), true);
    const struct { SoundAddress addr; int uses; } used[] = {
        {combi(B::B, 64), 3}, {combi(B::C, 51), 5}, {prog(B::A, 81), 2},
        {combi(B::C, 96), 11},  // the screenshot's own send makes it 12
    };
    for (const auto &u : used)
        for (int i = 0; i < u.uses; ++i)
            lib.recordUse(u.addr);
    cat.setUnitName(prog(B::A, 65), QStringLiteral("M1 Piano Soft"), QStringLiteral("Keyboard"),
                    QDateTime(QDate(2026, 11, 21), QTime(12, 0), Qt::UTC));
}

// Pumps the event loop until `done` or the timeout. Screenshot tool only.
bool waitFor(const std::function<bool()> &done, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (!done()) {
        if (t.elapsed() > timeoutMs)
            return false;
        QApplication::processEvents();
        QThread::msleep(2);
    }
    return true;
}

void applyState(MainWindow &w, const QString &state)
{
    DisplayStrip *strip = w.displayStrip();
    using B = SoundAddress::Bank;
    if (state == QLatin1String("live-disconnected")) {
        strip->clearSound();
        strip->setConnection(false);
    } else if (state == QLatin1String("settings-errors")) {
        strip->setSound(combi(B::C, 96));
        strip->setConnection(false, QStringLiteral("Port busy (another program is using the X50)"));
    } else if (state == QLatin1String("live-8")) {
        strip->setSound(prog(B::D, 104));
        strip->setConnection(true);
    } else if (state == QLatin1String("live-6")) {
        strip->setSound(prog(B::D, 1));
        strip->setConnection(true);
    } else {  // browse, browse-search, settings
        strip->setSound(combi(B::C, 96));
        strip->setConnection(true);
    }
}
} // namespace

namespace ScreenshotRunner {

namespace {
// The real settings dialog on the real link against the simulated X50. `settings`: every line
// passes. `settings-errors` matches docs/design/mockup-settings.png: Bank Change off (a real
// probe line), then the Bank Map line edited to "not reported" and the last line to "running"
// (the fake always answers Mode and State, so those two cannot come from a probe).
int runSettings(const CliOptions &options)
{
    const bool errors = options.screenshotState == QLatin1String("settings-errors");
    std::unique_ptr<MidiBackend> backend = CliCommon::makeBackend(true);
    if (auto *fake = dynamic_cast<FakeX50 *>(backend.get()))
        fake->setEnables(true, !errors, true);
    X50Link link(backend.get());
    link.setPortNames(CliCommon::defaultPortSubstring());
    link.start();
    waitFor([&] { return link.state() == X50Link::State::Connected; }, 3000);

    SettingsDialog dialog(&link, backend.get(), QString());
    dialog.setHeightLimit(kLogicalHeight);  // the virtual screen is small; show every line
    if (errors) {
        QVector<ProbeChecklist::Row> rows = dialog.checklist()->rows();
        if (rows.size() == 6) {
            rows[4] = {ProbeChecklist::State::Unknown, QStringLiteral("Bank Map: not reported"),
                       QStringLiteral("The X50 did not say which Bank Map it uses. Sending as GM; change Bank Map above if banks come out wrong.")};
            rows[5] = {ProbeChecklist::State::Running, QStringLiteral("SysEx replies"), QString()};
            dialog.checklist()->setRows(rows);
        }
        dialog.statusWord()->setText(QStringLiteral("Checking..."));
        dialog.channelSpin()->setEnabled(true);
        dialog.bankMapCombo()->setEnabled(true);
        dialog.fitToContent();
    } else {
        dialog.statusWord()->setText(QStringLiteral("Last check: 14:02"));
    }
    dialog.show();
    QApplication::processEvents();
    if (QWidget *f = dialog.focusWidget())
        f->clearFocus();  // the mockup shows no focus ring
    QApplication::processEvents();
    const QPixmap shot = dialog.grab();
    link.stop();
    if (!shot.save(options.screenshotPath, "PNG")) {
        QTextStream(stderr) << QStringLiteral("Could not write ") << options.screenshotPath << QStringLiteral("\n");
        return ExitCode::ScreenshotFailed;
    }
    return ExitCode::Ok;
}
} // namespace

void prepareEnvironment(const CliOptions &options)
{
    // Always offscreen, even when the caller's environment names another platform: the
    // render must be deterministic and must not need a display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // The device pixel ratio of the (virtual) screen; QApplication reads it at start.
    qputenv("QT_SCALE_FACTOR", QByteArray::number(options.screenshotScale));
}

int run(const CliOptions &options)
{
    Theme::loadFonts();
    Theme::applyStyleSheet(*qApp);

    if (options.screenshotState == QLatin1String("settings") ||
        options.screenshotState == QLatin1String("settings-errors"))
        return runSettings(options);

    const bool browse = options.screenshotState == QLatin1String("browse") ||
                        options.screenshotState == QLatin1String("browse-search");

    QTemporaryDir configDir;  // declared first so it outlives the library that saves into it
    Library library;
    Catalogue catalogue;
    QString error;
    if (!catalogue.loadFromResource(&error))
        qWarning("catalogue: %s", qPrintable(error));
    if (configDir.isValid()) {
        library.load(configDir.path());
        buildCannedLibrary(library);
        buildCannedMarks(library, catalogue);
    }

    // The browse states run the real link against the simulated X50, so the strip, the
    // current-sound marker and the status line come from real signals.
    std::unique_ptr<MidiBackend> backend = CliCommon::makeBackend(true);
    X50Link link(backend.get());
    link.setPortNames(CliCommon::defaultPortSubstring());
    MainWindow window(&catalogue, &library, browse ? &link : nullptr);
    if (browse) {
        link.start();
        const bool connected = waitFor([&] { return link.state() == X50Link::State::Connected; }, 3000);
        if (connected) {
            link.send(combi(SoundAddress::Bank::C, 96));
            waitFor([&] { return link.currentSound().has_value(); }, 3000);
        }
        BrowseView *bv = window.browseView();
        if (options.screenshotState == QLatin1String("browse-search")) {
            bv->setKinds(true, true, false);
            bv->setSearchText(QStringLiteral("piano"));
            bv->highlight(combi(SoundAddress::Bank::B, 64));
        } else {
            bv->highlight(combi(SoundAddress::Bank::C, 96));
        }
        bv->searchField()->setFocus();
    } else {
        applyState(window, options.screenshotState);
    }
    window.resize(kLogicalWidth, kLogicalHeight);
    window.show();
    QApplication::processEvents();
    QApplication::processEvents();
    if (browse) {
        // The highlight was set before the window had its size; now place the list.
        QTableView *table = window.browseView()->resultTable();
        if (options.screenshotState == QLatin1String("browse-search"))
            table->scrollToTop();
        else
            table->scrollTo(table->currentIndex(), QAbstractItemView::PositionAtCenter);
        QApplication::processEvents();
    }

    const QPixmap shot = window.grab();
    link.stop();
    if (!shot.save(options.screenshotPath, "PNG")) {
        QTextStream(stderr) << QStringLiteral("Could not write ") << options.screenshotPath << QStringLiteral("\n");
        return ExitCode::ScreenshotFailed;
    }
    return ExitCode::Ok;
}

} // namespace ScreenshotRunner
