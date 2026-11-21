#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <exception>
#include <string>
#include <vector>

#include <QMetaObject>

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

RtMidiBackend::RtMidiBackend(QObject *parent) : MidiBackend(parent) {}

RtMidiBackend::~RtMidiBackend() { closePorts(); }

MidiBackend::Status RtMidiBackend::openOutput(int index)
{
    try {
        auto out = std::make_unique<RtMidiOut>();
        if (index < 0 || static_cast<unsigned int>(index) >= out->getPortCount())
            return Status::failure(QStringLiteral("output port %1 does not exist").arg(index));
        try {
            out->openPort(static_cast<unsigned int>(index));
        } catch (const RtMidiError &e) {
            // The port exists, so the open failed because someone else holds it
            // (WinMM opens exclusively, SPEC 7.4).
            return Status::failure(portBusyText(), QString::fromStdString(e.getMessage()));
        }
        m_out = std::move(out);
        return Status::success();
    } catch (const RtMidiError &e) {
        return Status::failure(QStringLiteral("MIDI output unavailable"),
                               QString::fromStdString(e.getMessage()));
    } catch (const std::exception &e) {
        return Status::failure(QStringLiteral("MIDI output unavailable"), describe(e));
    }
}

MidiBackend::Status RtMidiBackend::openInput(int index)
{
    try {
        auto in = std::make_unique<RtMidiIn>();
        if (index < 0 || static_cast<unsigned int>(index) >= in->getPortCount())
            return Status::failure(QStringLiteral("input port %1 does not exist").arg(index));
        // SysEx on, timing and active sensing off: X50Protocol rejects any byte
        // >= 0x80 inside a SysEx, so real-time bytes must never reach it.
        in->ignoreTypes(false, true, true);
        in->setCallback(&RtMidiBackend::inputCallback, this);
        // No error callback: with one set RtMidi stops throwing, and the open
        // failure below is how "port busy" is detected.
        try {
            in->openPort(static_cast<unsigned int>(index));
        } catch (const RtMidiError &e) {
            return Status::failure(portBusyText(), QString::fromStdString(e.getMessage()));
        }
        m_in = std::move(in);
        return Status::success();
    } catch (const RtMidiError &e) {
        return Status::failure(QStringLiteral("MIDI input unavailable"),
                               QString::fromStdString(e.getMessage()));
    } catch (const std::exception &e) {
        return Status::failure(QStringLiteral("MIDI input unavailable"), describe(e));
    }
}

void RtMidiBackend::closePorts()
{
    // Destroying the input cancels the callback and joins RtMidi's thread
    // before anything it points to goes away.
    try {
        if (m_in) {
            m_in->cancelCallback();
            m_in->closePort();
        }
    } catch (...) {
    }
    m_in.reset();
    try {
        if (m_out)
            m_out->closePort();
    } catch (...) {
    }
    m_out.reset();
}

bool RtMidiBackend::isOutputOpen() const { return m_out && m_out->isPortOpen(); }
bool RtMidiBackend::isInputOpen() const { return m_in && m_in->isPortOpen(); }

MidiBackend::Status RtMidiBackend::send(const QByteArray &message)
{
    if (!m_out || !m_out->isPortOpen())
        return Status::failure(QStringLiteral("MIDI output is not open"));
    try {
        std::vector<unsigned char> bytes(message.begin(), message.end());
        m_out->sendMessage(&bytes);
        return Status::success();
    } catch (const RtMidiError &e) {
        return Status::failure(QStringLiteral("could not send to the X50"),
                               QString::fromStdString(e.getMessage()));
    } catch (const std::exception &e) {
        return Status::failure(QStringLiteral("could not send to the X50"), describe(e));
    }
}

// RtMidi's thread. Copy and post; touch nothing else.
void RtMidiBackend::inputCallback(double, std::vector<unsigned char> *message, void *user)
{
    if (!message || message->empty())
        return;
    auto *self = static_cast<RtMidiBackend *>(user);
    const QByteArray bytes(reinterpret_cast<const char *>(message->data()),
                           static_cast<qsizetype>(message->size()));
    QMetaObject::invokeMethod(
        self, [self, bytes]() { emit self->messageReceived(bytes); }, Qt::QueuedConnection);
}
