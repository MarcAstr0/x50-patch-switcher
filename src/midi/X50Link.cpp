#include "midi/X50Link.h"

#include <QDebug>
#include <QMetaObject>

using namespace X50Protocol;

namespace {

const SoundAddress::Bank kBanks[] = {SoundAddress::Bank::A, SoundAddress::Bank::B,
                                     SoundAddress::Bank::C, SoundAddress::Bank::D};

// Which kind of sound a mode selects, if it selects one (MIDI.md section 4).
std::optional<SoundAddress::Kind> kindOfMode(Mode m)
{
    if (m == Mode::CombiPlay) return SoundAddress::Kind::Combi;
    if (m == Mode::ProgPlay) return SoundAddress::Kind::Prog;
    return std::nullopt;
}

QString versionString(const DeviceInquiryReply &r)
{
    // Assumption (flagged to the foreman): vv.nn.rr = major, minor (two digits), release.
    QString v = QStringLiteral("%1.%2").arg(r.major).arg(r.minor, 2, 10, QLatin1Char('0'));
    if (r.release != 0)
        v += QStringLiteral(".%1").arg(r.release);
    return v;
}

const QString kMicroXText = QStringLiteral(
    "This is a microX; X50 Patch Switcher supports the X50 only");

// MIDI.md section 3 (gating settings): "MIDI Filter Exclusive checked, or every SysEx is ignored";
// "No reply to any SysEx is the symptom", reported as "no SysEx reply: check Exclusive".
const QString kExclusiveFix = QStringLiteral("No SysEx reply: check Exclusive. GLOBAL 1: MIDI, MIDI Filter \"Exclusive\" checked.");
const QString kNoReplyExtra = QStringLiteral(" Also check that the X50 is on and the USB cable is in.");

}  // namespace

bool X50Link::ProbeResult::allOk() const
{
    if (items.isEmpty())
        return false;
    for (const CheckItem &i : items)
        if (i.status != CheckItem::Status::Ok)
            return false;
    return true;
}

void X50Link::ReadResult::applyTo(Catalogue &catalogue) const
{
    for (auto it = names.cbegin(); it != names.cend(); ++it)
        catalogue.setUnitName(it.key(), it.value().name, it.value().category, it.value().readAt);
    if (haveCategories)
        catalogue.setUnitCategories(programCategories, combinationCategories);
}

QString X50Link::stripPortIndex(const QString &name)
{
    // RtMidi's WinMM backend appends " <portIndex>" to every name (RtMidi.cpp, getPortName).
    int i = static_cast<int>(name.size());
    while (i > 0 && name.at(i - 1) >= QLatin1Char('0') && name.at(i - 1) <= QLatin1Char('9'))
        --i;
    if (i == name.size() || i < 2 || name.at(i - 1) != QLatin1Char(' '))
        return name;
    return name.left(i - 1);
}

bool X50Link::portNameMatches(const QString &name, const QString &sub)
{
    if (sub.isEmpty())
        return false;
    return name.contains(sub, Qt::CaseInsensitive) ||
           stripPortIndex(name).contains(stripPortIndex(sub), Qt::CaseInsensitive);
}

X50Link::X50Link(MidiBackend *backend, QObject *parent) : QObject(parent), m_backend(backend)
{
    m_timer.setSingleShot(true);
    m_echoTimer.setSingleShot(true);
    m_poll.setSingleShot(false);
    m_poll.setInterval(m_timing.pollMs);
    connect(&m_timer, &QTimer::timeout, this, &X50Link::onTimeout);
    connect(&m_echoTimer, &QTimer::timeout, this, [this]() { m_echoActive = false; m_echo.clear(); });
    connect(&m_poll, &QTimer::timeout, this, &X50Link::onPoll);
    connect(m_backend, &MidiBackend::messageReceived, this, &X50Link::onMessage);
    connect(m_backend, &MidiBackend::portError, this, &X50Link::onBackendError);
}

void X50Link::setTiming(const Timing &t)
{
    m_timing = t;
    m_poll.setInterval(t.pollMs);
}

void X50Link::setPortNames(const QString &out, const QString &in)
{
    m_outSub = out;
    m_inSub = in.isEmpty() ? out : in;
}

void X50Link::setManualChannel(int channel)
{
    if (channel < 1 || channel > 16)
        return;
    m_manualChannel = channel;
    if (!m_channelLearned)  // a channel the X50 reported is never overwritten
        m_channel = channel;
}

void X50Link::setManualBankMap(BankMap map)
{
    m_manualBankMap = map;
    if (m_state != State::Connected) {
        m_bankMap = map;
        m_bankMapKnown = false;
    }
}

// ------------------------------------------------------------ plumbing

