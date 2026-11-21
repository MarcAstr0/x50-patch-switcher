#pragma once

#include <functional>
#include <optional>

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/SoundAddress.h"
#include "ui/PanelFrame.h"

class Catalogue;
class Library;
class QComboBox;
class QPoint;
class QPushButton;
class QToolButton;
class QGridLayout;
class SlotCell;

// The set editor panel (SPEC 4.1, DESIGN 7.9): set picker, New / Rename / Duplicate /
// Delete, the 6 | 8 size toggle and a grid of slot cells shaped like the live view.
// Every change goes through Library (so saves debounce) and the grid is rebuilt from
// Library::changed(), so Ctrl+1..8 in the browse view shows at once. Failures (a refused
// resize, say) are reported through statusMessage() with the model's own text.
//
// Nothing here sends MIDI and nothing needs a link: a click on a cell only selects it,
// a double-click or F2 edits the label, Del empties the slot. The prompts (a name, a
// delete confirmation) are modal dialogs, allowed in browse; tests replace them.
// No timers; a cell repaints only when its content or state changes.
class SetEditor : public PanelFrame
{
    Q_OBJECT
public:
    // Neither pointer is owned. Both are required.
    SetEditor(Catalogue *catalogue, Library *library, QWidget *parent = nullptr);

    // The sound the X50 is on (marks its cell with the corner triangle); nullopt clears it.
    void setCurrentSound(const std::optional<SoundAddress> &address);
    // Re-read sound names (after the unit's names were applied).
    void refreshNames();

    // ---- operations (what the buttons, menus and drops call); each reports in the status line
    bool newSet(const QString &name);       // same size as the current set (8 if none)
    bool renameCurrent(const QString &name);
    bool duplicateCurrent();
    bool deleteCurrent();                   // no confirmation here; the Delete button asks first
    bool resizeCurrent(int size);           // 6 or 8
    bool moveSlot(int from, int to);        // 0-based; swaps onto an occupied slot
    bool dropSound(int to, const SoundAddress &address);
    bool clearSlot(int idx);
    bool setLabel(int idx, const QString &label);
    void editLabel(int idx);                // opens the inline editor
    void selectSlot(int idx);               // -1 deselects
    int selectedSlot() const { return m_selected; }

    // ---- prompts, replaceable (tests). Defaults are QInputDialog / QMessageBox.
    // A text prompt returns nullopt when cancelled.
    using TextPrompt = std::function<std::optional<QString>(const QString &title, const QString &label, const QString &initial)>;
    using Confirm = std::function<bool(const QString &title, const QString &text)>;
    void setTextPrompt(TextPrompt p) { m_prompt = std::move(p); }
    void setConfirm(Confirm c) { m_confirm = std::move(c); }

    // ---- widgets (tests)
    QComboBox *picker() const { return m_picker; }
    QPushButton *newButton() const { return m_new; }
    QPushButton *renameButton() const { return m_rename; }
    QPushButton *duplicateButton() const { return m_duplicate; }
    QPushButton *deleteButton() const { return m_delete; }
    QToolButton *size6Button() const { return m_size6; }
    QToolButton *size8Button() const { return m_size8; }
    int visibleCellCount() const { return m_visible; }
    SlotCell *cell(int idx) const { return m_cells.at(idx); }

signals:
    void statusMessage(const QString &text, bool isError);

private:
    void build();
    void refresh();
    void layoutGrid(int size);
    void updateCells();
    void onPickerChanged(int index);
    void onContextMenu(int idx, const QPoint &globalPos);
    void say(const QString &text) { emit statusMessage(text, false); }
    void fail(const QString &text) { emit statusMessage(text, true); }
    QString describe(const SoundAddress &a) const;
    QString currentId() const;

    Catalogue *m_catalogue;
    Library *m_library;
    QComboBox *m_picker = nullptr;
    QPushButton *m_new = nullptr, *m_rename = nullptr, *m_duplicate = nullptr, *m_delete = nullptr;
    QToolButton *m_size6 = nullptr, *m_size8 = nullptr;
    QGridLayout *m_grid = nullptr;
    QVector<SlotCell *> m_cells;  // always 8; the last two are hidden for a 6 slot set
    int m_visible = 8;
    int m_selected = -1;
    QString m_shownSetId;
    QStringList m_pickerKey;  // ids and names the combo was built from
    std::optional<SoundAddress> m_current;
    bool m_updating = false;
    TextPrompt m_prompt;
    Confirm m_confirm;
};
