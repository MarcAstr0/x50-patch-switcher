#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include "core/Catalogue.h"
#include "core/SoundAddress.h"

// Browse-view search (SPEC 4.1): whitespace-separated tokens, all must match
// (AND). A token is one of
//   - an address (SPEC 3.1 grammar): the entry's address must be one of the
//     addresses the token parses to (a bare "A012" is both COMBI and PROG);
//   - "cat:<text>": category filter;
//   - free text: substring match.
// An empty query matches everything.
//
// Choices where the spec is silent (builder's, flagged to the foreman):
//  - Folding: both sides are Unicode-decomposed (NFD), combining marks dropped,
//    then lower-cased. "cafe" matches "Café"; "ä" matches "a".
//  - Free text is a substring match against the displayed name and the
//    displayed category (Catalogue::effectiveName/effectiveCategory, so for an
//    edited slot the unit name is searched, not the factory name).
//  - cat:<text> matches when the folded category starts with the folded text,
//    or when any part of it split at '/' starts with it. So "cat:bell" and
//    "cat:mallet" match "Bell/Mallet/Perc", "cat:comp" matches "Complex & SE".
//    A category with spaces is written in quotes: cat:"complex & se".
//    An empty "cat:" matches everything (the user is still typing).
//  - Split addresses (exact rule). Tokens are joined into one address only in
//    these shapes, tested longest first, on unquoted tokens:
//      [kind] [bank] [digits]   "prog a 12", "c b 100"
//      [kind] [bank+digits]     "P A012", "c b100"
//      [bank] [digits]          "a 12", "g(d) 1"
//    where kind is prog, p, combi or c and bank is a, b, c, d, e, g, g(d) or gd
//    (case-insensitive). So "c 12" is bank C 012 (both kinds) and "piano c 12"
//    is the text "piano" plus that address; "piano c" with no digits after the
//    c stays text. The decision is made on shape alone and is atomic: if the
//    joined text is not a valid address (out of range, bank E, kind/bank
//    mismatch such as "C D000") the whole sequence is an address token that
//    matches nothing; it is never re-split into text plus address.
//    A single token that looks like an address but is out of range or bank E
//    ("A200", "E5") likewise matches nothing (rejected, not clamped).
//  - Quoted tokens (any token containing a double quote, except the cat: form
//    cat:"...") are always text: "b3" searches names for b3, never address B003.
//  - Empty tokens (`"`, `""`) are dropped, so a query of only `""` is empty.
//  - Text matching is not applied to the address text; use an address token.
class SearchQuery {
public:
    static SearchQuery parse(const QString &text);

    bool isEmpty() const { return m_tokens.isEmpty() && !m_impossible; }
    bool matches(const Catalogue &cat, const Catalogue::Entry &entry) const;
    // All matching entries' addresses, in catalogue order.
    QVector<SoundAddress> search(const Catalogue &cat) const;

    // Fold for comparison: NFD, strip combining marks, lower-case.
    static QString fold(const QString &s);

private:
    struct Token {
        enum Type { Text, Category, Address } type = Text;
        QString text;                     // folded, for Text and Category
        QVector<SoundAddress> addresses;  // for Address
    };
    QVector<Token> m_tokens;
    bool m_impossible = false;
};