void X50Link::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    if (s == State::Disconnected && m_started)
        m_poll.start();
    else
        m_poll.stop();
    emit stateChanged(s);
}

void X50Link::setActivity(Activity a)
{
    if (m_activity == a)
        return;
    const bool wasBusy = m_activity != Activity::Idle;
    m_activity = a;
    if (wasBusy != (a != Activity::Idle))
        emit busyChanged();
}

void X50Link::setStatus(const QString &text, bool failure)
{
    m_statusFailure = failure;  // before the signal, so slots see the flag that goes with the text
    if (m_status == text)
        return;
    m_status = text;
    emit statusChanged(text);
}

void X50Link::raise(const QString &message, const QByteArray &raw)
{
    if (message != m_lastRaised)
        m_lastRaised.clear();  // a different failure: the connect poll may report the old one again
    setStatus(message);
    emit errorOccurred(message, raw);
}

bool X50Link::sendRaw(const QByteArray &bytes)
{
    const MidiBackend::Status st = m_backend->send(bytes);
    if (st.ok)
        return true;
    disconnectWith(st.detail.isEmpty() ? st.error : st.error + QStringLiteral(": ") + st.detail);
    return false;
}

void X50Link::clearLearned()
{
    m_channel = m_manualChannel;
    m_channelLearned = false;
    m_bankMap = m_manualBankMap;
    m_bankMapKnown = false;
    m_mode.reset();
    m_msb.reset();
    m_lsb.reset();
    m_current.reset();
    m_echoActive = false;
    m_echo.clear();
    m_modeStateOnly = false;
    m_echoTimer.stop();
}

bool X50Link::portsGone()
{
    const MidiBackend::Ports p = m_backend->ports();
    if (!p.error.isEmpty())
        return false;  // cannot tell
    // The exact names that were opened, not the substring: another port with a matching name
    // must not hide an unplugged X50. A shifted WinMM index (" 1" -> " 2") still counts as present.
    auto listed = [](const QStringList &names, const QString &opened) {
        if (opened.isEmpty() || names.contains(opened))
            return true;
        const QString stripped = stripPortIndex(opened);
        for (const QString &n : names)
            if (stripPortIndex(n).compare(stripped, Qt::CaseInsensitive) == 0)
                return true;
        return false;
    };
    return !listed(p.outputs, m_portName) || !listed(p.inputs, m_inPortName);
}

void X50Link::disconnectWith(const QString &reason, const QByteArray &raw)
{
    m_timer.stop();
    const Activity was = m_activity;
    const std::optional<SoundAddress> pending = m_pendingSend;
    setActivity(Activity::Idle);
    m_pendingSend.reset();
    m_inflightMode.reset();
    m_backend->closePorts();
    clearLearned();
    if (was == Activity::Reading) {
        const QString bank = m_stepIndex < m_steps.size() ? m_steps.at(m_stepIndex).label : QString();
        emit readFailed(bank, reason, raw);
    } else if (was == Activity::Probing) {
        CheckItem item;
        item.status = CheckItem::Status::Failed;
        item.text = reason;
        m_probe.items.append(item);
        m_lastProbe = m_probe;
        emit probeFinished(m_probe);
    }
    if (was == Activity::ModeWait && pending)
        emit sendFailed(*pending, reason);
    setState(State::Disconnected);
    raise(reason, raw);
}

void X50Link::onBackendError(const QString &message)
{
    if (m_state != State::Disconnected)
        disconnectWith(message);
}

// ------------------------------------------------------- connection

void X50Link::start()
{
    m_started = true;
    if (m_state == State::Disconnected) {
        attemptConnect();
        if (m_state == State::Disconnected)
            m_poll.start();
    }
}

void X50Link::stop()
{
    m_started = false;
    m_lastRaised.clear();
    m_poll.stop();
    m_timer.stop();
    const Activity was = m_activity;
    const std::optional<SoundAddress> pending = m_pendingSend;
    const QString readBank = (was == Activity::Reading && m_stepIndex < m_steps.size())
                                 ? m_steps.at(m_stepIndex).label : QString();
    setActivity(Activity::Idle);
    m_pendingSend.reset();
    m_inflightMode.reset();
    m_backend->closePorts();
    clearLearned();
    if (was == Activity::Reading)
        emit readFailed(readBank, QStringLiteral("Disconnected from the X50"), QByteArray());
    if (was == Activity::ModeWait && pending)
        emit sendFailed(*pending, QStringLiteral("Disconnected from the X50"));
    setState(State::Disconnected);
}

void X50Link::onPoll()
{
    if (m_state == State::Disconnected)
        attemptConnect();
}

int X50Link::findPort(const QStringList &names, const QString &sub) const
{
    return findPortIndex(names, sub);
}

