#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <exception>
#include <string>

namespace {
QString describe(const std::exception &e) { return QString::fromUtf8(e.what()); }
}

RtMidiBackend::Ports RtMidiBackend::listPorts()
{
    Ports result;
    QStringList errors;
    try {
        RtMidiOut out;
        for (unsigned int i = 0; i < out.getPortCount(); ++i)
            result.outputs << QString::fromStdString(out.getPortName(i));
    } catch (const RtMidiError &e) {
        errors << QString::fromStdString(e.getMessage());
    } catch (const std::exception &e) {
        errors << describe(e);
    }
    try {
        RtMidiIn in;
        for (unsigned int i = 0; i < in.getPortCount(); ++i)
            result.inputs << QString::fromStdString(in.getPortName(i));
    } catch (const RtMidiError &e) {
        errors << QString::fromStdString(e.getMessage());
    } catch (const std::exception &e) {
        errors << describe(e);
    }
    result.error = errors.join(QStringLiteral("; "));
    return result;
}
