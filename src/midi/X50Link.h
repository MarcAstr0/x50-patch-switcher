#pragma once

#include <optional>

#include <QByteArray>
#include <QDeadlineTimer>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include "core/BankMap.h"
#include "core/Catalogue.h"
#include "core/SoundAddress.h"
#include "midi/MidiBackend.h"
#include "midi/X50Protocol.h"

// The X50 state machine (SPEC 4.3-4.6, 9; docs/MIDI.md). Lives on the GUI thread
// and never blocks: every wait is a QTimer single-shot, every reply arrives
// through the backend's messageReceived signal.
//
// Timers (all injectable through Timing):
//   poll          2 s repeating, only while disconnected and started
//   request       1 s single-shot per probe request (inquiry, Mode and State)
//   mode wait     100 ms single-shot after a Mode Change, ended early by ACK/NAK
//   bank          10 s single-shot per bank read
//   echo          300 ms single-shot after our own send; panel CC/PC are ignored meanwhile
//
// Only one activity runs at a time (probe, waiting for a Mode Change reply, or a
// names read). A send during a probe or a read is refused with an error. A send
// during the mode wait replaces the pending sound (the newest wins) but the in-flight
// Mode Change still counts: when the wait ends the believed mode is its target, and
// another Mode Change follows if the newest sound needs a different mode.
//
// Learned state (channel, Bank Map, mode, bank-select bytes, current sound, echo) is
// cleared on disconnect, stop and at the start of every probe. The manual channel and
// Bank Map live in separate fields that learned values never overwrite.
class X50Link : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Disconnected,  // no ports open
        Probing,       // ports open, probe running
        Connected,     // X50 confirmed by Device Inquiry and Mode and State
        Manual,        // ports open, X50 not confirmed: manual channel and Bank Map
        Refused        // a microX answered; nothing is sent
    };
    Q_ENUM(State)

    struct Timing {
        int pollMs = 2000;
        int requestMs = 1000;
        int modeChangeWaitMs = 100;
        int bankMs = 10000;
        int echoMs = 300;
    };

    // One line of the settings checklist (SPEC 4.5). The UI adds the glyph.
    struct CheckItem {
        enum class Status { Ok, Warning, Failed };
        Status status = Status::Ok;
        QString text;
        QString fix;  // where to fix it on the X50; empty when Ok
        QByteArray raw;  // offending bytes for a refused reply
    };
    struct ProbeResult {
        QVector<CheckItem> items;
        bool x50Confirmed = false;
        bool microX = false;
        bool sysexReplies = false;
        bool inquiryRefused = false;  // a SysEx reply arrived but the parser refused it
        QByteArray inquiryRaw;        // ... and these are its bytes
        QString versionText;  // "1.05"
        std::optional<X50Protocol::DeviceInquiryReply> inquiry;
        std::optional<X50Protocol::ModeAndState> state;
        bool allOk() const;
    };

    struct RawDump {
        QString label;  // "Program A", "Combination B", "Global"
        QByteArray bytes;  // exactly as received
    };
    struct ReadResult {
        bool complete = false;  // every bank read and valid
        QMap<SoundAddress, Catalogue::UnitName> names;
        bool haveCategories = false;
        QStringList programCategories;  // 16, from the global dump
        QStringList combinationCategories;
        QStringList errors;  // "<bank>: <why>" per failed bank
        // Overlay the names and, if read, the category names onto the catalogue.
        void applyTo(Catalogue &catalogue) const;
    };

    explicit X50Link(MidiBackend *backend, QObject *parent = nullptr);

    void setTiming(const Timing &t);
    Timing timing() const { return m_timing; }
    // Port name substrings (case-insensitive). An empty input substring reuses the output one.
    void setPortNames(const QString &outputSubstring, const QString &inputSubstring = QString());
    // Manual mode values (SPEC 4.5): channel 1-16, Bank Map. Used until a probe confirms the unit.
    void setManualChannel(int channel);
    void setManualBankMap(BankMap map);

    // Start trying to connect now and every pollMs while disconnected.
    void start();
    // Close the ports and stop polling.
    void stop();
    // Re-run the probe on the open ports ("Check X50").
    void probe();
    // Mode and State only (read-only), for --diagnose after a Device Inquiry reply that
    // was refused: the channel is the manual one. Ends in State::Manual with probeFinished().
    void probeModeAndStateOnly();
    // Port names as RtMidi reports them end in " <index>" on Windows (WinMM). Both helpers
    // ignore that suffix so a name from the output list matches the same device in the input list.
    static QString stripPortIndex(const QString &name);
    // Case-insensitive: `name` contains `sub`, comparing without the trailing " <digits>" on either
    // side, or `name` contains `sub` as given.
    static bool portNameMatches(const QString &name, const QString &sub);
    // Index of the port `sub` selects, or -1. Three passes in order: exact name (case-insensitive),
    // raw substring over all names, then the index-stripped comparison.
    static int findPortIndex(const QStringList &names, const QString &sub);
    // SPEC 4.3. Result arrives as sent() or errorOccurred()/sendFailed().
    void send(const SoundAddress &address);
    // SPEC 4.6. With continueOnError a failed bank is recorded and the next is tried
    // (for --diagnose); otherwise the first failure stops the read.
    void readNames(bool continueOnError = false);
    void cancelRead();

    State state() const { return m_state; }
    QString statusText() const { return m_status; }
    bool isReading() const { return m_activity == Activity::Reading; }
    int channel() const { return m_channel; }
    BankMap bankMap() const { return m_bankMap; }
    bool bankMapKnown() const { return m_bankMapKnown; }
    std::optional<X50Protocol::Mode> believedMode() const { return m_mode; }
    std::optional<SoundAddress> currentSound() const { return m_current; }
    const ProbeResult &lastProbe() const { return m_lastProbe; }
    const QVector<RawDump> &rawDumps() const { return m_rawDumps; }
    const QString &connectedPortName() const { return m_portName; }

