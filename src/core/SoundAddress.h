#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "core/BankMap.h"

// One selectable slot (SPEC 3.1): kind, bank and number as printed.
// Numbers: A-D 0-127; G and g(d) 1-128.
//
// Ordering (operator<, used for prev/next; SPEC 3.1 gives no order, so this is
// the builder's choice, flagged to the foreman): kind (COMBI before PROG), then
// bank (A, B, C, D, G, g(d)), then number ascending.
class SoundAddress {
public:
    enum class Kind { Combi, Prog };
    enum class Bank { A, B, C, D, G, Gd };

    struct BankSelect {
        int msb = 0;      // CC0
        int lsb = 0;      // CC32
        int program = 0;  // Program Change byte, 0-127
        bool operator==(const BankSelect &o) const {
            return msb == o.msb && lsb == o.lsb && program == o.program;
        }
    };

    // Result of parse(). A bare bank+number matches both kinds when both exist,
    // so there can be one or two matches. On failure matches is empty.
    struct ParseResult {
        QVector<SoundAddress> matches;
        QString error;
        bool ok() const { return !matches.isEmpty(); }
    };

    SoundAddress() = default;
    SoundAddress(Kind k, Bank b, int n) : m_kind(k), m_bank(b), m_number(n) {}

    static ParseResult parse(const QString &text);

    Kind kind() const { return m_kind; }
    Bank bank() const { return m_bank; }
    int number() const { return m_number; }

    bool isValid() const;
    // Canonical form: "COMBI A000", "PROG G001", "PROG g(d)001". Empty if invalid.
    QString format() const;
    // MIDI.md section 3 table. Returns nothing for an invalid address, so a
    // caller can never send out-of-range bytes.
    std::optional<BankSelect> bankSelect(BankMap map) const;

    bool operator==(const SoundAddress &o) const {
        return m_kind == o.m_kind && m_bank == o.m_bank && m_number == o.m_number;
    }
    bool operator!=(const SoundAddress &o) const { return !(*this == o); }
    bool operator<(const SoundAddress &o) const;

private:
    Kind m_kind = Kind::Combi;
    Bank m_bank = Bank::A;
    int m_number = -1;
};
