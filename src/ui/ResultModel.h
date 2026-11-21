#pragma once

#include <optional>

#include <QAbstractTableModel>
#include <QString>
#include <QVector>

#include "core/SoundAddress.h"

class Catalogue;
class Library;

// The browse result list (DESIGN 7.7), one row per matching sound, in the order given.
// Every display string is built when the rows are set (and when favourites, recents or
// names change), so the delegate's paint path only draws.
//
// Columns: Star, Address, Name, Edited, Category, Used. The Edited column carries no
// text, only EditedRole. Header titles are upper case as DESIGN 7.7 shows them.
class ResultModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColStar, ColAddress, ColName, ColEdited, ColCategory, ColUsed, ColumnCount };
    enum Role {
        FavouriteRole = Qt::UserRole + 1,  // bool
        EditedRole,                        // bool
        CurrentRole                        // bool: the sound the X50 is on
    };

    // Neither pointer is owned. They must outlive the model.
    ResultModel(const Catalogue *catalogue, const Library *library, QObject *parent = nullptr);

    // Replace the rows (catalogue order is the caller's responsibility).
    void setResults(const QVector<SoundAddress> &addresses);
    // Re-read favourites and recents for the existing rows; emits dataChanged only for
    // the rows whose marks changed.
    void refreshMarks();
    // Re-read names and categories too (after the unit names changed).
    void refreshAll();
    // The marker for the sound the X50 is on; nullopt clears it.
    void setCurrent(const std::optional<SoundAddress> &address);

    SoundAddress address(int row) const { return m_rows.at(row).address; }
    // -1 if the address is not in the results.
    int rowOf(const SoundAddress &address) const;
    QVector<SoundAddress> addresses() const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    // Drag source for the set editor: one row as SlotMime::sound() (the canonical address).
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    Qt::DropActions supportedDragActions() const override { return Qt::CopyAction; }

private:
    struct Row {
        SoundAddress address;
        QString addressText, name, category, used, tooltip;
        bool favourite = false;
        bool edited = false;
    };
    void fill(Row &row) const;
    void fillMarks(Row &row) const;

    const Catalogue *m_catalogue;
    const Library *m_library;
    QVector<Row> m_rows;
    std::optional<SoundAddress> m_current;
};
