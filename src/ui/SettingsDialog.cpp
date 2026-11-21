#include "ui/SettingsDialog.h"

#include "midi/MidiBackend.h"
#include "ui/Glyphs.h"
#include "ui/Theme.h"

#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

namespace {
constexpr int kDialogWidth = 520, kDialogMinHeight = 560;
constexpr int kControlHeight = 40, kLabelColumn = 96;
constexpr int kChecklistWidth = kDialogWidth - 2 * Theme::s4 - 2 * Theme::s4;  // card padding inside the margin

// app.qss gives min-height/max-height as the content box; the 1 px border adds 2 px (see BrowseView).
void fixHeight(QWidget *w, const QString &selector, int total)
{
    const int content = total - 2 * Theme::borderControl;
    w->setStyleSheet(QStringLiteral("%1 { min-height: %2px; max-height: %2px; }").arg(selector).arg(content));
}

// A chevron pointing down (`up` false) or up, painted, into the square `box`.
void paintVerticalChevron(QPainter &p, const QRectF &box, bool up, const QColor &color)
{
    p.save();
    p.translate(box.center());
    p.rotate(up ? -90 : 90);
    p.translate(-box.center());
    Glyphs::chevron(p, box, false, color, 2.0);
    p.restore();
}

QColor arrowColor(const QWidget *w)
{
    return QColor::fromRgba(w->isEnabled() ? Theme::textDim : Theme::disabledText);
}

// The drop-down arrow is a painted chevron (DESIGN 8): QSS removes the native arrow.
class ChevronCombo : public QComboBox
{
public:
    using QComboBox::QComboBox;
protected:
    void paintEvent(QPaintEvent *e) override
    {
        QComboBox::paintEvent(e);
        QPainter p(this);
        const QRectF box(width() - 28 + 6, (height() - 14) / 2.0, 14, 14);
        paintVerticalChevron(p, box, false, arrowColor(this));
    }
};

class ChevronSpin : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void paintEvent(QPaintEvent *e) override
    {
        QSpinBox::paintEvent(e);
        QPainter p(this);
        const qreal cx = width() - 1 - 12;
        const QColor col = arrowColor(this);
        paintVerticalChevron(p, QRectF(cx - 5, height() * 0.25 - 5, 10, 10), true, col);
        paintVerticalChevron(p, QRectF(cx - 5, height() * 0.75 - 5, 10, 10), false, col);
    }
};

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text.toUpper(), parent);
    l->setProperty("role", QStringLiteral("sectionTitle"));
    return l;
}

QPushButton *makeButton(const QString &text, int width, bool primary, QWidget *parent)
{
    auto *b = new QPushButton(text, parent);
    b->setMinimumWidth(width);
    b->setProperty("role", primary ? QStringLiteral("primary") : QStringLiteral("secondary"));
    fixHeight(b, QStringLiteral("QPushButton"), kControlHeight);
    return b;
}

QString bankMapName(BankMap m) { return m == BankMap::Gm ? QStringLiteral("GM") : QStringLiteral("KORG"); }

QString hex(const QByteArray &raw) { return QString::fromLatin1(raw.toHex(' ').toUpper()); }

void selectPort(QComboBox *combo, const QString &saved, const QString &connected)
{
    QStringList names;
    for (int i = 0; i < combo->count(); ++i)
        names << combo->itemText(i);
    int idx = X50Link::findPortIndex(names, saved);
    if (idx < 0)
        idx = X50Link::findPortIndex(names, connected);
    if (idx < 0)
        idx = X50Link::findPortIndex(names, QStringLiteral("X50"));
    combo->setCurrentIndex(qMax(0, idx));
}
} // namespace

QVector<ProbeChecklist::Row> SettingsDialog::runningRows()
{
    using R = ProbeChecklist::Row;
    const char *names[] = {"X50 found", "Program Change", "Bank Change", "Combination Change", "Bank Map", "SysEx replies"};
    QVector<R> rows;
    for (const char *n : names)
        rows.append({ProbeChecklist::State::Running, QString::fromLatin1(n), QString()});
    return rows;
}

