#include "core/SetModel.h"

#include <utility>

#include <QStringList>

namespace {
SetModel::Result fail(const QString &msg)
{
    SetModel::Result r;
    r.error = msg;
    return r;
}
} // namespace

SetModel::SetModel(const QString &id, const QString &name, int size)
    : m_id(id), m_name(name), m_slots(isValidSize(size) ? size : 6)
{
}

bool SetModel::assignSlots(const QVector<Slot> &newSlots)
{
    if (!isValidSize(newSlots.size()))
        return false;
    m_slots = newSlots;
    return true;
}

SetModel::Result SetModel::rename(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return fail(QStringLiteral("A set needs a name."));
    m_name = trimmed;
    return {};
}

SetModel::Result SetModel::resize(int newSize)
{
    if (!isValidSize(newSize))
        return fail(QStringLiteral("A set has 6 or 8 slots."));
    if (newSize == size())
        return {};
    if (newSize > size()) {
        m_slots.resize(newSize);
        return {};
    }
    QStringList filled;
    for (int i = newSize; i < size(); ++i)
        if (!m_slots.at(i).isEmpty())
            filled << QString::number(i + 1);
    if (!filled.isEmpty()) {
        return fail(QStringLiteral("Cannot shrink to %1 slots: slot %2 %3 filled. Empty %4 first.")
                        .arg(newSize)
                        .arg(filled.join(QStringLiteral(" and ")))
                        .arg(filled.size() == 1 ? QStringLiteral("is") : QStringLiteral("are"))
                        .arg(filled.size() == 1 ? QStringLiteral("it") : QStringLiteral("them")));
    }
    m_slots.resize(newSize);
    return {};
}

SetModel::Result SetModel::moveSlot(int from, int to)
{
    if (!inRange(from) || !inRange(to))
        return fail(QStringLiteral("No such slot."));
    if (from != to)
        std::swap(m_slots[from], m_slots[to]);
    return {};
}

SetModel::Result SetModel::setSlot(int idx, const SoundAddress &address)
{
    if (!inRange(idx))
        return fail(QStringLiteral("No such slot."));
    if (!address.isValid())
        return fail(QStringLiteral("Not a valid sound address."));
    m_slots[idx].address = address;
    return {};
}

SetModel::Result SetModel::clearSlot(int idx)
{
    if (!inRange(idx))
        return fail(QStringLiteral("No such slot."));
    m_slots[idx] = Slot();
    return {};
}

SetModel::Result SetModel::setLabel(int idx, const QString &label)
{
    if (!inRange(idx))
        return fail(QStringLiteral("No such slot."));
    if (m_slots.at(idx).isEmpty())
        return fail(QStringLiteral("Slot %1 is empty; put a sound in it before labelling.").arg(idx + 1));
    m_slots[idx].label = label.trimmed();
    return {};
}
