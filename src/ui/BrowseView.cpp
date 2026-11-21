#include "ui/BrowseView.h"

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/SearchQuery.h"
#include "core/SetModel.h"
#include "midi/X50Link.h"
#include "ui/CategoryDelegate.h"
#include "ui/Glyphs.h"
#include "ui/KindToggle.h"
#include "ui/PanelFrame.h"
#include "ui/SetEditor.h"
#include "ui/ResultDelegate.h"
#include "ui/ResultModel.h"
#include "ui/ResultTable.h"
#include "ui/SearchField.h"
#include "ui/Theme.h"

#include <QAction>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSet>
#include <QSplitter>
#include <QTableView>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Category list rows: Favourites, Recent, separator, then All (DESIGN 7.6).
constexpr int kFirstAllRow = 3;

bool isGm(const SoundAddress &a)
{
    return a.bank() == SoundAddress::Bank::G || a.bank() == SoundAddress::Bank::Gd;
}

// app.qss gives min-height/max-height as the content box, so a 1 px border makes the control 2 px
// taller than DESIGN's 40 (7.3, 7.8). A widget-level rule corrects it (the app sheet is not ours
// to edit); `selector` is the widget's own type.
void fixToolbarHeight(QWidget *w, const QString &selector, int total)
{
    const int content = total - 2 * Theme::borderControl;
    w->setStyleSheet(QStringLiteral("%1 { min-height: %2px; max-height: %2px; }").arg(selector).arg(content));
}

QPushButton *makeButton(const QString &text, int width, QWidget *parent, bool primary = false)
{
    auto *b = new QPushButton(text, parent);
    b->setMinimumWidth(width);  // DESIGN widths are the floor: a wider text grows the button, never clips
    b->setProperty("role", primary ? QStringLiteral("primary") : QStringLiteral("secondary"));
    fixToolbarHeight(b, QStringLiteral("QPushButton"), Theme::browseToolbarHeight);
    return b;
}

// Distinct factory categories of a kind in order of first appearance, GM banks left out
// (Decisions log 2026-11-21: the unit's own order is unknown until it was read).
QStringList firstAppearanceCategories(const Catalogue &cat, SoundAddress::Kind kind)
{
    QStringList out;
    QSet<QString> seen;
    for (const Catalogue::Entry &e : cat.entries()) {
        if (e.address.kind() != kind || isGm(e.address))
            continue;
        if (!seen.contains(e.category)) {
            seen.insert(e.category);
            out << e.category;
        }
    }
    return out;
}

} // namespace

BrowseView::BrowseView(Catalogue *catalogue, Library *library, X50Link *link, QWidget *parent)
    : QWidget(parent)
    , m_catalogue(catalogue)
    , m_library(library)
    , m_link(link)
{
    buildUi();
    rebuildCategories();
    refilter();
    m_building = false;
    updateControls();

    if (m_link) {
        connect(m_link, &X50Link::sent, this, &BrowseView::onSent);
        connect(m_link, &X50Link::sendFailed, this, &BrowseView::onSendFailed);
        connect(m_link, &X50Link::followedSound, this, [this](const SoundAddress &) { syncCurrent(); });
        connect(m_link, &X50Link::followedMode, this, [this]() { syncCurrent(); });
        connect(m_link, &X50Link::stateChanged, this, [this]() { syncCurrent(); });
        connect(m_link, &X50Link::probeFinished, this, [this]() { syncCurrent(); });
        syncCurrent();
    }
    connect(m_library, &Library::changed, this, [this]() {
        m_model->refreshMarks();  // rows stay put (DESIGN 7.7): only a rebuild drops or reorders them
        updateEmptyText();
        updateControls();
    });

    // Focus on start (SPEC 4.1).
    m_search->setFocus();
}