QVector<ProbeChecklist::Row> SettingsDialog::rowsFromProbe(const X50Link::ProbeResult &result, BankMap effective)
{
    using S = ProbeChecklist::State;
    QVector<ProbeChecklist::Row> rows;
    for (const X50Link::CheckItem &item : result.items) {
        ProbeChecklist::Row r;
        r.text = item.text;
        r.hint = item.fix;
        switch (item.status) {
        case X50Link::CheckItem::Status::Ok: r.state = S::Pass; break;
        case X50Link::CheckItem::Status::Warning: r.state = S::Unknown; break;
        case X50Link::CheckItem::Status::Failed: r.state = S::Fail; break;
        }
        if (!item.raw.isEmpty()) {
            if (!r.hint.isEmpty())
                r.hint += QLatin1Char(' ');
            r.hint += QStringLiteral("Raw bytes: %1").arg(hex(item.raw));
        }
        rows.append(r);
    }
    if (result.x50Confirmed && !result.state && !result.microX) {
        // The Device Inquiry answered but Mode and State did not: the Bank Map is unknown.
        rows.append({S::Unknown, QStringLiteral("Bank Map: not reported"),
                     QStringLiteral("The X50 did not say which Bank Map it uses. Sending as %1; change Bank Map above if banks come out wrong.")
                         .arg(bankMapName(effective))});
    }
    return rows;
}

SettingsDialog::SettingsDialog(X50Link *link, MidiBackend *backend, const QString &configDir, QWidget *parent)
    : QDialog(parent)
    , m_link(link)
    , m_backend(backend)
    , m_configDir(configDir)
{
    Theme::loadFonts();
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(QStringLiteral("Settings"));
    setFixedWidth(kDialogWidth);
    setMinimumHeight(kDialogMinHeight);
    setModal(true);
    m_settings = ConnectionSettings::load(configDir);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(Theme::s4, Theme::s4, Theme::s4, Theme::s4);
    outer->setSpacing(Theme::s4);

    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("settingsCard"));
    auto *cl = new QVBoxLayout(card);
    cl->setContentsMargins(Theme::s4, Theme::s4, Theme::s4, Theme::s4);
    cl->setSpacing(Theme::s4);

    // ---- MIDI PORTS
    cl->addWidget(sectionTitle(QStringLiteral("MIDI ports"), card));
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(Theme::s3);
    grid->setVerticalSpacing(Theme::s2);
    grid->setColumnMinimumWidth(0, kLabelColumn - Theme::s3);
    grid->setColumnStretch(1, 1);
    m_output = new ChevronCombo(card);
    m_input = new ChevronCombo(card);
    for (QComboBox *c : {m_output, m_input}) {
        fixHeight(c, QStringLiteral("QComboBox"), kControlHeight);
        c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        c->setMinimumContentsLength(10);
    }
    m_output->setObjectName(QStringLiteral("outputPort"));
    m_input->setObjectName(QStringLiteral("inputPort"));
    m_test = makeButton(QStringLiteral("Test"), 80, false, card);
    m_test->setToolTip(QStringLiteral("Open the chosen ports and ask the X50 who it is (sends nothing that changes a sound)"));
    grid->addWidget(new QLabel(QStringLiteral("Output"), card), 0, 0);
    grid->addWidget(m_output, 0, 1);
    grid->addWidget(m_test, 0, 2);
    grid->addWidget(new QLabel(QStringLiteral("Input"), card), 1, 0);
    grid->addWidget(m_input, 1, 1, 1, 2);
    cl->addLayout(grid);

    // ---- IF THE X50 DOES NOT ANSWER
    cl->addWidget(sectionTitle(QStringLiteral("If the X50 does not answer"), card));
    auto *manual = new QHBoxLayout;
    manual->setSpacing(Theme::s3);
    m_channel = new ChevronSpin(card);
    m_channel->setRange(1, 16);
    m_channel->setObjectName(QStringLiteral("manualChannel"));
    m_channel->setFixedWidth(96);
    fixHeight(m_channel, QStringLiteral("QSpinBox"), kControlHeight);
    m_bankMap = new ChevronCombo(card);
    m_bankMap->setObjectName(QStringLiteral("manualBankMap"));
    m_bankMap->addItem(QStringLiteral("GM"));
    m_bankMap->addItem(QStringLiteral("KORG"));
    m_bankMap->setFixedWidth(160);
    fixHeight(m_bankMap, QStringLiteral("QComboBox"), kControlHeight);
    auto *chLabel = new QLabel(QStringLiteral("Channel"), card);
    chLabel->setMinimumWidth(kLabelColumn - Theme::s3);
    manual->addWidget(chLabel);
    manual->addWidget(m_channel);
    manual->addSpacing(Theme::s3);
    manual->addWidget(new QLabel(QStringLiteral("Bank Map"), card));
    manual->addWidget(m_bankMap);
    manual->addStretch(1);
    cl->addLayout(manual);
    auto *manualNote = new QLabel(QStringLiteral("Used only when the X50 gives no answer."), card);
    manualNote->setProperty("role", QStringLiteral("small"));
    cl->addWidget(manualNote);

    // ---- X50 CHECK
    cl->addWidget(sectionTitle(QStringLiteral("X50 check"), card));
    auto *checkRow = new QHBoxLayout;
    checkRow->setSpacing(Theme::s4);
    m_check = makeButton(QStringLiteral("Check X50"), 140, true, card);
    m_statusWord = new QLabel(QStringLiteral("Not checked yet"), card);
    m_statusWord->setProperty("role", QStringLiteral("small"));
    checkRow->addWidget(m_check);
    checkRow->addWidget(m_statusWord, 1);
    cl->addLayout(checkRow);
    m_note = new QLabel(card);
    m_note->setProperty("role", QStringLiteral("small"));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    m_note->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QColor::fromRgba(Theme::red).name()));
    m_note->hide();
    cl->addWidget(m_note);

    m_scroll = new QScrollArea(card);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    m_list = new ProbeChecklist;
    m_scroll->setWidget(m_list);
    cl->addWidget(m_scroll);
    cl->addStretch(1);
    outer->addWidget(card, 1);

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    m_close = makeButton(QStringLiteral("Close"), 120, true, this);
    m_close->setDefault(false);
    m_close->setAutoDefault(false);
    bottom->addWidget(m_close);
    outer->addLayout(bottom);

    // ---- values
    m_building = true;
    m_channel->setValue(m_settings.channel);
    m_bankMap->setCurrentIndex(m_settings.bankMap == BankMap::Korg ? 1 : 0);
    m_building = false;
    refreshPorts();

    connect(m_close, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_test, &QPushButton::clicked, this, [this]() { startTest(); });
    connect(m_check, &QPushButton::clicked, this, [this]() { startCheck(); });
    connect(m_output, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_building)
            return;
        // The same device is usually listed in both directions: follow the output.
        QStringList ins;
        for (int i = 0; i < m_input->count(); ++i)
            ins << m_input->itemText(i);
        const int idx = X50Link::findPortIndex(ins, m_output->currentText());
        if (idx >= 0) {
            m_building = true;
            m_input->setCurrentIndex(idx);
            m_building = false;
        }
        applyPorts();
    });
    connect(m_input, &QComboBox::currentIndexChanged, this, [this]() {
        if (!m_building)
            applyPorts();
    });
    connect(m_channel, &QSpinBox::valueChanged, this, [this]() { onManualChanged(); });
    connect(m_bankMap, &QComboBox::currentIndexChanged, this, [this]() { onManualChanged(); });
    if (m_link) {
        connect(m_link, &X50Link::stateChanged, this, [this]() { onLinkChanged(); });
        connect(m_link, &X50Link::probeFinished, this, [this]() { onLinkChanged(); });
        connect(m_link, &X50Link::errorOccurred, this, [this](const QString &message, const QByteArray &) {
            if (m_link->state() == X50Link::State::Disconnected)
                setNote(message);
        });
    }
    onLinkChanged();
    fitToContent();
}

