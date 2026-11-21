#pragma once

#include <memory>

#include <QObject>
#include <QString>

#include "app/CommandLine.h"
#include "midi/MidiBackend.h"
#include "midi/X50Link.h"

namespace CliCommon {
// RtMidiBackend, or FakeX50 when fake is true.
std::unique_ptr<MidiBackend> makeBackend(bool fake);
// Substring looked for when --port is not given.
QString defaultPortSubstring();
// "F0 42 30 7A ..." upper case, space separated.
QString hex(const QByteArray &bytes);
// "Name (index)" list for messages.
QString joinNames(const QStringList &names);
bool anyContains(const QStringList &names, const QString &substring);
} // namespace CliCommon

// --send and --probe (SPEC 6). Runs on a QCoreApplication event loop: nothing
// waits on MIDI; the loop quits when X50Link has finished.
class CliRunner : public QObject
{
    Q_OBJECT
public:
    explicit CliRunner(const CliOptions &options, QObject *parent = nullptr);
    // Needs a QCoreApplication. Returns the process exit code (ExitCode::*).
    int run();

private:
    void begin();
    void onProbeFinished(const X50Link::ProbeResult &result);
    void finish(int code);

    CliOptions m_options;
    std::unique_ptr<MidiBackend> m_backend;
    std::unique_ptr<X50Link> m_link;
    int m_exit = ExitCode::Ok;
    bool m_done = false;
};
