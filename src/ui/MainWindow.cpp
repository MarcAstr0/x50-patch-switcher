#include "ui/MainWindow.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/Version.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/LiveView.h"
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
#include <QStackedWidget>
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
    m_catalogue = catalogue;
    if (!m_library) {
        m_ownedLibrary = std::make_unique<Library>();  // never loaded: it never writes
        m_library = m_ownedLibrary.get();
    }

    // Unit names persisted in the library belong in the catalogue (restart, and names read later).
    // First on Library::changed, so every view's own slot sees the updated catalogue.
    applyUnitNames();
    connect(m_library, &Library::changed, this, [this]() { applyUnitNames(); });

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
    m_browsePage = root;
    m_live = new LiveView(catalogue, this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(root);
    m_stack->addWidget(m_live);
    setCentralWidget(m_stack);

    connect(m_strip, &DisplayStrip::heightChanged, this, [this](int) { updateMinimumHeight(); });
    updateMinimumHeight();
    resize(Theme::browseDefaultWidth, Theme::browseDefaultHeight);

    connect(m_browse, &BrowseView::statusMessage, this, &MainWindow::showStatus);
    connect(m_browse, &BrowseView::settingsRequested, this, &MainWindow::openSettings);
    connect(m_browse, &BrowseView::liveRequested, this, &MainWindow::enterLive);
    // Set changes while live (none can be made from the live page, but the library is shared).
    connect(m_library, &Library::changed, this, [this]() {
        if (!m_isLive)
            return;
        m_live->refresh();  // unit names read while live (only relays out if a shown name changed)
        syncLive();
    });
    connect(m_library, &Library::saveFailed, this, [this](const QString &error) {
        const QString text = QStringLiteral("Could not save the library: %1").arg(error);
        showStatus(text, true);
        if (m_isLive)
            m_live->flashSendFailure(text);  // live shows errors in the top bar only
    });

    // Live input (SPEC 4.2). The view only says what was chosen; sending is explicit and ours.
    connect(m_live, &LiveView::slotClicked, this, [this](int index) { sendSlot(index); });
    connect(m_live, &LiveView::setStepRequested, this, [this](int direction) { stepSet(direction); });
    connect(m_live, &LiveView::leaveRequested, this, &MainWindow::leaveLive);
    // A quit while live must not leave the display inhibit set.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() { m_sleepInhibitor.release(); });

    if (m_link) {
        connect(m_link, &X50Link::stateChanged, this, [this]() {
            m_linkError.clear();  // an error belongs to the state it was raised in
            syncFromLink();
        });
        connect(m_link, &X50Link::statusChanged, this, [this](const QString &text) {
            // Connect failures are only status text (no errorOccurred): tell them once, here.
            if (m_link->state() == X50Link::State::Disconnected && m_link->statusIsFailure()) {
                showLinkStatus(text);
                syncFromLink();
            }
        });
        // Every successful send counts as a use, whichever view made it.
        connect(m_link, &X50Link::sent, this, [this](const SoundAddress &address) {
            m_library->recordUse(address);
            syncFromLink();
            m_live->sendResolved(address);  // after the lit slot moved
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
            showLinkStatus(message);
            syncFromLink();
            // Connected, so the bar keeps its connection text: show the error in a red chip for 2 s.
            if (m_isLive && m_link->state() != X50Link::State::Disconnected && m_link->state() != X50Link::State::Refused)
                m_live->flashSendFailure(message);
        });
        connect(m_link, &X50Link::sendFailed, this, [this](const SoundAddress &address, const QString &reason) {
            m_live->sendResolved(address);  // the lit slot stays
            if (m_isLive)
                m_live->flashSendFailure(reason);
        });
        syncFromLink();
    }

    // Library problems found at load time are told once, here (never a dialog).
    const QString loadError = m_library->loadError();
    const QStringList warnings = m_library->loadWarnings();
    QString notice;
    if (!loadError.isEmpty())
        notice = loadError;
    else if (!warnings.isEmpty())
        notice = warnings.size() == 1 ? warnings.first()
                                      : QStringLiteral("%1 (and %2 more library warnings)").arg(warnings.first()).arg(warnings.size() - 1);
    if (!notice.isEmpty()) {
        showStatus(notice, true);
        m_loadNotice = notice;
    }

    m_browse->searchField()->setFocus();
}

MainWindow::~MainWindow()
{
    m_sleepInhibitor.release();
}

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
    if (text != m_loadNotice)
        m_loadNotice.clear();
    if (m_status->text() == text && m_statusError == isError)
        return;
    m_statusError = isError;
    m_status->setText(text);
    // `red` means "something is wrong" (DESIGN 1.1); a normal message uses the QSS colour.
    m_status->setStyleSheet(isError ? QStringLiteral("color: %1;").arg(QColor(Theme::red).name()) : QString());
}

