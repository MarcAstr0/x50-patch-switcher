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
// Split on whitespace; a double-quoted run (as in cat:"complex & se") keeps its
// spaces, the quotes are dropped.
QStringList splitTokens(const QString &text) {
    QStringList out;
    QString cur;
    bool inQuote = false, have = false;
    for (const QChar c : text) {
        if (c == QLatin1Char('"')) {
            inQuote = !inQuote;
            have = true;
        } else if (c.isSpace() && !inQuote) {
            if (have)
                out << cur;
            cur.clear();
            have = false;
        } else {
            cur += c;
            have = true;
        }
    }
    if (have)
        out << cur;
    return out;
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
    const QStringList toks = splitTokens(text);
    int i = 0;
    while (i < toks.size()) {
        // Address, possibly split over up to three tokens, longest first.
        bool done = false;
        for (int n = qMin(3, toks.size() - i); n >= 1 && !done; --n) {
            QString joined;
            for (int k = 0; k < n; ++k)
                joined += toks.at(i + k);
            const SoundAddress::ParseResult r = SoundAddress::parse(joined);
            if (r.ok()) {
                Token t;
                t.type = Token::Address;
                t.addresses = r.matches;
                q.m_tokens.append(t);
                i += n;
                done = true;
            }
        }
        if (done)
            continue;

        const QString &tok = toks.at(i++);
        if (tok.startsWith(QLatin1String("cat:"), Qt::CaseInsensitive)) {
            Token t;
            t.type = Token::Category;
            t.text = fold(tok.mid(4));
            q.m_tokens.append(t);
        } else if (looksLikeAddress(tok)) {
            q.m_impossible = true;  // out of range or bank E: matches nothing
        } else {
            Token t;
            t.type = Token::Text;
            t.text = fold(tok);
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
