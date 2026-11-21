#include "core/Library.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QUuid>

namespace {
const char *const kFileName = "library.json";

Library::Result fail(const QString &msg)
{
    Library::Result r;
    r.error = msg;
    return r;
}

QString isoTime(const QDateTime &t)
{
    return t.toUTC().toString(Qt::ISODate);
}

QDateTime parseTime(const QString &s)
{
    return QDateTime::fromString(s, Qt::ISODate).toUTC();
}

// Canonical text -> exactly one address, or nothing.
std::optional<SoundAddress> parseStored(const QString &text)
{
    const SoundAddress::ParseResult r = SoundAddress::parse(text);
    if (r.matches.size() != 1)
        return std::nullopt;
    return r.matches.first();
}

// An integral JSON number within [lo, hi]; checked on the double so no cast is UB.
bool isIntIn(const QJsonValue &v, double lo, double hi)
{
    if (!v.isDouble())
        return false;
    const double d = v.toDouble();
    return std::isfinite(d) && std::fabs(d) <= 9007199254740992.0 && d == std::floor(d) && d >= lo && d <= hi;
}

bool isInt(const QJsonValue &v)
{
    return isIntIn(v, -9007199254740992.0, 9007199254740992.0);
}

bool isIntRange(const QJsonValue &v, int lo, int hi)
{
    return isIntIn(v, lo, hi);
}

// A required timestamp string that parses.
bool validTime(const QJsonValue &v, QDateTime *out)
{
    if (!v.isString())
        return false;
    *out = parseTime(v.toString());
    return out->isValid();
}

QString makeId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
} // namespace

Library::Library(QObject *parent) : QObject(parent)
{
    m_timer.setSingleShot(true);
    m_timer.setInterval(kSaveDebounceMs);
    connect(&m_timer, &QTimer::timeout, this, [this]() { save(); });
}

Library::~Library()
{
    m_timer.stop();
    if (m_dirty)
        writeNow(false);
}

void Library::clearContent()
{
    m_favourites.clear();
    m_recents.clear();
    m_unitNames.clear();
    m_unit.reset();
    m_sets.clear();
    m_currentSetId.clear();
}

bool Library::load(const QString &configDir)
{
    m_timer.stop();
    m_dirty = false;
    m_path = QDir(configDir).filePath(QString::fromLatin1(kFileName));
    m_loadError.clear();
    m_loadWarnings.clear();
    m_brokenPath.clear();
    m_saveError.clear();
    m_writeBlocked = false;
    m_needBackup = false;
    clearContent();

    if (configDir.isEmpty()) {
        // Never fall back to a relative library.json in the working directory.
        m_path.clear();
        m_writeBlocked = true;
        m_saveError = QStringLiteral("No config directory is available; changes will not be saved.");
        return true;
    }
    QFile f(m_path);
    if (!f.exists())
        return true;
    if (!f.open(QIODevice::ReadOnly)) {
        // Unreadable is not the same as broken: do not move it, do not write over it.
        m_loadError = QStringLiteral("Cannot read %1: %2").arg(m_path, f.errorString());
        m_writeBlocked = true;
        return false;
    }
    const qint64 expected = f.size();
    const QByteArray data = f.readAll();
    if (f.error() != QFileDevice::NoError || (expected >= 0 && data.size() != expected)) {
        const QString why = f.error() != QFileDevice::NoError ? f.errorString()
                                                               : QStringLiteral("short read");
        m_loadError = QStringLiteral("Cannot read %1: %2").arg(m_path, why);
        m_writeBlocked = true;
        return false;
    }
    f.close();

    QString err;
    if (fromJson(data, &err)) {
        m_needBackup = !m_loadWarnings.isEmpty();
        return true;
    }

    clearContent();
    m_loadWarnings.clear();
    const bool moved = sideline();
    if (moved) {
        m_loadError = QStringLiteral("The library file was damaged (%1). It was kept as %2 and "
                                     "an empty library was started.")
                          .arg(err, QDir::toNativeSeparators(m_brokenPath));
    } else {
        m_loadError = QStringLiteral("The library file was damaged (%1) and could not be moved "
                                     "aside. Changes will not be saved.")
                          .arg(err);
        m_writeBlocked = true;
    }
    return false;
}

