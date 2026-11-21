#include "ui/Theme.h"

#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QtMath>

namespace {
struct FaceInfo
{
    const char *file;     // under :/fonts/
    QFont::Weight wanted; // weight to request when the family is the typographic one
    QString family;       // as reported by Qt; empty until loaded
    QFont::Weight weight = QFont::Normal;
    bool loaded = false;
};

constexpr int kFaceCount = 6;
FaceInfo g_faces[kFaceCount] = {
    {"AtkinsonHyperlegible-Regular.ttf", QFont::Normal, QString(), QFont::Normal, false},
    {"AtkinsonHyperlegible-Bold.ttf", QFont::Bold, QString(), QFont::Bold, false},
    {"BarlowSemiCondensed-Medium.ttf", QFont::Medium, QString(), QFont::Medium, false},
    {"BarlowSemiCondensed-SemiBold.ttf", QFont::DemiBold, QString(), QFont::DemiBold, false},
    {"PixelOperatorX50.ttf", QFont::Normal, QString(), QFont::Normal, false},
    {"PixelOperatorX50-Bold.ttf", QFont::Bold, QString(), QFont::Bold, false},
};
bool g_attempted = false;
bool g_allLoaded = false;
QStringList g_report;

// Q_INIT_RESOURCE must be used outside any namespace.
} // namespace

static void initUiResource() { Q_INIT_RESOURCE(x50ui); }

namespace Theme {

bool loadFonts()
{
    if (g_attempted)
        return g_allLoaded;
    g_attempted = true;
    initUiResource();
    g_allLoaded = true;
    for (int i = 0; i < kFaceCount; ++i) {
        FaceInfo &f = g_faces[i];
        const QString file = QStringLiteral(":/fonts/") + QString::fromLatin1(f.file);
        const int id = QFontDatabase::addApplicationFont(file);
        const QStringList fams = id < 0 ? QStringList() : QFontDatabase::applicationFontFamilies(id);
        g_report << QString::fromLatin1(f.file) + QStringLiteral(": ")
                        + (fams.isEmpty() ? QStringLiteral("(failed to load)") : fams.join(QStringLiteral(", ")));
        if (fams.isEmpty()) {
            f.loaded = false;
            f.family = QStringLiteral("Segoe UI");  // DESIGN 2 fallback, then the platform sans
            f.weight = f.wanted;
            g_allLoaded = false;
            continue;
        }
        f.loaded = true;
        // Take the string Qt returned and never a hard-coded one. If the family itself
        // names the style ("Barlow Semi Condensed SemiBold"), ask for weight Normal.
        f.family = fams.first();
        const bool styleInName = f.family.endsWith(QStringLiteral("Medium")) || f.family.endsWith(QStringLiteral("SemiBold"));
        f.weight = styleInName ? QFont::Normal : f.wanted;
    }
    for (const QString &line : qAsConst(g_report))
        qInfo("font: %s", qPrintable(line));
    return g_allLoaded;
}

bool fontsLoaded() { return g_attempted && g_allLoaded; }
QStringList loadedFamilyReport() { return g_report; }

QString family(Face face) { return g_faces[int(face)].family.isEmpty() ? QStringLiteral("Segoe UI") : g_faces[int(face)].family; }
QFont::Weight weight(Face face) { return g_faces[int(face)].weight; }

QFont font(Face face, int pixelSize)
{
    QFont f(family(face));
    f.setPixelSize(pixelSize);
    f.setWeight(weight(face));
    f.setHintingPreference(QFont::PreferDefaultHinting);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QFont roleFont(Role role)
{
    switch (role) {
    case Role::Body: return font(Face::AtkinsonRegular, 15);
    case Role::BodyStrong: return font(Face::AtkinsonBold, 15);
    case Role::Small: return font(Face::AtkinsonRegular, 13);
    case Role::Label: {
        QFont f = font(Face::BarlowSemiBold, 12);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.5);
        return f;
    }
    case Role::Address: {
        QFont f = font(Face::BarlowSemiBold, 15);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
        return f;
    }
    case Role::Title: return font(Face::AtkinsonBold, 20);
    }
    return font(Face::AtkinsonRegular, 15);
}

int pixelScale(qreal devicePixelRatio) { return qMax(1, qRound(devicePixelRatio)); }

QFont pixelFont(int k, bool bold)
{
    const Face face = bold ? Face::PixelBold : Face::PixelRegular;
    const bool have = g_attempted && g_faces[int(face)].loaded;
    // DESIGN 2.2 rule 5: without the face, Barlow SemiBold at the same pixel size.
    QFont f(have ? family(face) : family(Face::BarlowSemiBold));
    f.setWeight(have ? weight(face) : weight(Face::BarlowSemiBold));
    f.setPixelSize(16 * qMax(1, k));
    f.setHintingPreference(QFont::PreferNoHinting);
    // NoAntialias, not PreferAntialias: the face is drawn on a pixel grid and must produce only the
    // ink colour. CoreText (macOS, Qt 6.8) smooths and dilates outlines unless this is requested.
    f.setStyleStrategy(QFont::StyleStrategy(QFont::NoAntialias | QFont::NoSubpixelAntialias));
    f.setKerning(false);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0);
    return f;
}

void enterDeviceSpace(QPainter &p, const QPointF &logicalOrigin)
{
    // transform() alone would miss the device pixel ratio and the widget's offset in its
    // backing store; deviceTransform() carries both. resetTransform() drops only the
    // world transform, so the device pixel ratio is still applied and is undone here.
    const QTransform dt = p.deviceTransform();
    const double s = dt.m11() > 0 ? dt.m11() : 1.0;
    const QPointF origin = dt.map(QPointF(0, 0));
    const QPointF target = dt.map(logicalOrigin);
    const QPointF rounded(qRound(target.x()), qRound(target.y()));
    p.save();
    p.resetTransform();
    p.scale(1.0 / s, 1.0 / s);
    p.translate(rounded.x() - origin.x(), rounded.y() - origin.y());
}

int pixelBaseline(int top, int h, int k) { return top + qRound((h - 12 * k) / 2.0) + 9 * k; }

void applyStyleSheet(QApplication &app)
{
    initUiResource();
    QFile f(QStringLiteral(":/theme/app.qss"));
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning("theme: app.qss not found in resources");
        return;
    }
    app.setStyleSheet(QString::fromUtf8(f.readAll()));
}

} // namespace Theme
