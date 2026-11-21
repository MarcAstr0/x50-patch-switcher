#include "ui/DisplayStrip.h"

#include "core/Catalogue.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QPainter>
#include <QPaintEvent>
#include <QStringList>
#include <QTextOption>
#include <QtMath>

bool DisplayStrip::Texts::operator==(const Texts &o) const
{
    return known == o.known && mode == o.mode && pageBold == o.pageBold && pageRegular == o.pageRegular
        && bank == o.bank && category == o.category && bar == o.bar;
}

DisplayStrip::DisplayStrip(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("displayStrip"));
    setAttribute(Qt::WA_OpaquePaintEvent);  // paintEvent fills the whole rect
    m_texts = compose(std::nullopt, nullptr);
    relayout();
}

int DisplayStrip::heightForScale(qreal dpr)
{
    const int k = Theme::pixelScale(dpr);
    return qCeil(double(Theme::stripHeightK) * k / dpr);
}

QSize DisplayStrip::sizeHint() const
{
    return QSize(qCeil(double(Theme::stripPanelWidthK) * m_k / m_dpr) + 240, heightForScale(m_dpr));
}

QSize DisplayStrip::minimumSizeHint() const
{
    return QSize(240, heightForScale(m_dpr));
}

DisplayStrip::Texts DisplayStrip::compose(const std::optional<SoundAddress> &sound, const Catalogue *catalogue)
{
    Texts t;
    if (!sound || !sound->isValid()) {
        t.known = false;
        t.mode = QStringLiteral("--");
        t.bar = QStringLiteral("--");
        return t;
    }
    t.known = true;
    const bool combi = sound->kind() == SoundAddress::Kind::Combi;
    t.mode = combi ? QStringLiteral("COMBI 0:Play") : QStringLiteral("PROG 0:Play");
    t.pageBold = combi ? QStringLiteral("Combination") : QStringLiteral("Program");
    t.pageRegular = combi ? QStringLiteral(":Combi Select") : QStringLiteral(":Program Select");

    // format() is "<KIND> <bank><3 digits>": the bank text and the number come from it.
    const QString f = sound->format();
    const QString addr = f.mid(f.indexOf(QLatin1Char(' ')) + 1);
    const QString number = addr.right(3);
    t.bank = QStringLiteral("Bank ") + addr.left(addr.size() - 3);

    QString name = catalogue ? catalogue->effectiveName(*sound) : QString();
    if (name.isEmpty())
        name = QStringLiteral("---");
    t.bar = number + QStringLiteral(": ") + name;

    const QString cat = catalogue ? catalogue->effectiveCategory(*sound) : QString();
    if (catalogue && catalogue->hasUnitCategories()) {
        // The unit's 16 names list the programs' and combinations' categories only; the
        // GM banks keep factory categories, so they have no index (DESIGN 7.2).
        const bool gm = sound->bank() == SoundAddress::Bank::G || sound->bank() == SoundAddress::Bank::Gd;
        const int idx = (gm || cat.isEmpty()) ? -1 : catalogue->categories(sound->kind()).indexOf(cat);
        const QString num = idx >= 0 ? QStringLiteral("%1").arg(idx, 2, 10, QLatin1Char('0')) : QStringLiteral("--");
        t.category = QStringLiteral("Category ") + num + QLatin1Char(':') + (cat.isEmpty() ? QStringLiteral("--") : cat);
    } else {
        t.category = QStringLiteral("Category:") + (cat.isEmpty() ? QStringLiteral("--") : cat);
    }
    return t;
}

void DisplayStrip::setCatalogue(const Catalogue *catalogue)
{
    m_catalogue = catalogue;
    refresh();
}

void DisplayStrip::setSound(const SoundAddress &address)
{
    m_sound = address;
    refresh();
}

void DisplayStrip::clearSound()
{
    m_sound.reset();
    refresh();
}

void DisplayStrip::refresh()
{
    setTexts(compose(m_sound, m_catalogue));
}

void DisplayStrip::setTexts(const Texts &t)
{
    if (t == m_texts)
        return;
    m_texts = t;
    relayout();
    update();
}