void BrowseView::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(Theme::browseGap);

    // ---- toolbar (DESIGN 7.3)
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(Theme::s2);
    m_search = new SearchField(this);
    m_search->setObjectName(QStringLiteral("searchField"));
    m_search->setPlaceholderText(QStringLiteral("Search name, address (A012), cat:text"));
    m_search->setMinimumWidth(260);
    fixToolbarHeight(m_search, QStringLiteral("QLineEdit#searchField"), Theme::browseToolbarHeight);
    {
        auto *clear = new QAction(Glyphs::icon(Glyphs::Kind::Cross, QSize(16, 16), QColor(Theme::textDim)), QString(), m_search);
        clear->setVisible(false);
        m_search->addAction(clear, QLineEdit::TrailingPosition);
        connect(clear, &QAction::triggered, m_search, &QLineEdit::clear);
        connect(m_search, &QLineEdit::textChanged, this, [clear](const QString &t) { clear->setVisible(!t.isEmpty()); });
    }
    connect(m_search, &QLineEdit::textChanged, this, [this]() { refilter(); });
    toolbar->addWidget(m_search, 1);

    auto *kinds = new QHBoxLayout;
    kinds->setSpacing(Theme::s1);
    m_combis = new KindToggle(QStringLiteral("Combis"), this);
    m_programs = new KindToggle(QStringLiteral("Programs"), this);
    m_gm = new KindToggle(QStringLiteral("GM"), this);
    for (QToolButton *t : {m_combis, m_programs, m_gm}) {
        kinds->addWidget(t);
        connect(t, &QToolButton::toggled, this, [this]() {
            if (m_building)
                return;
            if (shownCategoryKind() != m_listedKind)
                rebuildCategories();
            refilter();
        });
    }
    toolbar->addLayout(kinds);

    m_readNames = makeButton(QStringLiteral("Read names"), Theme::readNamesWidth, this);
    m_readNames->setEnabled(false);  // T3.5 enables it while connected
    m_readNames->setToolTip(QStringLiteral("Not connected to an X50"));
    m_settings = makeButton(QStringLiteral("Settings"), Theme::settingsWidth, this);
    m_settings->setEnabled(m_link != nullptr);  // the dialog needs a link
    m_live = makeButton(QStringLiteral("Live  F5"), Theme::liveWidth, this, true);
    m_live->setToolTip(QStringLiteral("F5"));
    connect(m_readNames, &QPushButton::clicked, this, &BrowseView::readNamesRequested);
    connect(m_settings, &QPushButton::clicked, this, &BrowseView::settingsRequested);
    connect(m_live, &QPushButton::clicked, this, &BrowseView::liveRequested);
    toolbar->addWidget(m_readNames);
    toolbar->addWidget(m_settings);
    toolbar->addWidget(m_live);
    root->addLayout(toolbar);

    // ---- main area: category list | result list, transport, set editor (DESIGN 7.1)
    auto *split = new QSplitter(Qt::Horizontal, this);
    m_split = split;
    split->setChildrenCollapsible(false);
    split->setHandleWidth(Theme::browseSplitterHandle);

    auto *catPanel = new PanelFrame(split);
    catPanel->setObjectName(QStringLiteral("categoryPanel"));
    catPanel->setMinimumWidth(Theme::browseCategoryMinWidth);
    catPanel->setMaximumWidth(Theme::browseCategoryMaxWidth);
    auto *catLayout = new QVBoxLayout(catPanel);
    catLayout->setContentsMargins(0, 0, 0, 0);
    catLayout->setSpacing(0);
    m_categoryTitle = new QLabel(catPanel);
    m_categoryTitle->setProperty("role", QStringLiteral("sectionTitle"));
    m_categoryTitle->setFixedHeight(Theme::browseCategoryTitleHeight + Theme::s2);
    m_categoryTitle->setContentsMargins(Theme::s2 + Theme::s3, Theme::s2, 0, 0);
    m_categoryTitle->setFont(Theme::roleFont(Theme::Role::Label));
    catLayout->addWidget(m_categoryTitle);
    m_categories = new QListWidget(catPanel);
    m_categories->setObjectName(QStringLiteral("categoryList"));
    m_categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_categories->setItemDelegate(new CategoryDelegate(m_categories));
    m_categories->setMouseTracking(true);
    // The title block above is 32 high already (DESIGN 7.6: 8 padding + 24 title row), so the first
    // row starts right under it; app.qss would add another 8 px of top padding.
    m_categories->setStyleSheet(QStringLiteral("QListView#categoryList { padding: 0 8px 8px 8px; }"));
    connect(m_categories, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_building || row < 0)
            return;
        onCategoryRow(row);
    });
    catLayout->addWidget(m_categories, 1);

    auto *right = new QWidget(split);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(Theme::browseGap);

    m_model = new ResultModel(m_catalogue, m_library, this);
    m_delegate = new ResultDelegate(this);
    m_table = new ResultTable(right);
    m_table->setObjectName(QStringLiteral("resultList"));
    m_table->setModel(m_model);
    m_table->setItemDelegate(m_delegate);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setShowGrid(false);
    m_table->setCornerButtonEnabled(false);
    m_table->setWordWrap(false);
    m_table->setMouseTracking(true);
    m_table->setDragEnabled(true);  // a row can be dropped on a set editor cell (copy)
    m_table->setDragDropMode(QAbstractItemView::DragOnly);
    m_table->setDefaultDropAction(Qt::CopyAction);
    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->verticalHeader()->hide();
    m_table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->verticalHeader()->setDefaultSectionSize(Theme::resultRowHeight);
    QHeaderView *header = m_table->horizontalHeader();
    header->setSectionsClickable(false);
    header->setSectionsMovable(false);
    header->setHighlightSections(false);
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setSectionResizeMode(ResultModel::ColName, QHeaderView::Stretch);
    // DESIGN 7.7: the list's top border plus the header are 28 high together.
    header->setFixedHeight(Theme::resultHeaderHeight - Theme::borderControl);
    header->setMinimumSectionSize(Theme::colUsed);
    header->resizeSection(ResultModel::ColStar, Theme::resultCellPadding + Theme::colStar);
    header->resizeSection(ResultModel::ColAddress, Theme::colAddress);
    header->resizeSection(ResultModel::ColEdited, Theme::colEdited);
    header->resizeSection(ResultModel::ColCategory, Theme::colCategory);
    header->resizeSection(ResultModel::ColUsed, Theme::colUsed + Theme::resultCellPadding);
    rightLayout->addWidget(m_table, 1);

    connect(m_table->selectionModel(), &QItemSelectionModel::currentChanged, this, [this]() { updateControls(); });
    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this]() { updateControls(); });
    connect(m_table, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (idx.isValid() && idx.column() != ResultModel::ColStar)
            sendSound(m_model->address(idx.row()));
    });
    connect(m_table, &QWidget::customContextMenuRequested, this, &BrowseView::onContextMenu);
    connect(m_delegate, &ResultDelegate::starClicked, this, [this](int row) {
        if (row >= 0 && row < m_model->rowCount())
            m_library->toggleFavourite(m_model->address(row));
    });

    // Transport bar (DESIGN 7.8).
    auto *transport = new QWidget(right);
    transport->setObjectName(QStringLiteral("transportBar"));
    transport->setFixedHeight(Theme::browseTransportHeight);
    auto *tl = new QHBoxLayout(transport);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(Theme::s2);
    const QColor textCol(Theme::text);
    m_prev = makeButton(QStringLiteral("Prev"), Theme::transportPrevWidth, transport);
    m_prev->setIcon(Glyphs::icon(Glyphs::Kind::ChevronLeft, QSize(14, 14), textCol));
    m_prev->setToolTip(QStringLiteral("Ctrl+Left"));
    m_next = makeButton(QStringLiteral("Next"), Theme::transportNextWidth, transport);
    m_next->setIcon(Glyphs::icon(Glyphs::Kind::ChevronRight, QSize(14, 14), textCol));
    m_next->setLayoutDirection(Qt::RightToLeft);  // icon after the text
    m_next->setToolTip(QStringLiteral("Ctrl+Right"));
    m_play = makeButton(QStringLiteral("Play"), Theme::transportPlayWidth, transport, true);
    m_play->setToolTip(QStringLiteral("Enter"));
    m_star = makeButton(QStringLiteral("Star"), Theme::transportStarWidth, transport);
    m_star->setToolTip(QStringLiteral("Ctrl+D"));
    m_starOn = Glyphs::icon(Glyphs::Kind::StarFilled, QSize(16, 16), QColor(Theme::amber));
    m_starOff = Glyphs::icon(Glyphs::Kind::StarOutline, QSize(16, 16), QColor(Theme::textDim));
    m_star->setIcon(m_starOff);
    tl->addWidget(m_prev);
    tl->addWidget(m_next);
    tl->addWidget(m_play);
    tl->addStretch(1);
    tl->addWidget(m_star);
    connect(m_prev, &QPushButton::clicked, this, [this]() { step(-1); });
    connect(m_next, &QPushButton::clicked, this, [this]() { step(1); });
    connect(m_play, &QPushButton::clicked, this, [this]() { sendHighlighted(); });
    connect(m_star, &QPushButton::clicked, this, [this]() { toggleFavouriteHighlighted(); });
    rightLayout->addWidget(transport);

    // Set editor (T3.6): its messages go to the status line like ours.
    m_editor = new SetEditor(m_catalogue, m_library, right);
    m_editor->setFixedHeight(Theme::browseSetEditorHeight);
    connect(m_editor, &SetEditor::statusMessage, this, &BrowseView::statusMessage);
    rightLayout->addWidget(m_editor);

    split->addWidget(catPanel);
    split->addWidget(right);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setSizes({Theme::browseCategoryWidth, 10000});
    root->addWidget(split, 1);

    // One filter for every focusable child: the shortcuts must work wherever the focus is.
    const QList<QWidget *> children = findChildren<QWidget *>();
    for (QWidget *w : children)
        if (w->focusPolicy() != Qt::NoFocus)
            w->installEventFilter(this);
}

