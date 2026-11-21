#include "ui/LiveView.h"

#include "core/Catalogue.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QStringList>
#include <QTextOption>
#include <QtMath>

#include <algorithm>

namespace {
// Pixel Operator advance of one character in font pixels (DESIGN 2.2 rule 2).
int fpAdv(const QFontMetrics &fm1, QChar c) { return qRound(double(fm1.horizontalAdvance(c))); }

void buildRun(PixelRun::Run &run, const QFontMetrics &fm1, int k, int x, int baseline, const QString &text)
{
    run.clear();
    run.reserve(text.size());
    for (const QChar c : text) {
        run.append({QPoint(x, baseline), QString(c)});
        x += k * fpAdv(fm1, c);
    }
}

// Top of a text line whose capitals are centred on `cy` (device px).
double topForCentre(const QFontMetricsF &fm, double cy) { return qRound(cy + fm.capHeight() / 2.0 - fm.ascent()); }

QString addressOnly(const SoundAddress &a)
{
    const QString f = a.format();
    return f.mid(f.indexOf(QLatin1Char(' ')) + 1);
}
} // namespace

LiveView::LiveView(const Catalogue *catalogue, QWidget *parent)
    : QWidget(parent)
    , m_catalogue(catalogue)
    , m_set(QString(), QString(), 8)
{
    setObjectName(QStringLiteral("liveView"));
    setAttribute(Qt::WA_OpaquePaintEvent);  // paintEvent fills what it repaints
    setFocusPolicy(Qt::StrongFocus);        // the view owns the keyboard (T4.2); no focus ring is drawn
    setAttribute(Qt::WA_Hover, false);
    m_flashTimer.setSingleShot(true);
    m_flashTimer.setInterval(2000);  // DESIGN 5.5: the red chip lasts 2 s
    connect(&m_flashTimer, &QTimer::timeout, this, [this]() {
        if (!m_flash)
            return;
        m_flash = false;
        m_flashText.clear();
        layoutBar();
        update(m_bar.rect);
    });
    m_escTimer.setSingleShot(true);
    m_escTimer.setInterval(1000);  // SPEC 4.2: Esc held 1 s, or a second Esc within 1 s
    connect(&m_escTimer, &QTimer::timeout, this, [this]() { escTimeout(); });
    resize(1280, 720);
    layoutAll();
}

// ---------------------------------------------------------------- setters

static bool sameSlots(const SetModel &a, const SetModel &b)
{
    if (a.name() != b.name() || a.size() != b.size())
        return false;
    for (int i = 0; i < a.size(); ++i) {
        const SetModel::Slot &x = a.slot(i), &y = b.slot(i);
        if (x.address != y.address || x.label != y.label)
            return false;
    }
    return true;
}

void LiveView::setSet(const SetModel &set)
{
    if (sameSlots(set, m_set))
        return;
    setPressed(-1);  // whatever was held is another sound now: a release must send nothing
    m_set = set;
    m_target = -1;  // slots may mean other sounds now
    m_chosen = -1;
    layoutButtons();   // also drops a pressed slot that became empty
    m_lit = findLit();
    layoutBar();  // the set name, and whether the sound is in the set
    m_nameSig = nameSignature();
    update();
}

QStringList LiveView::nameSignature() const
{
    QStringList sig;
    if (!m_catalogue)
        return sig;
    for (int i = 0; i < m_set.size(); ++i)
        if (const auto &a = m_set.slot(i).address)
            sig << m_catalogue->effectiveName(*a);
    sig << (m_current ? m_catalogue->effectiveName(*m_current) : QString());
    return sig;
}

int LiveView::findLit() const
{
    if (!m_current)
        return -1;
    // A sound listed twice: the slot chosen here wins while it matches; a sound that came from
    // the panel lights its first slot.
    if (m_chosen >= 0 && m_chosen < m_set.size() && m_set.slot(m_chosen).address && *m_set.slot(m_chosen).address == *m_current)
        return m_chosen;
    for (int i = 0; i < m_set.size(); ++i)
        if (m_set.slot(i).address && *m_set.slot(i).address == *m_current)
            return i;  // a sound listed twice lights its first slot
    return -1;
}

void LiveView::setCurrentSound(const std::optional<SoundAddress> &sound)
{
    const std::optional<SoundAddress> s = (sound && sound->isValid()) ? sound : std::nullopt;
    if (s == m_current)
        return;
    const int old = m_lit;
    m_current = s;
    if (m_chosen >= 0 && m_chosen < m_set.size() && !(s && m_set.slot(m_chosen).address && *m_set.slot(m_chosen).address == *s)
        && m_target != m_chosen)
        m_chosen = -1;  // the sound moved on without us: back to first match
    m_lit = findLit();
    layoutBar();
    m_nameSig = nameSignature();
    update(m_bar.rect);
    if (old != m_lit) {
        if (old >= 0)
            update(m_buttons[old].rect);
        if (m_lit >= 0)
            update(m_buttons[m_lit].rect);
    }
}

void LiveView::setConnection(bool connected, const QString &message)
{
    if (connected == m_connected && message == m_connMessage)
        return;
    m_connected = connected;
    m_connMessage = message;
    layoutBar();
    update(m_bar.rect);
}

void LiveView::flashSendFailure(const QString &reason)
{
    m_flashTimer.start();  // restarts a running flash
    if (m_flash && reason == m_flashText)
        return;
    m_flash = true;
    m_flashText = reason;
    layoutBar();
    update(m_bar.rect);
}

void LiveView::setEscArmed(bool armed)
{
    if (armed == m_escArmed)
        return;
    m_escArmed = armed;
    layoutHint();
    update(m_hint.rect);
}

