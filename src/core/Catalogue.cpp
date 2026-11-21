#include "core/Catalogue.h"

#include <algorithm>

#include <QFile>
#include <QResource>
#include <QSet>

namespace {
QString rtrim(const QString &s) {
    int n = s.size();
    while (n > 0 && s.at(n - 1).isSpace())
        --n;
    return s.left(n);
}
}  // namespace

// Q_INIT_RESOURCE must be used outside any namespace.
static void initCatalogueResource() { Q_INIT_RESOURCE(x50data); }

bool Catalogue::loadFromResource(QString *error) {
    initCatalogueResource();
    return loadFromFile(QStringLiteral(":/data/x50-factory.tsv"), error);
}

bool Catalogue::loadFromFile(const QString &path, QString *error) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_entries.clear();
        if (error)
            *error = QStringLiteral("cannot open %1: %2").arg(path, f.errorString());
        return false;
    }
    return loadFromText(f.readAll(), error);
}

bool Catalogue::loadFromText(const QByteArray &tsv, QString *error) {
    QVector<Entry> out;
    QSet<QString> seen;
    const QStringList lines = QString::fromUtf8(tsv).split(QLatin1Char('\n'));
    auto fail = [&](int lineNo, const QString &why) {
        m_entries.clear();
        if (error)
            *error = QStringLiteral("catalogue line %1: %2").arg(lineNo).arg(why);
        return false;
    };

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        if (line.isEmpty())
            continue;
        const QStringList c = line.split(QLatin1Char('\t'));
        const int ln = i + 1;
        if (c.size() != 5 && c.size() != 6)
            return fail(ln, QStringLiteral("expected 5 or 6 columns, got %1").arg(c.size()));

        SoundAddress::Kind kind;
        if (c[0] == QLatin1String("COMBI"))
            kind = SoundAddress::Kind::Combi;
        else if (c[0] == QLatin1String("PROG"))
            kind = SoundAddress::Kind::Prog;
        else
            return fail(ln, QStringLiteral("unknown kind '%1'").arg(c[0]));

        // Reuse the address grammar for the bank; the kind is explicit here.
        const SoundAddress::ParseResult pr = SoundAddress::parse(
            (kind == SoundAddress::Kind::Combi ? QStringLiteral("COMBI") : QStringLiteral("PROG")) +
            c[1] + c[2]);
        if (pr.matches.size() != 1)
            return fail(ln, QStringLiteral("bad address '%1 %2 %3': %4")
                                .arg(c[0], c[1], c[2], pr.error));
        if (c[2].size() != 3)
            return fail(ln, QStringLiteral("number must have 3 digits: '%1'").arg(c[2]));

        Entry e;
        e.address = pr.matches.first();
        e.name = c[3];
        e.category = c[4];
        if (e.name.isEmpty() || e.name.size() > 16)
            return fail(ln, QStringLiteral("name empty or longer than 16 characters: '%1'").arg(e.name));
        if (e.category.isEmpty())
            return fail(ln, QStringLiteral("empty category"));
        const QString osc = c.size() == 6 ? c[5] : QString();
        if (osc.isEmpty()) {
            // blank: drum programs
        } else if (kind == SoundAddress::Kind::Prog &&
                   (osc == QLatin1String("s") || osc == QLatin1String("d") ||
                    osc == QLatin1String("D"))) {
            e.osc = osc.at(0);
        } else {
            return fail(ln, QStringLiteral("bad oscillator flag '%1'").arg(osc));
        }
        if (seen.contains(e.address.format()))
            return fail(ln, QStringLiteral("duplicate address %1").arg(e.address.format()));
        seen.insert(e.address.format());
        out.append(e);
    }
    if (out.isEmpty()) {
        m_entries.clear();
        if (error)
            *error = QStringLiteral("catalogue is empty");
        return false;
    }
    std::sort(out.begin(), out.end(),
              [](const Entry &a, const Entry &b) { return a.address < b.address; });
    m_entries = out;
    return true;
}

int Catalogue::indexOf(const SoundAddress &a) const {
    const auto it = std::lower_bound(
        m_entries.begin(), m_entries.end(), a,
        [](const Entry &e, const SoundAddress &x) { return e.address < x; });
    if (it == m_entries.end() || it->address != a)
        return -1;
    return static_cast<int>(it - m_entries.begin());
}

const Catalogue::Entry *Catalogue::find(const SoundAddress &a) const {
    const int i = indexOf(a);
    return i < 0 ? nullptr : &m_entries.at(i);
}

void Catalogue::setUnitName(const SoundAddress &a, const QString &name, const QString &category,
                            const QDateTime &readAt) {
    if (!a.isValid())
        return;
    m_unit.insert(key(a), UnitName{name, category, readAt});
}

void Catalogue::clearUnitNames() { m_unit.clear(); }

const Catalogue::UnitName *Catalogue::unitName(const SoundAddress &a) const {
    const auto it = m_unit.constFind(key(a));
    return it == m_unit.constEnd() ? nullptr : &it.value();
}

bool Catalogue::isEdited(const SoundAddress &a) const {
    const UnitName *u = unitName(a);
    const Entry *e = find(a);
    return u && e && rtrim(u->name) != rtrim(e->name);
}

QString Catalogue::effectiveName(const SoundAddress &a) const {
    if (const UnitName *u = unitName(a))
        return rtrim(u->name);
    const Entry *e = find(a);
    return e ? e->name : QString();
}

QString Catalogue::effectiveCategory(const SoundAddress &a) const {
    if (const UnitName *u = unitName(a))
        if (!u->category.isEmpty())
            return u->category;
    const Entry *e = find(a);
    return e ? e->category : QString();
}

bool Catalogue::setUnitCategories(const QStringList &program, const QStringList &combi,
                                  QString *error) {
    if (program.size() != 16 || combi.size() != 16) {
        if (error)
            *error = QStringLiteral("expected 16 program and 16 combination category names, got %1 and %2")
                         .arg(program.size())
                         .arg(combi.size());
        return false;
    }
    m_unitProgCats = program;
    m_unitCombiCats = combi;
    m_haveUnitCats = true;
    return true;
}

void Catalogue::clearUnitCategories() {
    m_unitProgCats.clear();
    m_unitCombiCats.clear();
    m_haveUnitCats = false;
}

QStringList Catalogue::categories(SoundAddress::Kind kind) const {
    if (m_haveUnitCats)
        return kind == SoundAddress::Kind::Prog ? m_unitProgCats : m_unitCombiCats;
    QSet<QString> set;
    for (const Entry &e : m_entries)
        if (e.address.kind() == kind)
            set.insert(e.category);
    QStringList l = set.values();
    std::sort(l.begin(), l.end(), [](const QString &a, const QString &b) {
        return a.compare(b, Qt::CaseInsensitive) < 0;
    });
    return l;
}