// ---------------------------------------------------------------- state

QString BrowseView::searchText() const { return m_search->text(); }
void BrowseView::setSearchText(const QString &text) { m_search->setText(text); }

bool BrowseView::kindEnabled(SoundAddress::Kind kind, bool gm) const
{
    if (kind == SoundAddress::Kind::Combi)
        return m_combis->isChecked();
    return gm ? m_gm->isChecked() : m_programs->isChecked();
}

void BrowseView::setKinds(bool combis, bool programs, bool gm)
{
    m_combis->setChecked(combis);
    m_programs->setChecked(programs);
    m_gm->setChecked(gm);
}

SoundAddress::Kind BrowseView::shownCategoryKind() const
{
    // DESIGN 7.6: combinations only shows the combination categories; every other mix
    // shows the program categories.
    return (m_combis->isChecked() && !m_programs->isChecked() && !m_gm->isChecked()) ? SoundAddress::Kind::Combi
                                                                                   : SoundAddress::Kind::Prog;
}

void BrowseView::rebuildCategories()
{
    const bool wasBuilding = m_building;
    m_building = true;
    m_listedKind = shownCategoryKind();
    const QStringList names = m_catalogue->hasUnitCategories() ? m_catalogue->categories(m_listedKind)
                                                               : firstAppearanceCategories(*m_catalogue, m_listedKind);
    m_categories->clear();
    auto add = [this](const QString &text, int kind) {
        auto *item = new QListWidgetItem(text);
        item->setData(CategoryDelegate::KindRole, kind);
        if (kind == CategoryDelegate::Separator)
            item->setFlags(Qt::NoItemFlags);  // neither selectable nor focusable: Up/Down skip it
        m_categories->addItem(item);
    };
    add(QStringLiteral("Favourites"), CategoryDelegate::Favourites);
    add(QStringLiteral("Recent"), CategoryDelegate::Recent);
    add(QString(), CategoryDelegate::Separator);
    add(QStringLiteral("All"), CategoryDelegate::All);
    for (const QString &n : names)
        add(n, CategoryDelegate::Category);
    int row = kFirstAllRow;
    if (m_source == Source::Favourites) {
        row = 0;
    } else if (m_source == Source::Recent) {
        row = 1;
    } else if (!m_category.isEmpty()) {
        const int found = names.indexOf(m_category);
        if (found >= 0)
            row = found + kFirstAllRow + 1;
        else
            m_category.clear();
    }
    m_categories->setCurrentRow(row);
    m_categoryTitle->setText(m_listedKind == SoundAddress::Kind::Combi ? QStringLiteral("CATEGORY (COMBI)")
                                                                       : QStringLiteral("CATEGORY (PROG)"));
    m_building = wasBuilding;
}

