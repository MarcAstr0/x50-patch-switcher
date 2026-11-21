#pragma once

#include <memory>

#include <QMainWindow>
#include <QString>

#include "midi/X50Link.h"

class BrowseView;
class Catalogue;
class DisplayStrip;
class Library;
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
    QString m_linkError;  // the link's last error while disconnected, shown in the strip
};
