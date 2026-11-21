#include "ui/MainWindow.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/Version.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QColor>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(Catalogue *catalogue, Library *library, X50Link *link, QWidget *parent)
    : QMainWindow(parent)
    , m_library(library)
    , m_link(link)
{
    Theme::loadFonts();
    setWindowTitle(Version::appName());
    setWindowIcon(QIcon(QStringLiteral(":/icons/app-256.png")));

    if (!catalogue) {
        m_ownedCatalogue = std::make_unique<Catalogue>();
        catalogue = m_ownedCatalogue.get();
    }
    if (!m_library) {
        m_ownedLibrary = std::make_unique<Library>();  // never loaded: it never writes
        m_library = m_ownedLibrary.get();
    }

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("browseRoot"));
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(Theme::browseMargin, Theme::browseMargin, Theme::browseMargin, Theme::browseMargin);
    layout->setSpacing(Theme::browseGap);
    m_strip = new DisplayStrip(root);
    m_strip->setCatalogue(catalogue);
    layout->addWidget(m_strip);
    m_browse = new BrowseView(catalogue, m_library, m_link, root);
    layout->addWidget(m_browse, 1);
    m_status = new QLabel(root);
    m_status->setObjectName(QStringLiteral("statusLine"));
    m_status->setFixedHeight(Theme::browseStatusHeight);
    m_status->setTextFormat(Qt::PlainText);
    layout->addWidget(m_status);
    setCentralWidget(root);

    connect(m_strip, &DisplayStrip::heightChanged, this, [this](int) { updateMinimumHeight(); });
    updateMinimumHeight();
    resize(Theme::browseDefaultWidth, Theme::browseDefaultHeight);

    connect(m_browse, &BrowseView::statusMessage, this, &MainWindow::showStatus);
    connect(m_browse, &BrowseView::settingsRequested, this, &MainWindow::openSettings);
    connect(m_library, &Library::saveFailed, this,
            [this](const QString &error) { showStatus(QStringLiteral("Could not save the library: %1").arg(error), true); });

    if (m_link) {
        connect(m_link, &X50Link::stateChanged, this, [this]() {
            m_linkError.clear();  // an error belongs to the state it was raised in
            syncFromLink();
        });
        connect(m_link, &X50Link::statusChanged, this, [this](const QString &text) {
            // Connect failures are only status text (no errorOccurred): tell them once, here.
            if (m_link->state() == X50Link::State::Disconnected && isConnectFailure(text)) {
                showStatus(text, true);
                syncFromLink();
            }
        });
        // Every successful send counts as a use, whichever view made it.
        connect(m_link, &X50Link::sent, this, [this](const SoundAddress &address) {
            m_library->recordUse(address);
            syncFromLink();
        });
        connect(m_link, &X50Link::followedSound, this, [this]() { syncFromLink(); });
        connect(m_link, &X50Link::followedMode, this, [this]() { syncFromLink(); });
        connect(m_link, &X50Link::probeFinished, this, [this](const X50Link::ProbeResult &result) {
            // A completed probe that confirmed an X50 and read its state records the unit (SPEC 3.3).
            if (result.x50Confirmed && result.inquiry && result.state) {
                Library::UnitInfo unit;
                unit.memberCode = result.inquiry->memberCode;
                unit.version = result.versionText;
                unit.globalChannel = result.inquiry->channel;
                unit.bankMap = result.state->bankMap;
                unit.lastSeen = QDateTime::currentDateTimeUtc();
                m_library->setUnit(unit);
            }
            syncFromLink();
        });
        connect(m_link, &X50Link::errorOccurred, this, [this](const QString &message, const QByteArray &) {
            m_linkError = message;
            showStatus(message, true);
            syncFromLink();
        });
        syncFromLink();
    }

    // Library problems found at load time are told once, here (never a dialog).
    const QString loadError = m_library->loadError();
    const QStringList warnings = m_library->loadWarnings();
    if (!loadError.isEmpty())
        showStatus(loadError, true);
    else if (!warnings.isEmpty())
        showStatus(warnings.size() == 1 ? warnings.first()
                                        : QStringLiteral("%1 (and %2 more library warnings)").arg(warnings.first()).arg(warnings.size() - 1),
                   true);

    m_browse->searchField()->setFocus();
}

MainWindow::~MainWindow() = default;

bool MainWindow::openSettings()
{
    if (!m_link || !m_settingsAllowed)
        return false;
    SettingsDialog dialog(m_link, m_backend, m_configDir, this);
    dialog.setLibrary(m_library);
    dialog.exec();
    if (dialog.saveFailed())
        showStatus(SettingsDialog::saveFailedText(), true);
    return true;
}

void MainWindow::updateMinimumHeight()
{
    setMinimumSize(Theme::browseMinWidth,
                   Theme::browseMinHeight + (m_strip->height() - Theme::stripHeightK));
}

