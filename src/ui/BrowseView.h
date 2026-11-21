#pragma once

#include <optional>

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "core/SoundAddress.h"

class Catalogue;
class Library;
class PanelFrame;
class QKeyEvent;
class QListWidget;
class QSplitter;
class QLabel;
class QPushButton;
class QTableView;
class QToolButton;
class QLineEdit;
class ResultDelegate;
class ResultModel;
class X50Link;

// The browse page (SPEC 4.1, DESIGN 7.3-7.9): search, kind toggles, category list, the
// result list, the transport bar and a placeholder for the set editor (T3.6). The display
// strip (DESIGN 7.2) and the status line (7.10) belong to MainWindow, which listens to
// statusMessage().
//
// MIDI goes out only on an explicit action: Enter, double-click or Play on the highlighted
// sound, and Prev/Next (Ctrl+Left/Right anywhere; PgUp/PgDn when neither list has focus).
// Highlighting never sends. Auto-repeated key events never send. A successful send
// (X50Link::sent) is recorded in the Library's recents; a failed one is reported through
// statusMessage(). No timers; the filter runs synchronously (about 1 000 rows).
//
// Prev/Next stop at the ends of the result list (no wrap; SPEC 4.1 is silent, SPEC 4.2 says
// no wrap for live view). With nothing highlighted, Next takes the first row and Prev the last.
//
// Shortcuts (handled in one event filter, because QLineEdit would swallow Ctrl+Left/Right
// and, on X11, Ctrl+D as text-editing keys): Enter/Return, Ctrl+Left/Right, PgUp/PgDn,
// Ctrl+D, Ctrl+1..8, F5, Up/Down in the search box (move the highlight, never send).
class BrowseView : public QWidget
{
    Q_OBJECT
public:
    // `catalogue` and `library` are required, `link` may be null (every send then reports
    // "not connected"). None is owned.
    BrowseView(Catalogue *catalogue, Library *library, X50Link *link, QWidget *parent = nullptr);

    // ---- state, also for tests and the screenshot harness
    QString searchText() const;
    void setSearchText(const QString &text);  // filters at once
    bool kindEnabled(SoundAddress::Kind kind, bool gm) const;
    void setKinds(bool combis, bool programs, bool gm);
    // Select a category by name; an empty name selects "All". False if the list lacks it.
    bool setCategory(const QString &name);
    QString category() const { return m_category; }
    // Rebuild the category list (after the unit's category names were read).
    void refreshCategories();
    // Re-read names from the catalogue (after unit names were applied).
    void refreshNames();
    QStringList categoryNames() const;  // as listed, "All" first
    QString categoryTitle() const;
    // Width of the category column (the splitter position), remembered between runs.
    int categoryWidth() const;
    void setCategoryWidth(int width);  // clamped to DESIGN 7.1's 160..260

    QVector<SoundAddress> results() const;
    int resultCount() const;
    std::optional<SoundAddress> highlighted() const;
    // Highlight a result without sending. False if it is not in the results.
    bool highlight(const SoundAddress &address);
    void clearHighlight();

    // ---- actions (what the keys and buttons call)
    void sendHighlighted();
    // dir -1 previous, +1 next: highlight the neighbour and send it. No wrap.
    void step(int dir);
    void toggleFavouriteHighlighted();
    // slot is 1-based as in the UI (Ctrl+1..8).
    void addHighlightedToSlot(int slot);

    // ---- widgets (tests)
    QLineEdit *searchField() const { return m_search; }
    QToolButton *combisToggle() const { return m_combis; }
    QToolButton *programsToggle() const { return m_programs; }
    QToolButton *gmToggle() const { return m_gm; }
    QListWidget *categoryList() const { return m_categories; }
    QTableView *resultTable() const { return m_table; }
    ResultModel *resultModel() const { return m_model; }
    QPushButton *prevButton() const { return m_prev; }
    QPushButton *nextButton() const { return m_next; }
    QPushButton *playButton() const { return m_play; }
    QPushButton *starButton() const { return m_star; }
    // Enabled by T3.4 (settings) and T3.5 (read names); disabled until then.
    QPushButton *settingsButton() const { return m_settings; }
    QPushButton *readNamesButton() const { return m_readNames; }
    QPushButton *liveButton() const { return m_live; }
    PanelFrame *setEditorPanel() const { return m_setEditor; }

signals:
    // A message for the status line. isError: it means something is wrong (red).
    void statusMessage(const QString &text, bool isError);
    // F5 or the Live button. Nothing listens until M4 (live view); then MainWindow switches.
    void liveRequested();
    void settingsRequested();
    void readNamesRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void buildUi();
    void rebuildCategories();
    void refilter();
    void updateControls();
    void syncCurrent();
    bool handleKey(QObject *source, QKeyEvent *event);
    void moveHighlight(int delta);
    void selectRow(int row);
    void sendSound(const SoundAddress &address);
    void onSent(const SoundAddress &address);
    void onSendFailed(const SoundAddress &address, const QString &reason);
    void onContextMenu(const QPoint &pos);
    QString describe(const SoundAddress &address) const;
    int currentRow() const;
    SoundAddress::Kind shownCategoryKind() const;

    Catalogue *m_catalogue;
    Library *m_library;
    X50Link *m_link;

    QLineEdit *m_search = nullptr;
    QToolButton *m_combis = nullptr, *m_programs = nullptr, *m_gm = nullptr;
    QPushButton *m_readNames = nullptr, *m_settings = nullptr, *m_live = nullptr;
    QSplitter *m_split = nullptr;
    QLabel *m_categoryTitle = nullptr;
    QListWidget *m_categories = nullptr;
    QTableView *m_table = nullptr;
    ResultModel *m_model = nullptr;
    ResultDelegate *m_delegate = nullptr;
    QPushButton *m_prev = nullptr, *m_next = nullptr, *m_play = nullptr, *m_star = nullptr;
    PanelFrame *m_setEditor = nullptr;

    QIcon m_starOn, m_starOff;
    QString m_category;  // empty = All
    SoundAddress::Kind m_listedKind = SoundAddress::Kind::Prog;
    bool m_building = true;
};
