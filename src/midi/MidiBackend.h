#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QStringList>

// Abstract MIDI port pair (one output, one input). Implemented by RtMidiBackend
// (real ports) and FakeX50 (in-process simulated X50).
//
// Threading contract: every method is called on the GUI thread, and
// messageReceived / portError are always emitted on the GUI thread (a backend
// whose driver calls back on another thread must marshal with a queued
// connection). Nothing here blocks. Incoming messages are complete: one
// channel message, or one whole SysEx from F0 to F7. Real-time bytes and active
// sensing never reach the listener.
class MidiBackend : public QObject
{
    Q_OBJECT
public:
    struct Ports
    {
        QStringList outputs;
        QStringList inputs;
        QString error; // empty when enumeration succeeded
    };

    // Outcome of open/send. `error` is user-facing text, `detail` the driver's own.
    struct Status
    {
        bool ok = false;
        QString error;
        QString detail;
        static Status success() { Status s; s.ok = true; return s; }
        static Status failure(const QString &e, const QString &d = QString())
        {
            Status s; s.error = e; s.detail = d; return s;
        }
    };

    // User-facing text for an open that fails on an existing port (SPEC 7.4).
    static QString portBusyText()
    {
        return QStringLiteral("port busy (another program is using the X50)");
    }

    explicit MidiBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~MidiBackend() override = default;

    virtual Ports ports() = 0;
    // Index into ports().outputs / ports().inputs. Never throws.
    virtual Status openOutput(int index) = 0;
    virtual Status openInput(int index) = 0;
    virtual void closePorts() = 0;
    virtual bool isOutputOpen() const = 0;
    virtual bool isInputOpen() const = 0;
    // One complete message. Never throws; a failure is returned, not emitted.
    virtual Status send(const QByteArray &message) = 0;

signals:
    void messageReceived(const QByteArray &message);
    // A port died or the driver reported an error while open.
    void portError(const QString &message);
};
