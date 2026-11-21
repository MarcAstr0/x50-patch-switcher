#pragma once

#include <memory>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "app/CommandLine.h"
#include "app/LoggingBackend.h"
#include "midi/X50Link.h"

// --diagnose <dir> (SPEC 6): probe, then read every bank (read only), and write
// report.txt plus one raw .syx per bank to the directory. Runs on a
// QCoreApplication event loop. Sends only Device Inquiry, Mode and State and
// dump requests (enforced by the LoggingBackend allow-list): no Mode Change, no
// program change, nothing that writes (docs/MIDI.md section 9).
class DiagnoseRunner : public QObject
{
    Q_OBJECT
public:
    struct Attempt
    {
        QString outSub, inSub;    // port name substrings used
        QString why;              // why this pair was tried
        QString result;           // one line
        bool noPort = false;      // the names matched no port
        bool answered = false;    // something replied to the Device Inquiry
        qint64 probeMs = -1;
        X50Link::ProbeResult probe;
        X50Link::State state = X50Link::State::Disconnected;
        QString portName;
    };
    struct BankTiming
    {
        QString label;
        qint64 ms = 0;
    };

    explicit DiagnoseRunner(const CliOptions &options, QObject *parent = nullptr);
    // Needs a QCoreApplication. Returns the process exit code (ExitCode::*).
    int run();

    // File name for a raw dump label: "Program A" -> "prog-A.syx".
    static QString fileNameForLabel(const QString &label);

private:
    void begin();
    void planAttempts();
    void startAttempt();
    void nextAttempt();
    void onProbeFinished(const X50Link::ProbeResult &result);
    void onReadProgress(int done, int total, const QString &completedBank);
    void onReadFinished(const X50Link::ReadResult &result);
    void onReadFailed(const QString &bank, const QString &reason, const QByteArray &raw);
    void finishRun();
    bool writeOne(const QString &name, const QByteArray &bytes, bool text, QString *error);
    bool writeFiles();
    QString buildReport() const;

    CliOptions m_options;
    LoggingBackend *m_backend = nullptr;  // owned by m_backendOwner
    std::unique_ptr<LoggingBackend> m_backendOwner;
    std::unique_ptr<X50Link> m_link;
    QElapsedTimer m_total, m_attemptClock, m_stepClock;
    MidiBackend::Ports m_ports;
    QString m_choiceNote;
    QVector<Attempt> m_attempts;
    int m_current = -1;
    QStringList m_errors;
    QVector<BankTiming> m_bankTimes;
    QVector<X50Link::RawDump> m_dumps;
    QStringList m_readErrors;
    bool m_readAttempted = false;
    bool m_readComplete = false;
    bool m_microX = false;
    bool m_finished = false;
    QStringList m_writtenFiles;
    QStringList m_fileErrors;
    int m_exit = ExitCode::Ok;
};
