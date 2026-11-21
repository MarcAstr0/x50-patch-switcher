#include "midi/RtMidiBackend.h"

#include <RtMidi.h>

#include <exception>
#include <string>
#include <vector>

#include <QDebug>
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
        // WinMM (RtMidi 6.0.0) hands every filled SysEx buffer to the callback as its own
        // chunk; the default buffers are small, so a bank dump arrives in many pieces.
        // The assembler below joins them. 16 buffers of 16 KB = 256 KB of driver buffering,
        // more than the largest dump (a combination bank: 128 x 544 internal bytes in
        // 7-bit form is about 80 KB), so a burst never starves the driver while the callback
        // thread copies. Must be set before openPort (WinMM allocates at open); other
        // APIs ignore it.
        in->setBufferSize(16384, 16);
        m_assembler.reset();
        ++m_generation;
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
    ++m_generation;
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
    m_assembler.reset();
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

// RtMidi's thread. Reassemble, copy and post; touch nothing else.
void RtMidiBackend::inputCallback(double, std::vector<unsigned char> *message, void *user)
{
    if (!message || message->empty())
        return;
    auto *self = static_cast<RtMidiBackend *>(user);
    const QByteArray chunk(reinterpret_cast<const char *>(message->data()),
                           static_cast<qsizetype>(message->size()));
    QStringList problems;
    const QList<QByteArray> msgs = self->m_assembler.feed(chunk, &problems);
    for (const QString &p : problems)
        qWarning().noquote() << "MIDI input:" << p;
    const unsigned int generation = self->m_generation.load();
    for (const QByteArray &bytes : msgs) {
        QMetaObject::invokeMethod(
            self,
            [self, bytes, generation]() {
                if (generation == self->m_generation.load())
                    emit self->messageReceived(bytes);
            },
            Qt::QueuedConnection);
    }
}