void DisplayStrip::setConnection(bool connected, const QString &message)
{
    const QString text = message.isEmpty() ? (connected ? QStringLiteral("X50 CONNECTED") : QStringLiteral("NO X50")) : message;
    if (connected == m_connected && text == m_connText && message == m_connMessage)
        return;
    m_connected = connected;
    m_connText = text;
    m_connMessage = message;
    relayout();
    update();
}

void DisplayStrip::syncScale()
{
    if (!qFuzzyCompare(devicePixelRatioF(), m_dpr)) {
        relayout();
        update();
    }
}

void DisplayStrip::relayout()
{
    m_dpr = devicePixelRatioF();
    m_k = Theme::pixelScale(m_dpr);
    const int k = m_k;
    const int h = heightForScale(m_dpr);
    if (h != m_lastHeight) {
        m_lastHeight = h;
        setFixedHeight(h);
        emit heightChanged(h);
    }
    m_regular16 = Theme::pixelFont(k, false);
    m_bold16 = Theme::pixelFont(k, true);
    m_regular32 = Theme::pixelFont(2 * k, false);
    m_label = Theme::roleFont(Theme::Role::Label);
    m_label.setPixelSize(Theme::stripLabelPx);
    m_label.setLetterSpacing(QFont::AbsoluteSpacing, Theme::stripLabelTracking);

    // ---- connection zone (logical px), right aligned, centred on the 74k strip ----
    const double cy = 37.0 * k / m_dpr;
    const double maxZone = width() * 0.4;
    const double ledBlock = Theme::stripLedDiameter + Theme::stripLedGap;
    const double availText = qMax(40.0, maxZone - ledBlock);
    QString shown = m_connText.toUpper();
    const QFontMetricsF lfm(m_label);
    const double lineH = lfm.lineSpacing();
    m_connStatic = QStaticText();
    m_connStatic.setTextFormat(Qt::PlainText);
    QTextOption opt;
    opt.setWrapMode(QTextOption::WordWrap);
    m_connStatic.setTextOption(opt);
    m_connStatic.setText(shown);
    m_connStatic.prepare(QTransform(), m_label);
    if (m_connStatic.size().width() > availText) {
        m_connStatic.setTextWidth(availText);
        // Up to two lines (DESIGN 7.2): shorten with an ellipsis until it fits.
        int guard = shown.size();
        while (m_connStatic.size().height() > 2 * lineH + 1 && shown.size() > 1 && guard-- > 0) {
            shown.chop(2);
            shown = shown.trimmed() + QChar(0x2026);
            m_connStatic.setText(shown);
            m_connStatic.prepare(QTransform(), m_label);
        }
    }
    const QSizeF ts = m_connStatic.size();
    const double zoneW = ledBlock + ts.width();
    const double zoneLeft = width() - zoneW;
    m_l.led = QRectF(zoneLeft, cy - Theme::stripLedDiameter / 2.0, Theme::stripLedDiameter, Theme::stripLedDiameter);
    m_l.connTextPos = QPointF(zoneLeft + ledBlock, cy - ts.height() / 2.0);

    // ---- LCD panel (device px, DESIGN 7.2); the panel is clipped first on a narrow window ----
    const int bez = Theme::stripBezelK * k;
    const int fullW = Theme::stripPanelWidthK * k;
    const int availPanel = qRound((zoneLeft - Theme::s4) * m_dpr);
    const int panelW = qMax(0, qMin(fullW, availPanel));
    m_l.clipped = panelW < fullW;
    m_l.panel = QRect(0, 0, panelW, Theme::stripHeightK * k);
    m_l.field = QRect(bez, bez, fullW - 2 * bez, m_l.panel.height() - 2 * bez);

    const QFontMetrics fmB(m_bold16), fmR(m_regular16), fmBar(m_regular32);
    const int ox = bez, oy = bez;  // field origin inside the panel
    const int baseRow1 = oy + 14 * k, baseRow2 = oy + 32 * k;

    const int modeW = fmB.horizontalAdvance(m_texts.mode);
    m_l.modeSeg = QRect(ox + 4 * k, oy + 3 * k, modeW + 8 * k, 16 * k);
    m_l.modeText = QPoint(ox + 8 * k, baseRow1);
    m_l.bar = QRect(ox + 4 * k, oy + 37 * k, 468 * k, 30 * k);
    m_l.barText = QPoint(ox + 10 * k, oy + 58 * k);
    if (m_texts.known) {
        const int boldW = fmB.horizontalAdvance(m_texts.pageBold);
        const int pageW = boldW + fmR.horizontalAdvance(m_texts.pageRegular);
        const int segW = pageW + 8 * k;
        const int segLeft = ox + 472 * k - segW;
        m_l.pageSeg = QRect(segLeft, oy + 3 * k, segW, 16 * k);
        m_l.pageBoldText = QPoint(segLeft + 4 * k, baseRow1);
        m_l.pageRegText = QPoint(segLeft + 4 * k + boldW, baseRow1);
        m_l.bankText = QPoint(ox + 8 * k, baseRow2);
        m_l.categoryText = QPoint(ox + 160 * k, baseRow2);
        // Names never shrink; if one does not fit, clip with an ellipsis (DESIGN 7.2).
        m_l.barElided = fmBar.elidedText(m_texts.bar, Qt::ElideRight, 456 * k);
    } else {
        m_l.pageSeg = QRect();
        m_l.barElided = m_texts.bar;
    }
}