int X50Link::findPortIndex(const QStringList &names, const QString &sub)
{
    if (sub.isEmpty())
        return -1;
    // Pass 1: the exact name, case-insensitive (diagnose pairs pass full names).
    for (int i = 0; i < names.size(); ++i)
        if (names.at(i).compare(sub, Qt::CaseInsensitive) == 0)
            return i;
    // Pass 2: the raw substring over all names, so "Device 1" picks "USB MIDI Device 1".
    for (int i = 0; i < names.size(); ++i)
        if (names.at(i).contains(sub, Qt::CaseInsensitive))
            return i;
    // Pass 3: the index-stripped comparison (WinMM numbers differ between lists).
    const QString strippedSub = stripPortIndex(sub);
    if (strippedSub.isEmpty())
        return -1;
    for (int i = 0; i < names.size(); ++i)
        if (stripPortIndex(names.at(i)).contains(strippedSub, Qt::CaseInsensitive))
            return i;
    return -1;
}

void X50Link::attemptConnect()
{
    const MidiBackend::Ports ports = m_backend->ports();
    const int out = findPort(ports.outputs, m_outSub);
    const int in = findPort(ports.inputs, m_inSub);
    if (out < 0 || in < 0) {
        const QString why = !ports.error.isEmpty()
            ? QStringLiteral("MIDI is unavailable: %1").arg(ports.error)
            : (m_outSub.isEmpty() ? QStringLiteral("No MIDI port chosen")
                                  : QStringLiteral("No X50 port found (looking for \"%1\")").arg(m_outSub));
        setStatus(why, true);
        return;
    }
    MidiBackend::Status st = m_backend->openOutput(out);
    if (st.ok)
        st = m_backend->openInput(in);
    if (!st.ok) {
        m_backend->closePorts();
        const QString why = st.error;
        // The poll retries every 2 s: only report a changed message.
        if (why != m_lastRaised) {
            m_lastRaised = why;
            raise(why, QByteArray());
        }
        return;
    }
    m_lastRaised.clear();
    m_portName = ports.outputs.at(out);
    m_inPortName = ports.inputs.at(in);
    setStatus(QStringLiteral("Connected to %1; checking the X50").arg(m_portName));
    beginProbe();
}

// ---------------------------------------------------------------- probe

void X50Link::probe()
{
    if (m_state == State::Disconnected) {
        raise(QStringLiteral("Not connected: no X50 port is open"));
        return;
    }
    if (m_activity == Activity::Reading) {
        raise(QStringLiteral("Busy reading names from the X50"));
        return;
    }
    beginProbe();
}

void X50Link::beginProbe()
{
    m_timer.stop();
    const std::optional<SoundAddress> pending = m_activity == Activity::ModeWait ? m_pendingSend
                                                                                  : std::nullopt;
    m_pendingSend.reset();
    m_inflightMode.reset();
    if (pending)
        emit sendFailed(*pending, QStringLiteral("Interrupted: the X50 is being checked"));
    clearLearned();
    m_probe = ProbeResult();
    setActivity(Activity::Probing);
    m_probeStage = ProbeStage::Inquiry;
    setState(State::Probing);
    m_timer.start(m_timing.requestMs);  // started first so a failed send cannot leave it running
    if (!sendRaw(deviceInquiryRequest()))
        return;
}

void X50Link::probeModeAndStateOnly()
{
    if (m_state == State::Disconnected || m_state == State::Refused) {
        raise(m_state == State::Refused ? kMicroXText : QStringLiteral("Not connected: no X50 port is open"));
        return;
    }
    if (m_activity != Activity::Idle) {
        raise(QStringLiteral("The X50 link is busy"));
        return;
    }
    m_timer.stop();
    clearLearned();  // the channel is the manual one again
    m_modeStateOnly = true;
    m_probe = ProbeResult();
    setActivity(Activity::Probing);
    m_probeStage = ProbeStage::ModeState;
    setState(State::Probing);
    m_timer.start(m_timing.requestMs);
    sendRaw(modeAndStateRequest(m_channel).value);
}

void X50Link::probeFailedStage(const QString &text, const QString &fix, const QByteArray &raw)
{
    CheckItem item;
    item.status = CheckItem::Status::Failed;
    item.text = text;
    item.fix = fix;
    item.raw = raw;
    m_probe.items.append(item);
}

void X50Link::finishProbe(State finalState)
{
    m_timer.stop();
    setActivity(Activity::Idle);
    m_modeStateOnly = false;
    m_lastProbe = m_probe;
    setState(finalState);
    switch (finalState) {
    case State::Connected:
        setStatus(QStringLiteral("X50 connected"));
        break;
    case State::Refused:
        setStatus(kMicroXText);
        break;
    default:
        setStatus(QStringLiteral("Could not confirm an X50: manual mode (channel %1, Bank Map %2)")
                      .arg(m_channel).arg(m_bankMap == BankMap::Gm ? QStringLiteral("GM")
                                                                    : QStringLiteral("KORG")));
        break;
    }
    emit probeFinished(m_probe);
}

