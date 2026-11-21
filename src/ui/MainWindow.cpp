#include "ui/MainWindow.h"

#include "core/Version.h"
#include "ui/DisplayStrip.h"
#include "ui/Theme.h"

#include <QIcon>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(const Catalogue *catalogue, QWidget *parent)
    : QMainWindow(parent)
{
    Theme::loadFonts();
    setWindowTitle(Version::appName());
    setWindowIcon(QIcon(QStringLiteral(":/icons/app-256.png")));

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("browseRoot"));
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(Theme::browseMargin, Theme::browseMargin, Theme::browseMargin, Theme::browseMargin);
    layout->setSpacing(Theme::browseGap);
    m_strip = new DisplayStrip(root);
    m_strip->setCatalogue(catalogue);
    layout->addWidget(m_strip);
    layout->addStretch(1);  // T3.2 replaces this with the toolbar and the main area
    setCentralWidget(root);

    connect(m_strip, &DisplayStrip::heightChanged, this, [this](int) { updateMinimumHeight(); });
    updateMinimumHeight();
    resize(Theme::browseDefaultWidth, Theme::browseDefaultHeight);
}

void MainWindow::updateMinimumHeight()
{
    setMinimumSize(Theme::browseMinWidth,
                   Theme::browseMinHeight + (m_strip->height() - Theme::stripHeightK));
}