bool Library::sideline()
{
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
    QString target = m_path + QStringLiteral(".broken-") + stamp;
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = m_path + QStringLiteral(".broken-") + stamp + QStringLiteral("-") + QString::number(n);
    if (!QFile::rename(m_path, target))
        return false;
    m_brokenPath = target;
    return true;
}

bool Library::fromJson(const QByteArray &data, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (doc.isNull()) {
        *error = QStringLiteral("not valid JSON: %1").arg(pe.errorString());
        return false;
    }
    if (!doc.isObject()) {
        *error = QStringLiteral("top level is not an object");
        return false;
    }
    const QJsonObject root = doc.object();
    auto badType = [&](const char *field, const char *expected) {
        *error = QStringLiteral("\"%1\" must be %2")
                     .arg(QString::fromLatin1(field), QString::fromLatin1(expected));
        return false;
    };

    // favourites
    if (root.contains(QStringLiteral("favourites"))) {
        const QJsonValue v = root.value(QStringLiteral("favourites"));
        if (!v.isArray())
            return badType("favourites", "an array");
        for (const QJsonValue &e : v.toArray()) {
            if (!e.isString())
                return badType("favourites", "an array of strings");
            const auto a = parseStored(e.toString());
            if (a)
                m_favourites.insert(a->format());
            else
                m_loadWarnings << QStringLiteral("Favourite \"%1\" is not a sound address; dropped.")
                                      .arg(e.toString());
        }
    }

    // sets
    QSet<QString> ids;
    if (root.contains(QStringLiteral("sets"))) {
        const QJsonValue v = root.value(QStringLiteral("sets"));
        if (!v.isArray())
            return badType("sets", "an array");
        int n = 0;
        for (const QJsonValue &sv : v.toArray()) {
            ++n;
            if (!sv.isObject()) {
                *error = QStringLiteral("set %1 is not an object").arg(n);
                return false;
            }
            const QJsonObject so = sv.toObject();
            const QString id = so.value(QStringLiteral("id")).toString();
            const QString name = so.value(QStringLiteral("name")).toString();
            if (id.isEmpty() || !so.value(QStringLiteral("id")).isString()) {
                *error = QStringLiteral("set %1 has no id").arg(n);
                return false;
            }
            if (ids.contains(id)) {
                *error = QStringLiteral("set id \"%1\" appears twice").arg(id);
                return false;
            }
            ids.insert(id);
            if (!so.value(QStringLiteral("name")).isString() || name.trimmed().isEmpty()) {
                *error = QStringLiteral("set %1 has no name").arg(n);
                return false;
            }
            const QJsonValue sizeV = so.value(QStringLiteral("size"));
            if (!isIntRange(sizeV, 6, 8) || !SetModel::isValidSize(sizeV.toInt())) {
                *error = QStringLiteral("set \"%1\": size must be 6 or 8").arg(name);
                return false;
            }
            const int size = sizeV.toInt();
            const QJsonValue slotsV = so.value(QStringLiteral("slots"));
            if (!slotsV.isArray() || slotsV.toArray().size() != size) {
                *error = QStringLiteral("set \"%1\": slots must be an array of %2").arg(name).arg(size);
                return false;
            }
            QVector<SetModel::Slot> slotList;
            int idx = 0;
            for (const QJsonValue &slv : slotsV.toArray()) {
                ++idx;
                SetModel::Slot slot;
                if (slv.isNull()) {
                    slotList.append(slot);
                    continue;
                }
                if (!slv.isObject()) {
                    *error = QStringLiteral("set \"%1\": slot %2 is neither null nor an object")
                                 .arg(name).arg(idx);
                    return false;
                }
                const QJsonObject slo = slv.toObject();
                if (!slo.value(QStringLiteral("address")).isString()) {
                    *error = QStringLiteral("set \"%1\": slot %2 has no address").arg(name).arg(idx);
                    return false;
                }
                const QString addrText = slo.value(QStringLiteral("address")).toString();
                const auto a = parseStored(addrText);
                if (!a) {
                    m_loadWarnings << QStringLiteral("Set \"%1\", slot %2: \"%3\" is not a sound address; "
                                                     "the slot was emptied.")
                                          .arg(name).arg(idx).arg(addrText);
                    slotList.append(slot);
                    continue;
                }
                slot.address = *a;
                const QJsonValue lv = slo.value(QStringLiteral("label"));
                if (!lv.isUndefined() && !lv.isNull()) {
                    if (!lv.isString()) {
                        *error = QStringLiteral("set \"%1\": slot %2 label must be a string")
                                     .arg(name).arg(idx);
                        return false;
                    }
                    slot.label = lv.toString();
                }
                slotList.append(slot);
            }
            SetModel set(id, name, size);
            set.assignSlots(slotList);
            m_sets.append(set);
        }
    }

    // currentSetId
    if (root.contains(QStringLiteral("currentSetId"))) {
        const QJsonValue v = root.value(QStringLiteral("currentSetId"));
        if (!v.isString() && !v.isNull())
            return badType("currentSetId", "a string");
        m_currentSetId = v.toString();
    }
    if (!m_currentSetId.isEmpty() && !ids.contains(m_currentSetId)) {
        m_loadWarnings << QStringLiteral("currentSetId \"%1\" matches no set; using the first.")
                              .arg(m_currentSetId);
        m_currentSetId.clear();
    }
    if (m_currentSetId.isEmpty() && !m_sets.isEmpty())
        m_currentSetId = m_sets.first().id();

    // recents
    if (root.contains(QStringLiteral("recents"))) {
        const QJsonValue v = root.value(QStringLiteral("recents"));
        if (!v.isArray())
            return badType("recents", "an array");
        for (const QJsonValue &e : v.toArray()) {
            if (!e.isObject())
                return badType("recents", "an array of objects");
            const QJsonObject o = e.toObject();
            const QJsonValue av = o.value(QStringLiteral("address"));
            const QJsonValue cv = o.value(QStringLiteral("count"));
            QDateTime when;
            if (!av.isString())
                return badType("recents address", "a string");
            if (!isIntRange(cv, 1, std::numeric_limits<int>::max()))
                return badType("recents count", "a positive integer");
            if (!validTime(o.value(QStringLiteral("lastUsed")), &when))
                return badType("recents lastUsed", "an ISO 8601 time string");
            const QString text = av.toString();
            const auto a = parseStored(text);
            if (!a) {
                m_loadWarnings << QStringLiteral("Recent \"%1\" is not a sound address; dropped.").arg(text);
                continue;
            }
            bool dup = false;
            for (const Recent &r : std::as_const(m_recents))
                dup = dup || r.address == *a;
            if (dup)
                continue;
            m_recents.append({*a, cv.toInt(), when});
        }
        std::stable_sort(m_recents.begin(), m_recents.end(),
                         [](const Recent &x, const Recent &y) { return x.lastUsed > y.lastUsed; });
        if (m_recents.size() > kRecentsCap)
            m_recents.resize(kRecentsCap);
    }

    // unitNames
    if (root.contains(QStringLiteral("unitNames"))) {
        const QJsonValue v = root.value(QStringLiteral("unitNames"));
        if (!v.isObject())
            return badType("unitNames", "an object");
        const QJsonObject o = v.toObject();
        for (auto it = o.begin(); it != o.end(); ++it) {
            if (!it.value().isObject())
                return badType("unitNames entries", "objects");
            const QJsonObject eo = it.value().toObject();
            if (!eo.value(QStringLiteral("name")).isString())
                return badType("unitNames name", "a string");
            if (eo.contains(QStringLiteral("category")) && !eo.value(QStringLiteral("category")).isString())
                return badType("unitNames category", "a string");
            Catalogue::UnitName un;
            if (!validTime(eo.value(QStringLiteral("readAt")), &un.readAt))
                return badType("unitNames readAt", "an ISO 8601 time string");
            un.name = eo.value(QStringLiteral("name")).toString();
            un.category = eo.value(QStringLiteral("category")).toString();
            const auto a = parseStored(it.key());
            if (!a) {
                m_loadWarnings << QStringLiteral("Unit name for \"%1\": not a sound address; dropped.")
                                      .arg(it.key());
                continue;
            }
            m_unitNames.insert(*a, un);
        }
    }

    // unit
    if (root.contains(QStringLiteral("unit")) && !root.value(QStringLiteral("unit")).isNull()) {
        const QJsonValue v = root.value(QStringLiteral("unit"));
        if (!v.isObject())
            return badType("unit", "an object");
        const QJsonObject o = v.toObject();
        UnitInfo u;
        if (!isIntRange(o.value(QStringLiteral("memberCode")), 0, std::numeric_limits<int>::max()) ||
            !isInt(o.value(QStringLiteral("globalChannel"))))
            return badType("unit.memberCode and unit.globalChannel", "integers");
        u.memberCode = o.value(QStringLiteral("memberCode")).toInt();
        u.globalChannel = o.value(QStringLiteral("globalChannel")).toInt();
        if (!isIntRange(o.value(QStringLiteral("globalChannel")), 1, 16))
            return badType("unit.globalChannel", "1-16");
        if (!o.value(QStringLiteral("version")).isString())
            return badType("unit.version", "a string");
        u.version = o.value(QStringLiteral("version")).toString();
        const QString bm = o.value(QStringLiteral("bankMap")).toString();
        if (bm == QStringLiteral("korg"))
            u.bankMap = BankMap::Korg;
        else if (bm == QStringLiteral("gm"))
            u.bankMap = BankMap::Gm;
        else
            return badType("unit.bankMap", "\"korg\" or \"gm\"");
        if (!validTime(o.value(QStringLiteral("lastSeen")), &u.lastSeen))
            return badType("unit.lastSeen", "an ISO 8601 time string");
        m_unit = u;
    }
    return true;
}

