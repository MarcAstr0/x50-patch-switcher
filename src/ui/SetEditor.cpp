#include "ui/SetEditor.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "ui/SlotCell.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kHeaderHeight = 36;
constexpr int kButtonWidth = 84;

// The combo with the chevron the QSS leaves out (`::down-arrow { image: none }`).
class SetCombo : public QComboBox
{
public:
    using QComboBox::QComboBox;

protected:
    void paintEvent(QPaintEvent *e) override
    {
        QComboBox::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(QColor(Theme::textDim), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const QPointF c(width() - 18, height() / 2.0);
        p.drawLine(QPointF(c.x() - 4, c.y() - 2), QPointF(c.x(), c.y() + 2));
        p.drawLine(QPointF(c.x(), c.y() + 2), QPointF(c.x() + 4, c.y() - 2));
    }
};

QPushButton *makeButton(const QString &text, QWidget *parent, const char *role)
{
    auto *b = new QPushButton(text, parent);
    b->setMinimumWidth(kButtonWidth);
    b->setProperty("role", QString::fromLatin1(role));
    b->setStyleSheet(QStringLiteral("QPushButton { min-height: %1px; max-height: %1px; }").arg(kHeaderHeight - 2));
    return b;
}

QToolButton *makeSizeToggle(const QString &text, QWidget *parent)
{
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setObjectName(QStringLiteral("kindToggle"));
    b->setCheckable(true);
    b->setFixedSize(kHeaderHeight, kHeaderHeight);
    // Same recipe as KindToggle: sizes by content box, border aware, no check-mark padding.
    b->setStyleSheet(QStringLiteral(
        "QToolButton#kindToggle { padding-left: 0; min-width: %1px; max-width: %1px; min-height: %1px; max-height: %1px; }"
        "QToolButton#kindToggle:checked { min-width: %2px; max-width: %2px; min-height: %2px; max-height: %2px; }")
                         .arg(kHeaderHeight - 2 * Theme::borderControl)
                         .arg(kHeaderHeight - 2 * Theme::borderFocus));
    return b;
}
} // namespace

SetEditor::SetEditor(Catalogue *catalogue, Library *library, QWidget *parent)
    : PanelFrame(parent)
    , m_catalogue(catalogue)
    , m_library(library)
{
    setObjectName(QStringLiteral("setEditor"));
    m_prompt = [this](const QString &title, const QString &label, const QString &initial) -> std::optional<QString> {
        bool ok = false;
        const QString text = QInputDialog::getText(this, title, label, QLineEdit::Normal, initial, &ok);
        if (!ok)
            return std::nullopt;
        return text;
    };
    m_confirm = [this](const QString &title, const QString &text) {
        return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) ==
               QMessageBox::Yes;
    };
    build();
    connect(m_library, &Library::changed, this, [this]() { refresh(); });
    refresh();
}

