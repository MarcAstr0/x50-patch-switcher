#include "app/ScreenshotRunner.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/SoundAddress.h"
#include "ui/DisplayStrip.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QByteArray>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTextStream>

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

void prepareEnvironment(const CliOptions &options)
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    // The device pixel ratio of the (virtual) screen; QApplication reads it at start.
    qputenv("QT_SCALE_FACTOR", QByteArray::number(options.screenshotScale));
}

int run(const CliOptions &options)
{
    Theme::loadFonts();
    Theme::applyStyleSheet(*qApp);

    QTemporaryDir configDir;  // declared first so it outlives the library that saves into it
    Library library;
    if (configDir.isValid()) {
        library.load(configDir.path());
        buildCannedLibrary(library);
    }
    Catalogue catalogue;
    QString error;
    if (!catalogue.loadFromResource(&error))
        qWarning("catalogue: %s", qPrintable(error));

    MainWindow window(&catalogue);
    applyState(window, options.screenshotState);
    window.resize(kLogicalWidth, kLogicalHeight);
    window.show();
    QApplication::processEvents();
    QApplication::processEvents();

    const QPixmap shot = window.grab();
    if (!shot.save(options.screenshotPath, "PNG")) {
        QTextStream(stderr) << QStringLiteral("Could not write ") << options.screenshotPath << QStringLiteral("\n");
        return ExitCode::SendFailed;
    }
    return ExitCode::Ok;
}

} // namespace ScreenshotRunner
