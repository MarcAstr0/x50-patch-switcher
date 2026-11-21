#pragma once

#include <optional>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "core/BankMap.h"
#include "core/SoundAddress.h"
#include "midi/MidiBackend.h"
#include "midi/SysExAssembler.h"
#include "midi/X50Protocol.h"

// An in-process X50 behind the MidiBackend interface (docs/MIDI.md section 10).
// It implements the X50's side of sections 3-8: global channel, Mode and State,
// Device Inquiry, Mode Change (ACK or NAK), program/bank select, and bank and
// global dump requests answered from a synthetic memory image laid out as in
// section 7. Every byte it produces comes from X50Protocol's X50-side builders.
//
// Replies are never delivered from inside send(): send() logs the message and
// queues its handling on the event loop, and replies are emitted from there
// (after an optional configured delay).
//
// Knobs (all default to a healthy X50 on channel 1, Bank Map GM, all enables on):
// microX, Bank Map, each enable flag, NAK, dropped replies, malformed dumps,
// panel changes, port busy / unplugged.
class FakeX50 : public MidiBackend
{
    Q_OBJECT
public:
    // Which replies setDrop() can suppress.
    enum class Reply { DeviceInquiry, ModeAndState, Ack, Dump };
    // How dump replies can be broken (for "malformed dump rejected" tests).
    enum class Malformation {
        None,
        FewerRecords,      // 127 records in a bank dump (record aligned)
        OddLength,         // decoded length not a multiple of the record size
        StatusByteInData,  // a data byte with bit 7 set
        WrongBank,         // a dump of another bank than requested
        NoEnd              // F7 missing
    };

    explicit FakeX50(QObject *parent = nullptr);

    // ---- MidiBackend
    Ports ports() override;
    int portListCount() const { return m_portsCalls; }  // how often ports() was asked
    Status openOutput(int index) override;
    Status openInput(int index) override;
    void closePorts() override;
    bool isOutputOpen() const override { return m_outOpen; }
    bool isInputOpen() const override { return m_inOpen; }
    Status send(const QByteArray &message) override;

    // ---- configuration
    void setPortName(const QString &name) { m_portName = name; }
    void setPortBusy(bool busy) { m_busy = busy; }      // opens fail with "port busy"
    // Ports vanish. With raisePortError the open ports die and portError is emitted;
    // without, the driver says nothing (the link must notice by itself).
    void unplug(bool raisePortError = true);
    void plug() { m_present = true; }
    void setGlobalChannel(int channel) { m_channel = channel; }  // 1-16
    void setMicroX(bool microX) { m_microX = microX; }
    void setMemberCode(int code) { m_memberCode = code; m_memberOverride = true; }
    void setVersion(int major, int minor, int release, int build);
    void setBankMap(BankMap map) { m_bankMap = map; }
    void setEnables(bool programChange, bool bankChange, bool combinationChange);
    void setMode(X50Protocol::Mode mode) { m_mode = mode; }
    // Force the program bank reported in Mode and State (4 = E, to look like a microX).
    void setReportedProgramBank(std::optional<int> bank) { m_reportedProgramBank = bank; }
    void setDropReplies(bool all) { m_dropAll = all; }  // "Exclusive" off: SysEx unanswered
    void setDrop(Reply which, bool drop);
    void setReplyDelayMs(int ms) { m_replyDelayMs = ms; }
    void setNakModeChange(std::optional<int> code) { m_nakModeChange = code; }
    void setNakDumps(std::optional<int> code) { m_nakDumps = code; }
    void setMalformedDump(Malformation m) { m_malformation = m; }
    // Deliver SysEx the way WinMM does: in chunks of at most `bytes` (0 = whole), joined
    // again by a SysExAssembler exactly as RtMidiBackend does.
    void setSysExFragmentSize(int bytes) { m_fragmentSize = bytes; }
    // send() reports a write failure while set.
    void setFailSend(bool fail) { m_failSend = fail; }
    // Bumped on every open and close; messages queued before are dropped.
    unsigned int generation() const { return m_generation; }

    // Memory image. Names are ASCII, truncated or space padded to 16.
    void setProgramName(int bank, int number, const QString &name);
    void setProgramCategory(int bank, int number, int category);
    void setCombinationName(int bank, int number, const QString &name);
    void setCombinationCategory(int bank, int number, int category);
    void setCategoryNames(const QStringList &program, const QStringList &combination);

    // ---- the panel: the player changes things on the X50 itself
    void panelChangeMode(X50Protocol::Mode mode);   // transmits F0 42 3g 7A 4E 0m F7
    void panelSelect(const SoundAddress &address);  // transmits CC0, CC32, PC

    // ---- inspection
    QList<QByteArray> received() const { return m_received; }  // everything the app sent
    void clearReceived() { m_received.clear(); }
    X50Protocol::Mode mode() const { return m_mode; }
    SoundAddress currentProgram() const { return m_program; }
    SoundAddress currentCombination() const { return m_combination; }
    // The sound the X50 is on in its present mode.
    SoundAddress currentSound() const;
    int globalChannel() const { return m_channel; }

private:
    void handle(const QByteArray &msg);
    void handleChannelMessage(const X50Protocol::ChannelMessage &m);
    void reply(Reply kind, const QByteArray &bytes);
    void deliver(const QByteArray &bytes);
    void queueDeliver(const QByteArray &bytes);
    void replyNak(int code);
    QByteArray buildModeAndStateReply() const;
    void replyDump(int function, int bank);
    void replyGlobal();
    QByteArray breakDump(QByteArray msg) const;

    QString m_portName = QStringLiteral("X50 Fake MIDI 1");
    bool m_present = true, m_busy = false, m_outOpen = false, m_inOpen = false;
    int m_channel = 1;
    bool m_microX = false, m_memberOverride = false;
    int m_memberCode = 0x01;
    int m_major = 1, m_minor = 5, m_release = 0, m_build = 0;
    BankMap m_bankMap = BankMap::Gm;
    bool m_enablePc = true, m_enableBank = true, m_enableCombi = true;
    X50Protocol::Mode m_mode = X50Protocol::Mode::ProgPlay;
    std::optional<int> m_reportedProgramBank;
    bool m_dropAll = false;
    bool m_dropInquiry = false, m_dropState = false, m_dropAck = false, m_dropDump = false;
    int m_replyDelayMs = 0;
    int m_fragmentSize = 0;
    bool m_failSend = false;
    unsigned int m_generation = 0;
    SysExAssembler m_assembler;
    std::optional<int> m_nakModeChange, m_nakDumps;
    Malformation m_malformation = Malformation::None;

    SoundAddress m_program{SoundAddress::Kind::Prog, SoundAddress::Bank::A, 0};
    SoundAddress m_combination{SoundAddress::Kind::Combi, SoundAddress::Bank::A, 0};
    int m_reportBank = 0, m_reportNumber = 0;  // last A-D program (bank G/g(d) are not reportable)
    std::optional<int> m_msb, m_lsb;

    QByteArray m_programs[4];      // 128 x 444 internal bytes each
    QByteArray m_combinations[3];  // 128 x 544
    QByteArray m_global;

    QList<QByteArray> m_received;
    int m_portsCalls = 0;
};
