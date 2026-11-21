#include "ui/ResultModel.h"
#include "ui/SlotCell.h"
#include <QMimeData>

#include "core/Catalogue.h"
#include "core/Library.h"

#include <QtGlobal>

ResultModel::ResultModel(const Catalogue *catalogue, const Library *library, QObject *parent)
    : QAbstractTableModel(parent)
    , m_catalogue(catalogue)
    , m_library(library)
{
}

void ResultModel::fillMarks(Row &row) const
{
    row.favourite = m_library->isFavourite(row.address);
    const int count = m_library->recentCount(row.address);
    row.used = count > 0 ? QStringLiteral("x%1").arg(count) : QString();
}

void ResultModel::fill(Row &row) const
{
    row.addressText = row.address.format();
    row.name = m_catalogue->effectiveName(row.address);
    row.category = m_catalogue->effectiveCategory(row.address);
    row.edited = m_catalogue->isEdited(row.address);
    row.tooltip = row.name;
    if (row.edited) {
        if (const Catalogue::Entry *e = m_catalogue->find(row.address))
            row.tooltip = QStringLiteral("%1\nFactory name: %2").arg(row.name, e->name);
    }
    fillMarks(row);
}

void ResultModel::setResults(const QVector<SoundAddress> &addresses)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(addresses.size());
    for (const SoundAddress &a : addresses) {
        Row r;
        r.address = a;
        fill(r);
        m_rows.append(r);
    }
    endResetModel();
}

void ResultModel::refreshMarks()
{
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &r = m_rows[i];
        const bool fav = r.favourite;
        const QString used = r.used;
        fillMarks(r);
        if (fav != r.favourite || used != r.used)
            emit dataChanged(index(i, 0), index(i, ColumnCount - 1));
    }
}

void ResultModel::refreshAll()
{
    for (Row &r : m_rows)
        fill(r);
    if (!m_rows.isEmpty())
        emit dataChanged(index(0, 0), index(m_rows.size() - 1, ColumnCount - 1));
}

void ResultModel::setCurrent(const std::optional<SoundAddress> &address)
{
    if (m_current == address)
        return;
    const std::optional<SoundAddress> old = m_current;
    m_current = address;
    for (const std::optional<SoundAddress> &a : {old, address}) {
        if (!a)
            continue;
        const int r = rowOf(*a);
        if (r >= 0)
            emit dataChanged(index(r, 0), index(r, ColumnCount - 1));
    }
}

int ResultModel::rowOf(const SoundAddress &address) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).address == address)
            return i;
    return -1;
}

QVector<SoundAddress> ResultModel::addresses() const
{
    QVector<SoundAddress> out;
    out.reserve(m_rows.size());
    for (const Row &r : m_rows)
        out.append(r.address);
    return out;
}

int ResultModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }
int ResultModel::columnCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : ColumnCount; }

QVariant ResultModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return QVariant();
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case ColAddress: return r.addressText;
        case ColName: return r.name;
        case ColCategory: return r.category;
        case ColUsed: return r.used;
        default: return QVariant();
        }
    case Qt::ToolTipRole:
        return index.column() == ColName || index.column() == ColEdited ? QVariant(r.tooltip) : QVariant();
    case FavouriteRole: return r.favourite;
    case EditedRole: return r.edited;
    case CurrentRole: return m_current.has_value() && *m_current == r.address;
    default: return QVariant();
    }
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();
    if (role == Qt::DisplayRole) {
        switch (section) {
        case ColAddress: return QStringLiteral("ADDRESS");
        case ColName: return QStringLiteral("NAME");
        case ColCategory: return QStringLiteral("CATEGORY");
        case ColUsed: return QStringLiteral("USED");
        default: return QString();
        }
    }
    if (role == Qt::TextAlignmentRole)
        return int(section == ColUsed ? (Qt::AlignRight | Qt::AlignVCenter) : (Qt::AlignLeft | Qt::AlignVCenter));
    return QVariant();
}

Qt::ItemFlags ResultModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled) : Qt::NoItemFlags;
}

QStringList ResultModel::mimeTypes() const
{
    return {SlotMime::sound()};
}

QMimeData *ResultModel::mimeData(const QModelIndexList &indexes) const
{
    for (const QModelIndex &i : indexes) {
        if (!i.isValid() || i.row() >= m_rows.size())
            continue;
        auto *mime = new QMimeData;
        mime->setData(SlotMime::sound(), m_rows.at(i.row()).address.format().toUtf8());
        return mime;
    }
    return nullptr;
}
