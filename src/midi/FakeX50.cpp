#include "midi/FakeX50.h"

#include <QMetaObject>
#include <QTimer>

using namespace X50Protocol;

namespace {

void putName(QByteArray &image, int offset, const QString &name)
{
    for (int i = 0; i < kNameLength; ++i) {
        char c = ' ';
        if (i < name.size()) {
            const ushort u = name.at(i).unicode();
            c = (u >= 0x20 && u < 0x7F) ? static_cast<char>(u) : '?';
        }
        image[offset + i] = c;
    }
}

}  // namespace

FakeX50::FakeX50(QObject *parent) : MidiBackend(parent)
{
    const QChar letters[] = {QLatin1Char('A'), QLatin1Char('B'), QLatin1Char('C'), QLatin1Char('D')};
    for (int b = 0; b < 4; ++b) {
        m_programs[b] = QByteArray(kRecordsPerBank * kProgramRecordSize, '\0');
        for (int n = 0; n < kRecordsPerBank; ++n) {
            setProgramName(b, n, QStringLiteral("Prog %1%2").arg(letters[b]).arg(n, 3, 10, QLatin1Char('0')));
            setProgramCategory(b, n, n % 16);
        }
    }
    for (int b = 0; b < 3; ++b) {
        m_combinations[b] = QByteArray(kRecordsPerBank * kCombinationRecordSize, '\0');
        for (int n = 0; n < kRecordsPerBank; ++n) {
            setCombinationName(b, n, QStringLiteral("Combi %1%2").arg(letters[b]).arg(n, 3, 10, QLatin1Char('0')));
            setCombinationCategory(b, n, n % 16);
        }
    }
    m_global = QByteArray(850, '\0');  // IMP TABLE 4 goes to offset 849
    QStringList p, c;
    for (int i = 0; i < 16; ++i) {
        p << QStringLiteral("PCat%1").arg(i);
        c << QStringLiteral("CCat%1").arg(i);
    }
    setCategoryNames(p, c);
}

// ------------------------------------------------------------- MidiBackend

MidiBackend::Ports FakeX50::ports()
{
    ++m_portsCalls;
    Ports p;
    if (m_present) {
        p.outputs << m_portName;
        p.inputs << m_portName;
    }
    return p;
}

MidiBackend::Status FakeX50::openOutput(int index)
{
    if (!m_present || index != 0)
        return Status::failure(QStringLiteral("output port %1 does not exist").arg(index));
    if (m_busy)
        return Status::failure(portBusyText(), QStringLiteral("fake: busy"));
    m_outOpen = true;
    ++m_generation;
    return Status::success();
}

MidiBackend::Status FakeX50::openInput(int index)
{
    if (!m_present || index != 0)
        return Status::failure(QStringLiteral("input port %1 does not exist").arg(index));
    if (m_busy)
        return Status::failure(portBusyText(), QStringLiteral("fake: busy"));
    m_inOpen = true;
    m_assembler.reset();
    ++m_generation;
    return Status::success();
}

void FakeX50::closePorts()
{
    m_outOpen = false;
    m_inOpen = false;
    m_assembler.reset();
    ++m_generation;
}

void FakeX50::unplug(bool raisePortError)
{
    m_present = false;
    if (!raisePortError)
        return;
    const bool wasOpen = m_outOpen || m_inOpen;
    closePorts();
    if (wasOpen) {
        QMetaObject::invokeMethod(
            this, [this]() { emit portError(QStringLiteral("port disconnected")); },
            Qt::QueuedConnection);
    }
}

MidiBackend::Status FakeX50::send(const QByteArray &message)
{
    if (!m_outOpen)
        return Status::failure(QStringLiteral("MIDI output is not open"));
    if (m_failSend)
        return Status::failure(QStringLiteral("could not send to the X50"), QStringLiteral("fake: write failed"));
    m_received.append(message);
    const unsigned int gen = m_generation;
    QMetaObject::invokeMethod(
        this, [this, message, gen]() { if (gen == m_generation) handle(message); },
        Qt::QueuedConnection);
    return Status::success();
}

// ---------------------------------------------------------------- config

void FakeX50::setVersion(int major, int minor, int release, int build)
{
    m_major = major; m_minor = minor; m_release = release; m_build = build;
}

void FakeX50::setEnables(bool programChange, bool bankChange, bool combinationChange)
{
    m_enablePc = programChange;
    m_enableBank = bankChange;
    m_enableCombi = combinationChange;
}

void FakeX50::setDrop(Reply which, bool drop)
{
    switch (which) {
    case Reply::DeviceInquiry: m_dropInquiry = drop; break;
    case Reply::ModeAndState: m_dropState = drop; break;
    case Reply::Ack: m_dropAck = drop; break;
    case Reply::Dump: m_dropDump = drop; break;
    }
}