void X50Link::onInquiryReply(const QByteArray &msg)
{
    const auto r = parseDeviceInquiryReply(msg);
    if (!r.ok) {
        m_probe.inquiryRefused = true;
        m_probe.inquiryRaw = r.raw;
        probeFailedStage(QStringLiteral("Device Inquiry reply refused: %1").arg(r.error),
                         QStringLiteral("Report the raw bytes"), r.raw);
        raise(QStringLiteral("Device Inquiry reply refused: %1").arg(r.error), r.raw);
        finishProbe(State::Manual);
        return;
    }
    m_probe.inquiry = r.value;
    m_probe.versionText = versionString(r.value);
    m_channel = r.value.channel;
    using F = DeviceInquiryReply::Family;
    if (r.value.family == F::MicroX) {
        m_probe.microX = true;
        probeFailedStage(kMicroXText, QString(), QByteArray());
        finishProbe(State::Refused);
        return;
    }
    if (r.value.family != F::X50) {
        probeFailedStage(
            QStringLiteral("A device answered but it is not an X50 (member code %1)")
                .arg(r.value.memberCode, 2, 16, QLatin1Char('0')),
            QStringLiteral("Working in manual mode: choose the channel and Bank Map yourself"),
            msg);
        finishProbe(State::Manual);
        return;
    }
    m_channelLearned = true;
    m_probe.x50Confirmed = true;
    CheckItem ok;
    ok.text = QStringLiteral("X50 found (system %1, global channel %2)")
                  .arg(m_probe.versionText).arg(r.value.channel);
    m_probe.items.append(ok);

    const auto req = modeAndStateRequest(m_channel);
    m_probeStage = ProbeStage::ModeState;
    m_timer.start(m_timing.requestMs);
    sendRaw(req.value);
}

void X50Link::onModeStateReply(const QByteArray &msg)
{
    const auto r = parseModeAndState(msg, m_channel);
    if (!r.ok) {
        probeFailedStage(QStringLiteral("Mode and State reply refused: %1").arg(r.error),
                         QStringLiteral("Working in manual mode; report the raw bytes"), r.raw);
        raise(QStringLiteral("Mode and State reply refused: %1").arg(r.error), r.raw);
        finishProbe(State::Manual);
        return;
    }
    const ModeAndState &s = r.value;
    m_probe.state = s;
    m_probe.sysexReplies = true;

    auto flag = [this](bool on, const QString &what, const QString &tick) {
        CheckItem i;
        i.status = on ? CheckItem::Status::Ok : CheckItem::Status::Failed;
        i.text = on ? QStringLiteral("%1 enabled").arg(what) : QStringLiteral("%1 disabled").arg(what);
        if (!on)
            i.fix = QStringLiteral("GLOBAL 1: MIDI, tick \"%1\"").arg(tick);
        m_probe.items.append(i);
    };
    flag(s.enableProgramChange, QStringLiteral("Program Change"), QStringLiteral("Program Change"));
    flag(s.enableBankChange, QStringLiteral("Bank Change"), QStringLiteral("Bank"));
    flag(s.enableCombinationChange, QStringLiteral("Combination Change"), QStringLiteral("Combi"));
    CheckItem bm;
    bm.text = QStringLiteral("Bank Map: %1").arg(s.bankMap == BankMap::Gm ? QStringLiteral("GM")
                                                                          : QStringLiteral("KORG"));
    m_probe.items.append(bm);
    CheckItem sx;
    sx.text = QStringLiteral("SysEx: replies received");
    m_probe.items.append(sx);
    if (s.programBank == 4) {
        CheckItem sus;
        sus.status = CheckItem::Status::Warning;
        sus.text = QStringLiteral("Suspect: Mode and State reports program bank E, which only the microX has");
        sus.fix = QStringLiteral("Report the raw bytes");
        sus.raw = msg;
        m_probe.items.append(sus);
    }

    m_bankMap = s.bankMap;
    m_bankMapKnown = true;
    m_mode = s.mode;
    m_msb.reset();
    m_lsb.reset();
    m_current.reset();
    if (s.mode == Mode::CombiPlay && s.combinationBank <= 2)
        m_current = SoundAddress(SoundAddress::Kind::Combi, kBanks[s.combinationBank], s.combinationNumber);
    else if (s.mode == Mode::ProgPlay && s.programBank <= 3)
        m_current = SoundAddress(SoundAddress::Kind::Prog, kBanks[s.programBank], s.programNumber);
    finishProbe(m_modeStateOnly ? State::Manual : State::Connected);
}