QString SettingsDialog::note() const { return m_note->isHidden() ? QString() : m_note->text(); }

bool SettingsDialog::manualEnabled() const { return m_channel->isEnabled() && m_bankMap->isEnabled(); }

void SettingsDialog::setNote(const QString &text)
{
    if (m_note->text() == text && m_note->isHidden() == text.isEmpty())
        return;
    m_note->setText(text);
    m_note->setVisible(!text.isEmpty());
    fitToContent();
}

void SettingsDialog::refreshPorts()
{
    const MidiBackend::Ports ports = m_backend ? m_backend->ports() : MidiBackend::Ports();
    const QString connected = m_link ? m_link->connectedPortName() : QString();
    const QString wantOut = m_output->count() > 0 ? m_output->currentText() : m_settings.outputPort;
    const QString wantIn = m_input->count() > 0 ? m_input->currentText() : m_settings.inputPort;
    m_building = true;
    m_output->clear();
    m_input->clear();
    m_output->addItems(ports.outputs);
    m_input->addItems(ports.inputs);
    selectPort(m_output, wantOut, connected);
    selectPort(m_input, wantIn, connected);
    m_building = false;
    m_hasPorts = !ports.outputs.isEmpty() && !ports.inputs.isEmpty();
    if (!ports.error.isEmpty())
        setNote(QStringLiteral("MIDI is unavailable: %1").arg(ports.error));
    else if (!m_hasPorts)
        setNote(QStringLiteral("No MIDI ports found. Is the X50 on and the USB cable in?"));
    else if (m_note->text().startsWith(QLatin1String("No MIDI ports")) || m_note->text().startsWith(QLatin1String("MIDI is unavailable")))
        setNote(QString());
    updateEnabled();
}

void SettingsDialog::saveSettings()
{
    m_settings.outputPort = m_output->count() ? m_output->currentText() : m_settings.outputPort;
    m_settings.inputPort = m_input->count() ? m_input->currentText() : m_settings.inputPort;
    m_settings.channel = m_channel->value();
    m_settings.bankMap = m_bankMap->currentIndex() == 1 ? BankMap::Korg : BankMap::Gm;
    m_settings.save(m_configDir);
}