void LiveView::sendResolved(const SoundAddress &address)
{
    if (m_target >= 0 && m_target < m_set.size() && m_set.slot(m_target).address && *m_set.slot(m_target).address == address) {
        // A same-address resend from another slot: the sound did not change, so setCurrentSound
        // did nothing; light the slot that was pressed.
        if (m_current && *m_current == address && m_lit != m_target) {
            const int old = m_lit;
            m_lit = m_target;
            m_chosen = m_target;
            if (old >= 0)
                update(m_buttons[old].rect);
            update(m_buttons[m_lit].rect);
        }
        m_target = -1;
    }
}

void LiveView::refresh()
{
    const QStringList sig = nameSignature();
    if (sig == m_nameSig)
        return;  // recordUse and friends fire this on every send: nothing shown changed
    layoutAll();
}

// ---------------------------------------------------------------- accessors

QRect LiveView::buttonRect(int i) const { return (i >= 0 && i < m_buttons.size()) ? m_buttons[i].rect : QRect(); }
QRect LiveView::buttonRectDevice(int i) const { return (i >= 0 && i < m_buttons.size()) ? m_buttons[i].dev : QRect(); }
QRect LiveView::nameBoxDevice(int i) const { return (i >= 0 && i < m_buttons.size()) ? m_buttons[i].nameBox : QRect(); }
NameFit::Result LiveView::nameFit(int i) const { return (i >= 0 && i < m_buttons.size()) ? m_buttons[i].fit : NameFit::Result(); }

LiveView::ButtonState LiveView::buttonState(int i) const
{
    return (i >= 0 && i < m_buttons.size()) ? stateOf(i) : ButtonState::Empty;
}

LiveView::ButtonState LiveView::stateOf(int i) const
{
    if (m_buttons[i].empty)
        return ButtonState::Empty;
    const bool lit = i == m_lit, pressed = i == m_pressed;
    return lit ? (pressed ? ButtonState::PressedLit : ButtonState::Lit) : (pressed ? ButtonState::Pressed : ButtonState::Normal);
}

// ---------------------------------------------------------------- layout helpers

int LiveView::dv(double designPx) const { return qRound(qRound(designPx * m_u) * m_dpr); }

// Stroke device width (DESIGN 3.5): max(1, round(w * u * dpr)), never rounded in logical px first.
int LiveView::sd(double designPx) const { return qMax(1, qRound(designPx * m_u * m_dpr)); }

int LiveView::fontPx(double designPx) const
{
    // DESIGN 2.1: live text other than the big name never renders under 11 logical px.
    return qRound(qMax(11, qRound(designPx * m_u)) * m_dpr);
}

QRect LiveView::toDevice(const QRect &r) const
{
    const int x0 = qRound(r.left() * m_dpr), y0 = qRound(r.top() * m_dpr);
    const int x1 = qRound((r.right() + 1) * m_dpr), y1 = qRound((r.bottom() + 1) * m_dpr);
    return QRect(x0, y0, x1 - x0, y1 - y0);
}

QFont LiveView::textFont(int face, double designPx, double trackingDesign) const
{
    QFont f = Theme::font(Theme::Face(face), fontPx(designPx));
    if (trackingDesign != 0)
        f.setLetterSpacing(QFont::AbsoluteSpacing, trackingDesign * m_u * m_dpr);
    return f;
}

void LiveView::prep(QStaticText &st, const QString &text, const QFont &font, int wrapWidth) const
{
    st = QStaticText();
    st.setTextFormat(Qt::PlainText);
    st.setPerformanceHint(QStaticText::AggressiveCaching);
    if (wrapWidth > 0) {
        QTextOption opt;
        opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);  // a port name has no word boundary
        st.setTextOption(opt);
        st.setTextWidth(wrapWidth);
    }
    st.setText(text);
    st.prepare(m_prepTransform, font);
}

// ---------------------------------------------------------------- layout

void LiveView::layoutAll()
{
    m_dpr = devicePixelRatioF();
    m_prepTransform = QTransform::fromScale(1.0 / m_dpr, 1.0 / m_dpr);
    // DESIGN 5.1: u from the view's own size, rounded to 3 decimals.
    const double raw = qBound(0.4, qMin(width() / 1280.0, height() / 720.0), 3.0);
    m_u = qRound(raw * 1000.0) / 1000.0;
    buildTools();
    layoutButtons();
    layoutBar();
    layoutHint();
    m_nameSig = nameSignature();
    update();
}