void BrowseView::onCategoryRow(int row)
{
    const int kind = m_categories->item(row)->data(CategoryDelegate::KindRole).toInt();
    Source source = Source::Browse;
    QString name;
    if (kind == CategoryDelegate::Favourites)
        source = Source::Favourites;
    else if (kind == CategoryDelegate::Recent)
        source = Source::Recent;
    else if (kind == CategoryDelegate::Category)
        name = m_categories->item(row)->text();
    else if (kind != CategoryDelegate::All)
        return;  // the separator (cannot be current)
    // Re-selecting the same view does nothing: only a change rebuilds (DESIGN 7.7).
    if (source == m_source && name == m_category)
        return;
    m_source = source;
    m_category = name;
    refilter();
}

void BrowseView::setSource(Source source)
{
    if (source == Source::Browse) {
        setCategory(QString());
        return;
    }
    m_categories->setCurrentRow(source == Source::Favourites ? 0 : 1);
}

bool BrowseView::setCategory(const QString &name)
{
    if (name.isEmpty()) {
        m_categories->setCurrentRow(kFirstAllRow);
        return true;
    }
    for (int i = kFirstAllRow + 1; i < m_categories->count(); ++i) {
        if (m_categories->item(i)->text() == name) {
            m_categories->setCurrentRow(i);
            return true;
        }
    }
    return false;
}