// Port choices take effect at the next (re)connect: this only tells the link and saves them.
void SettingsDialog::applyPorts()
{
    saveSettings();
    if (m_link && m_hasPorts)
        m_link->setPortNames(m_settings.outputPort, m_settings.inputPort);
}

void SettingsDialog::onManualChanged()
{
    if (m_building)
        return;
    saveSettings();
    if (!m_link)
        return;
    m_link->setManualChannel(m_settings.channel);
    m_link->setManualBankMap(m_settings.bankMap);
}

void SettingsDialog::startTest()
{
    if (!m_link)
        return;
    refreshPorts();
    if (!m_hasPorts)
        return;
    applyPorts();
    setNote(QString());
    m_checkedThisSession = true;
    m_link->stop();   // close the ports and forget what was learned ...
    m_link->start();  // ... open the chosen ones; connecting runs the probe
    if (m_link->state() == X50Link::State::Disconnected)
        setNote(m_link->statusText());
    onLinkChanged();
}

void SettingsDialog::startCheck()
{
    if (!m_link)
        return;
    setNote(QString());
    m_checkedThisSession = true;
    if (m_link->state() == X50Link::State::Disconnected) {
        refreshPorts();
        if (!m_hasPorts)
            return;
        applyPorts();
        m_link->start();
        if (m_link->state() == X50Link::State::Disconnected)
            setNote(m_link->statusText());
    } else {
        m_link->probe();
    }
    onLinkChanged();
}

void SettingsDialog::onLinkChanged()
{
    if (!m_link)
        return;
    const bool probing = m_link->state() == X50Link::State::Probing;
    if (probing) {
        m_list->setRows(runningRows());
        m_statusWord->setText(QStringLiteral("Checking..."));
        setNote(QString());
    } else {
        const X50Link::ProbeResult &r = m_link->lastProbe();
        if (r.items.isEmpty()) {
            m_list->setRows({});
        } else {
            m_list->setRows(rowsFromProbe(r, m_link->bankMap()));
            if (m_checkedThisSession)
                m_statusWord->setText(QStringLiteral("Last check: %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm"))));
            else if (m_statusWord->text() == QLatin1String("Not checked yet") || m_statusWord->text() == QLatin1String("Checking..."))
                m_statusWord->setText(QStringLiteral("Last check: earlier"));
        }
        if (m_link->state() == X50Link::State::Connected || m_link->state() == X50Link::State::Manual)
            setNote(QString());
    }
    updateEnabled();
    fitToContent();
}

void SettingsDialog::updateEnabled()
{
    const bool probing = m_link && m_link->state() == X50Link::State::Probing;
    m_test->setEnabled(m_link && m_hasPorts && !probing);
    m_check->setEnabled(m_link && !probing);
    // The manual values are for a probe that could not read them (SPEC 4.5): a probe has run,
    // it learned no Mode and State, and the unit is not a microX.
    bool manual = false;
    if (m_link && !probing) {
        const X50Link::ProbeResult &r = m_link->lastProbe();
        manual = !r.items.isEmpty() && !r.state.has_value() && !r.microX;
    }
    m_channel->setEnabled(manual);
    m_bankMap->setEnabled(manual);
}

void SettingsDialog::setHeightLimit(int pixels)
{
    m_heightLimit = pixels;
    fitToContent();
}

void SettingsDialog::fitToContent()
{
    // The checklist scrolls inside when the dialog would pass 80% of the screen (DESIGN 8).
    const QScreen *screen = QGuiApplication::primaryScreen();
    const int limit = m_heightLimit > 0 ? m_heightLimit
                                        : static_cast<int>((screen ? screen->availableGeometry().height() : 1080) * 0.8);
    const int want = m_list->heightForWidth(kChecklistWidth);
    const int fixedAbove = 460 + (m_note->isHidden() ? 0 : m_note->sizeHint().height() + Theme::s4);
    const int room = qMax(2 * 32 + 8, qMax(limit, kDialogMinHeight) - fixedAbove);
    m_scroll->setFixedHeight(qMin(want, room));
    // Not adjustSize(): it caps a window at two thirds of the screen.
    // Invalidation reaches the dialog's layout only through queued events: do it by hand.
    for (QWidget *w = m_scroll->parentWidget(); w; w = w->parentWidget()) {
        if (w->layout())
            w->layout()->invalidate();
        w->updateGeometry();  // clears the parent layout's cached size of this widget
        if (w == this)
            break;
    }
    layout()->activate();
    resize(kDialogWidth, qMax(kDialogMinHeight, layout()->sizeHint().height()));
}
