#include "midi/X50Protocol.h"

#include <initializer_list>
#include <utility>

#include <QChar>

namespace X50Protocol {

namespace {

constexpr char kSysexStart = static_cast<char>(0xF0);
constexpr char kSysexEnd = static_cast<char>(0xF7);

QByteArray bytes(std::initializer_list<int> list) {
    QByteArray b;
    b.reserve(static_cast<int>(list.size()));
    for (int v : list) {
        b.append(static_cast<char>(v));
    }
    return b;
}

int at(const QByteArray &m, int i) {
    return static_cast<unsigned char>(m.at(i));
}

bool channelOk(int channel) { return channel >= 1 && channel <= 16; }

QString badChannel(int channel) {
    return QStringLiteral("MIDI channel %1 is out of range 1-16").arg(channel);
}

// F0 42 3g 7A <function> ... F7 with g = channel - 1 (MIDI.md section 2).
QByteArray korgHeader(int channel, int function) {
    return bytes({0xF0, 0x42, 0x30 | (channel - 1), 0x7A, function});
}

// Common validation of a Korg SysEx frame (IMP 3.): markers, channel nibble,
// function code, minimum length, all interior bytes are 7-bit data.
bool checkKorgFrame(const QByteArray &m, int function, int minLength,
                    std::optional<int> expectedChannel, int *channel, QString *err) {
    if (m.size() < minLength) {
        *err = QStringLiteral("message too short: %1 bytes, need at least %2")
                   .arg(m.size()).arg(minLength);
        return false;
    }
    if (m.at(0) != kSysexStart) {
        *err = QStringLiteral("missing F0 at start");
        return false;
    }
    if (m.at(m.size() - 1) != kSysexEnd) {
        *err = QStringLiteral("missing F7 at end");
        return false;
    }
    for (int i = 1; i < m.size() - 1; ++i) {
        if (at(m, i) & 0x80) {
            *err = QStringLiteral("status byte inside SysEx at index %1").arg(i);
            return false;
        }
    }
    if (at(m, 1) != 0x42) {
        *err = QStringLiteral("not a KORG message: manufacturer byte is not 42");
        return false;
    }
    if ((at(m, 2) & 0xF0) != 0x30) {
        *err = QStringLiteral("byte 2 is not 3g");
        return false;
    }
    if (at(m, 3) != 0x7A) {
        *err = QStringLiteral("byte 3 is not 7A (X50/microX family)");
        return false;
    }
    if (at(m, 4) != function) {
        *err = QStringLiteral("function code is %1, expected %2")
                   .arg(at(m, 4), 2, 16, QLatin1Char('0')).arg(function, 2, 16, QLatin1Char('0'));
        return false;
    }
    const int g = (at(m, 2) & 0x0F) + 1;
    if (expectedChannel && *expectedChannel != g) {
        *err = QStringLiteral("message is on channel %1, expected %2").arg(g).arg(*expectedChannel);
        return false;
    }
    *channel = g;
    return true;
}

bool knownMode(int m) {
    return (m >= 0 && m <= 4) || m == 6 || m == 0x7F;
}

template <typename T>
Result<T> fail(const QString &why, const QByteArray &raw) {
    return Result<T>::failure(why, raw);
}

// Shared by 4C and 4D. `combination` selects the layout (MIDI.md section 7):
//   4C 0X kb pp data...   (X: 0 = X50)
//   4D 00 kb cc data...
Result<BankDump> parseBankDump(const QByteArray &msg, bool combination,
                               std::optional<int> expectedChannel) {
    QString err;
    int ch = 0;
    if (!checkKorgFrame(msg, combination ? 0x4D : 0x4C, 9, expectedChannel, &ch, &err)) {
        return fail<BankDump>(err, msg);
    }
    if (at(msg, 5) != 0) {
        return fail<BankDump>(
            combination ? QStringLiteral("combination dump reserved byte 5 is not 00")
                        : QStringLiteral("program dump byte 5 is not 00 (not an X50 dump)"),
            msg);
    }
    const int kb = at(msg, 6);
    const int kind = (kb >> 4) & 0x03;
    const int bank = kb & 0x07;  // bits 0-2, range checked below
    if ((kb & 0xC8) != 0 || kind > 2) {
        return fail<BankDump>(QStringLiteral("kind/bank byte %1 is malformed").arg(kb, 2, 16, QLatin1Char('0')), msg);
    }
    const int maxBank = combination ? 2 : 4;  // A-C; A-D, 4 = E on microX (IMP *4)
    if (bank > maxBank) {
        return fail<BankDump>(QStringLiteral("bank %1 is out of range").arg(bank), msg);
    }
    const QByteArray encoded = msg.mid(8, msg.size() - 9);
    const Result<QByteArray> decoded = decode78(encoded);
    if (!decoded.ok) {
        return fail<BankDump>(QStringLiteral("data: %1").arg(decoded.error), msg);
    }
    BankDump d;
    d.channel = ch;
    d.kind = kind;
    d.bank = bank;
    d.number = at(msg, 7);
    d.data = decoded.value;
    return Result<BankDump>::success(d);
}

Result<QVector<SoundRecord>> sliceRecords(const QByteArray &decoded, int recordSize,
                                          int categoryOffset, bool combination) {
    if (decoded.isEmpty() || decoded.size() % recordSize != 0) {
        return fail<QVector<SoundRecord>>(
            QStringLiteral("decoded length %1 is not a non-zero multiple of %2")
                .arg(decoded.size()).arg(recordSize),
            decoded.left(64));
    }
    const int count = decoded.size() / recordSize;
    QVector<SoundRecord> out;
    out.reserve(count);
    for (int i = 0; i < count; ++i) {
        const int base = i * recordSize;
        SoundRecord r;
        r.name = decodeName(decoded.mid(base, kNameLength));
        const int c = at(decoded, base + categoryOffset);
        if (combination) {
            r.category = c & 0x0F;
        } else {
            r.category = c;
            r.categoryValid = c <= 15;
        }
        out.append(r);
    }
    return Result<QVector<SoundRecord>>::success(out);
}

}  // namespace

// ---------------------------------------------------------------- builders

Mode modeForKind(SoundAddress::Kind kind) {
    return kind == SoundAddress::Kind::Combi ? Mode::CombiPlay : Mode::ProgPlay;
}

Result<QList<QByteArray>> programSelect(const SoundAddress &address, BankMap map, int channel) {
    if (!channelOk(channel)) {
        return fail<QList<QByteArray>>(badChannel(channel), QByteArray());
    }
    const auto bs = address.bankSelect(map);
    if (!bs) {
        return fail<QList<QByteArray>>(QStringLiteral("invalid sound address"), QByteArray());
    }
    const int g = channel - 1;
    QList<QByteArray> out;
    out.append(bytes({0xB0 | g, 0x00, bs->msb}));
    out.append(bytes({0xB0 | g, 0x20, bs->lsb}));
    out.append(bytes({0xC0 | g, bs->program}));
    return Result<QList<QByteArray>>::success(out);
}

Result<QByteArray> modeChange(int channel, Mode mode) {
    if (!channelOk(channel)) {
        return fail<QByteArray>(badChannel(channel), QByteArray());
    }
    if (mode != Mode::CombiPlay && mode != Mode::ProgPlay) {
        return fail<QByteArray>(QStringLiteral("the app only requests COMBI PLAY or PROG PLAY"),
                                QByteArray());
    }
    QByteArray m = korgHeader(channel, 0x4E);
    m.append(static_cast<char>(static_cast<int>(mode)));
    m.append(kSysexEnd);
    return Result<QByteArray>::success(m);
}

QByteArray deviceInquiryRequest() { return bytes({0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7}); }

Result<QByteArray> modeAndStateRequest(int channel) {
    if (!channelOk(channel)) {
        return fail<QByteArray>(badChannel(channel), QByteArray());
    }
    QByteArray m = korgHeader(channel, 0x12);
    m.append(kSysexEnd);
    return Result<QByteArray>::success(m);
}

namespace {
// 1C/1D, 00kk 00bb with kk = 1 (one bank), program/combination no. 00, reserved 00.
Result<QByteArray> bankDumpRequest(int channel, int function, int bank, int maxBank) {
    if (!channelOk(channel)) {
        return fail<QByteArray>(badChannel(channel), QByteArray());
    }
    if (bank < 0 || bank > maxBank) {
        return fail<QByteArray>(QStringLiteral("bank %1 is out of range 0-%2").arg(bank).arg(maxBank),
                                QByteArray());
    }
    QByteArray m = korgHeader(channel, function);
    m.append(static_cast<char>(0x10 | bank));
    m.append('\0');
    m.append('\0');
    m.append(kSysexEnd);
    return Result<QByteArray>::success(m);
}
}  // namespace

Result<QByteArray> programBankDumpRequest(int channel, int bank) {
    return bankDumpRequest(channel, 0x1C, bank, 3);
}

Result<QByteArray> combinationBankDumpRequest(int channel, int bank) {
    return bankDumpRequest(channel, 0x1D, bank, 2);
}

Result<QByteArray> globalDumpRequest(int channel) {
    if (!channelOk(channel)) {
        return fail<QByteArray>(badChannel(channel), QByteArray());
    }
    QByteArray m = korgHeader(channel, 0x0E);
    m.append('\0');
    m.append(kSysexEnd);
    return Result<QByteArray>::success(m);
}

// ----------------------------------------------------------------- parsers

Result<DeviceInquiryReply> parseDeviceInquiryReply(const QByteArray &m,
                                                   std::optional<int> expectedChannel) {
    // F0 7E 0g 06 02 42 7A 00 mm 00 vv nn rr bb F7  (15 bytes)
    if (m.size() != 15) {
        return fail<DeviceInquiryReply>(
            QStringLiteral("device inquiry reply must be 15 bytes, got %1").arg(m.size()), m);
    }
    if (m.at(0) != kSysexStart || m.at(14) != kSysexEnd) {
        return fail<DeviceInquiryReply>(QStringLiteral("missing F0 or F7"), m);
    }
    for (int i = 1; i < 14; ++i) {
        if (at(m, i) & 0x80) {
            return fail<DeviceInquiryReply>(QStringLiteral("status byte inside reply at index %1").arg(i), m);
        }
    }
    if (at(m, 1) != 0x7E || at(m, 3) != 0x06 || at(m, 4) != 0x02) {
        return fail<DeviceInquiryReply>(QStringLiteral("not a device inquiry reply (7E .. 06 02)"), m);
    }
    if ((at(m, 2) & 0xF0) != 0x00) {
        return fail<DeviceInquiryReply>(QStringLiteral("byte 2 is not 0g"), m);
    }
    if (at(m, 5) != 0x42) {
        return fail<DeviceInquiryReply>(QStringLiteral("not a KORG reply: manufacturer byte is not 42"), m);
    }
    if (at(m, 6) != 0x7A || at(m, 7) != 0x00 || at(m, 9) != 0x00) {
        return fail<DeviceInquiryReply>(QStringLiteral("family bytes are not 7A 00 .. 00"), m);
    }
    const int g = (at(m, 2) & 0x0F) + 1;
    if (expectedChannel && *expectedChannel != g) {
        return fail<DeviceInquiryReply>(
            QStringLiteral("reply is on channel %1, expected %2").arg(g).arg(*expectedChannel), m);
    }
    DeviceInquiryReply r;
    r.channel = g;
    r.memberCode = at(m, 8);
    r.family = r.memberCode == 0x01   ? DeviceInquiryReply::Family::X50
               : r.memberCode == 0x09 ? DeviceInquiryReply::Family::MicroX
                                      : DeviceInquiryReply::Family::Unknown;
    r.major = at(m, 10);
    r.minor = at(m, 11);
    r.release = at(m, 12);
    r.build = at(m, 13);
    return Result<DeviceInquiryReply>::success(r);
}

Result<ModeAndState> parseModeAndState(const QByteArray &m, std::optional<int> expectedChannel) {
    QString err;
    int ch = 0;
    if (m.size() != 27) {
        return fail<ModeAndState>(
            QStringLiteral("mode and state reply must be 27 bytes, got %1").arg(m.size()), m);
    }
    if (!checkKorgFrame(m, 0x42, 27, expectedChannel, &ch, &err)) {
        return fail<ModeAndState>(err, m);
    }
    struct Marker { int index; int value; };
    static const Marker markers[] = {{6, 0x06}, {10, 0x02}, {13, 0x00}, {14, 0x00},
                                     {17, 0x00}, {18, 0x04}, {22, 0x21}};
    for (const Marker &mk : markers) {
        if (at(m, mk.index) != mk.value) {
            return fail<ModeAndState>(QStringLiteral("byte %1 should be %2 but is %3")
                                          .arg(mk.index)
                                          .arg(mk.value, 2, 16, QLatin1Char('0'))
                                          .arg(at(m, mk.index), 2, 16, QLatin1Char('0')),
                                      m);
        }
    }
    const int mode = at(m, 5);
    if (!knownMode(mode)) {
        return fail<ModeAndState>(QStringLiteral("unknown mode value %1").arg(mode), m);
    }
    if (at(m, 11) > 4) {
        return fail<ModeAndState>(QStringLiteral("program bank %1 out of range").arg(at(m, 11)), m);
    }
    if (at(m, 15) > 2) {
        return fail<ModeAndState>(QStringLiteral("combination bank %1 out of range").arg(at(m, 15)), m);
    }
    const int s1 = at(m, 7);
    const int s2 = at(m, 8);
    ModeAndState s;
    s.channel = ch;
    s.mode = static_cast<Mode>(mode);
    s.bankMap = (s1 & 0x10) ? BankMap::Gm : BankMap::Korg;
    s.localControl = s1 & 0x01;
    s.midiClock = (s1 >> 1) & 0x03;
    s.powerOnMemorize = s1 & 0x20;
    s.extRealtime = s1 & 0x40;
    s.enableProgramChange = s2 & 0x01;
    s.enableBankChange = s2 & 0x02;
    s.enableCombinationChange = s2 & 0x04;
    s.enableAftertouch = s2 & 0x08;
    s.enableControlChange = s2 & 0x10;
    s.memoryProtectBits = at(m, 9);
    s.programBank = at(m, 11);
    s.programNumber = at(m, 12);
    s.combinationBank = at(m, 15);
    s.combinationNumber = at(m, 16);
    return Result<ModeAndState>::success(s);
}

Result<AckNak> parseAckNak(const QByteArray &m, std::optional<int> expectedChannel) {
    QString err;
    int ch = 0;
    if (m.size() == 6) {
        if (!checkKorgFrame(m, 0x23, 6, expectedChannel, &ch, &err)) {
            return fail<AckNak>(err, m);
        }
        AckNak a;
        a.channel = ch;
        a.ack = true;
        return Result<AckNak>::success(a);
    }
    if (m.size() == 7) {
        if (!checkKorgFrame(m, 0x24, 7, expectedChannel, &ch, &err)) {
            return fail<AckNak>(err, m);
        }
        AckNak a;
        a.channel = ch;
        a.ack = false;
        a.errorCode = at(m, 5);
        return Result<AckNak>::success(a);
    }
    return fail<AckNak>(QStringLiteral("ACK is 6 bytes and NAK is 7 bytes, got %1").arg(m.size()), m);
}

QString nakCodeText(int code) {
    switch (code) {
    case 0x00: return QStringLiteral("destination memory is protected");
    case 0x01: return QStringLiteral("destination bank/program/parameter does not exist");
    case 0x02: return QStringLiteral("the mode is wrong");
    case 0x03: return QStringLiteral("memory overflow");
    case 0x40: return QStringLiteral("another type of error");
    default: return QStringLiteral("unknown error code %1").arg(code, 2, 16, QLatin1Char('0'));
    }
}

Result<ModeChange> parseModeChange(const QByteArray &m, std::optional<int> expectedChannel) {
    QString err;
    int ch = 0;
    if (m.size() != 7) {
        return fail<ModeChange>(QStringLiteral("mode change must be 7 bytes, got %1").arg(m.size()), m);
    }
    if (!checkKorgFrame(m, 0x4E, 7, expectedChannel, &ch, &err)) {
        return fail<ModeChange>(err, m);
    }
    const int mode = at(m, 5);
    if (!knownMode(mode) || mode == 0x7F) {
        return fail<ModeChange>(QStringLiteral("unknown mode value %1").arg(mode), m);
    }
    ModeChange c;
    c.channel = ch;
    c.mode = static_cast<Mode>(mode);
    return Result<ModeChange>::success(c);
}

Result<BankDump> parseProgramDump(const QByteArray &m, std::optional<int> expectedChannel) {
    return parseBankDump(m, false, expectedChannel);
}

Result<BankDump> parseCombinationDump(const QByteArray &m, std::optional<int> expectedChannel) {
    return parseBankDump(m, true, expectedChannel);
}

Result<GlobalDump> parseGlobalDump(const QByteArray &m, std::optional<int> expectedChannel) {
    QString err;
    int ch = 0;
    if (!checkKorgFrame(m, 0x51, 7, expectedChannel, &ch, &err)) {
        return fail<GlobalDump>(err, m);
    }
    if (at(m, 5) != 0) {
        return fail<GlobalDump>(QStringLiteral("global dump reserved byte 5 is not 00"), m);
    }
    const Result<QByteArray> decoded = decode78(m.mid(6, m.size() - 7));
    if (!decoded.ok) {
        return fail<GlobalDump>(QStringLiteral("data: %1").arg(decoded.error), m);
    }
    if (decoded.value.size() < kGlobalMinimumSize) {
        return fail<GlobalDump>(QStringLiteral("global data is %1 bytes, need at least %2")
                                    .arg(decoded.value.size()).arg(kGlobalMinimumSize), m);
    }
    GlobalDump g;
    g.channel = ch;
    g.data = decoded.value;
    return Result<GlobalDump>::success(g);
}

// ------------------------------------------------------------------- codec

QByteArray encode78(const QByteArray &in) {
    QByteArray out;
    out.reserve(in.size() + in.size() / 7 + 1);
    for (int pos = 0; pos < in.size(); pos += 7) {
        const int k = qMin(7, static_cast<int>(in.size()) - pos);
        int top = 0;
        for (int j = 0; j < k; ++j) {
            top |= ((at(in, pos + j) >> 7) & 1) << j;
        }
        out.append(static_cast<char>(top));
        for (int j = 0; j < k; ++j) {
            out.append(static_cast<char>(at(in, pos + j) & 0x7F));
        }
    }
    return out;
}

Result<QByteArray> decode78(const QByteArray &midi) {
    for (int i = 0; i < midi.size(); ++i) {
        if (at(midi, i) & 0x80) {
            return fail<QByteArray>(QStringLiteral("byte %1 has bit 7 set").arg(i), midi);
        }
    }
    QByteArray out;
    out.reserve(midi.size());
    int pos = 0;
    while (pos < midi.size()) {
        const int remaining = static_cast<int>(midi.size()) - pos;
        if (remaining == 1) {
            return fail<QByteArray>(QStringLiteral("final group has a header byte and no data"), midi);
        }
        const int k = qMin(7, remaining - 1);
        const int top = at(midi, pos);
        if ((top >> k) != 0) {
            return fail<QByteArray>(QStringLiteral("final group header sets bits for missing bytes"), midi);
        }
        for (int j = 0; j < k; ++j) {
            out.append(static_cast<char>(at(midi, pos + 1 + j) | (((top >> j) & 1) << 7)));
        }
        pos += 1 + k;
    }
    return Result<QByteArray>::success(out);
}

// ----------------------------------------------------------------- records

NameField decodeName(const QByteArray &sixteen) {
    NameField f;
    f.raw = sixteen;
    int end = static_cast<int>(sixteen.size());
    while (end > 0 && sixteen.at(end - 1) == ' ') {
        --end;
    }
    QString text;
    text.reserve(end);
    for (int i = 0; i < end; ++i) {
        const int c = at(sixteen, i);
        if (c >= 0x20 && c <= 0x7F) {
            text.append(QChar(static_cast<ushort>(c)));
        } else {
            text.append(QChar(QChar::ReplacementCharacter));
            f.nonPrintable = true;
        }
    }
    // Bytes in the trimmed tail are spaces, which are printable; nothing hidden there.
    f.text = text;
    return f;
}

Result<QVector<SoundRecord>> sliceProgramRecords(const QByteArray &decoded) {
    return sliceRecords(decoded, kProgramRecordSize, kProgramCategoryOffset, false);
}

Result<QVector<SoundRecord>> sliceCombinationRecords(const QByteArray &decoded) {
    return sliceRecords(decoded, kCombinationRecordSize, kCombinationCategoryOffset, true);
}

Result<GlobalCategories> parseGlobalCategories(const QByteArray &d) {
    if (d.size() < kGlobalMinimumSize) {
        return fail<GlobalCategories>(QStringLiteral("global data is %1 bytes, need at least %2")
                                          .arg(d.size()).arg(kGlobalMinimumSize),
                                      d.left(64));
    }
    GlobalCategories g;
    for (int i = 0; i < 16; ++i) {
        g.programCategories.append(
            decodeName(d.mid(kGlobalProgramCategoryNames + i * kNameLength, kNameLength)));
        g.combinationCategories.append(
            decodeName(d.mid(kGlobalCombinationCategoryNames + i * kNameLength, kNameLength)));
    }
    return Result<GlobalCategories>::success(g);
}

}  // namespace X50Protocol
