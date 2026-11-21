#include "core/SoundAddress.h"

#include <QRegularExpression>

namespace {
// Indexed by static_cast<int>(Bank); the order is asserted below.
const char16_t *const kBankNames[] = {u"A", u"B", u"C", u"D", u"G", u"g(d)"};
static_assert(static_cast<int>(SoundAddress::Bank::A) == 0 && static_cast<int>(SoundAddress::Bank::B) == 1 &&
                  static_cast<int>(SoundAddress::Bank::C) == 2 && static_cast<int>(SoundAddress::Bank::D) == 3 &&
                  static_cast<int>(SoundAddress::Bank::G) == 4 && static_cast<int>(SoundAddress::Bank::Gd) == 5,
              "kBankNames is indexed by the Bank enum");
static_assert(sizeof(kBankNames) / sizeof(kBankNames[0]) == 6, "one name per Bank");

bool isGmBank(SoundAddress::Bank b) {
    return b == SoundAddress::Bank::G || b == SoundAddress::Bank::Gd;
}
}  // namespace

bool SoundAddress::isValid() const {
    if (m_kind == Kind::Combi && (m_bank == Bank::D || isGmBank(m_bank)))
        return false;
    if (isGmBank(m_bank))
        return m_number >= 1 && m_number <= 128;
    return m_number >= 0 && m_number <= 127;
}

QString SoundAddress::format() const {
    if (!isValid())
        return QString();
    return QStringLiteral("%1 %2%3")
        .arg(m_kind == Kind::Combi ? QStringLiteral("COMBI") : QStringLiteral("PROG"),
             QString::fromUtf16(kBankNames[static_cast<int>(m_bank)]),
             QStringLiteral("%1").arg(m_number, 3, 10, QLatin1Char('0')));
}

std::optional<SoundAddress::BankSelect> SoundAddress::bankSelect(BankMap map) const {
    if (!isValid())
        return std::nullopt;
    // docs/MIDI.md section 3, "Bank select values" table.
    BankSelect s;
    switch (m_bank) {
    case Bank::A:
    case Bank::B:
    case Bank::C:
    case Bank::D:
        // Rows "Program bank A-D" and "Combination bank A-C": MSB 00 (KORG) or
        // 3F (GM); LSB 00..03 for A..D; program change 0-127.
        s.msb = (map == BankMap::Korg) ? 0x00 : 0x3F;
        switch (m_bank) {
        case Bank::A: s.lsb = 0; break;
        case Bank::B: s.lsb = 1; break;
        case Bank::C: s.lsb = 2; break;
        default:      s.lsb = 3; break;  // Bank::D
        }
        s.program = m_number;
        break;
    case Bank::G:
        // Row "Program bank G (GM)": 79, 00 in both maps; PC 0-127 = G001-G128.
        s.msb = 0x79;
        s.lsb = 0x00;
        s.program = m_number - 1;
        break;
    case Bank::Gd:
        // Row "Program bank g(d)": 78, 00 in both maps; PC 0-127 = g(d)001-128.
        s.msb = 0x78;
        s.lsb = 0x00;
        s.program = m_number - 1;
        break;
    }
    return s;
}

bool SoundAddress::operator<(const SoundAddress &o) const {
    if (m_kind != o.m_kind)
        return m_kind == Kind::Combi;
    if (m_bank != o.m_bank)
        return static_cast<int>(m_bank) < static_cast<int>(o.m_bank);
    return m_number < o.m_number;
}

SoundAddress::ParseResult SoundAddress::parse(const QString &text) {
    ParseResult r;
    QString t;
    for (const QChar c : text)
        if (!c.isSpace())
            t += c.toLower();
    if (t.isEmpty()) {
        r.error = QStringLiteral("empty address");
        return r;
    }

    static const QRegularExpression bankNum(
        QStringLiteral("^(g\\(d\\)|gd|[a-eg])(\\d{1,3})$"));
    struct Prefix {
        const char16_t *text;
        int kind;  // 0 combi, 1 prog, -1 none
    };
    static const Prefix prefixes[] = {{u"combi", 0}, {u"prog", 1}, {u"p", 1}, {u"c", 0}, {u"", -1}};

    for (const Prefix &p : prefixes) {
        const QString pre = QString::fromUtf16(p.text);
        if (!t.startsWith(pre))
            continue;
        const QRegularExpressionMatch m = bankNum.match(t.mid(pre.size()));
        if (!m.hasMatch())
            continue;

        const QString b = m.captured(1);
        Bank bank = Bank::A;
        if (b == QLatin1String("a")) bank = Bank::A;
        else if (b == QLatin1String("b")) bank = Bank::B;
        else if (b == QLatin1String("c")) bank = Bank::C;
        else if (b == QLatin1String("d")) bank = Bank::D;
        else if (b == QLatin1String("g")) bank = Bank::G;
        else if (b == QLatin1String("e")) {
            r.error = QStringLiteral("bank E does not exist on the X50");
            return r;
        } else bank = Bank::Gd;
        const int number = m.captured(2).toInt();

        auto tryKind = [&](Kind k) { return SoundAddress(k, bank, number); };
        if (p.kind >= 0) {
            const SoundAddress a = tryKind(p.kind == 0 ? Kind::Combi : Kind::Prog);
            if (!a.isValid()) {
                r.error = QStringLiteral("address out of range: %1").arg(text.trimmed());
                return r;
            }
            r.matches.append(a);
        } else {
            const SoundAddress c = tryKind(Kind::Combi);
            const SoundAddress g = tryKind(Kind::Prog);
            if (c.isValid())
                r.matches.append(c);
            if (g.isValid())
                r.matches.append(g);
            if (r.matches.isEmpty())
                r.error = QStringLiteral("address out of range: %1").arg(text.trimmed());
        }
        return r;
    }
    r.error = QStringLiteral("not an address: %1").arg(text.trimmed());
    return r;
}