void SetEditor::build()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(Theme::s3, Theme::s3, Theme::s3, Theme::s3);
    root->setSpacing(Theme::s2);

    auto *header = new QHBoxLayout;
    header->setSpacing(Theme::s2);
    m_picker = new SetCombo(this);
    m_picker->setMinimumWidth(160);
    m_picker->setStyleSheet(QStringLiteral("QComboBox { min-height: %1px; max-height: %1px; }").arg(kHeaderHeight - 2));
    m_picker->setToolTip(QStringLiteral("Current set"));
    m_new = makeButton(QStringLiteral("New"), this, "secondary");
    m_rename = makeButton(QStringLiteral("Rename"), this, "secondary");
    m_duplicate = makeButton(QStringLiteral("Duplicate"), this, "secondary");
    m_delete = makeButton(QStringLiteral("Delete"), this, "danger");
    m_size6 = makeSizeToggle(QStringLiteral("6"), this);
    m_size8 = makeSizeToggle(QStringLiteral("8"), this);
    m_size6->setToolTip(QStringLiteral("Six slots"));
    m_size8->setToolTip(QStringLiteral("Eight slots"));
    header->addWidget(m_picker, 1);
    header->addWidget(m_new);
    header->addWidget(m_rename);
    header->addWidget(m_duplicate);
    header->addWidget(m_delete);
    header->addWidget(m_size6);
    header->addWidget(m_size8);
    root->addLayout(header);

    m_grid = new QGridLayout;
    // Each cell keeps a 4 px margin inside its widget rect for the focus ring (DESIGN 7.9: 2 px
    // ring, offset 2), so the grid has no spacing of its own and bleeds 4 px into the panel
    // padding: the visible cells and the 8 px gaps are exactly the designed size.
    m_grid->setContentsMargins(-SlotCell::kFocusMargin, -SlotCell::kFocusMargin, -SlotCell::kFocusMargin, -SlotCell::kFocusMargin);
    m_grid->setSpacing(0);
    for (int i = 0; i < 8; ++i) {
        auto *c = new SlotCell(i, this);
        m_cells.append(c);
        connect(c, &SlotCell::clicked, this, [this](int idx) { selectSlot(idx); });
        connect(c, &SlotCell::editRequested, this, [this](int idx) { editLabel(idx); });
        connect(c, &SlotCell::clearRequested, this, [this](int idx) { clearSlot(idx); });
        connect(c, &SlotCell::contextMenuRequested, this, &SetEditor::onContextMenu);
        connect(c, &SlotCell::labelCommitted, this, [this](int idx, const QString &t) { setLabel(idx, t); });
        connect(c, &SlotCell::slotDropped, this, [this](int from, int to) { moveSlot(from, to); });
        connect(c, &SlotCell::soundDropped, this, [this](int to, const QString &text) {
            const SoundAddress::ParseResult r = SoundAddress::parse(text);
            if (r.matches.size() == 1)
                dropSound(to, r.matches.first());
            else
                fail(QStringLiteral("Could not read the dropped sound: %1").arg(text));
        });
    }
    layoutGrid(8);
    root->addLayout(m_grid, 1);

    connect(m_picker, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SetEditor::onPickerChanged);
    connect(m_new, &QPushButton::clicked, this, [this]() {
        const std::optional<QString> name = m_prompt(QStringLiteral("New set"), QStringLiteral("Name of the new set"), QStringLiteral("New set"));
        if (name)
            newSet(*name);
    });
    connect(m_rename, &QPushButton::clicked, this, [this]() {
        const SetModel *s = m_library->currentSet();
        if (!s)
            return;
        const std::optional<QString> name = m_prompt(QStringLiteral("Rename set"), QStringLiteral("New name"), s->name());
        if (name)
            renameCurrent(*name);
    });
    connect(m_duplicate, &QPushButton::clicked, this, [this]() { duplicateCurrent(); });
    connect(m_delete, &QPushButton::clicked, this, [this]() {
        const SetModel *s = m_library->currentSet();
        if (s && m_confirm(QStringLiteral("Delete set"), QStringLiteral("Delete the set \"%1\"? This cannot be undone.").arg(s->name())))
            deleteCurrent();
    });
    connect(m_size6, &QToolButton::clicked, this, [this]() { resizeCurrent(6); });
    connect(m_size8, &QToolButton::clicked, this, [this]() { resizeCurrent(8); });
}

void SetEditor::layoutGrid(int size)
{
    while (m_grid->count() > 0)
        delete m_grid->takeAt(0);
    const int cols = size == 6 ? 3 : 4;
    for (int c = 0; c < 4; ++c)
        m_grid->setColumnStretch(c, c < cols ? 1 : 0);
    for (int i = 0; i < m_cells.size(); ++i) {
        const bool shown = i < size;
        m_cells[i]->setVisible(shown);
        if (shown)
            m_grid->addWidget(m_cells[i], i / cols, i % cols);
    }
    m_grid->setRowStretch(0, 1);
    m_grid->setRowStretch(1, 1);
    m_visible = size;
}

QString SetEditor::currentId() const
{
    return m_library->currentSetId();
}

QString SetEditor::describe(const SoundAddress &a) const
{
    return QStringLiteral("%1 %2").arg(a.format(), m_catalogue->effectiveName(a));
}