void MainWindow::applyUnitNames()
{
    const auto &names = m_library->unitNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        const Catalogue::UnitName *have = m_catalogue->unitName(it.key());
        if (have && have->name == it.value().name && have->category == it.value().category && have->readAt == it.value().readAt)
            continue;
        m_catalogue->setUnitName(it.key(), it.value().name, it.value().category, it.value().readAt);
    }
}

void MainWindow::showLinkStatus(const QString &text)
{
    // A library problem found at load stays until the user acts (any other message replaces it);
    // the link's own state is on the connection strip meanwhile.
    if (!m_loadNotice.isEmpty())
        return;
    showStatus(text, true);
}

void MainWindow::syncLive()
{
    const SetModel *set = m_library->currentSet();
    m_live->setSet(set ? *set : SetModel(QString(), QString(), 8));
    if (!m_link) {
        m_live->setConnection(false);
        return;
    }
    // The lit button stays as last known while disconnected (DESIGN 6.3): the link forgets its
    // learned state then, the view does not.
    if (const auto sound = m_link->currentSound())
        m_live->setCurrentSound(*sound);
    else if (m_link->state() == X50Link::State::Connected)
        m_live->clearCurrentSound();
    switch (m_link->state()) {
    case X50Link::State::Connected:
        m_live->setConnection(true);
        break;
    case X50Link::State::Probing:
        m_live->setConnection(true, QStringLiteral("CHECKING X50"));
        break;
    case X50Link::State::Manual:
        m_live->setConnection(true, QStringLiteral("X50 NOT CONFIRMED"));
        break;
    case X50Link::State::Refused:
        m_live->setConnection(false, m_linkError.isEmpty() ? m_link->statusText() : m_linkError);
        break;
    case X50Link::State::Disconnected:
        m_live->setConnection(false, m_link->statusIsFailure() ? m_link->statusText() : m_linkError);
        break;
    }
}

void MainWindow::sendSlot(int index)
{
    if (!m_isLive || !m_link) {
        m_live->clearTarget();
        return;
    }
    const SetModel::Slot &slot = m_live->set().slot(index);  // bounds-checked
    if (slot.address)
        m_link->send(*slot.address);
}

void MainWindow::stepSet(int direction)
{
    if (!m_isLive)
        return;
    const QVector<SetModel> &sets = m_library->sets();
    if (sets.isEmpty())
        return;
    int idx = -1;
    for (int i = 0; i < sets.size(); ++i)
        if (sets[i].id() == m_library->currentSetId())
            idx = i;
    const int target = idx < 0 ? (direction > 0 ? 0 : sets.size() - 1) : idx + direction;
    if (target < 0 || target >= sets.size() || target == idx)
        return;  // no wrap; switching a set sends nothing
    m_library->setCurrentSet(sets[target].id());
}

bool MainWindow::enterLive()
{
    if (m_isLive)
        return false;
    m_isLive = true;
    m_sleepInhibitor.acquire();
    setSettingsAllowed(false);
    m_stateBeforeLive = windowState();
    m_geometryBeforeLive = (isMaximized() || isFullScreen() || isMinimized()) ? normalGeometry() : QRect(pos(), size());
    m_live->refresh();  // unit names may have been read in browse
    syncLive();
    m_stack->setCurrentWidget(m_live);
    setMinimumSize(0, 0);  // the browse minimum must not stop a small screen or a scaled test window
    if (m_liveFullScreen)
        showFullScreen();
    m_live->setFocus();
    return true;
}

void MainWindow::leaveLive()
{
    if (!m_isLive)
        return;
    m_isLive = false;
    m_sleepInhibitor.release();
    m_live->setEscArmed(false);
    m_stack->setCurrentWidget(m_browsePage);
    setSettingsAllowed(true);
    updateMinimumHeight();
    if (!m_liveFullScreen) {
        m_browse->searchField()->setFocus();
        return;
    }
    if (m_stateBeforeLive.testFlag(Qt::WindowFullScreen)) {
        showFullScreen();
    } else if (m_stateBeforeLive.testFlag(Qt::WindowMaximized)) {
        showMaximized();
    } else {
        showNormal();
        resize(m_geometryBeforeLive.size());
        move(m_geometryBeforeLive.topLeft());
    }
    m_browse->searchField()->setFocus();
}

void MainWindow::syncFromLink()
{
    if (m_isLive)
        syncLive();
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
        m_strip->setConnection(false, m_link->statusIsFailure() ? m_link->statusText() : m_linkError);
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
    // While live and full screen, the layout to remember is the one from before going live.
    const bool fullLive = m_isLive && isFullScreen();
    const bool maximized = fullLive ? m_stateBeforeLive.testFlag(Qt::WindowMaximized) : isMaximized();
    const QRect normal = fullLive ? m_geometryBeforeLive
                                  : ((maximized || isMinimized()) ? normalGeometry() : QRect(pos(), size()));
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
