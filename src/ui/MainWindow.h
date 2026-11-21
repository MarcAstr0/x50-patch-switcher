#pragma once

#include <memory>

#include <QMainWindow>
#include <QRect>
#include <QSize>
#include <QString>

#include "midi/X50Link.h"

class BrowseView;
class Catalogue;
class DisplayStrip;
class Library;
class QCloseEvent;
class QLabel;

// Browse window (DESIGN 7.1): display strip, the browse page, status line, in a column with
// a 16 px margin and 12 px gaps.
//
// The strip follows the X50Link (its own sends, panel changes, connection state). The status
// line shows, one message at a time and never in a dialog: the browse view's messages, the
// link's errors, and the Library's load errors and warnings (at start) and save failures.
//
// None of the three collaborators is owned. Any may be null (the default constructor, used
// by the scaffold test, then makes an empty catalogue and library and no link).
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(Catalogue *catalogue = nullptr, Library *library = nullptr, X50Link *link = nullptr,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    DisplayStrip *displayStrip() const { return m_strip; }
    BrowseView *browseView() const { return m_browse; }
    QString statusText() const;
    bool statusIsError() const { return m_statusError; }

    // Shows a message in the status line (replacing the previous one).
    void showStatus(const QString &text, bool isError);

    // Remembers the window geometry and the category splitter between runs, in
    // "<configDir>/window.ini" (QSettings, INI format, so portable mode keeps it next to the
    // exe and nothing goes to the system registry). Call before show(); the layout is saved when
    // the window closes. Without this call nothing is read or written (tests, screenshots).
    // Missing or unreadable settings leave the defaults. The geometry is clamped to the
    // available area of the screen it was saved on (or the primary screen).
    // `availableOverride` (tests) replaces the screen's available area when valid.
    void restoreLayout(const QString &configDir, const QRect &availableOverride = QRect());
    void saveLayout();
    static QString layoutFileName() { return QStringLiteral("window.ini"); }
    // Pure: `saved` (frame top-left and client size) moved and shrunk to lie inside `available`,
    // never smaller than `minimum` (when the screen is smaller than the minimum, the minimum wins).
    static QRect clampToScreen(const QRect &saved, const QRect &available, const QSize &minimum);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    // DESIGN 7.1: the minimum height grows with the strip when k > 1; recomputed when it changes.
    void updateMinimumHeight();
    void syncFromLink();

    std::unique_ptr<Catalogue> m_ownedCatalogue;
    std::unique_ptr<Library> m_ownedLibrary;
    Library *m_library = nullptr;
    X50Link *m_link = nullptr;
    DisplayStrip *m_strip = nullptr;
    BrowseView *m_browse = nullptr;
    QLabel *m_status = nullptr;
    bool m_statusError = false;
    QString m_layoutPath;  // empty: layout is not remembered
    QString m_linkError;  // the link's last error while disconnected, shown in the strip
};