void SetEditor::refresh()
{
    if (m_updating)
        return;
    m_updating = true;

    // Picker: rebuilt only when the list of sets (ids, names) differs.
    QStringList key;
    for (const SetModel &s : m_library->sets())
        key << s.id() << s.name();
    if (key != m_pickerKey) {
        const QSignalBlocker block(m_picker);
        m_picker->clear();
        for (const SetModel &s : m_library->sets())
            m_picker->addItem(s.name(), s.id());
        m_pickerKey = key;
    }
    const SetModel *set = m_library->currentSet();
    {
        const QSignalBlocker block(m_picker);
        m_picker->setCurrentIndex(set ? m_picker->findData(set->id()) : -1);
    }
    const QString id = set ? set->id() : QString();
    if (id != m_shownSetId) {
        m_selected = -1;
        m_shownSetId = id;
    }
    const int size = set ? set->size() : 8;
    if (m_selected >= size)
        m_selected = size - 1;  // a shrink 8 -> 6 must not leave the selection on a hidden cell
    if (size != m_visible)
        layoutGrid(size);
    m_size6->setChecked(size == 6);
    m_size8->setChecked(size == 8);
    const bool have = set != nullptr;
    m_rename->setEnabled(have);
    m_duplicate->setEnabled(have);
    m_delete->setEnabled(have);
    m_size6->setEnabled(have);
    m_size8->setEnabled(have);
    updateCells();
    m_updating = false;
}

void SetEditor::updateCells()
{
    const SetModel *set = m_library->currentSet();
    for (int i = 0; i < m_cells.size(); ++i) {
        SlotCell *c = m_cells[i];
        if (!set || i >= set->size()) {
            c->setContent(true, QString(), QString());
            c->setCurrent(false);
            c->setSelected(false);
            c->setEnabled(false);
            continue;
        }
        c->setEnabled(true);
        const SetModel::Slot &s = set->slot(i);
        if (s.isEmpty()) {
            c->setContent(true, QString(), QString());
        } else {
            const QString name = m_catalogue->effectiveName(*s.address);
            const QString addr = s.address->format();
            if (s.label.isEmpty())
                c->setContent(false, name, addr);
            else
                c->setContent(false, s.label, QStringLiteral("%1 - %2").arg(addr, name));
        }
        c->setCurrent(!s.isEmpty() && m_current && *s.address == *m_current);
        c->setSelected(i == m_selected);
    }
}

void SetEditor::setCurrentSound(const std::optional<SoundAddress> &address)
{
    if (m_current == address)
        return;
    m_current = address;
    updateCells();
}

void SetEditor::refreshNames()
{
    updateCells();
}

void SetEditor::onPickerChanged(int index)
{
    if (m_updating || index < 0)
        return;
    const QString id = m_picker->itemData(index).toString();
    const SetModel::Result r = m_library->setCurrentSet(id);
    if (!r.ok())
        fail(r.error);
}

void SetEditor::selectSlot(int idx)
{
    const SetModel *set = m_library->currentSet();
    if (idx >= 0 && (!set || idx >= set->size()))
        idx = -1;
    if (idx == m_selected)
        return;
    m_selected = idx;
    updateCells();
}

// ---------------------------------------------------------------- operations

bool SetEditor::newSet(const QString &name)
{
    const QString trimmed = name.trimmed();
    const SetModel *cur = m_library->currentSet();
    const SetModel::Result r = m_library->createSet(trimmed, cur ? cur->size() : 8);
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Created set %1").arg(trimmed));
    return true;
}

bool SetEditor::renameCurrent(const QString &name)
{
    const SetModel::Result r = m_library->renameSet(currentId(), name.trimmed());
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Renamed set to %1").arg(name.trimmed()));
    return true;
}

bool SetEditor::duplicateCurrent()
{
    const SetModel *set = m_library->currentSet();
    if (!set)
        return false;
    const QString from = set->name();
    QString newId;
    const SetModel::Result r = m_library->duplicateSet(set->id(), &newId);
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    const SetModel *copy = m_library->setById(newId);
    say(QStringLiteral("Duplicated %1 as %2").arg(from, copy ? copy->name() : QString()));
    return true;
}

