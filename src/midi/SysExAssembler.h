#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// Reassembles SysEx that a driver delivers in fragments (RtMidi 6.0.0 on WinMM hands
// every filled SysEx buffer to the callback as its own chunk). Pure bytes, no Qt
// event loop: usable on RtMidi's thread. Not thread-safe; one owner.
//
// Rules, per chunk:
//  - starts with F0 and ends with F7: one complete message, emitted as is.
//  - starts with F0, no F7 at the end: starts a buffer (a partial one is discarded
//    first, with a problem reported).
//  - continuation (first byte below 0x80) while assembling: appended; a trailing F7
//    completes and emits the message. A continuation with nothing to continue is dropped.
//  - a lone F7 while assembling completes the message.
//  - any other status byte (a short channel message) is emitted on its own and does
//    not disturb a message in progress.
//  - more than kMaxSize bytes: the partial message is dropped, a problem is reported.
class SysExAssembler
{
public:
    static constexpr int kMaxSize = 256 * 1024;

    // Complete messages produced by this chunk, in order. Human-readable problems
    // (discarded partials, runaway data) are appended to `problems` when given.
    QList<QByteArray> feed(const QByteArray &chunk, QStringList *problems = nullptr);
    void reset() { m_buffer.clear(); m_active = false; }
    bool assembling() const { return m_active; }
    int pendingSize() const { return static_cast<int>(m_buffer.size()); }

private:
    QByteArray m_buffer;
    bool m_active = false;
};
