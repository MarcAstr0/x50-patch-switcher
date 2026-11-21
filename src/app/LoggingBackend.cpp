#include "app/LoggingBackend.h"

LoggingBackend::LoggingBackend(MidiBackend *inner, QObject *parent) : MidiBackend(parent), m_inner(inner)
{
    m_inner->setParent(this);
    m_clock.start();
    connect(m_inner, &MidiBackend::messageReceived, this, [this](const QByteArray &m) {
        m_log.append({m_clock.elapsed(), Entry::Dir::In, m, QString()});
        emit messageReceived(m);
    });
    connect(m_inner, &MidiBackend::portError, this, [this](const QString &text) {
        addEvent(QStringLiteral("port error: %1").arg(text));
        emit portError(text);
    });
}

void LoggingBackend::addEvent(const QString &text)
{
    m_log.append({m_clock.elapsed(), Entry::Dir::Event, QByteArray(), text});
}

MidiBackend::Status LoggingBackend::openOutput(int index)
{
    const Status s = m_inner->openOutput(index);
    addEvent(QStringLiteral("open output %1: %2").arg(index).arg(s.ok ? QStringLiteral("ok") : s.error + QStringLiteral(" ") + s.detail));
    return s;
}

MidiBackend::Status LoggingBackend::openInput(int index)
{
    const Status s = m_inner->openInput(index);
    addEvent(QStringLiteral("open input %1: %2").arg(index).arg(s.ok ? QStringLiteral("ok") : s.error + QStringLiteral(" ") + s.detail));
    return s;
}

void LoggingBackend::closePorts()
{
    m_inner->closePorts();
    addEvent(QStringLiteral("ports closed"));
}

MidiBackend::Status LoggingBackend::send(const QByteArray &message)
{
    if (m_restricted && !m_allowed.contains(message)) {
        m_log.append({m_clock.elapsed(), Entry::Dir::Event, message,
                      QStringLiteral("REFUSED by the diagnose send filter (not a read-only request)")});
        return Status::failure(QStringLiteral("refused by the diagnose send filter"));
    }
    m_log.append({m_clock.elapsed(), Entry::Dir::Out, message, QString()});
    return m_inner->send(message);
}
