#include "core/SearchQuery.h"

#include <QRegularExpression>

QString SearchQuery::fold(const QString &s) {
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar c : d) {
        const QChar::Category cat = c.category();
        if (cat == QChar::Mark_NonSpacing || cat == QChar::Mark_SpacingCombining ||
            cat == QChar::Mark_Enclosing)
            continue;
        out += c.toLower();
    }
    return out;
}

namespace {
struct Tok {
    QString text;
    bool quoted = false;       // a quote character appeared anywhere in the token
    bool quotedStart = false;  // the token began with a quote
};

// Split on whitespace; a double-quoted run (as in cat:"complex & se") keeps its
// spaces, the quotes are dropped. Tokens left empty by that (`"`, `""`) are
// dropped.
QVector<Tok> splitTokens(const QString &text) {
    QVector<Tok> out;
    Tok cur;
    bool inQuote = false;
    auto flush = [&]() {
        if (!cur.text.isEmpty())
            out.append(cur);
        cur = Tok();
    };
    for (const QChar c : text) {
        if (c == QLatin1Char('"')) {
            if (cur.text.isEmpty() && !cur.quoted)
                cur.quotedStart = true;
            inQuote = !inQuote;
            cur.quoted = true;
        } else if (c.isSpace() && !inQuote) {
            flush();
        } else {
            cur.text += c;
        }
    }
    flush();
    return out;
}

bool isKindPrefix(const QString &t) {
    static const QRegularExpression re(QStringLiteral("^(combi|prog|p|c)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}
bool isBankOnly(const QString &t) {
    static const QRegularExpression re(QStringLiteral("^(g\\(d\\)|gd|[a-eg])$"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}
bool isBankDigits(const QString &t) {
    static const QRegularExpression re(QStringLiteral("^(g\\(d\\)|gd|[a-eg])\\d+$"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}
bool isDigits(const QString &t) {
    static const QRegularExpression re(QStringLiteral("^\\d+$"));
    return re.match(t).hasMatch();
}
bool looksLikeAddress(const QString &t) {
    static const QRegularExpression re(
        QStringLiteral("^(combi|prog|p|c)?(g\\(d\\)|gd|[a-eg])\\d+$"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(t).hasMatch();
}

bool categoryMatches(const QString &foldedCat, const QString &prefix) {
    if (foldedCat.startsWith(prefix))
        return true;
    const QStringList parts = foldedCat.split(QLatin1Char('/'));
    for (const QString &p : parts)
        if (p.startsWith(prefix))
            return true;
    return false;
}
}  // namespace

SearchQuery SearchQuery::parse(const QString &text) {
    SearchQuery q;
    const QVector<Tok> toks = splitTokens(text);
    auto plain = [&](int k) { return k < toks.size() && !toks.at(k).quoted; };
    int i = 0;
    while (i < toks.size()) {
        const Tok &t0 = toks.at(i);

        // Split address: decide by shape alone; if the shape is an address but
        // the joined text does not validate, nothing matches (never re-split).
        int n = 0;
        if (plain(i) && plain(i + 1)) {
            const QString &a = t0.text, &b = toks.at(i + 1).text;
            if (isKindPrefix(a) && plain(i + 2) && isBankOnly(b) && isDigits(toks.at(i + 2).text))
                n = 3;
            else if (isKindPrefix(a) && isBankDigits(b))
                n = 2;
            else if (isBankOnly(a) && isDigits(b))
                n = 2;
        }
        if (n > 0) {
            QString joined;
            for (int k = 0; k < n; ++k)
                joined += toks.at(i + k).text;
            i += n;
            const SoundAddress::ParseResult r = SoundAddress::parse(joined);
            if (r.ok()) {
                Token t;
                t.type = Token::Address;
                t.addresses = r.matches;
                q.m_tokens.append(t);
            } else {
                q.m_impossible = true;
            }
            continue;
        }

        ++i;
        if (!t0.quoted) {
            const SoundAddress::ParseResult r = SoundAddress::parse(t0.text);
            if (r.ok()) {
                Token t;
                t.type = Token::Address;
                t.addresses = r.matches;
                q.m_tokens.append(t);
                continue;
            }
            if (looksLikeAddress(t0.text)) {
                q.m_impossible = true;  // out of range or bank E: matches nothing
                continue;
            }
        }
        if (!t0.quotedStart && t0.text.startsWith(QLatin1String("cat:"), Qt::CaseInsensitive)) {
            Token t;
            t.type = Token::Category;
            t.text = fold(t0.text.mid(4));
            q.m_tokens.append(t);
        } else {
            Token t;
            t.type = Token::Text;
            t.text = fold(t0.text);
            q.m_tokens.append(t);
        }
    }
    return q;
}

bool SearchQuery::matches(const Catalogue &cat, const Catalogue::Entry &e) const {
    if (m_impossible)
        return false;
    QString name, category;
    bool haveText = false;
    for (const Token &t : m_tokens) {
        switch (t.type) {
        case Token::Address:
            if (!t.addresses.contains(e.address))
                return false;
            break;
        case Token::Category:
        case Token::Text:
            if (!haveText) {
                name = fold(cat.effectiveName(e.address));
                category = fold(cat.effectiveCategory(e.address));
                haveText = true;
            }
            if (t.type == Token::Category) {
                if (!t.text.isEmpty() && !categoryMatches(category, t.text))
                    return false;
            } else if (!name.contains(t.text) && !category.contains(t.text)) {
                return false;
            }
            break;
        }
    }
    return true;
}

QVector<SoundAddress> SearchQuery::search(const Catalogue &cat) const {
    QVector<SoundAddress> out;
    for (const Catalogue::Entry &e : cat.entries())
        if (matches(cat, e))
            out.append(e.address);
    return out;
}