signals:
    void stateChanged(X50Link::State state);
    void statusChanged(const QString &text);
    void probeFinished(const X50Link::ProbeResult &result);
    void sent(const SoundAddress &address);
    void sendFailed(const SoundAddress &address, const QString &reason);
    // The player changed sound / mode on the X50's panel (SPEC 4.4).
    void followedSound(const SoundAddress &address);
    void followedMode(X50Protocol::Mode mode);
    void readProgress(int done, int total, const QString &completedBank);
    void readFinished(const X50Link::ReadResult &result);
    void readFailed(const QString &bank, const QString &reason, const QByteArray &raw);
    void readCancelled();
    // Any error, with the raw bytes that caused it when there are some.
    void errorOccurred(const QString &message, const QByteArray &raw);

private:
    enum class Activity { Idle, Probing, ModeWait, Reading };
    enum class ProbeStage { Inquiry, ModeState };
    struct ReadStep {
        enum class Type { Program, Combination, Global };
        Type type;
        int bank;
        QString label;
    };

    using Mode = X50Protocol::Mode;
    void onMessage(const QByteArray &msg);
    void onBackendError(const QString &message);
    void onTimeout();
    void onPoll();
    void attemptConnect();
    int findPort(const QStringList &names, const QString &substring) const;
    void setState(State s);
    void setStatus(const QString &text);
    void raise(const QString &message, const QByteArray &raw = QByteArray());
    void disconnectWith(const QString &reason, const QByteArray &raw = QByteArray());
    bool sendRaw(const QByteArray &bytes);

    // probe
    void beginProbe();
    void onInquiryReply(const QByteArray &msg);
    void onModeStateReply(const QByteArray &msg);
    void probeFailedStage(const QString &text, const QString &fix, const QByteArray &raw);
    void finishProbe(State finalState);

    // send
    void failSend(const SoundAddress &a, const QString &reason, const QByteArray &raw = QByteArray());
    void transmit(const SoundAddress &address);
    void startModeChange(Mode target, const SoundAddress &address);
    // Ends the wait for a Mode Change reply. `failure` non-empty: the change was refused.
    // `nak`: the failure is the X50's NAK. A NAK of a Mode Change that a newer send already
    // superseded (the newest sound needs another mode) does not fail that newest send.
    void finishModeWait(const QString &failure, const QByteArray &raw = QByteArray(), bool nak = false);
    void clearLearned();
    // True when the ports that were opened are no longer listed by the backend.
    bool portsGone();

    // follow
    void onChannelMessage(const QByteArray &msg);
    void onModeChange(const QByteArray &msg);

    // read
    void requestStep();
    void onDump(const QByteArray &msg);
    void onReadNak(const QByteArray &msg);
    void onUnexpectedDump(const QByteArray &msg, X50Protocol::MessageKind got);
    void stepFailed(const QString &reason, const QByteArray &raw);
    void stepDone();
    void finishRead(bool failedStop);

    MidiBackend *m_backend;
    Timing m_timing;
    QTimer m_timer;      // the current request's timeout; single-shot
    QTimer m_echoTimer;  // single-shot
    QTimer m_poll;       // repeating, only while disconnected and started

    State m_state = State::Disconnected;
    Activity m_activity = Activity::Idle;
    ProbeStage m_probeStage = ProbeStage::Inquiry;
    bool m_started = false;
    QString m_outSub, m_inSub, m_portName, m_inPortName, m_status, m_lastRaised;

    // Effective values: learned from the X50, or the manual ones when nothing is learned.
    int m_channel = 1;
    BankMap m_bankMap = BankMap::Gm;
    int m_manualChannel = 1;
    BankMap m_manualBankMap = BankMap::Gm;
    bool m_bankMapKnown = false;
    std::optional<X50Protocol::Mode> m_mode;
    std::optional<SoundAddress> m_current;
    std::optional<int> m_msb, m_lsb;
    bool m_echoActive = false;
    // Every send whose echo may still arrive (overlapping sends): the CC0, CC32, PC of each
    // in order, how many came back, and when it stops counting.
    struct EchoTuple {
        QList<QByteArray> expected;
        int index = 0;
        QDeadlineTimer deadline;
    };
    QList<EchoTuple> m_echo;
    bool m_modeStateOnly = false;
    std::optional<SoundAddress> m_pendingSend;  // the newest sound waiting for the mode wait
    std::optional<Mode> m_inflightMode;         // target of the Mode Change on the wire

    ProbeResult m_probe, m_lastProbe;

    QVector<ReadStep> m_steps;
    int m_stepIndex = 0;
    bool m_continueOnError = false;
    QVector<RawDump> m_rawDumps;
    ReadResult m_readResult;
    QVector<QVector<X50Protocol::SoundRecord>> m_stepRecords;
    X50Protocol::GlobalCategories m_categories;
    bool m_haveCategories = false;
    QString m_firstFailBank, m_firstFailReason;
    QByteArray m_firstFailRaw;
};

Q_DECLARE_METATYPE(X50Link::ProbeResult)
Q_DECLARE_METATYPE(X50Link::ReadResult)