void DisplayStrip::resizeEvent(QResizeEvent *)
{
    relayout();
}

void DisplayStrip::showEvent(QShowEvent *)
{
    syncScale();
}

bool DisplayStrip::event(QEvent *e)
{
    if (e->type() == QEvent::ScreenChangeInternal
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || e->type() == QEvent::DevicePixelRatioChange
#endif
    )
        syncScale();
    return QWidget::event(e);
}

void DisplayStrip::paintEvent(QPaintEvent *)
{
    if (!qFuzzyCompare(devicePixelRatioF(), m_dpr))
        relayout();  // rare: the DPR changed without a screen-change event
    QPainter p(this);
    p.fillRect(rect(), QColor(Theme::bgBrowse));

    // ---- connection zone, outside the panel, logical px ----
    p.setRenderHint(QPainter::Antialiasing, true);
    const QColor status(m_connected ? Theme::text : Theme::red);
    if (m_connected) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Theme::cyan));
        p.drawEllipse(m_l.led);
    } else {
        const double ring = 3.0;
        const double rad = m_l.led.width() / 2.0 - ring / 2.0;
        const QPointF c = m_l.led.center();
        QPen pen(QColor(Theme::red), ring);
        pen.setCapStyle(Qt::FlatCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, rad, rad);
        const double d = rad * 0.7071;
        p.drawLine(QPointF(c.x() - d, c.y() + d), QPointF(c.x() + d, c.y() - d));
    }
    p.setPen(status);
    p.setFont(m_label);
    p.drawStaticText(m_l.connTextPos, m_connStatic);

    // ---- LCD panel in device pixels (DESIGN 2.2 rule 1) ----
    const int k = m_k;
    Theme::enterDeviceSpace(p, QPointF(0, 0));
    p.setClipRect(m_l.panel);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(Theme::lcdBezel));
    p.drawRoundedRect(QRectF(0, 0, Theme::stripPanelWidthK * k, Theme::stripHeightK * k), 4.0 * k, 4.0 * k);
    p.setBrush(QColor(Theme::lcdField));
    p.drawRoundedRect(QRectF(m_l.field), 2.0 * k, 2.0 * k);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QColor ink(Theme::lcdInk), paper(Theme::lcdField);
    p.fillRect(m_l.modeSeg, ink);
    p.setPen(paper);
    p.setFont(m_bold16);
    p.drawText(m_l.modeText, m_texts.mode);
    if (m_texts.known) {
        p.fillRect(m_l.pageSeg, ink);
        p.drawText(m_l.pageBoldText, m_texts.pageBold);
        p.setFont(m_regular16);
        p.drawText(m_l.pageRegText, m_texts.pageRegular);
        p.setPen(ink);
        p.setFont(m_bold16);
        p.drawText(m_l.bankText, m_texts.bank);
        p.setFont(m_regular16);
        p.drawText(m_l.categoryText, m_texts.category);
        p.fillRect(m_l.bar, ink);
        p.setPen(paper);
    } else {
        // Unknown sound: outline of k device px, no fill; text in ink.
        p.fillRect(m_l.bar, ink);
        p.fillRect(m_l.bar.adjusted(k, k, -k, -k), paper);
        p.setPen(ink);
    }
    p.setFont(m_regular32);
    p.drawText(m_l.barText, m_l.barElided);
    p.restore();
}