void LiveView::buildTools()
{
    m_fBadge = textFont(int(Theme::Face::BarlowSemiBold), 28, 0);
    m_fTag = textFont(int(Theme::Face::BarlowSemiBold), 22, 2);
    m_fAddr = textFont(int(Theme::Face::BarlowSemiBold), 22, 1);
    m_fHint = textFont(int(Theme::Face::BarlowMedium), 18, 0.5);
    m_fNotInSet = textFont(int(Theme::Face::BarlowSemiBold), 18, 2);

    m_radius = dv(Theme::radiusLiveButton);
    m_badgeRadius = dv(Theme::radiusBadge);
    m_radiusChip = dv(Theme::radiusChip);
    m_radiusBezel = dv(6);
    m_radiusField = dv(4);
    // DESIGN 3.5: stroke device width = max(1, round(w * dpr)), w already multiplied by u.
    auto sw = [this](double v) { return qreal(sd(v)); };
    m_wNormal = sw(Theme::borderLiveNormal);
    m_wLit = sw(Theme::borderLiveLit);
    m_wPressed = sw(Theme::borderLivePressed);
    m_wEmpty = sw(Theme::borderLiveEmpty);
    m_wBadge = sw(2);

    auto solid = [](QRgb c, qreal w = 1) {
        QPen p{QColor(c)};
        p.setWidthF(w);
        p.setJoinStyle(Qt::RoundJoin);
        return p;
    };
    auto dashed = [&](QRgb c, qreal w) {
        QPen p = solid(c, w);
        p.setCapStyle(Qt::FlatCap);
        p.setDashPattern({qreal(dv(8)) / w, qreal(dv(6)) / w});  // DESIGN 3.4: dash 8u, gap 6u
        return p;
    };
    m_penNone = QPen(Qt::NoPen);
    m_penNormal = solid(Theme::lineStrong, m_wNormal);
    m_penLit = solid(Theme::text, m_wLit);
    m_penPressed = solid(Theme::cyanText, m_wPressed);
    m_penEmpty = dashed(Theme::line, m_wEmpty);
    m_penBadge = solid(Theme::lineStrong, m_wBadge);
    m_penBadgeEmpty = dashed(Theme::line, m_wBadge);
    m_penText = QPen(QColor(Theme::text));
    m_penTextDim = QPen(QColor(Theme::textDim));
    m_penTextEmpty = QPen(QColor(Theme::textEmpty));
    m_penOnCyan = QPen(QColor(Theme::onCyan));
    m_penCyanText = QPen(QColor(Theme::cyanText));
    m_penRed = QPen(QColor(Theme::red));
    m_penOnRed = QPen(QColor(Theme::onRed));
    m_penLcdInk = QPen(QColor(Theme::lcdInk));
    m_penLcdField = QPen(QColor(Theme::lcdFieldDim));
    m_penTag = solid(Theme::cyanText, sd(2));
    m_penCap = solid(Theme::textDim, sd(2));
    m_penCapArmed = solid(Theme::text, sd(2));
    const qreal ringW = sd(3);
    m_penRing = solid(Theme::red, ringW);
    m_penRing.setCapStyle(Qt::FlatCap);
    m_penRingOnRed = solid(Theme::onRed, ringW);
    m_penRingOnRed.setCapStyle(Qt::FlatCap);

    m_brNone = QBrush(Qt::NoBrush);
    m_brSurface = QBrush(QColor(Theme::surface));
    m_brPressed = QBrush(QColor(Theme::surfacePressed));
    m_brCyan = QBrush(QColor(Theme::cyan));
    m_brCyanText = QBrush(QColor(Theme::cyanText));
    m_brOnCyan = QBrush(QColor(Theme::onCyan));
    m_brBar = QBrush(QColor(Theme::bar));
    m_brLine = QBrush(QColor(Theme::line));
    m_brBezel = QBrush(QColor(Theme::lcdBezel));
    m_brField = QBrush(QColor(Theme::lcdFieldDim));
    m_brInk = QBrush(QColor(Theme::lcdInk));
    m_brRed = QBrush(QColor(Theme::red));
    m_brBg = QBrush(QColor(Theme::bgLive));
}

