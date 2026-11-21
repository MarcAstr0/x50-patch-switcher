#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QSet>
#include <QString>
#include <QVector>

#include "midi/MidiBackend.h"

// A MidiBackend that wraps another one, forwards everything and keeps a
// timestamped log of what was opened, sent and received (for --diagnose).
// It can also restrict what may be sent to an allow-list, so a diagnose run
// provably cannot write to the X50 (docs/MIDI.md section 9): anything not on
// the list is refused and logged, never forwarded.
class LoggingBackend : public MidiBackend
{
    Q_OBJECT
public:
    struct Entry
    {
        enum class Dir { Out, In, Event };
        qint64 ms = 0;  // since construction
        Dir dir = Dir::Event;
        QByteArray bytes;
        QString text;   // Event description
    };

    // Takes ownership of inner.
    explicit LoggingBackend(MidiBackend *inner, QObject *parent = nullptr);

    // Empty set = no restriction.
    void setAllowedSends(const QSet<QByteArray> &allowed) { m_allowed = allowed; }
    const QVector<Entry> &log() const { return m_log; }
    void addEvent(const QString &text);

    Ports ports() override { return m_inner->ports(); }
    Status openOutput(int index) override;
    Status openInput(int index) override;
    void closePorts() override;
    bool isOutputOpen() const override { return m_inner->isOutputOpen(); }
    bool isInputOpen() const override { return m_inner->isInputOpen(); }
    Status send(const QByteArray &message) override;

private:
    MidiBackend *m_inner;
    QSet<QByteArray> m_allowed;
    QVector<Entry> m_log;
    QElapsedTimer m_clock;
};