QString MainWindow::statusText() const { return m_status->text(); }

void MainWindow::showStatus(const QString &text, bool isError)
{
    if (m_status->text() == text && m_statusError == isError)
        return;
    m_statusError = isError;
    m_status->setText(text);
    // `red` means "something is wrong" (DESIGN 1.1); a normal message uses the QSS colour.
    m_status->setStyleSheet(isError ? QStringLiteral("color: %1;").arg(QColor(Theme::red).name()) : QString());
}

bool MainWindow::isConnectFailure(const QString &text)
{
    return text.startsWith(QStringLiteral("MIDI is unavailable")) || text.startsWith(QStringLiteral("No X50 port found"))
        || text.startsWith(QStringLiteral("No MIDI port chosen"));
}

void MainWindow::syncFromLink()
{
    if (!m_link)
        return;
    if (const auto sound = m_link->currentSound())
        m_strip->setSound(*sound);
    else
        m_strip->clearSound();
    switch (m_link->state()) {
    case X50Link::State::Connected:
        m_strip->setConnection(true);
        break;
    case X50Link::State::Probing:
        m_strip->setConnection(true, QStringLiteral("CHECKING X50"));
        break;
    case X50Link::State::Manual:
        m_strip->setConnection(true, QStringLiteral("X50 NOT CONFIRMED"));
        break;
    case X50Link::State::Refused:
        m_strip->setConnection(false, m_linkError.isEmpty() ? m_link->statusText() : m_linkError);
        break;
    case X50Link::State::Disconnected:
        m_strip->setConnection(false, isConnectFailure(m_link->statusText()) ? m_link->statusText() : m_linkError);
        break;
    }
}

QRect MainWindow::clampToScreen(const QRect &saved, const QRect &available, const QSize &minimum)
{
    const int w = qMax(minimum.width(), qMin(saved.width(), available.width()));
    const int h = qMax(minimum.height(), qMin(saved.height(), available.height()));
    const int x = qBound(available.left(), saved.x(), qMax(available.left(), available.right() + 1 - w));
    const int y = qBound(available.top(), saved.y(), qMax(available.top(), available.bottom() + 1 - h));
    return QRect(x, y, w, h);
}

void MainWindow::restoreLayout(const QString &configDir, const QRect &availableOverride)
{
    m_configDir = configDir;
    m_layoutPath = QDir(configDir).filePath(layoutFileName());
    if (!m_quitHooked) {
        m_quitHooked = true;
        connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { saveLayout(); });
    }
    const QSettings settings(m_layoutPath, QSettings::IniFormat);
    const QSize size(settings.value(QStringLiteral("window/width")).toInt(), settings.value(QStringLiteral("window/height")).toInt());
    if (size.width() > 0 && size.height() > 0 && settings.contains(QStringLiteral("window/x"))) {
        const QPoint pos(settings.value(QStringLiteral("window/x")).toInt(), settings.value(QStringLiteral("window/y")).toInt());
        QRect available = availableOverride;
        if (!available.isValid()) {
            const QScreen *screen = QGuiApplication::screenAt(pos + QPoint(size.width() / 2, size.height() / 2));
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            if (screen)
                available = screen->availableGeometry();
        }
        const QRect r = available.isValid() ? clampToScreen(QRect(pos, size), available, minimumSize()) : QRect(pos, size);
        resize(r.size());
        move(r.topLeft());
        if (settings.value(QStringLiteral("window/maximized")).toBool())
            setWindowState(windowState() | Qt::WindowMaximized);
    }
    const int cat = settings.value(QStringLiteral("browse/categoryWidth")).toInt();
    if (cat > 0)
        m_browse->setCategoryWidth(cat);
}

void MainWindow::saveLayout()
{
    if (m_layoutPath.isEmpty())
        return;
    // aboutToQuit can run after the window was closed (hidden, geometry still valid): idempotent.
    QSettings settings(m_layoutPath, QSettings::IniFormat);
    // A maximized or minimized window remembers its normal size and position.
    const bool maximized = isMaximized();
    const QRect normal = (maximized || isMinimized()) ? normalGeometry() : QRect(pos(), size());
    settings.setValue(QStringLiteral("window/x"), normal.x());
    settings.setValue(QStringLiteral("window/y"), normal.y());
    settings.setValue(QStringLiteral("window/width"), normal.width());
    settings.setValue(QStringLiteral("window/height"), normal.height());
    settings.setValue(QStringLiteral("window/maximized"), maximized);
    settings.setValue(QStringLiteral("browse/categoryWidth"), m_browse->categoryWidth());
    settings.sync();  // failure (read-only folder) is ignored: remembering the layout is a courtesy
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveLayout();
    QMainWindow::closeEvent(event);
}