void BrowseView::refreshCategories()
{
    rebuildCategories();
    refilter();
}

void BrowseView::refreshNames()
{
    m_model->refreshAll();
    refreshCategories();
    m_editor->refreshNames();
}

QStringList BrowseView::categoryNames() const
{
    QStringList l;
    for (int i = kFirstAllRow; i < m_categories->count(); ++i)
        l << m_categories->item(i)->text();
    return l;
}

int BrowseView::categoryWidth() const { return m_split->sizes().value(0); }

void BrowseView::setCategoryWidth(int width)
{
    const int w = qBound(Theme::browseCategoryMinWidth, width, Theme::browseCategoryMaxWidth);
    m_split->setSizes({w, qMax(1, m_split->width() - w)});
}

QString BrowseView::categoryTitle() const { return m_categoryTitle->text(); }

QVector<SoundAddress> BrowseView::results() const { return m_model->addresses(); }
int BrowseView::resultCount() const { return m_model->rowCount(); }

int BrowseView::currentRow() const
{
    const QItemSelectionModel *sel = m_table->selectionModel();
    const QModelIndex idx = sel->currentIndex();
    return (idx.isValid() && sel->isRowSelected(idx.row())) ? idx.row() : -1;
}

std::optional<SoundAddress> BrowseView::highlighted() const
{
    const int row = currentRow();
    if (row < 0)
        return std::nullopt;
    return m_model->address(row);
}

void BrowseView::selectRow(int row)
{
    const QModelIndex idx = m_model->index(row, 0);
    m_table->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_table->scrollTo(idx);
}

bool BrowseView::highlight(const SoundAddress &address)
{
    const int row = m_model->rowOf(address);
    if (row < 0)
        return false;
    selectRow(row);
    return true;
}

void BrowseView::clearHighlight()
{
    m_table->selectionModel()->clear();
}

void BrowseView::refilter()
{
    const std::optional<SoundAddress> keep = highlighted();
    const SearchQuery query = SearchQuery::parse(m_search->text());
    const bool wantCombi = m_combis->isChecked(), wantProg = m_programs->isChecked(), wantGm = m_gm->isChecked();
    auto accept = [&](const Catalogue::Entry &e) {
        const SoundAddress &a = e.address;
        if (a.kind() == SoundAddress::Kind::Combi ? !wantCombi : (isGm(a) ? !wantGm : !wantProg))
            return false;
        if (m_source == Source::Browse && !m_category.isEmpty() &&
            m_catalogue->effectiveCategory(a).compare(m_category, Qt::CaseInsensitive) != 0)
            return false;
        return query.matches(*m_catalogue, e);
    };
    QVector<SoundAddress> out;
    out.reserve(m_catalogue->size());
    if (m_source == Source::Recent) {
        // Most recent first, the library's order (DESIGN 7.7), not catalogue order.
        for (const Library::Recent &r : m_library->recents()) {
            const Catalogue::Entry *e = m_catalogue->find(r.address);
            if (e && accept(*e))
                out.append(r.address);
        }
    } else {
        for (const Catalogue::Entry &e : m_catalogue->entries()) {
            if (m_source == Source::Favourites && !m_library->isFavourite(e.address))
                continue;
            if (accept(e))
                out.append(e.address);
        }
    }
    m_model->setResults(out);
    int row = keep ? m_model->rowOf(*keep) : -1;
    // A changed result set highlights its first row, so typing and Enter send it (the highlight
    // itself never sends). Not at construction: nothing is highlighted until the user acts.
    if (row < 0 && !m_building && m_model->rowCount() > 0)
        row = 0;
    if (row >= 0)
        selectRow(row);
    updateEmptyText();
    updateControls();
}

