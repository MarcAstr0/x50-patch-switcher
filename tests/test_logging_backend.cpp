// T1.8: the diagnose send filter. Anything that is not a read-only request built by
// X50Protocol (docs/MIDI.md section 9: never write, never select) is refused and not
// forwarded to the wrapped backend.
#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include "app/LoggingBackend.h"
#include "midi/FakeX50.h"
#include "midi/X50Protocol.h"

class TestLoggingBackend : public QObject {
    Q_OBJECT
private slots:
    void filterRefusesWrites() {
        auto *fake = new FakeX50;
        LoggingBackend log(fake);
        QSet<QByteArray> allowed;
        allowed.insert(X50Protocol::deviceInquiryRequest());
        allowed.insert(X50Protocol::modeAndStateRequest(1).value);
        log.setAllowedSends(allowed);
        QVERIFY(log.openOutput(0).ok);
        QVERIFY(log.openInput(0).ok);

        QVERIFY(log.send(X50Protocol::deviceInquiryRequest()).ok);
        // Mode Change (MIDI.md section 4) and a program change (section 3) are refused.
        QVERIFY(!log.send(X50Protocol::modeChange(1, X50Protocol::Mode::ProgPlay).value).ok);
        QVERIFY(!log.send(QByteArray::fromHex("C000")).ok);
        QCOMPARE(fake->received().size(), 1);
        QCOMPARE(fake->received().first(), X50Protocol::deviceInquiryRequest());
    }
    void logsTraffic() {
        auto *fake = new FakeX50;
        LoggingBackend log(fake);
        QVERIFY(log.openOutput(0).ok);
        QVERIFY(log.openInput(0).ok);
        QSignalSpy spy(&log, &MidiBackend::messageReceived);
        QVERIFY(log.send(X50Protocol::deviceInquiryRequest()).ok);
        QVERIFY(spy.wait(1000));
        int out = 0, in = 0;
        for (const auto &e : log.log()) {
            out += e.dir == LoggingBackend::Entry::Dir::Out;
            in += e.dir == LoggingBackend::Entry::Dir::In;
        }
        QCOMPARE(out, 1);
        QCOMPARE(in, 1);
    }
};

QTEST_GUILESS_MAIN(TestLoggingBackend)
#include "test_logging_backend.moc"