void LiveView::layoutButtons()
{
    auto L = [this](double v) { return qRound(v * m_u); };
    const int W = width(), H = height();
    const int n = m_set.size();
    m_cols = n == 6 ? 3 : 4;
    const int m = L(Theme::liveMargin), g = L(Theme::liveGap), pad = L(Theme::liveInnerPadding);
    const int top = L(Theme::liveTopBar) + g;
    const int areaH = qMax(0, H - L(Theme::liveTopBar) - g - g - L(Theme::liveHintStrip));
    const int rowH = (areaH - g) / 2;
    const int cellW = qRound((W - 2.0 * m - (m_cols - 1) * g) / double(m_cols));
    const int badgeL = L(Theme::liveBadge);
    const double U = m_u * m_dpr;

    const QFontMetrics fm1(Theme::pixelFont(1, true));
    const QFontMetricsF fmBadge(m_fBadge), fmTag(m_fTag), fmAddr(m_fAddr);
    const double tagTrk = m_fTag.letterSpacing();

    m_buttons.clear();
    m_buttons.resize(n);
    // (the pressed slot is validated below, once the buttons know which are empty)
    int x = m;
    QVector<int> xs, ws;
    for (int c = 0; c < m_cols; ++c) {
        const int w = c < m_cols - 1 ? cellW : W - m - x;
        xs << x;
        ws << w;
        x += w + g;
    }
    for (int i = 0; i < n; ++i) {
        Button &b = m_buttons[i];
        const int c = i % m_cols, row = i / m_cols;
        const int bw = ws[c], bh = row == 0 ? rowH : areaH - rowH - g;
        b.rect = QRect(xs[c], top + row * (rowH + g), bw, bh);
        b.dev = toDevice(b.rect);
        const SetModel::Slot &slot = m_set.slot(i);
        b.empty = slot.isEmpty();

        const int ix = b.rect.x() + pad, iy = b.rect.y() + pad, iw = bw - 2 * pad;
        b.badge = QRectF(qRound(ix * m_dpr), qRound(iy * m_dpr), dv(Theme::liveBadge), dv(Theme::liveBadge));
        const QString digit = QString::number(i + 1);
        prep(b.badgeDigit, digit, m_fBadge);
        b.badgeDigitPos = QPointF(b.badge.center().x() - fmBadge.horizontalAdvance(digit) / 2.0,
                                  topForCentre(fmBadge, b.badge.center().y()));
        const double badgeCy = b.badge.center().y();

        if (b.empty) {
            const QString t = QStringLiteral("EMPTY");
            prep(b.emptyText, t, m_fTag);
            const double tw = fmTag.horizontalAdvance(t) - tagTrk;
            b.emptyPos = QPointF(b.dev.center().x() + 0.5 - tw / 2.0, topForCentre(fmTag, b.dev.center().y() + 0.5));
            continue;
        }

        // ---- name box (DESIGN 5.3): logical box first, then device px
        const int nbLogicalY = iy + badgeL + L(8);
        const int nbLogicalH = qMax(0, bh - 2 * pad - badgeL - L(8) - L(8) - L(48));
        b.nameBox = QRect(qRound(ix * m_dpr), qRound(nbLogicalY * m_dpr), qRound(iw * m_dpr), qRound(nbLogicalH * m_dpr));
        QString soundName = m_catalogue ? m_catalogue->effectiveName(*slot.address) : QString();
        if (soundName.isEmpty())
            soundName = QStringLiteral("---");
        const bool hasLabel = !slot.label.isEmpty();
        const QString big = hasLabel ? slot.label : soundName;
        b.fit = NameFit::fit(big, b.nameBox.width(), b.nameBox.height(), U, fm1);
        const int k = b.fit.k;
        b.nameFont = Theme::pixelFont(k, true);
        b.nameRun.clear();
        const int th = b.fit.height();
        const int ytop = b.nameBox.y() + qRound((b.nameBox.height() - th) / 2.0);
        QVector<PixelRun::Glyph> all;
        for (int l = 0; l < b.fit.lines.size(); ++l) {
            const int lx = b.nameBox.x() + qRound((b.nameBox.width() - b.fit.advances[l]) / 2.0);
            PixelRun::Run line;
            buildRun(line, fm1, k, lx, ytop + 9 * k + 14 * k * l, b.fit.lines[l]);
            for (const PixelRun::Glyph &gl : qAsConst(line))
                b.nameRun.append(gl);
        }

        // ---- NOW tag (drawn only when lit)
        const double innerRight = qRound((ix + iw) * m_dpr);
        const QString now = QStringLiteral("NOW");
        prep(b.nowText, now, m_fTag);
        const double nowW = fmTag.horizontalAdvance(now) - tagTrk;
        b.nowPos = QPointF(innerRight - nowW, topForCentre(fmTag, badgeCy));
        const double triX = b.nowPos.x() - dv(8), triW = dv(14), triH = dv(16);
        b.triangle = QPolygonF({QPointF(triX - triW, badgeCy - triH / 2.0), QPointF(triX, badgeCy),
                                QPointF(triX - triW, badgeCy + triH / 2.0)});

        // ---- bottom block: address, then the sound name when a label took the big text
        const double bottom = qRound((b.rect.y() + bh - pad) * m_dpr);
        const int innerW = qRound(iw * m_dpr);
        double soundBase = bottom;
        b.hasSound = hasLabel;
        if (hasLabel) {
            static const int sizes[] = {20, 18, 16, 14};
            QString shown = soundName;
            for (int s : sizes) {
                b.soundFont = Theme::font(Theme::Face::AtkinsonRegular, fontPx(s));
                if (QFontMetricsF(b.soundFont).horizontalAdvance(soundName) <= innerW || s == 14) {
                    break;
                }
            }
            const QFontMetricsF fmS(b.soundFont);
            shown = fmS.elidedText(soundName, Qt::ElideRight, innerW);  // never needed at 14u for 16 characters
            prep(b.soundText, shown, b.soundFont);
            soundBase = bottom - qRound(fmS.descent() / 2.0);
            b.soundPos = QPointF(qRound(ix * m_dpr), qRound(soundBase - fmS.ascent()));
        }
        const QString addr = slot.address->format();
        prep(b.addrText, addr, m_fAddr);
        const double addrBase = hasLabel ? soundBase - b.soundFont.pixelSize() : bottom - qRound(fmAddr.descent() / 2.0);
        b.addrPos = QPointF(qRound(ix * m_dpr), qRound(addrBase - fmAddr.ascent()));
    }
    if (m_pressed >= m_buttons.size() || (m_pressed >= 0 && m_buttons[m_pressed].empty))
        m_pressed = -1;
}

