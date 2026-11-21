#pragma once

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPoint>
#include <QPointF>
#include <QRgb>
#include <QString>
#include <QStringList>

class QApplication;

// Single C++ source for every colour token, size and font of docs/design/DESIGN.md.
// Names mirror the DESIGN tokens in camelCase (bg-live -> bgLive). assets/theme/app.qss
// keeps its own copy of the colours for the widgets styled by QSS; QPainter code
// takes everything from here and never hard-codes hex (DESIGN section 10).
namespace Theme {

// ---- DESIGN 1.1 palette (sRGB, opaque) ----
constexpr QRgb bgLive = 0xFF0B0D0F;
constexpr QRgb bar = 0xFF12171B;
constexpr QRgb surface = 0xFF1A1F24;
constexpr QRgb surfacePressed = 0xFF2A333A;
constexpr QRgb line = 0xFF2A3238;
constexpr QRgb lineStrong = 0xFF5F6C76;
constexpr QRgb text = 0xFFF2F5F7;
constexpr QRgb textDim = 0xFFAEB8C2;
constexpr QRgb textEmpty = 0xFF7E8A94;
constexpr QRgb strip = 0xFFB2BBCA;
constexpr QRgb cyan = 0xFF5DB0D0;
constexpr QRgb cyanText = 0xFF7CC8E4;
constexpr QRgb onCyan = 0xFF04141B;
constexpr QRgb red = 0xFFFF8080;
constexpr QRgb onRed = 0xFF04141B;
constexpr QRgb amber = 0xFFF0B65A;
constexpr QRgb green = 0xFF7BD39A;
constexpr QRgb bgBrowse = 0xFF121518;
constexpr QRgb panel = 0xFF1B2024;
constexpr QRgb panelRaised = 0xFF232A30;
constexpr QRgb field = 0xFF101417;
constexpr QRgb selRow = 0xFF1E4A5C;
constexpr QRgb border = 0xFF46525A;
constexpr QRgb disabledText = 0xFF6B7780;
constexpr QRgb lcdField = 0xFFE6E8E1;
constexpr QRgb lcdFieldDim = 0xFFBFC3BA;
constexpr QRgb lcdInk = 0xFF111411;
constexpr QRgb lcdBezel = 0xFF3A413B;
constexpr QRgb lcdWarn = 0xFF8A1414;  // reserved, currently unused (DESIGN 1.1)
constexpr QRgb placeholder = 0xFF8A97A2;

// ---- DESIGN 3.1 spacing scale (px, browse and settings) ----
constexpr int s1 = 4, s2 = 8, s3 = 12, s4 = 16, s5 = 24, s6 = 32;

// ---- DESIGN 3.2 live spacing, design px at u = 1 (multiply by u) ----
constexpr int liveMargin = 24, liveGap = 20, liveInnerPadding = 16;
constexpr int liveTopBar = 56, liveHintStrip = 32, liveBadge = 44;

// ---- DESIGN 3.3 radii ----
constexpr int radiusLiveButton = 14;  // x u
constexpr int radiusBadge = 8;        // x u
constexpr int radiusChip = 6;         // x u (live tag, mode chip)
constexpr int radiusControl = 6;      // px
constexpr int radiusPanel = 8;        // px
constexpr int radiusPill = 4;         // px

// ---- DESIGN 3.4 border widths ----
constexpr int borderLiveNormal = 2, borderLiveLit = 6, borderLiveEmpty = 2, borderLivePressed = 4;  // x u
constexpr int borderControl = 1, borderFocus = 2, selectedRowAccent = 4;                              // px

// ---- DESIGN 4 and 7.1 window and browse layout (logical px) ----
constexpr int browseDefaultWidth = 1180, browseDefaultHeight = 760;
constexpr int browseMinWidth = 1000, browseMinHeight = 700;
constexpr int browseMargin = 16, browseGap = 12;
constexpr int browseToolbarHeight = 40, browseStatusHeight = 24;
constexpr int browseCategoryWidth = 200, browseSplitterHandle = 6;
constexpr int browseTransportHeight = 56, browseSetEditorHeight = 200;
constexpr int browseCategoryMinWidth = 160, browseCategoryMaxWidth = 260;
constexpr int browseCategoryTitleHeight = 24, browseCategoryRowHeight = 32, browseCategorySeparatorHeight = 9;

// ---- DESIGN 7.3-7.8 toolbar, kind toggles, transport, result list (logical px) ----
constexpr int kindToggleWidth = 100, readNamesWidth = 120, settingsWidth = 100, liveWidth = 100;
constexpr int transportPrevWidth = 88, transportNextWidth = 88, transportPlayWidth = 120, transportStarWidth = 88;
constexpr int resultHeaderHeight = 28, resultRowHeight = 36;
// Result columns (DESIGN 7.7 table: Star 36, Address 112, Edited 60, Category 140, Used 44). The row
// padding (resultCellPadding) is added outside the Star column on the left and the Used column on
// the right only; the columns in between start at their own left edge.
constexpr int colStar = 36, colAddress = 112, colEdited = 60, colCategory = 140, colUsed = 44;
constexpr int resultStarSize = 18, resultCellPadding = 12;

// ---- DESIGN 7.2 display strip, in units of k (device px per font pixel) ----
constexpr int stripHeightK = 74;      // strip height = 74k device px
constexpr int stripPanelWidthK = 480; // LCD panel box
constexpr int stripBezelK = 2;        // bezel thickness; field is 476k x 70k
constexpr int stripLedDiameter = 16, stripLedGap = 12;  // px, connection zone
constexpr int stripLabelPx = 15;      // `label` role pixel size
constexpr double stripLabelTracking = 1.5;

// ---- Fonts (DESIGN 2) ----
// The six font files, in the order they are loaded.
enum class Face { AtkinsonRegular, AtkinsonBold, BarlowMedium, BarlowSemiBold, PixelRegular, PixelBold };

// Loads the six TTFs from the resources (idempotent; the first call logs the family
// names Qt reported). Returns true when every file loaded. Needs a QGuiApplication.
bool loadFonts();
bool fontsLoaded();
// Family names exactly as QFontDatabase::addApplicationFont reported them, one entry
// per file ("<file>: <family>[, <family>]"), for the startup log and the tests.
QStringList loadedFamilyReport();
// The family string used for a face and the weight to ask for. Taken from what Qt
// returned (DESIGN 2: Barlow may register as "Barlow Semi Condensed SemiBold" on
// Windows); falls back to "Segoe UI" if the file failed to load.
QString family(Face face);
QFont::Weight weight(Face face);
// A font of the face at an integer pixel size, DESIGN 2.1 hinting default, AA on.
QFont font(Face face, int pixelSize);

// DESIGN 2.1 browse and settings roles (px sizes are design px).
enum class Role { Body, BodyStrong, Small, Label, Address, Title };
QFont roleFont(Role role);

// ---- Pixel font rendering (DESIGN 2.2) ----
// k = max(1, round(devicePixelRatio)): device pixels per font pixel for browse LCDs.
int pixelScale(qreal devicePixelRatio);
// The Pixel Operator X50 font at 16*k device px (Regular or Bold), no hinting, no
// subpixel AA, kerning off, no letter spacing. Falls back to Barlow SemiBold at the
// same pixel size if the face is missing. `k` is whole device pixels per font pixel.
QFont pixelFont(int k, bool bold);
// Rule 1: maps `logicalOrigin` through the painter's device transform, rounds it to
// whole device pixels, then save() and a transform under which one painter unit is
// exactly one device pixel and (0, 0) is that rounded device pixel. The caller paints
// with integer coordinates relative to the origin, then calls p.restore().
void enterDeviceSpace(QPainter &p, const QPointF &logicalOrigin);
// Rule 4: baseline for ink centred in a box of height h (device px) whose top is `top`.
int pixelBaseline(int top, int h, int k);

// Applies assets/theme/app.qss (from the resources) to the application.
void applyStyleSheet(QApplication &app);

} // namespace Theme
