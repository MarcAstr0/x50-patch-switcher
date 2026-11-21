#pragma once

#include <atomic>
#include <memory>

#include "midi/MidiBackend.h"
#include "midi/SysExAssembler.h"

class RtMidiIn;
class RtMidiOut;

// MidiBackend over RtMidi. The input callback runs on RtMidi's thread: it only
// copies the bytes and posts them to the GUI thread with a queued call.
class RtMidiBackend : public MidiBackend
{
    Q_OBJECT
public:
    // Kept for the command line: enumeration without an instance.
    // Never throws: RtMidiError is caught and reported in Ports::error.
    static Ports listPorts();

    explicit RtMidiBackend(QObject *parent = nullptr);
    ~RtMidiBackend() override;

    Ports ports() override { return listPorts(); }
    Status openOutput(int index) override;
    Status openInput(int index) override;
    void closePorts() override;
    bool isOutputOpen() const override;
    bool isInputOpen() const override;
    Status send(const QByteArray &message) override;

private:
    static void inputCallback(double timestamp, std::vector<unsigned char> *message, void *user);

    // Bumped on every open and close (GUI thread). A message queued by the callback
    // carries the value it saw and is dropped if the connection has changed since.
    std::atomic<unsigned int> m_generation{0};
    // Touched only by RtMidi's callback thread, and by the GUI thread while no
    // callback can run (before openPort, after the input is destroyed).
    SysExAssembler m_assembler;

    std::unique_ptr<RtMidiIn> m_in;
    std::unique_ptr<RtMidiOut> m_out;
};
