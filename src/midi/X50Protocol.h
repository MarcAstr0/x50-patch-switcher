#pragma once

#include <optional>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/BankMap.h"
#include "core/SoundAddress.h"

// Every byte the app sends to or reads from a KORG X50 is built and parsed here
// and nowhere else (docs/MIDI.md sections 3-7; IMP = Korg's MIDI Implementation).
// Pure functions: no QObject, no I/O, no timers. Messages are QByteArray.
//
// Channels are 1-16 (the global channel as the player sees it); on the wire
// they are g = channel - 1. An out-of-range channel is an error, never a clamp.
//
// Parsers validate every fixed byte, the channel nibble and every length, and on
// failure return an error string plus the raw input. Nothing is guessed.
namespace X50Protocol {

template <typename T>
struct Result {
    bool ok = false;
    T value{};
    QString error;   // empty when ok
    QByteArray raw;  // the offending input when !ok (empty for builder errors)

    explicit operator bool() const { return ok; }
    static Result success(T v) {
        Result r;
        r.ok = true;
        r.value = std::move(v);
        return r;
    }
    static Result failure(const QString &message, const QByteArray &input = QByteArray()) {
        Result r;
        r.error = message;
        r.raw = input;
        return r;
    }
};

// MIDI.md section 4 table (IMP *18).
enum class Mode {
    CombiPlay = 0,
    CombiEdit = 1,
    ProgPlay = 2,
    ProgEdit = 3,
    Multi = 4,
    Global = 6,
    Demo = 0x7F  // only ever reported in Mode and State
};

// Record sizes and offsets (MIDI.md section 7, IMP TABLE 1, 2, 4).
constexpr int kProgramRecordSize = 444;
constexpr int kCombinationRecordSize = 544;
constexpr int kRecordsPerBank = 128;
constexpr int kNameLength = 16;
constexpr int kProgramCategoryOffset = 110;      // 0-15
constexpr int kCombinationCategoryOffset = 118;  // bits 0-3
constexpr int kGlobalProgramCategoryNames = 328;  // 16 x 16
constexpr int kGlobalCombinationCategoryNames = 584;  // 16 x 16
constexpr int kGlobalMinimumSize = 840;           // end of the combination category names

// ---------------------------------------------------------------- builders

// Mode the X50 must be in to select this kind of sound (MIDI.md section 3).
Mode modeForKind(SoundAddress::Kind kind);

// MIDI.md section 3: CC0, CC32, PC as three separate short messages, in order,
// on channel g. Error for an invalid address or channel. The Mode Change that
// may have to precede them is a separate message (modeChange()).
Result<QList<QByteArray>> programSelect(const SoundAddress &address, BankMap map, int channel);

// MIDI.md section 4. The app sends only CombiPlay and ProgPlay; any other mode is an error.
Result<QByteArray> modeChange(int channel, Mode mode);

// MIDI.md section 6: F0 7E 7F 06 01 F7 (any channel).
QByteArray deviceInquiryRequest();

// MIDI.md section 5.
Result<QByteArray> modeAndStateRequest(int channel);

// MIDI.md section 7 requests. Program bank 0-3 (A-D), combination bank 0-2 (A-C).
Result<QByteArray> programBankDumpRequest(int channel, int bank);
Result<QByteArray> combinationBankDumpRequest(int channel, int bank);
Result<QByteArray> globalDumpRequest(int channel);

// ----------------------------------------------------------------- parsers
// expectedChannel: if set, a reply on another channel is an error. If empty any
// channel is accepted (used while discovering g).

// MIDI.md section 6 / SPEC section 8.
struct DeviceInquiryReply {
    enum class Family { X50, MicroX, Unknown };
    int channel = 0;  // 1-16
    int memberCode = 0;
    Family family = Family::Unknown;
    int major = 0, minor = 0, release = 0, build = 0;
};
Result<DeviceInquiryReply> parseDeviceInquiryReply(
    const QByteArray &msg, std::optional<int> expectedChannel = std::nullopt);

// MIDI.md section 5.
struct ModeAndState {
    int channel = 0;  // 1-16
    Mode mode = Mode::CombiPlay;
    BankMap bankMap = BankMap::Korg;  // byte 7 bit 4
    // byte 7
    bool localControl = false;  // bit 0
    int midiClock = 0;          // bits 1-2
    bool powerOnMemorize = false;  // bit 5
    bool extRealtime = false;   // bit 6
    // byte 8
    bool enableProgramChange = false;      // bit 0
    bool enableBankChange = false;         // bit 1
    bool enableCombinationChange = false;  // bit 2
    bool enableAftertouch = false;         // bit 3
    bool enableControlChange = false;      // bit 4
    int memoryProtectBits = 0;  // byte 9, raw
    int programBank = 0;        // 0-3 = A-D, 4 = E (microX)
    int programNumber = 0;      // 0-127
    int combinationBank = 0;    // 0-2 = A-C
    int combinationNumber = 0;  // 0-127
};
Result<ModeAndState> parseModeAndState(
    const QByteArray &msg, std::optional<int> expectedChannel = std::nullopt);

// MIDI.md section 3 step 1 and section 4: ACK F0 42 3g 7A 23 F7, NAK F0 42 3g 7A 24 cc F7.
struct AckNak {
    int channel = 0;
    bool ack = false;
    int errorCode = -1;  // NAK only: cc (0, 1, 2, 3, 0x40 per IMP *22)
};
Result<AckNak> parseAckNak(const QByteArray &msg,
                           std::optional<int> expectedChannel = std::nullopt);
// Human text for a NAK code (IMP *22).
QString nakCodeText(int code);

// MIDI.md section 4 / 8: incoming F0 42 3g 7A 4E 0m F7. Demo (7F) is refused.
struct ModeChange {
    int channel = 0;
    Mode mode = Mode::CombiPlay;
};
Result<ModeChange> parseModeChange(const QByteArray &msg,
                                   std::optional<int> expectedChannel = std::nullopt);

// MIDI.md section 7. Program dump 4C / combination dump 4D. `data` is already
// 7/8-decoded and is not sliced; see sliceProgramRecords / sliceCombinationRecords.
struct BankDump {
    int channel = 0;
    int kind = 0;    // 0 all, 1 one bank, 2 one record (IMP *6, *9)
    int bank = 0;
    int number = 0;  // program / combination number byte as sent
    QByteArray data;
};
Result<BankDump> parseProgramDump(const QByteArray &msg,
                                  std::optional<int> expectedChannel = std::nullopt);
Result<BankDump> parseCombinationDump(const QByteArray &msg,
                                      std::optional<int> expectedChannel = std::nullopt);

// MIDI.md section 7: 51 00 data F7. Decoded data must hold at least
// kGlobalMinimumSize bytes (the category names end at offset 839).
struct GlobalDump {
    int channel = 0;
    QByteArray data;
};
Result<GlobalDump> parseGlobalDump(const QByteArray &msg,
                                   std::optional<int> expectedChannel = std::nullopt);

// ------------------------------------------------------------------- codec

// MIDI.md section 7 "Data encoding" (IMP *5).
QByteArray encode78(const QByteArray &internal);
// Rejects: a byte with bit 7 set; a final group of exactly one byte (header with
// no data); a final group whose header has bits set for bytes that are not there.
Result<QByteArray> decode78(const QByteArray &midi);

// ----------------------------------------------------------------- records

// A 16-byte ASCII field. Bytes outside 0x20-0x7F are not dropped: each becomes
// U+FFFD in `text` and `nonPrintable` is set; `raw` holds all 16 bytes.
// Trailing spaces (0x20) are trimmed from `text`.
struct NameField {
    QString text;
    bool nonPrintable = false;
    QByteArray raw;
};
NameField decodeName(const QByteArray &sixteenBytes);

struct SoundRecord {
    NameField name;
    int category = 0;            // program: raw byte 110; combination: byte 118 & 0x0F
    bool categoryValid = true;   // false if a program's byte 110 is > 15
};

// Decoded length must be a non-zero exact multiple of the record size.
Result<QVector<SoundRecord>> sliceProgramRecords(const QByteArray &decoded);
Result<QVector<SoundRecord>> sliceCombinationRecords(const QByteArray &decoded);

struct GlobalCategories {
    QVector<NameField> programCategories;      // 16
    QVector<NameField> combinationCategories;  // 16
};
Result<GlobalCategories> parseGlobalCategories(const QByteArray &decodedGlobal);

// ---------------------------------------------------------- classification

// Cheap header-only classification of an incoming message, so a caller can route
// it before parsing. It checks the frame start and header bytes only; the
// parsers above do the full validation. `channel` is 1-16 when the header has a
// channel nibble (0 otherwise).
enum class MessageKind {
    Other,
    DeviceInquiryReply,  // F0 7E 0g 06 02 ...
    ModeAndState,        // F0 42 3g 7A 42 ...
    Ack,                 // ... 23
    Nak,                 // ... 24
    ModeChange,          // ... 4E
    ProgramDump,         // ... 4C
    CombinationDump,     // ... 4D
    GlobalDump,          // ... 51
    OtherKorgSysEx,      // F0 42 3g 7A <other function>
    ControlChange,       // Bn cc vv
    ProgramChange        // Cn pp
};
struct Classification {
    MessageKind kind = MessageKind::Other;
    int channel = 0;
};
Classification classify(const QByteArray &msg);

// MIDI.md section 3 / 8: an incoming CC or PC (X50 follow mode).
struct ChannelMessage {
    bool isProgramChange = false;
    int channel = 0;     // 1-16
    int controller = 0;  // CC number (0 = bank MSB, 32 = bank LSB); 0 for PC
    int value = 0;       // CC value, or the program number for PC
};
Result<ChannelMessage> parseChannelMessage(const QByteArray &msg,
                                           std::optional<int> expectedChannel = std::nullopt);

// Reverse of SoundAddress::bankSelect() (MIDI.md section 3 table): the address
// a CC0/CC32/PC triple selects, for the given Bank Map and kind. Nothing if no
// address matches.
std::optional<SoundAddress> addressFromBankSelect(int msb, int lsb, int program, BankMap map,
                                                  SoundAddress::Kind kind);

// ------------------------------------------------- X50 side (FakeX50 only)
// What the X50 itself transmits (MIDI.md sections 4-8). The app never sends
// these; FakeX50 uses them so that the bytes still live in this one file.

QByteArray buildDeviceInquiryReply(const DeviceInquiryReply &r);
Result<QByteArray> buildModeAndState(const ModeAndState &s);
Result<QByteArray> buildAck(int channel);
Result<QByteArray> buildNak(int channel, int errorCode);
// Any mode of the section 4 table except Demo (the panel can enter any of them).
Result<QByteArray> buildModeChangeNotice(int channel, Mode mode);
// Panel side: Bn 00 mm, Bn 20 bb, Cn pp (MIDI.md section 8).
Result<QList<QByteArray>> buildPanelSelect(const SoundAddress &address, BankMap map, int channel);
// `decoded` is the internal (not 7/8 encoded) data; it is encoded here. The
// number byte of a bank dump is 00 (assumption, as in the request; the parsers
// accept any value).
Result<QByteArray> buildProgramDump(int channel, int bank, const QByteArray &decoded);
Result<QByteArray> buildCombinationDump(int channel, int bank, const QByteArray &decoded);
Result<QByteArray> buildGlobalDump(int channel, const QByteArray &decoded);

}  // namespace X50Protocol
