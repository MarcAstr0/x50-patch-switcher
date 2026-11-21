#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <exception>
#include <string>

RtMidiBackend::Ports RtMidiBackend::listPorts()
{
    Ports result;
    try {
        RtMidiOut out;
        for (unsigned int i = 0; i < out.getPortCount(); ++i)
            result.outputs << QString::fromStdString(out.getPortName(i));
        RtMidiIn in;
        for (unsigned int i = 0; i < in.getPortCount(); ++i)
            result.inputs << QString::fromStdString(in.getPortName(i));
    } catch (const RtMidiError &e) {
        result.error = QString::fromStdString(e.getMessage());
    } catch (const std::exception &e) {
        result.error = QString::fromUtf8(e.what());
    }
    return result;
}
