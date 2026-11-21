#include "midi/SysExAssembler.h"

namespace {
constexpr unsigned char kStart = 0xF0;
constexpr unsigned char kEnd = 0xF7;
}  // namespace

QList<QByteArray> SysExAssembler::feed(const QByteArray &chunk, QStringList *problems)
{
    QList<QByteArray> out;
    if (chunk.isEmpty())
        return out;
    const unsigned char first = static_cast<unsigned char>(chunk.at(0));
    const bool endsWithEnd = static_cast<unsigned char>(chunk.at(chunk.size() - 1)) == kEnd;

    auto problem = [problems](const QString &text) {
        if (problems)
            problems->append(text);
    };
    auto overflow = [&]() {
        problem(QStringLiteral("SysEx longer than %1 bytes dropped").arg(kMaxSize));
        reset();
    };

    if (first == kStart) {
        if (m_active) {
            problem(QStringLiteral("SysEx restarted: discarded %1 partial bytes").arg(m_buffer.size()));
            reset();
        }
        if (endsWithEnd && chunk.size() >= 2) {
            if (chunk.size() > kMaxSize)
                problem(QStringLiteral("SysEx longer than %1 bytes dropped").arg(kMaxSize));
            else
                out.append(chunk);
            return out;
        }
        if (chunk.size() > kMaxSize) {
            problem(QStringLiteral("SysEx longer than %1 bytes dropped").arg(kMaxSize));
            return out;
        }
        m_buffer = chunk;
        m_active = true;
        return out;
    }

    if (first >= 0x80) {
        if (first == kEnd && m_active) {
            m_buffer.append(chunk);
            out.append(m_buffer);
            reset();
            return out;
        }
        out.append(chunk);  // a short message: on its own, partial SysEx untouched
        return out;
    }

    // Continuation bytes.
    if (!m_active) {
        problem(QStringLiteral("%1 stray data bytes without a SysEx start dropped").arg(chunk.size()));
        return out;
    }
    if (m_buffer.size() + chunk.size() > kMaxSize) {
        overflow();
        return out;
    }
    m_buffer.append(chunk);
    if (endsWithEnd) {
        out.append(m_buffer);
        reset();
    }
    return out;
}