void LiveView::layoutBar()
{
    auto L = [this](double v) { return qRound(v * m_u); };
    const int W = width();
    const int Wd = qRound(W * m_dpr);
    m_bar.rect = QRect(0, 0, W, L(Theme::liveTopBar));
    m_bar.dev = toDevice(m_bar.rect);
    const int ruleH = sd(2);
    m_bar.ruleDev = QRect(0, m_bar.dev.bottom() + 1 - ruleH, Wd, ruleH);
    const int pad = dv(Theme::liveMargin), gap24 = dv(24);
    const double cy = (m_bar.dev.height() - ruleH) / 2.0;

    // ---- right zone: connection (DESIGN 5.5 item 3)
    const bool flashMessage = m_flash && !m_flashText.isEmpty();
    const bool error = flashMessage || (!m_connected && !m_connMessage.isEmpty());
    QString status = flashMessage ? m_flashText
                                  : (m_connMessage.isEmpty() ? (m_connected ? QStringLiteral("X50 CONNECTED") : QStringLiteral("NO X50"))
                                                             : m_connMessage);
    status = status.toUpper();
    m_conn.font = textFont(int(Theme::Face::BarlowSemiBold), error ? 18 : 22, 1);
    const double ledD = dv(16), ledGap = dv(12);
    {
        const QFontMetricsF fm(m_conn.font);
        const double trk = m_conn.font.letterSpacing();
        QString shown = status;
        double textW, textH;
        if (error) {
            const int avail = qMax(40, int(0.45 * Wd - ledD - ledGap));
            prep(m_conn.text, shown, m_conn.font, avail);
            int guard = shown.size();
            // Up to two lines (DESIGN 5.5): shorten with an ellipsis until it fits.
            while (m_conn.text.size().height() > 2 * fm.lineSpacing() + 1 && shown.size() > 1 && guard-- > 0) {
                shown.chop(2);
                shown = shown.trimmed() + QChar(0x2026);
                prep(m_conn.text, shown, m_conn.font, avail);
            }
            textW = m_conn.text.size().width();
            textH = m_conn.text.size().height();
        } else {
            prep(m_conn.text, shown, m_conn.font);
            textW = fm.horizontalAdvance(shown) - trk;
            textH = fm.height();
        }
        m_conn.shown = shown;
        const double zoneW = ledD + ledGap + textW;
        const double zoneLeft = Wd - pad - zoneW;
        m_conn.led = QRectF(zoneLeft, cy - ledD / 2.0, ledD, ledD);
        const double ringW = m_penRing.widthF();
        m_conn.ring = m_conn.led.adjusted(ringW / 2, ringW / 2, -ringW / 2, -ringW / 2);
        const double d = m_conn.ring.width() / 2.0 * 0.7071;
        const QPointF c = m_conn.ring.center();
        m_conn.diag = QLineF(c.x() - d, c.y() + d, c.x() + d, c.y() - d);
        const double ty = (error && m_conn.text.size().height() > fm.lineSpacing() * 1.5) ? cy - textH / 2.0 : topForCentre(fm, cy);
        m_conn.textPos = QPointF(zoneLeft + ledD + ledGap, ty);
        const double chipH = qMax(ledD, textH) + 2.0 * dv(8);
        m_conn.chip = QRectF(zoneLeft - dv(12), cy - chipH / 2.0, zoneW + 2.0 * dv(12), chipH);
    }
    const double zoneW = Wd - pad - m_conn.led.left();

    // ---- left zone: set name, Atkinson Bold 24u down to 18u, then elided (DESIGN 5.5 item 1)
    double setW = 0;
    {
        const double maxW = 0.34 * Wd;
        QString shown = m_set.name();
        QFont f;
        for (int d = 24; d >= 18; d -= 2) {
            f = Theme::font(Theme::Face::AtkinsonBold, fontPx(d));
            if (QFontMetricsF(f).horizontalAdvance(shown) <= maxW)
                break;
        }
        const QFontMetricsF fm(f);
        if (fm.horizontalAdvance(shown) > maxW)
            shown = fm.elidedText(shown, Qt::ElideRight, maxW);
        m_bar.setText = shown;
        m_bar.setFont = f;
        prep(m_bar.setStatic, shown, f);
        setW = fm.horizontalAdvance(shown);
        m_bar.setPos = QPointF(pad, topForCentre(fm, cy));
    }

    // ---- NOT IN SET tag (outside the panel)
    m_notInSet = m_current.has_value() && m_lit < 0;
    double tagW = 0;
    if (m_notInSet) {
        const QFontMetricsF fm(m_fNotInSet);
        const QString t = QStringLiteral("NOT IN SET");
        const double h = dv(32), padX = dv(10);
        tagW = fm.horizontalAdvance(t) - m_fNotInSet.letterSpacing() + 2 * padX;
        m_notInSetTag.rect = QRectF(0, cy - h / 2.0, tagW, h);  // x set below
        prep(m_notInSetTag.text, t, m_fNotInSet);
        m_notInSetTag.textPos = QPointF(0, topForCentre(fm, cy));
    } else {
        m_notInSetTag.rect = QRectF();
    }

    // ---- centre: the LCD readout (DESIGN 5.5 item 2, 2.2)
    Readout &r = m_readout;
    r.known = m_current.has_value();
    const double U = m_u * m_dpr;
    const QFontMetrics fmB(Theme::pixelFont(1, true)), fmR(Theme::pixelFont(1, false));
    QString head, name;
    if (r.known) {
        r.modeText = m_current->kind() == SoundAddress::Kind::Combi ? QStringLiteral("COMBI 0:Play") : QStringLiteral("PROG 0:Play");
        head = addressOnly(*m_current) + QStringLiteral(": ");
        name = m_catalogue ? m_catalogue->effectiveName(*m_current) : QString();
        if (name.isEmpty())
            name = QStringLiteral("---");
    } else {
        r.modeText = QStringLiteral("--");
        head = QStringLiteral("--");
    }
    const QFontMetrics &fmBar = r.known ? fmB : fmR;
    const int bz = qMax(1, qRound(2 * U)), inset = qRound(4 * U), ph = qRound(40 * U), sh = qRound(32 * U);
    const int gap = qRound(6 * U), padM = qRound(8 * U), padB = qRound(10 * U);
    const int kb0 = qMax(1, qRound(1.8 * U)), km0 = qMax(1, qRound(1.2 * U));
    const double avail = qMax(0.0, Wd - 2.0 * pad - setW - 2.0 * gap24 - zoneW - (m_notInSet ? tagW + dv(16) : 0.0));
    auto barString = [&](const QString &n) { return r.known ? head + n : head; };
    int kb = kb0, km = qMin(km0, kb0);
    QString barText = barString(name);
    auto panelWidth = [&](int kbb, int kmm, const QString &bt) {
        const int mw = kmm * NameFit::fontPixelAdvance(fmB, r.modeText) + 2 * padM;
        const int bw = kbb * NameFit::fontPixelAdvance(fmBar, bt) + 2 * padB;
        return bz + inset + mw + gap + bw + inset + bz;
    };
    // Lower kb (and km with it) until the panel fits; never the mode text on its own.
    while (panelWidth(kb, km, barText) > avail && kb > 1) {
        --kb;
        km = qMin(km, kb);
    }
    if (r.known && panelWidth(kb, km, barText) > avail) {
        const QChar ell(0x2026);
        QString n = name;
        while (!n.isEmpty() && panelWidth(kb, km, head + n + ell) > avail)
            n.chop(1);
        barText = head + n + ell;
    }
    r.kb = kb;
    r.km = km;
    r.barText = barText;
    r.outline = qMax(1, qRound(2 * U));
    const int modeW = km * NameFit::fontPixelAdvance(fmB, r.modeText) + 2 * padM;
    const int barW = kb * NameFit::fontPixelAdvance(fmBar, barText) + 2 * padB;
    const int pw = bz + inset + modeW + gap + barW + inset + bz;
    const int px = pad + qRound(setW) + gap24;
    const int py = qRound(cy - ph / 2.0);
    const int sy = py + (ph - sh) / 2;
    r.panel = QRect(px, py, pw, ph);
    r.field = r.panel.adjusted(bz, bz, -bz, -bz);
    r.modeSeg = QRect(px + bz + inset, sy, modeW, sh);
    r.barSeg = QRect(r.modeSeg.right() + 1 + gap, sy, barW, sh);
    r.modeFont = Theme::pixelFont(km, true);
    r.barFont = Theme::pixelFont(kb, r.known);
    buildRun(r.modeRun, fmB, km, r.modeSeg.x() + padM, Theme::pixelBaseline(sy, sh, km), r.modeText);
    buildRun(r.barRun, fmBar, kb, r.barSeg.x() + padB, Theme::pixelBaseline(sy, sh, kb), barText);

    if (m_notInSet) {
        const double tx = r.panel.right() + 1 + dv(16);
        m_notInSetTag.rect.moveLeft(tx);
        m_notInSetTag.textPos.setX(tx + dv(10));
    }
}