void FakeX50::setProgramName(int bank, int number, const QString &name)
{
    putName(m_programs[bank], number * kProgramRecordSize, name);
}
void FakeX50::setProgramCategory(int bank, int number, int category)
{
    m_programs[bank][number * kProgramRecordSize + kProgramCategoryOffset] = static_cast<char>(category);
}
void FakeX50::setCombinationName(int bank, int number, const QString &name)
{
    putName(m_combinations[bank], number * kCombinationRecordSize, name);
}
void FakeX50::setCombinationCategory(int bank, int number, int category)
{
    m_combinations[bank][number * kCombinationRecordSize + kCombinationCategoryOffset] =
        static_cast<char>(category & 0x0F);
}
void FakeX50::setCategoryNames(const QStringList &program, const QStringList &combination)
{
    for (int i = 0; i < 16; ++i) {
        putName(m_global, kGlobalProgramCategoryNames + i * kNameLength,
                i < program.size() ? program.at(i) : QString());
        putName(m_global, kGlobalCombinationCategoryNames + i * kNameLength,
                i < combination.size() ? combination.at(i) : QString());
    }
}

SoundAddress FakeX50::currentSound() const
{
    switch (m_mode) {
    case Mode::CombiPlay:
    case Mode::CombiEdit:
        return m_combination;
    default:
        return m_program;
    }
}

// ----------------------------------------------------------------- panel

void FakeX50::panelChangeMode(Mode mode)
{
    m_mode = mode;
    const auto n = buildModeChangeNotice(m_channel, mode);
    if (n.ok)
        queueDeliver(n.value);
}

void FakeX50::panelSelect(const SoundAddress &address)
{
    if (address.kind() == SoundAddress::Kind::Combi) {
        m_combination = address;
    } else {
        m_program = address;
        if (address.bank() != SoundAddress::Bank::G && address.bank() != SoundAddress::Bank::Gd) {
            m_reportBank = static_cast<int>(address.bank());
            m_reportNumber = address.number();
        }
    }
    const auto msgs = buildPanelSelect(address, m_bankMap, m_channel);
    if (!msgs.ok)
        return;
    for (const QByteArray &b : msgs.value)
        queueDeliver(b);
}

// ---------------------------------------------------------------- X50 side

void FakeX50::queueDeliver(const QByteArray &bytes)
{
    const unsigned int gen = m_generation;
    QMetaObject::invokeMethod(
        this, [this, bytes, gen]() { if (gen == m_generation) deliver(bytes); },
        Qt::QueuedConnection);
}

void FakeX50::deliver(const QByteArray &bytes)
{
    if (!m_inOpen)
        return;
    if (m_fragmentSize <= 0 || bytes.size() <= m_fragmentSize || bytes.at(0) != static_cast<char>(0xF0)) {
        emit messageReceived(bytes);
        return;
    }
    // WinMM style: chunks, reassembled before the listener sees them.
    for (qsizetype pos = 0; pos < bytes.size(); pos += m_fragmentSize) {
        const QList<QByteArray> msgs = m_assembler.feed(bytes.mid(pos, m_fragmentSize));
        for (const QByteArray &m : msgs)
            emit messageReceived(m);
    }
}

void FakeX50::reply(Reply kind, const QByteArray &bytes)
{
    if (m_dropAll)
        return;
    const bool dropped = (kind == Reply::DeviceInquiry && m_dropInquiry) ||
                         (kind == Reply::ModeAndState && m_dropState) ||
                         (kind == Reply::Ack && m_dropAck) || (kind == Reply::Dump && m_dropDump);
    if (dropped)
        return;
    if (m_replyDelayMs > 0) {
        const unsigned int gen = m_generation;
        QTimer::singleShot(m_replyDelayMs, this, [this, bytes, gen]() { if (gen == m_generation) deliver(bytes); });
    } else {
        deliver(bytes);
    }
}

void FakeX50::replyNak(int code)
{
    const auto n = buildNak(m_channel, code);
    if (n.ok)
        reply(Reply::Ack, n.value);  // a NAK is the answer where an ACK would be
}

QByteArray FakeX50::buildModeAndStateReply() const
{
    ModeAndState s;
    s.channel = m_channel;
    s.mode = m_mode;
    s.bankMap = m_bankMap;
    s.localControl = true;
    s.enableProgramChange = m_enablePc;
    s.enableBankChange = m_enableBank;
    s.enableCombinationChange = m_enableCombi;
    s.programBank = m_reportedProgramBank ? *m_reportedProgramBank : m_reportBank;
    s.programNumber = m_reportNumber;
    s.combinationBank = static_cast<int>(m_combination.bank());
    s.combinationNumber = m_combination.number();
    return buildModeAndState(s).value;
}

QByteArray FakeX50::breakDump(QByteArray msg) const
{
    if (m_malformation == Malformation::StatusByteInData && msg.size() > 20)
        msg[20] = static_cast<char>(0x90);
    else if (m_malformation == Malformation::NoEnd)
        msg.chop(1);
    return msg;
}

