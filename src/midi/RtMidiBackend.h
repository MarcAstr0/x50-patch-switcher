#pragma once

#include <memory>

#include "midi/MidiBackend.h"

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

    std::unique_ptr<RtMidiIn> m_in;
    std::unique_ptr<RtMidiOut> m_out;
};