void LiveView::layoutHint()
{
    auto L = [this](double v) { return qRound(v * m_u); };
    m_hint.rect = QRect(0, height() - L(Theme::liveHintStrip), width(), L(Theme::liveHintStrip));
    m_hint.dev = toDevice(m_hint.rect);
    if (m_escArmed) {
        m_hint.pre.clear();
        m_hint.post = QStringLiteral("pressed: press Esc again, or keep holding, to leave");
    } else {
        m_hint.pre = QStringLiteral("Hold");
        m_hint.post = QStringLiteral("for 1 second, or press Esc twice, to leave live view");
    }
    const QFontMetricsF fm(m_fHint);
    const double trk = m_fHint.letterSpacing();
    const double sp = fm.horizontalAdvance(QLatin1Char(' '));
    const QString esc = QStringLiteral("Esc");
    const double padX = dv(6), capH = dv(24);
    const double preW = m_hint.pre.isEmpty() ? 0.0 : fm.horizontalAdvance(m_hint.pre) + sp;
    const double capW = fm.horizontalAdvance(esc) - trk + 2 * padX;
    const double postW = fm.horizontalAdvance(m_hint.post);
    const double total = preW + capW + sp + postW;
    double x = qRound((m_hint.dev.width() - total) / 2.0);
    const double cy = m_hint.dev.y() + m_hint.dev.height() / 2.0;
    const double top = topForCentre(fm, cy);
    m_hint.prePos = QPointF(x, top);
    prep(m_hint.preText, m_hint.pre, m_fHint);
    x += preW;
    m_hint.cap = QRectF(x, cy - capH / 2.0, capW, capH);
    m_hint.capTextPos = QPointF(x + padX, top);
    prep(m_hint.capText, esc, m_fHint);
    x += capW + sp;
    m_hint.postPos = QPointF(x, top);
    prep(m_hint.postText, m_hint.post, m_fHint);
}

// ---------------------------------------------------------------- events

void LiveView::resizeEvent(QResizeEvent *)
{
    layoutAll();
}

void LiveView::showEvent(QShowEvent *)
{
    syncScale();
}

void LiveView::hideEvent(QHideEvent *)
{
    m_target = -1;
    setPressed(-1);
    resetEsc();
}

bool LiveView::event(QEvent *e)
{
    if (e->type() == QEvent::ScreenChangeInternal
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        || e->type() == QEvent::DevicePixelRatioChange
#endif
    )
        syncScale();
    return QWidget::event(e);
}

void LiveView::syncScale()
{
    m_syncQueued = false;
    if (!qFuzzyCompare(devicePixelRatioF(), m_dpr))
        layoutAll();
}

int LiveView::hit(const QPoint &pos) const
{
    for (int i = 0; i < m_buttons.size(); ++i)
        if (m_buttons[i].rect.contains(pos))
            return i;
    return -1;
}

void LiveView::setPressed(int idx)
{
    if (idx == m_pressed)
        return;
    const int old = m_pressed;
    m_pressed = idx;
    if (old >= 0)
        update(m_buttons[old].rect);
    if (idx >= 0)
        update(m_buttons[idx].rect);
}

void LiveView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const int i = hit(e->position().toPoint());
    // An empty slot does nothing and never shows a pressed state (DESIGN 5.4).
    setPressed(i >= 0 && !m_buttons[i].empty ? i : -1);
}

void LiveView::mouseMoveEvent(QMouseEvent *e)
{
    // Leaving the button cancels the press: a stray touch that slides away sends nothing.
    if (m_pressed >= 0 && !m_buttons[m_pressed].rect.contains(e->position().toPoint()))
        setPressed(-1);
}

void LiveView::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    const int i = m_pressed;
    const bool inside = i >= 0 && i < m_buttons.size() && !m_buttons[i].empty
                        && m_buttons[i].rect.contains(e->position().toPoint());
    setPressed(-1);
    if (inside)
        choose(i);
}

// ---------------------------------------------------------------- keys (SPEC 4.2)

int LiveView::stepTarget(int direction) const
{
    const int n = m_buttons.size();
    // The current sound is not in the set (or unknown): Next goes to the first non-empty slot,
    // Previous to the last. Otherwise the next/previous non-empty one; none past the ends (no wrap).
    // While a send is in flight the target is where we are going: step from it, not from the lit slot.
    const int from = m_target >= 0 ? m_target : m_lit;
    int i = from >= 0 ? from + direction : (direction > 0 ? 0 : n - 1);
    for (; i >= 0 && i < n; i += direction)
        if (!m_buttons[i].empty)
            return i;
    return -1;
}