void FakeX50::replyDump(int function, int bank)
{
    if (m_nakDumps) {
        replyNak(*m_nakDumps);
        return;
    }
    const bool combi = function == 0x1D;
    QByteArray data = combi ? m_combinations[bank] : m_programs[bank];
    const int recSize = combi ? kCombinationRecordSize : kProgramRecordSize;
    int sendBank = bank;
    if (m_malformation == Malformation::FewerRecords)
        data = data.left(recSize * (kRecordsPerBank - 1));
    else if (m_malformation == Malformation::OddLength)
        data = data.left(recSize * kRecordsPerBank - 5);
    else if (m_malformation == Malformation::WrongBank)
        sendBank = (bank + 1) % 3;
    const auto msg = combi ? buildCombinationDump(m_channel, sendBank, data)
                           : buildProgramDump(m_channel, sendBank, data);
    if (msg.ok)
        reply(Reply::Dump, breakDump(msg.value));
}

void FakeX50::replyGlobal()
{
    if (m_nakDumps) {
        replyNak(*m_nakDumps);
        return;
    }
    QByteArray data = m_global;
    if (m_malformation == Malformation::FewerRecords || m_malformation == Malformation::OddLength)
        data = data.left(100);
    const auto msg = buildGlobalDump(m_channel, data);
    if (msg.ok)
        reply(Reply::Dump, breakDump(msg.value));
}

void FakeX50::handle(const QByteArray &msg)
{
    if (!m_present)
        return;
    if (msg == deviceInquiryRequest()) {
        DeviceInquiryReply r;
        r.channel = m_channel;
        r.memberCode = m_memberOverride ? m_memberCode : (m_microX ? 0x09 : 0x01);
        r.major = m_major; r.minor = m_minor; r.release = m_release; r.build = m_build;
        reply(Reply::DeviceInquiry, buildDeviceInquiryReply(r));
        return;
    }
    const Classification c = classify(msg);
    if (c.kind == MessageKind::ControlChange || c.kind == MessageKind::ProgramChange) {
        const auto cm = parseChannelMessage(msg, m_channel);
        if (cm.ok)
            handleChannelMessage(cm.value);
        return;
    }
    if (c.channel != m_channel)
        return;  // another global channel: the X50 does not answer
    if (msg == modeAndStateRequest(m_channel).value) {
        reply(Reply::ModeAndState, buildModeAndStateReply());
        return;
    }
    const auto mc = parseModeChange(msg, m_channel);
    if (mc.ok) {
        if (m_nakModeChange) {
            replyNak(*m_nakModeChange);
            return;
        }
        m_mode = mc.value.mode;
        const auto ack = buildAck(m_channel);
        reply(Reply::Ack, ack.value);
        return;
    }
    for (int b = 0; b < 4; ++b) {
        if (msg == programBankDumpRequest(m_channel, b).value) {
            replyDump(0x1C, b);
            return;
        }
    }
    for (int b = 0; b < 3; ++b) {
        if (msg == combinationBankDumpRequest(m_channel, b).value) {
            replyDump(0x1D, b);
            return;
        }
    }
    if (msg == globalDumpRequest(m_channel).value)
        replyGlobal();
    // Anything else (parameter changes, writes) is not part of this fake.
}

void FakeX50::handleChannelMessage(const ChannelMessage &m)
{
    if (!m.isProgramChange) {
        if (!m_enableBank)
            return;  // MIDI Filter "Bank" off: bank select ignored (MIDI.md section 3)
        if (m.controller == 0x00)
            m_msb = m.value;
        else if (m.controller == 0x20)
            m_lsb = m.value;
        return;
    }
    if (!m_enablePc)
        return;
    const bool combi = m_mode == Mode::CombiPlay;
    if (combi && !m_enableCombi)
        return;  // would change a timbre's program instead
    if (m_mode != Mode::CombiPlay && m_mode != Mode::ProgPlay)
        return;
    const SoundAddress::Kind kind = combi ? SoundAddress::Kind::Combi : SoundAddress::Kind::Prog;
    const SoundAddress cur = combi ? m_combination : m_program;
    int msb, lsb;
    if (m_msb && m_lsb) {
        msb = *m_msb;
        lsb = *m_lsb;
    } else {
        const auto bs = SoundAddress(kind, cur.bank(), 0).bankSelect(m_bankMap);
        msb = bs ? bs->msb : 0;
        lsb = bs ? bs->lsb : 0;
    }
    const auto a = addressFromBankSelect(msb, lsb, m.value, m_bankMap, kind);
    if (!a)
        return;
    if (combi) {
        m_combination = *a;
    } else {
        m_program = *a;
        if (a->bank() != SoundAddress::Bank::G && a->bank() != SoundAddress::Bank::Gd) {
            m_reportBank = static_cast<int>(a->bank());
            m_reportNumber = a->number();
        }
    }
}