// ------------------------------------------------------------- incoming

void X50Link::onMessage(const QByteArray &msg)
{
    if (m_state == State::Disconnected)
        return;
    // While the Device Inquiry is pending, anything shaped like an inquiry reply (F0 7E .. 06 02)
    // counts as the answer, even with a byte 2 that is not the 0g form: the parser then refuses
    // it and the raw bytes are reported, instead of the reply being ignored as "no reply".
    if (m_activity == Activity::Probing && m_probeStage == ProbeStage::Inquiry && msg.size() >= 5 &&
        static_cast<unsigned char>(msg.at(0)) == 0xF0 && static_cast<unsigned char>(msg.at(1)) == 0x7E &&
        msg.at(3) == 0x06 && msg.at(4) == 0x02) {
        onInquiryReply(msg);
        return;
    }
    const Classification c = classify(msg);
    switch (c.kind) {
    case MessageKind::DeviceInquiryReply:
        if (m_activity == Activity::Probing && m_probeStage == ProbeStage::Inquiry)
            onInquiryReply(msg);
        return;
    default:
        break;
    }
    // Everything else is a Korg message or channel message: ours only.
    if (c.channel != 0 && c.channel != m_channel)
        return;
    switch (c.kind) {
    case MessageKind::ModeAndState:
        if (m_activity == Activity::Probing && m_probeStage == ProbeStage::ModeState)
            onModeStateReply(msg);
        break;
    case MessageKind::Ack:
    case MessageKind::Nak:
        if (m_activity == Activity::ModeWait) {
            const auto a = parseAckNak(msg, m_channel);
            if (!a.ok)
                finishModeWait(QStringLiteral("Mode Change reply refused: %1").arg(a.error), a.raw);
            else if (!a.value.ack)
                finishModeWait(QStringLiteral("The X50 refused the mode change: %1")
                                   .arg(nakCodeText(a.value.errorCode)),
                               msg, true);
            else
                finishModeWait(QString());
        } else if (m_activity == Activity::Reading && c.kind == MessageKind::Nak) {
            onReadNak(msg);
        } else if (m_activity == Activity::Probing && m_probeStage == ProbeStage::ModeState &&
                   c.kind == MessageKind::Nak) {
            const auto a = parseAckNak(msg, m_channel);
            probeFailedStage(QStringLiteral("Mode and State refused by the X50: %1")
                                 .arg(a.ok ? nakCodeText(a.value.errorCode) : a.error),
                             QStringLiteral("Working in manual mode"), msg);
            finishProbe(State::Manual);
        }
        break;
    case MessageKind::ModeChange:
        onModeChange(msg);
        break;
    case MessageKind::ControlChange:
    case MessageKind::ProgramChange:
        onChannelMessage(msg);
        break;
    case MessageKind::ProgramDump:
    case MessageKind::CombinationDump:
    case MessageKind::GlobalDump:
        if (m_activity == Activity::Reading)
            onDump(msg);
        break;
    default:
        break;  // 40/49 current-buffer dumps, other Korg SysEx, real-time bytes: ignored
    }
}

void X50Link::onTimeout()
{
    switch (m_activity) {
    case Activity::Probing:
        if (portsGone()) {
            disconnectWith(QStringLiteral("The X50 port is gone: check the USB cable"));
            break;
        }
        if (m_probeStage == ProbeStage::Inquiry) {
            probeFailedStage(
                QStringLiteral("No reply from the X50"), kExclusiveFix + kNoReplyExtra, QByteArray());
        } else {
            probeFailedStage(QStringLiteral("No reply to Mode and State: check Exclusive"),
                             kExclusiveFix, QByteArray());
        }
        finishProbe(State::Manual);
        break;
    case Activity::ModeWait:
        // No ACK within the wait: carry on (MIDI.md section 3 step 1).
        finishModeWait(QString());
        break;
    case Activity::Reading:
        if (portsGone()) {
            disconnectWith(QStringLiteral("The X50 port is gone: check the USB cable"));
            break;
        }
        stepFailed(QStringLiteral("no reply within %1 s").arg(m_timing.bankMs / 1000.0, 0, 'g', 3),
                   QByteArray());
        break;
    case Activity::Idle:
        break;
    }
}

// ----------------------------------------------------------------- send

void X50Link::failSend(const SoundAddress &a, const QString &reason, const QByteArray &raw)
{
    raise(reason, raw);
    emit sendFailed(a, reason);
}