bool LiveView::focusNextPrevChild(bool)
{
    return false;  // Tab never moves focus in live view
}

void LiveView::keyPressEvent(QKeyEvent *e)
{
    e->accept();  // every key is ours; none may reach a shortcut or a parent
    const int key = e->key();
    const Qt::KeyboardModifiers mods = e->modifiers() & ~Qt::KeypadModifier;
    if (key == Qt::Key_Escape) {
        if (!e->isAutoRepeat() && mods == Qt::NoModifier)
            escPressed();
        return;
    }
    if (e->isAutoRepeat())
        return;  // a held key or a stuck pedal must never burst MIDI
    if (mods == Qt::ControlModifier && (key == Qt::Key_Left || key == Qt::Key_Right)) {
        emit setStepRequested(key == Qt::Key_Right ? 1 : -1);
        return;
    }
    if (mods != Qt::NoModifier)
        return;
    int target = -1;
    switch (key) {
    case Qt::Key_1: case Qt::Key_2: case Qt::Key_3: case Qt::Key_4:
    case Qt::Key_5: case Qt::Key_6: case Qt::Key_7: case Qt::Key_8:
        target = key - Qt::Key_1;
        if (target >= m_buttons.size() || m_buttons[target].empty)
            target = -1;
        break;
    case Qt::Key_Right: case Qt::Key_Down: case Qt::Key_Space: case Qt::Key_PageDown:
        target = stepTarget(1);
        break;
    case Qt::Key_Left: case Qt::Key_Up: case Qt::Key_PageUp:
        target = stepTarget(-1);
        break;
    case Qt::Key_BracketRight:
        emit setStepRequested(1);
        return;
    case Qt::Key_BracketLeft:
        emit setStepRequested(-1);
        return;
    default:
        return;  // F5 and everything else: nothing
    }
    if (target >= 0)
        choose(target);
}

void LiveView::choose(int slot)
{
    m_target = slot;
    m_chosen = slot;
    emit slotClicked(slot);
}

void LiveView::keyReleaseEvent(QKeyEvent *e)
{
    e->accept();
    if (e->key() == Qt::Key_Escape && !e->isAutoRepeat())
        escReleased();
}

void LiveView::focusOutEvent(QFocusEvent *e)
{
    resetEsc();  // a release we will never see must not complete a hold
    setPressed(-1);  // nor a button stay pressed
    QWidget::focusOutEvent(e);
}

// DESIGN 5.6: the first Esc arms the hint and starts the 1 s timer. A second press while it runs
// leaves; so does the timer ending with Esc still down (a hold). Otherwise it disarms.
void LiveView::escPressed()
{
    if (m_escTimer.isActive()) {
        resetEsc();
        emit leaveRequested();
        return;
    }
    m_escDown = true;
    setEscArmed(true);
    m_escTimer.start();
}

void LiveView::escReleased()
{
    m_escDown = false;  // the timer keeps running: a second press may still come
}

void LiveView::escTimeout()
{
    const bool held = m_escDown;
    resetEsc();
    if (held)
        emit leaveRequested();
}

void LiveView::resetEsc()
{
    m_escTimer.stop();
    m_escDown = false;
    setEscArmed(false);
}

// ---------------------------------------------------------------- painting

void LiveView::paintEvent(QPaintEvent *event)
{
    ++m_paintCount;
    if (!m_syncQueued && !qFuzzyCompare(devicePixelRatioF(), m_dpr)) {
        // The device pixel ratio changed without a screen-change event: lay out right after this paint.
        m_syncQueued = true;
        QMetaObject::invokeMethod(this, &LiveView::syncScale, Qt::QueuedConnection);
    }
    QPainter p(this);
    Theme::enterDeviceSpace(p, QPointF(0, 0));
    const QRect &clip = event->rect();
    // At a fractional device pixel ratio the logical clip rounds inwards on some edges: pad by 1 device px.
    QRect fill = toDevice(clip);
    if (!qFuzzyCompare(m_dpr, qRound(m_dpr) * 1.0))
        fill.adjust(-1, -1, 1, 1);
    p.fillRect(fill, m_brBg);
    if (clip.intersects(m_bar.rect))
        paintBar(p);
    for (int i = 0; i < m_buttons.size(); ++i)
        if (clip.intersects(m_buttons[i].rect))
            paintButton(p, i);
    if (clip.intersects(m_hint.rect))
        paintHint(p);
    p.restore();
}