QString BrowseView::emptyText() const { return static_cast<ResultTable *>(m_table)->emptyText(); }

void BrowseView::updateEmptyText()
{
    // "Nothing at all" texts apply when the view has no entries before search and kinds.
    QString text = QStringLiteral("No sounds match.");
    if (m_source == Source::Favourites && m_library->favourites().isEmpty())
        text = QStringLiteral("No favourites yet. Star a sound with Ctrl+D or the star.");
    else if (m_source == Source::Recent && m_library->recents().isEmpty())
        text = QStringLiteral("Nothing played yet. Sounds you send appear here.");
    static_cast<ResultTable *>(m_table)->setEmptyText(text);
}

void BrowseView::updateControls()
{
    if (m_building && !m_play)
        return;
    const std::optional<SoundAddress> h = highlighted();
    m_play->setEnabled(h.has_value());
    m_star->setEnabled(h.has_value());
    m_star->setIcon(h && m_library->isFavourite(*h) ? m_starOn : m_starOff);
    const int row = currentRow();
    m_prev->setEnabled(m_model->rowCount() > 0 && row != 0);
    m_next->setEnabled(m_model->rowCount() > 0 && row != m_model->rowCount() - 1);
}

PanelFrame *BrowseView::setEditorPanel() const { return m_editor; }

void BrowseView::syncCurrent()
{
    const std::optional<SoundAddress> cur = m_link ? m_link->currentSound() : std::nullopt;
    m_model->setCurrent(cur);
    if (m_editor)
        m_editor->setCurrentSound(cur);
}

QString BrowseView::describe(const SoundAddress &a) const
{
    return QStringLiteral("%1 %2").arg(a.format(), m_catalogue->effectiveName(a));
}

// ---------------------------------------------------------------- actions

void BrowseView::sendSound(const SoundAddress &address)
{
    if (!m_link) {
        emit statusMessage(QStringLiteral("Not connected to the X50"), true);
        return;
    }
    m_link->send(address);
}

void BrowseView::sendHighlighted()
{
    const std::optional<SoundAddress> h = highlighted();
    if (!h) {
        emit statusMessage(QStringLiteral("Highlight a sound first"), false);
        return;
    }
    sendSound(*h);
}

void BrowseView::step(int dir)
{
    const int rows = m_model->rowCount();
    if (rows == 0)
        return;
    const int cur = currentRow();
    const int next = cur < 0 ? (dir > 0 ? 0 : rows - 1) : cur + dir;
    if (next < 0 || next >= rows) {
        emit statusMessage(dir > 0 ? QStringLiteral("End of the results") : QStringLiteral("Start of the results"), false);
        return;
    }
    selectRow(next);
    sendSound(m_model->address(next));
}

void BrowseView::moveHighlight(int delta)
{
    const int rows = m_model->rowCount();
    if (rows == 0)
        return;
    const int cur = currentRow();
    const int next = cur < 0 ? (delta > 0 ? 0 : rows - 1) : qBound(0, cur + delta, rows - 1);
    selectRow(next);  // highlight only: nothing is sent
}

void BrowseView::toggleFavouriteHighlighted()
{
    const std::optional<SoundAddress> h = highlighted();
    if (!h) {
        emit statusMessage(QStringLiteral("Highlight a sound first"), false);
        return;
    }
    const bool on = m_library->toggleFavourite(*h);
    emit statusMessage(QStringLiteral("%1 %2").arg(on ? QStringLiteral("Starred") : QStringLiteral("Unstarred"), describe(*h)), false);
}

void BrowseView::addHighlightedToSlot(int slot)
{
    const std::optional<SoundAddress> h = highlighted();
    if (!h) {
        emit statusMessage(QStringLiteral("Highlight a sound first"), false);
        return;
    }
    const SetModel *set = m_library->currentSet();
    if (!set) {
        emit statusMessage(QStringLiteral("No current set to add to"), true);
        return;
    }
    if (slot < 1 || slot > set->size()) {
        emit statusMessage(QStringLiteral("Slot %1 does not exist: %2 has %3 slots").arg(slot).arg(set->name()).arg(set->size()), true);
        return;
    }
    const QString name = set->name();
    const SetModel::Result r = m_library->setSlot(set->id(), slot - 1, *h);
    if (!r.ok()) {
        emit statusMessage(r.error, true);
        return;
    }
    emit statusMessage(QStringLiteral("Put %1 in slot %2 of %3").arg(describe(*h)).arg(slot).arg(name), false);
}

