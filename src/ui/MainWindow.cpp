#include "ui/MainWindow.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/Version.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/Theme.h"

#include <QColor>
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
    connect(m_library, &Library::saveFailed, this,
            [this](const QString &error) { showStatus(QStringLiteral("Could not save the library: %1").arg(error), true); });

    if (m_link) {
        connect(m_link, &X50Link::stateChanged, this, [this]() {
            if (m_link->state() == X50Link::State::Connected || m_link->state() == X50Link::State::Manual)
                m_linkError.clear();
            syncFromLink();
        });
        connect(m_link, &X50Link::sent, this, [this]() { syncFromLink(); });
        connect(m_link, &X50Link::followedSound, this, [this]() { syncFromLink(); });
        connect(m_link, &X50Link::followedMode, this, [this]() { syncFromLink(); });
        connect(m_link, &X50Link::probeFinished, this, [this]() { syncFromLink(); });
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
        m_strip->setConnection(false, m_linkError);
        break;
    }
}