QByteArray Library::toJson() const
{
    QJsonObject root;

    QJsonArray fav;
    for (const SoundAddress &a : favourites())
        fav.append(a.format());
    root.insert(QStringLiteral("favourites"), fav);

    QJsonArray sets;
    for (const SetModel &s : m_sets) {
        QJsonArray slotArr;
        for (const SetModel::Slot &sl : s.allSlots()) {
            if (sl.isEmpty()) {
                slotArr.append(QJsonValue(QJsonValue::Null));
                continue;
            }
            QJsonObject so;
            so.insert(QStringLiteral("address"), sl.address->format());
            if (!sl.label.isEmpty())
                so.insert(QStringLiteral("label"), sl.label);
            slotArr.append(so);
        }
        QJsonObject o;
        o.insert(QStringLiteral("id"), s.id());
        o.insert(QStringLiteral("name"), s.name());
        o.insert(QStringLiteral("size"), s.size());
        o.insert(QStringLiteral("slots"), slotArr);
        sets.append(o);
    }
    root.insert(QStringLiteral("sets"), sets);
    root.insert(QStringLiteral("currentSetId"), m_currentSetId);

    QJsonArray rec;
    for (const Recent &r : m_recents) {
        QJsonObject o;
        o.insert(QStringLiteral("address"), r.address.format());
        o.insert(QStringLiteral("count"), r.count);
        o.insert(QStringLiteral("lastUsed"), isoTime(r.lastUsed));
        rec.append(o);
    }
    root.insert(QStringLiteral("recents"), rec);

    QJsonObject names;
    for (auto it = m_unitNames.begin(); it != m_unitNames.end(); ++it) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), it.value().name);
        if (!it.value().category.isEmpty())
            o.insert(QStringLiteral("category"), it.value().category);
        o.insert(QStringLiteral("readAt"), isoTime(it.value().readAt));
        names.insert(it.key().format(), o);
    }
    root.insert(QStringLiteral("unitNames"), names);

    if (m_unit) {
        QJsonObject o;
        o.insert(QStringLiteral("memberCode"), m_unit->memberCode);
        o.insert(QStringLiteral("version"), m_unit->version);
        o.insert(QStringLiteral("globalChannel"), m_unit->globalChannel);
        o.insert(QStringLiteral("bankMap"),
                 m_unit->bankMap == BankMap::Gm ? QStringLiteral("gm") : QStringLiteral("korg"));
        o.insert(QStringLiteral("lastSeen"), isoTime(m_unit->lastSeen));
        root.insert(QStringLiteral("unit"), o);
    }
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString Library::backupOriginal()
{
    // Load dropped entries; keep the original once before overwriting it.
    if (!QFileInfo::exists(m_path))
        return {};
    const QString stamp = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd'T'HHmmss"));
    QString target = m_path + QStringLiteral(".bak-") + stamp;
    for (int n = 2; QFileInfo::exists(target); ++n)
        target = m_path + QStringLiteral(".bak-") + stamp + QStringLiteral("-") + QString::number(n);
    if (!QFile::copy(m_path, target))
        return QStringLiteral("Cannot back up %1 before saving.").arg(QDir::toNativeSeparators(m_path));
    return {};
}