bool SetEditor::deleteCurrent()
{
    const SetModel *set = m_library->currentSet();
    if (!set)
        return false;
    const QString name = set->name();
    const SetModel::Result r = m_library->deleteSet(set->id());
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Deleted set %1").arg(name));
    return true;
}

bool SetEditor::resizeCurrent(int size)
{
    const SetModel *set = m_library->currentSet();
    if (!set)
        return false;
    if (set->size() == size) {
        refresh();  // the toggle flipped itself on click; put it back
        return true;
    }
    const SetModel::Result r = m_library->resizeSet(set->id(), size);
    if (!r.ok()) {
        fail(r.error);
        refresh();  // restores the toggle state
        return false;
    }
    say(QStringLiteral("%1 now has %2 slots").arg(set->name()).arg(size));
    return true;
}

bool SetEditor::moveSlot(int from, int to)
{
    const SetModel *set = m_library->currentSet();
    if (!set || from == to)
        return false;
    const int before = m_selected;
    m_selected = to;  // set first: Library::changed() refreshes the cells with it
    const SetModel::Result r = m_library->moveSlot(set->id(), from, to);
    if (!r.ok()) {
        m_selected = before;
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Moved slot %1 to slot %2").arg(from + 1).arg(to + 1));
    return true;
}

bool SetEditor::dropSound(int to, const SoundAddress &address)
{
    const SetModel *set = m_library->currentSet();
    if (!set) {
        fail(QStringLiteral("No current set to add to"));
        return false;
    }
    const QString setName = set->name();
    const int before = m_selected;
    m_selected = to;  // set first: Library::changed() refreshes the cells with it
    const SetModel::Result r = m_library->setSlot(set->id(), to, address);
    if (!r.ok()) {
        m_selected = before;
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Put %1 in slot %2 of %3").arg(describe(address)).arg(to + 1).arg(setName));
    return true;
}

bool SetEditor::clearSlot(int idx)
{
    const SetModel *set = m_library->currentSet();
    if (!set || idx < 0 || idx >= set->size() || set->slot(idx).isEmpty())
        return false;
    const SetModel::Result r = m_library->clearSlot(set->id(), idx);
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    say(QStringLiteral("Emptied slot %1").arg(idx + 1));
    return true;
}

bool SetEditor::setLabel(int idx, const QString &label)
{
    const SetModel *set = m_library->currentSet();
    if (!set || idx < 0 || idx >= set->size())
        return false;
    const QString trimmed = label.trimmed();
    if (set->slot(idx).label == trimmed)
        return true;  // nothing changed (also when an empty cell was left alone)
    const SetModel::Result r = m_library->setLabel(set->id(), idx, trimmed);
    if (!r.ok()) {
        fail(r.error);
        return false;
    }
    say(trimmed.isEmpty() ? QStringLiteral("Cleared the label of slot %1").arg(idx + 1)
                          : QStringLiteral("Labelled slot %1 \"%2\"").arg(idx + 1).arg(trimmed));
    return true;
}

void SetEditor::editLabel(int idx)
{
    const SetModel *set = m_library->currentSet();
    if (!set || idx < 0 || idx >= set->size())
        return;
    if (set->slot(idx).isEmpty()) {
        say(QStringLiteral("Slot %1 is empty: put a sound in it first").arg(idx + 1));
        return;
    }
    selectSlot(idx);
    m_cells[idx]->beginEdit(set->slot(idx).label);
}

void SetEditor::onContextMenu(int idx, const QPoint &globalPos)
{
    const SetModel *set = m_library->currentSet();
    if (!set || idx >= set->size())
        return;
    selectSlot(idx);
    const bool filled = !set->slot(idx).isEmpty();
    QMenu menu(this);
    QAction *edit = menu.addAction(QStringLiteral("Edit label"));
    QAction *clear = menu.addAction(QStringLiteral("Empty slot"));
    edit->setEnabled(filled);
    clear->setEnabled(filled);
    const QAction *chosen = menu.exec(globalPos);
    if (chosen == edit)
        editLabel(idx);
    else if (chosen == clear)
        clearSlot(idx);
}
