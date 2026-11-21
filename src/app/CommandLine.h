#pragma once

#include <QString>
#include <QStringList>

#include "core/SoundAddress.h"

// Process exit codes (SPEC 6, extended by T1.8; also listed in --help).
namespace ExitCode {
constexpr int Ok = 0;
constexpr int Failed = 1;     // --probe: not every line passed; --diagnose: a step failed or
                              // no X50 answered; --list-ports: enumeration failed
constexpr int BadUsage = 2;   // bad address, bad option value, unknown option
constexpr int NoPort = 3;     // no MIDI port matched (or none exist)
constexpr int SendFailed = 4; // port busy or died, the X50 refused (NAK), a file could not be written
constexpr int MicroX = 5;     // a microX answered; nothing is sent
} // namespace ExitCode

// What the command line asked for, once validated.
struct CliOptions
{
    enum class Job { None, Send, Probe, Diagnose };
    Job job = Job::None;
    SoundAddress address;   // Job::Send
    QString diagnoseDir;    // Job::Diagnose, absolute
    QString port;           // --port substring, empty = default ("X50")
    QString inPort;         // --in-port substring, empty = same as port
    int channel = 0;        // --channel, 0 = not given
    bool fakeMidi = false;  // --fake-midi
    QString configDir;      // --config-dir
};

class CommandLine
{
public:
    // Gui: start the GUI. Exit: a flag was handled (or was wrong) and the
    // process should exit with exitCode(). Run: a non-GUI job (--send, --probe,
    // --diagnose) needs a QCoreApplication event loop; see options().
    enum class Result { Gui, Exit, Run };
    // Parses argv-style arguments (first element is the program name). Needs
    // no application object.
    Result process(const QStringList &arguments);
    int exitCode() const { return m_exitCode; }
    // Value of --config-dir, empty if not given (SPEC 6).
    QString configDir() const { return m_options.configDir; }
    const CliOptions &options() const { return m_options; }

private:
    int m_exitCode = 0;
    CliOptions m_options;
};