QString Library::save()
{
    return writeNow(true);
}

QString Library::writeNow(bool notify)
{
    m_timer.stop();
    QString err;
    if (m_path.isEmpty()) {
        err = QStringLiteral("No config directory set.");
    } else if (m_writeBlocked) {
        err = QStringLiteral("Saving is disabled because the library file could not be read or moved aside.");
    } else if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) {
        err = QStringLiteral("Cannot create the folder %1.")
                  .arg(QDir::toNativeSeparators(QFileInfo(m_path).absolutePath()));
    } else {
        if (m_needBackup) {
            err = backupOriginal();
            if (err.isEmpty())
                m_needBackup = false;
        }
        if (err.isEmpty()) {
            QSaveFile f(m_path);
            if (!f.open(QIODevice::WriteOnly)) {
                err = QStringLiteral("Cannot write %1: %2")
                          .arg(QDir::toNativeSeparators(m_path), f.errorString());
            } else {
                f.write(toJson());
                if (!f.commit())
                    err = QStringLiteral("Cannot write %1: %2")
                              .arg(QDir::toNativeSeparators(m_path), f.errorString());
            }
        }
    }
    m_saveError = err;
    if (err.isEmpty()) {
        m_dirty = false;
        ++m_writeCount;
        if (notify)
            emit saved();
    } else if (notify) {
        // Stay dirty so flush() retries; do not retry on a timer.
        emit saveFailed(err);
    }
    return err;
}