void X50Link::send(const SoundAddress &address)
{
    if (!address.isValid()) {
        failSend(address, QStringLiteral("Invalid sound address"));
        return;
    }
    switch (m_state) {
    case State::Disconnected:
        failSend(address, QStringLiteral("Not connected to the X50"));
        return;
    case State::Refused:
        failSend(address, kMicroXText);
        return;
    case State::Probing:
        failSend(address, QStringLiteral("Still checking the X50; try again in a moment"));
        return;
    default:
        break;
    }
    if (m_activity == Activity::Reading || m_activity == Activity::Probing) {
        failSend(address, QStringLiteral("Busy reading names from the X50"));
        return;
    }
    // A send during the mode wait only replaces the pending sound: the Mode Change on
    // the wire still counts and finishModeWait() re-evaluates the mode for the newest.
    if (m_activity == Activity::ModeWait) {
        m_pendingSend = address;
        return;
    }
    const Mode target = modeForKind(address.kind());
    if (m_mode && *m_mode == target) {
        transmit(address);
        return;
    }
    startModeChange(target, address);
}

void X50Link::startModeChange(Mode target, const SoundAddress &address)
{
    const auto mc = modeChange(m_channel, target);
    setActivity(Activity::ModeWait);
    m_pendingSend = address;
    m_inflightMode = target;
    m_timer.start(m_timing.modeChangeWaitMs);
    sendRaw(mc.value);  // on failure disconnectWith() reports sendFailed for the pending sound
}

void X50Link::finishModeWait(const QString &failure, const QByteArray &raw, bool nak)
{
    m_timer.stop();
    const std::optional<SoundAddress> pending = m_pendingSend;
    const std::optional<Mode> inflight = m_inflightMode;
    setActivity(Activity::Idle);
    m_pendingSend.reset();
    m_inflightMode.reset();
    if (!failure.isEmpty()) {
        if (nak && pending && inflight && modeForKind(pending->kind()) != *inflight) {
            // The refused change was for an older send; the newest needs another mode and the
            // X50 is still in the one it was in. Start over from there.
            raise(failure, raw);
            const Mode target = modeForKind(pending->kind());
            if (m_mode && *m_mode == target)
                transmit(*pending);
            else
                startModeChange(target, *pending);
            return;
        }
        if (pending)
            failSend(*pending, failure, raw);
        return;
    }
    if (inflight) {
        // ACK or silence: the X50 is now in the mode we asked for.
        m_mode = inflight;
        const auto k = kindOfMode(*inflight);
        if (m_current && (!k || *k != m_current->kind()))
            m_current.reset();
    }
    if (!pending)
        return;
    const Mode target = modeForKind(pending->kind());
    if (m_mode && *m_mode == target)
        transmit(*pending);
    else
        startModeChange(target, *pending);
}

void X50Link::transmit(const SoundAddress &address)
{
    const auto msgs = programSelect(address, m_bankMap, m_channel);
    if (!msgs.ok) {
        failSend(address, msgs.error);
        return;
    }
    for (const QByteArray &m : msgs.value) {
        if (!sendRaw(m)) {
            emit sendFailed(address, m_status);
            return;
        }
    }
    EchoTuple tuple;
    tuple.expected = msgs.value;
    tuple.deadline = QDeadlineTimer(m_timing.echoMs);
    m_echo.append(tuple);
    const auto bs = address.bankSelect(m_bankMap);
    m_msb = bs->msb;
    m_lsb = bs->lsb;
    m_mode = modeForKind(address.kind());
    m_current = address;
    m_echoActive = true;
    m_echoTimer.start(m_timing.echoMs);
    emit sent(address);
}

// --------------------------------------------------------------- follow

void X50Link::onModeChange(const QByteArray &msg)
{
    const auto r = parseModeChange(msg, m_channel);
    if (!r.ok) {
        raise(QStringLiteral("Mode change from the X50 refused: %1").arg(r.error), r.raw);
        return;
    }
    const bool changed = !m_mode || *m_mode != r.value.mode;
    m_mode = r.value.mode;
    const auto k = kindOfMode(r.value.mode);
    if (m_current && (!k || *k != m_current->kind()))
        m_current.reset();  // the sound in the new mode is unknown until the panel sends one
    if (changed)
        emit followedMode(r.value.mode);
}