void BrowseView::onSent(const SoundAddress &address)
{
    m_library->recordUse(address);  // on a successful send only (the `sent` signal)
    syncCurrent();
    emit statusMessage(QStringLiteral("Sent %1").arg(describe(address)), false);
}

void BrowseView::onSendFailed(const SoundAddress &address, const QString &reason)
{
    emit statusMessage(QStringLiteral("Could not send %1: %2").arg(address.format(), reason), true);
}

void BrowseView::onContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_table->indexAt(pos);
    if (!idx.isValid())
        return;
    selectRow(idx.row());
    QMenu menu(this);
    QMenu *sub = menu.addMenu(QStringLiteral("Add to set"));
    const SetModel *set = m_library->currentSet();
    if (!set) {
        sub->setEnabled(false);
    } else {
        for (int i = 0; i < set->size(); ++i) {
            const SetModel::Slot &s = set->slot(i);
            QString what = QStringLiteral("empty");
            if (s.address)
                what = s.label.isEmpty() ? m_catalogue->effectiveName(*s.address) : s.label;
            QAction *a = sub->addAction(QStringLiteral("%1   %2").arg(i + 1).arg(what));
            connect(a, &QAction::triggered, this, [this, i]() { addHighlightedToSlot(i + 1); });
        }
    }
    menu.exec(m_table->viewport()->mapToGlobal(pos));
}

// ---------------------------------------------------------------- keys

bool BrowseView::handleKey(QObject *source, QKeyEvent *ev)
{
    // Keys typed in the set editor (the label field, a focused cell) are never browse shortcuts:
    // PgUp/PgDn and Ctrl+Left/Right would send MIDI, Ctrl+1..8 would overwrite a slot.
    if (const auto *w = qobject_cast<const QWidget *>(source); w && m_editor && (w == m_editor || m_editor->isAncestorOf(w)))
        return false;
    const Qt::KeyboardModifiers mods = ev->modifiers() & ~Qt::KeypadModifier;
    const int key = ev->key();
    const bool repeat = ev->isAutoRepeat();  // a held key never repeats a send
    const bool inSearch = source == m_search;
    const bool inList = source == m_table || source == m_categories;

    if ((key == Qt::Key_Return || key == Qt::Key_Enter) && mods == Qt::NoModifier && (inSearch || source == m_table)) {
        if (!repeat)
            sendHighlighted();
        return true;
    }
    if (mods == Qt::ControlModifier && (key == Qt::Key_Left || key == Qt::Key_Right)) {
        if (!repeat)
            step(key == Qt::Key_Right ? 1 : -1);
        return true;
    }
    if (mods == Qt::NoModifier && (key == Qt::Key_PageUp || key == Qt::Key_PageDown) && !inList) {
        if (!repeat)
            step(key == Qt::Key_PageDown ? 1 : -1);
        return true;
    }
    if (mods == Qt::ControlModifier && key == Qt::Key_D) {
        if (!repeat)
            toggleFavouriteHighlighted();
        return true;
    }
    if (mods == Qt::ControlModifier && key >= Qt::Key_1 && key <= Qt::Key_8) {
        if (!repeat)
            addHighlightedToSlot(key - Qt::Key_0);
        return true;
    }
    if (key == Qt::Key_F5 && mods == Qt::NoModifier) {
        if (!repeat)
            emit liveRequested();
        return true;
    }
    if (inSearch && mods == Qt::NoModifier && (key == Qt::Key_Up || key == Qt::Key_Down)) {
        moveHighlight(key == Qt::Key_Down ? 1 : -1);
        return true;
    }
    return false;
}

bool BrowseView::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress && handleKey(watched, static_cast<QKeyEvent *>(event)))
        return true;
    return QWidget::eventFilter(watched, event);
}

void BrowseView::keyPressEvent(QKeyEvent *event)
{
    // A key a list did not use (PgDn on its last row) propagates up to here: it is still the
    // list's key and must not step Prev/Next (SPEC 4.1), so judge it by the focused widget.
    QWidget *source = focusWidget() ? focusWidget() : this;
    if (!handleKey(source, event))
        QWidget::keyPressEvent(event);
}