void Library::flush()
{
    m_timer.stop();
    if (m_dirty)
        save();
}

void Library::touch()
{
    m_dirty = true;
    m_timer.start();
    emit changed();
}

// ---- favourites, recents, unit ----

QVector<SoundAddress> Library::favourites() const
{
    QVector<SoundAddress> out;
    for (const QString &s : m_favourites) {
        const auto a = parseStored(s);
        if (a)
            out.append(*a);
    }
    std::sort(out.begin(), out.end());
    return out;
}

void Library::setFavourite(const SoundAddress &a, bool on)
{
    if (!a.isValid())
        return;
    const QString key = a.format();
    if (on == m_favourites.contains(key))
        return;
    if (on)
        m_favourites.insert(key);
    else
        m_favourites.remove(key);
    touch();
}

bool Library::toggleFavourite(const SoundAddress &a)
{
    const bool now = !isFavourite(a);
    setFavourite(a, now);
    return isFavourite(a);
}

int Library::recentCount(const SoundAddress &a) const
{
    for (const Recent &r : m_recents)
        if (r.address == a)
            return r.count;
    return 0;
}

void Library::recordUse(const SoundAddress &a, const QDateTime &when)
{
    if (!a.isValid())
        return;
    int count = 1;
    for (int i = 0; i < m_recents.size(); ++i) {
        if (m_recents.at(i).address == a) {
            count = m_recents.at(i).count + 1;
            m_recents.remove(i);
            break;
        }
    }
    m_recents.prepend({a, count, when.toUTC()});
    if (m_recents.size() > kRecentsCap)
        m_recents.resize(kRecentsCap);
    touch();
}