void X50Link::onChannelMessage(const QByteArray &msg)
{
    if (m_state != State::Connected && m_state != State::Manual)
        return;
    if (m_echoActive) {
        // Only the echo we expect is swallowed (SPEC 4.4): the next member of the CC0, CC32, PC
        // triple of one of our own sends that has not expired (sends may overlap, oldest first).
        // Anything else is the player on the panel and ends suppression.
        for (int i = m_echo.size() - 1; i >= 0; --i)
            if (m_echo.at(i).deadline.hasExpired())
                m_echo.removeAt(i);
        for (int i = 0; i < m_echo.size(); ++i) {
            EchoTuple &t = m_echo[i];
            if (t.index < t.expected.size() && msg == t.expected.at(t.index)) {
                if (++t.index >= t.expected.size())
                    m_echo.removeAt(i);
                if (m_echo.isEmpty()) {
                    m_echoActive = false;
                    m_echoTimer.stop();
                }
                return;
            }
        }
        m_echo.clear();
        m_echoActive = false;
        m_echoTimer.stop();
    }
    const auto r = parseChannelMessage(msg, m_channel);
    if (!r.ok)
        return;
    const ChannelMessage &cm = r.value;
    if (!cm.isProgramChange) {
        if (cm.controller == 0x00)
            m_msb = cm.value;
        else if (cm.controller == 0x20)
            m_lsb = cm.value;
        return;
    }
    if (!m_mode || !m_msb || !m_lsb)
        return;
    const auto kind = kindOfMode(*m_mode);
    if (!kind)
        return;
    const auto a = addressFromBankSelect(*m_msb, *m_lsb, cm.value, m_bankMap, *kind);
    if (!a)
        return;
    m_current = *a;
    emit followedSound(*a);
}

// ----------------------------------------------------------------- read

void X50Link::readNames(bool continueOnError)
{
    if (m_state != State::Connected && m_state != State::Manual) {
        raise(m_state == State::Refused ? kMicroXText : QStringLiteral("Not connected to the X50"));
        return;
    }
    if (m_activity != Activity::Idle) {
        raise(QStringLiteral("The X50 link is busy"));
        return;
    }
    m_steps.clear();
    static const char *const letters = "ABCD";
    for (int b = 0; b < 4; ++b)
        m_steps.append({ReadStep::Type::Program, b, QStringLiteral("Program %1").arg(QLatin1Char(letters[b]))});
    for (int b = 0; b < 3; ++b)
        m_steps.append({ReadStep::Type::Combination, b, QStringLiteral("Combination %1").arg(QLatin1Char(letters[b]))});
    m_steps.append({ReadStep::Type::Global, 0, QStringLiteral("Global")});
    m_stepRecords = QVector<QVector<SoundRecord>>(m_steps.size());
    m_stepIndex = 0;
    m_continueOnError = continueOnError;
    m_rawDumps.clear();
    m_readResult = ReadResult();
    m_haveCategories = false;
    m_firstFailBank.clear();
    m_firstFailReason.clear();
    m_firstFailRaw.clear();
    setActivity(Activity::Reading);
    emit readProgress(0, m_steps.size(), QString());
    if (m_activity == Activity::Reading)
        requestStep();
}

void X50Link::cancelRead()
{
    if (m_activity != Activity::Reading)
        return;
    m_timer.stop();
    setActivity(Activity::Idle);
    emit readCancelled();
}

void X50Link::requestStep()
{
    const ReadStep &s = m_steps.at(m_stepIndex);
    X50Protocol::Result<QByteArray> req;
    switch (s.type) {
    case ReadStep::Type::Program: req = programBankDumpRequest(m_channel, s.bank); break;
    case ReadStep::Type::Combination: req = combinationBankDumpRequest(m_channel, s.bank); break;
    case ReadStep::Type::Global: req = globalDumpRequest(m_channel); break;
    }
    m_timer.start(m_timing.bankMs);
    sendRaw(req.value);
}

void X50Link::stepFailed(const QString &reason, const QByteArray &raw)
{
    const QString label = m_steps.at(m_stepIndex).label;
    m_readResult.errors << QStringLiteral("%1: %2").arg(label, reason);
    if (m_firstFailBank.isEmpty()) {
        m_firstFailBank = label;
        m_firstFailReason = reason;
        m_firstFailRaw = raw;
    }
    if (!m_continueOnError) {
        finishRead(true);
        return;
    }
    stepDone();
}

void X50Link::stepDone()
{
    m_timer.stop();
    const int done = m_stepIndex + 1;
    const QString label = m_steps.at(m_stepIndex).label;
    ++m_stepIndex;
    emit readProgress(done, m_steps.size(), label);
    if (m_activity != Activity::Reading)
        return;  // cancelled from the progress slot
    if (m_stepIndex < m_steps.size())
        requestStep();
    else
        finishRead(false);
}

void X50Link::onReadNak(const QByteArray &msg)
{
    const auto a = parseAckNak(msg, m_channel);
    stepFailed(a.ok ? QStringLiteral("the X50 refused the request: %1").arg(nakCodeText(a.value.errorCode))
                    : QStringLiteral("NAK reply refused: %1").arg(a.error),
               msg);
}