void LiveView::paintBar(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(m_bar.dev, m_brBar);
    p.fillRect(m_bar.ruleDev, m_brLine);

    p.setPen(m_penText);
    p.setFont(m_bar.setFont);
    p.drawStaticText(m_bar.setPos, m_bar.setStatic);

    // ---- LCD readout panel
    const Readout &r = m_readout;
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(m_penNone);
    p.setBrush(m_brBezel);
    p.drawRoundedRect(QRectF(r.panel), m_radiusBezel, m_radiusBezel);
    p.setBrush(m_brField);
    p.drawRoundedRect(QRectF(r.field), m_radiusField, m_radiusField);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(r.modeSeg, m_brInk);
    p.setPen(m_penLcdField);
    p.setFont(r.modeFont);
    PixelRun::draw(p, r.modeRun);
    p.fillRect(r.barSeg, m_brInk);
    if (r.known) {
        p.setFont(r.barFont);
        PixelRun::draw(p, r.barRun);
    } else {
        // Unknown sound: not filled but an outline of `max(1, round(2u))` device px (DESIGN 5.5).
        p.fillRect(r.barSeg.adjusted(r.outline, r.outline, -r.outline, -r.outline), m_brField);
        p.setPen(m_penLcdInk);
        p.setFont(r.barFont);
        PixelRun::draw(p, r.barRun);
    }

    // ---- NOT IN SET tag
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_notInSet) {
        const qreal w = m_penTag.widthF();
        p.setPen(m_penTag);
        p.setBrush(m_brNone);
        p.drawRoundedRect(m_notInSetTag.rect.adjusted(w / 2, w / 2, -w / 2, -w / 2), m_radiusChip - w / 2, m_radiusChip - w / 2);
        p.setFont(m_fNotInSet);
        p.drawStaticText(m_notInSetTag.textPos, m_notInSetTag.text);
    }

    // ---- connection LED and text; the red chip while a failed send is flagged
    if (m_flash) {
        p.setPen(m_penNone);
        p.setBrush(m_brRed);
        p.drawRoundedRect(m_conn.chip, m_radiusChip, m_radiusChip);
    }
    if (m_connected) {
        p.setPen(m_penNone);
        p.setBrush(m_flash ? m_brOnCyan : m_brCyan);
        p.drawEllipse(m_conn.led);
    } else {
        p.setPen(m_flash ? m_penRingOnRed : m_penRing);
        p.setBrush(m_brNone);
        p.drawEllipse(m_conn.ring);
        p.drawLine(m_conn.diag);
    }
    p.setPen(m_flash ? m_penOnRed : (m_connected ? m_penText : m_penRed));
    p.setFont(m_conn.font);
    p.drawStaticText(m_conn.textPos, m_conn.text);
}

void LiveView::paintButton(QPainter &p, int i)
{
    const Button &b = m_buttons[i];
    const ButtonState st = stateOf(i);
    const bool lit = st == ButtonState::Lit || st == ButtonState::PressedLit;
    const QRectF r(b.dev);
    const qreal rad = m_radius;
    p.setRenderHint(QPainter::Antialiasing, true);

    // Body and border; borders are drawn inside the rect (DESIGN 3.5).
    auto stroke = [&](const QPen &pen) {
        const qreal h = pen.widthF() / 2.0;
        p.setPen(pen);
        p.setBrush(m_brNone);
        p.drawRoundedRect(r.adjusted(h, h, -h, -h), rad - h, rad - h);
    };
    switch (st) {
    case ButtonState::Empty:
        stroke(m_penEmpty);
        break;
    case ButtonState::Lit:
    case ButtonState::PressedLit:
        p.setPen(m_penNone);
        p.setBrush(st == ButtonState::PressedLit ? m_brCyanText : m_brCyan);
        p.drawRoundedRect(r, rad, rad);
        stroke(m_penLit);
        break;
    case ButtonState::Pressed:
        p.setPen(m_penNone);
        p.setBrush(m_brPressed);
        p.drawRoundedRect(r, rad, rad);
        stroke(m_penPressed);
        break;
    case ButtonState::Normal:
        p.setPen(m_penNone);
        p.setBrush(m_brSurface);
        p.drawRoundedRect(r, rad, rad);
        stroke(m_penNormal);
        break;
    }

    // Badge.
    if (lit) {
        p.setPen(m_penNone);
        p.setBrush(m_brOnCyan);
        p.drawRoundedRect(b.badge, m_badgeRadius, m_badgeRadius);
    } else {
        const QPen &pen = b.empty ? m_penBadgeEmpty : m_penBadge;
        const qreal h = pen.widthF() / 2.0;
        p.setPen(pen);
        p.setBrush(m_brNone);
        p.drawRoundedRect(b.badge.adjusted(h, h, -h, -h), m_badgeRadius - h, m_badgeRadius - h);
    }
    p.setPen(b.empty ? m_penTextEmpty : m_penText);
    p.setFont(m_fBadge);
    p.drawStaticText(b.badgeDigitPos, b.badgeDigit);

    if (b.empty) {
        p.setFont(m_fTag);
        p.drawStaticText(b.emptyPos, b.emptyText);
        return;
    }

    if (lit) {
        p.setPen(m_penNone);
        p.setBrush(m_brOnCyan);
        p.drawPolygon(b.triangle);
        p.setPen(m_penOnCyan);
        p.setFont(m_fTag);
        p.drawStaticText(b.nowPos, b.nowText);
    }

    // Big name in whole-pixel Pixel Operator: no antialiasing, one drawText per character.
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(lit ? m_penOnCyan : m_penText);
    p.setFont(b.nameFont);
    PixelRun::draw(p, b.nameRun);

    p.setPen(lit ? m_penOnCyan : m_penTextDim);
    p.setFont(m_fAddr);
    p.drawStaticText(b.addrPos, b.addrText);
    if (b.hasSound) {
        p.setFont(b.soundFont);
        p.drawStaticText(b.soundPos, b.soundText);
    }
}

void LiveView::paintHint(QPainter &p)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    const QPen &capPen = m_escArmed ? m_penCapArmed : m_penCap;
    const qreal h = capPen.widthF() / 2.0;
    p.setPen(capPen);
    p.setBrush(m_brNone);
    const qreal capRadius = dv(4);
    p.drawRoundedRect(m_hint.cap.adjusted(h, h, -h, -h), capRadius - h, capRadius - h);
    p.setFont(m_fHint);
    p.setPen(m_penText);
    p.drawStaticText(m_hint.capTextPos, m_hint.capText);
    p.setPen(m_escArmed ? m_penText : m_penTextDim);
    if (!m_hint.pre.isEmpty())
        p.drawStaticText(m_hint.prePos, m_hint.preText);
    p.drawStaticText(m_hint.postPos, m_hint.postText);
}