void Library::setUnitNames(const QMap<SoundAddress, Catalogue::UnitName> &names)
{
    m_unitNames = names;
    for (auto it = m_unitNames.begin(); it != m_unitNames.end(); ++it)
        if (!it.value().readAt.isValid())
            it.value().readAt = QDateTime::currentDateTimeUtc();  // the file requires a time
    touch();
}

void Library::setUnit(const UnitInfo &info)
{
    m_unit = info;
    if (!m_unit->lastSeen.isValid())
        m_unit->lastSeen = QDateTime::currentDateTimeUtc();  // the file requires a time
    touch();
}

// ---- sets ----

const SetModel *Library::setById(const QString &id) const
{
    for (const SetModel &s : m_sets)
        if (s.id() == id)
            return &s;
    return nullptr;
}

SetModel *Library::mutableSet(const QString &id)
{
    for (SetModel &s : m_sets)
        if (s.id() == id)
            return &s;
    return nullptr;
}

Library::Result Library::applyToSet(const QString &id, const std::function<SetModel::Result(SetModel &)> &op)
{
    SetModel *s = mutableSet(id);
    if (!s)
        return fail(QStringLiteral("No such set."));
    const Result r = op(*s);
    if (r.ok())
        touch();
    return r;
}

Library::Result Library::setCurrentSet(const QString &id)
{
    if (!setById(id))
        return fail(QStringLiteral("No such set."));
    if (id != m_currentSetId) {
        m_currentSetId = id;
        touch();
    }
    return {};
}

Library::Result Library::createSet(const QString &name, int size, QString *newId)
{
    if (!SetModel::isValidSize(size))
        return fail(QStringLiteral("A set has 6 or 8 slots."));
    SetModel s(makeId(), QString(), size);
    const Result r = s.rename(name);
    if (!r.ok())
        return r;
    if (newId)
        *newId = s.id();
    m_currentSetId = s.id();
    m_sets.append(s);
    touch();
    return {};
}

Library::Result Library::renameSet(const QString &id, const QString &name)
{
    return applyToSet(id, [&](SetModel &s) { return s.rename(name); });
}

Library::Result Library::duplicateSet(const QString &id, QString *newId)
{
    int pos = -1;
    for (int i = 0; i < m_sets.size(); ++i)
        if (m_sets.at(i).id() == id)
            pos = i;
    if (pos < 0)
        return fail(QStringLiteral("No such set."));
    SetModel copy = m_sets.at(pos);
    copy.setId(makeId());
    copy.rename(copy.name() + QStringLiteral(" copy"));
    if (newId)
        *newId = copy.id();
    m_sets.insert(pos + 1, copy);
    touch();
    return {};
}

Library::Result Library::deleteSet(const QString &id)
{
    for (int i = 0; i < m_sets.size(); ++i) {
        if (m_sets.at(i).id() != id)
            continue;
        m_sets.remove(i);
        if (m_currentSetId == id) {
            if (m_sets.isEmpty())
                m_currentSetId.clear();
            else
                m_currentSetId = m_sets.at(std::min(i, int(m_sets.size()) - 1)).id();
        }
        touch();
        return {};
    }
    return fail(QStringLiteral("No such set."));
}

Library::Result Library::resizeSet(const QString &id, int newSize)
{
    return applyToSet(id, [&](SetModel &s) { return s.resize(newSize); });
}

Library::Result Library::moveSlot(const QString &id, int from, int to)
{
    return applyToSet(id, [&](SetModel &s) { return s.moveSlot(from, to); });
}

Library::Result Library::setSlot(const QString &id, int idx, const SoundAddress &address)
{
    return applyToSet(id, [&](SetModel &s) { return s.setSlot(idx, address); });
}

Library::Result Library::clearSlot(const QString &id, int idx)
{
    return applyToSet(id, [&](SetModel &s) { return s.clearSlot(idx); });
}

Library::Result Library::setLabel(const QString &id, int idx, const QString &label)
{
    return applyToSet(id, [&](SetModel &s) { return s.setLabel(idx, label); });
}