void X50Link::onUnexpectedDump(const QByteArray &msg, MessageKind got)
{
    // Not the answer to this request. A well formed late dump of an earlier request is ignored
    // (the step's own timeout decides); a malformed one fails the step, with its bytes.
    const ReadStep &step = m_steps.at(m_stepIndex);
    QString error;
    QByteArray raw = msg;
    if (got == MessageKind::ProgramDump) {
        const auto d = parseProgramDump(msg, m_channel);
        if (!d.ok) { error = d.error; raw = d.raw; }
    } else if (got == MessageKind::CombinationDump) {
        const auto d = parseCombinationDump(msg, m_channel);
        if (!d.ok) { error = d.error; raw = d.raw; }
    } else {
        const auto d = parseGlobalDump(msg, m_channel);
        if (!d.ok) { error = d.error; raw = d.raw; }
    }
    if (!error.isEmpty()) {
        stepFailed(QStringLiteral("unexpected reply is malformed: %1").arg(error), raw);
        return;
    }
    qDebug().noquote() << "X50Link: ignored a dump of another kind while reading" << step.label;
}

void X50Link::onDump(const QByteArray &msg)
{
    const ReadStep &step = m_steps.at(m_stepIndex);
    const MessageKind got = classify(msg).kind;
    const MessageKind want = step.type == ReadStep::Type::Program       ? MessageKind::ProgramDump
                             : step.type == ReadStep::Type::Combination ? MessageKind::CombinationDump
                                                                        : MessageKind::GlobalDump;
    if (got != want) {
        onUnexpectedDump(msg, got);
        return;
    }
    m_rawDumps.append({step.label, msg});

    if (step.type == ReadStep::Type::Global) {
        const auto g = parseGlobalDump(msg, m_channel);
        if (!g.ok) {
            stepFailed(g.error, g.raw);
            return;
        }
        const auto cats = parseGlobalCategories(g.value.data);
        if (!cats.ok) {
            stepFailed(cats.error, msg);
            return;
        }
        m_categories = cats.value;
        m_haveCategories = true;
        m_stepRecords[m_stepIndex] = QVector<SoundRecord>();
        stepDone();
        return;
    }

    const bool combi = step.type == ReadStep::Type::Combination;
    const auto d = combi ? parseCombinationDump(msg, m_channel) : parseProgramDump(msg, m_channel);
    if (!d.ok) {
        stepFailed(d.error, d.raw);
        return;
    }
    if (d.value.kind != 1 || d.value.bank != step.bank) {
        // Well formed but for another bank: ignore, do not fail the step.
        qDebug().noquote() << "X50Link: ignored a dump for kind" << d.value.kind << "bank"
                           << d.value.bank << "while reading" << step.label;
        m_rawDumps.removeLast();  // not this step's dump
        return;
    }
    const auto recs = combi ? sliceCombinationRecords(d.value.data) : sliceProgramRecords(d.value.data);
    if (!recs.ok) {
        stepFailed(recs.error, msg);
        return;
    }
    if (recs.value.size() != kRecordsPerBank) {
        stepFailed(QStringLiteral("bank holds %1 records, expected %2")
                       .arg(recs.value.size()).arg(kRecordsPerBank),
                   msg);
        return;
    }
    m_stepRecords[m_stepIndex] = recs.value;
    stepDone();
}

void X50Link::finishRead(bool failedStop)
{
    m_timer.stop();
    setActivity(Activity::Idle);
    if (failedStop) {
        emit readFailed(m_firstFailBank, m_firstFailReason, m_firstFailRaw);
        return;
    }
    ReadResult &res = m_readResult;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    res.haveCategories = m_haveCategories;
    if (m_haveCategories) {
        for (const NameField &f : m_categories.programCategories)
            res.programCategories << f.text;
        for (const NameField &f : m_categories.combinationCategories)
            res.combinationCategories << f.text;
    }
    for (int i = 0; i < m_steps.size(); ++i) {
        const ReadStep &s = m_steps.at(i);
        const QVector<SoundRecord> &recs = m_stepRecords.at(i);
        if (s.type == ReadStep::Type::Global || recs.isEmpty())
            continue;
        const bool combi = s.type == ReadStep::Type::Combination;
        for (int n = 0; n < recs.size(); ++n) {
            const SoundRecord &r = recs.at(n);
            Catalogue::UnitName u;
            u.name = r.name.text;
            if (m_haveCategories && r.categoryValid && r.category >= 0 && r.category < 16)
                u.category = combi ? res.combinationCategories.at(r.category)
                                   : res.programCategories.at(r.category);
            u.readAt = now;
            res.names.insert(SoundAddress(combi ? SoundAddress::Kind::Combi : SoundAddress::Kind::Prog,
                                          kBanks[s.bank], n),
                             u);
        }
    }
    res.complete = res.errors.isEmpty();
    emit readFinished(res);
}
