// T3.3b: Favourites and Recent in the browse category list (SPEC 4.1, DESIGN 7.6 and 7.7).
// No bytes are asserted here except "nothing was sent": every action in this file is a view
// or star action, and MIDI goes out only on explicit send actions (docs/MIDI.md section 3
// describes what a send looks like; the view-only paths must produce none). Registered twice
// in CMake (1x and QT_SCALE_FACTOR=2) for the screenshot smoke.
#include <memory>

#include <QApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPixmap>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include "core/Catalogue.h"
#include "core/Library.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/ResultDelegate.h"
#include "ui/ResultModel.h"
#include "ui/ResultTable.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
using K = SoundAddress::Kind;
using B = SoundAddress::Bank;
A combi(B b, int n) { return A(K::Combi, b, n); }
A prog(B b, int n) { return A(K::Prog, b, n); }
} // namespace

class TestFavouritesRecent : public QObject {
    Q_OBJECT

    Catalogue m_catalogue;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Library> m_library;
    std::unique_ptr<FakeX50> m_fake;
    std::unique_ptr<X50Link> m_link;
    std::unique_ptr<BrowseView> m_view;

    BrowseView &view() { return *m_view; }
    ResultModel *model() { return m_view->resultModel(); }
    QListWidget *cats() { return m_view->categoryList(); }
    ResultTable *table() { return static_cast<ResultTable *>(m_view->resultTable()); }

private slots:
    void initTestCase()
    {
        Theme::loadFonts();
        Theme::applyStyleSheet(*static_cast<QApplication *>(QCoreApplication::instance()));
        QString err;
        QVERIFY2(m_catalogue.loadFromResource(&err), qPrintable(err));
    }

    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        m_library = std::make_unique<Library>();
        m_library->load(m_dir->path());
        m_fake = std::make_unique<FakeX50>();
        m_link = std::make_unique<X50Link>(m_fake.get());
        m_link->setPortNames(QStringLiteral("X50"));
        m_view = std::make_unique<BrowseView>(&m_catalogue, m_library.get(), m_link.get());
        // MainWindow records uses on `sent` (not BrowseView); do the same here.
        connect(m_link.get(), &X50Link::sent, m_library.get(), [this](const SoundAddress &a) { m_library->recordUse(a); });
        m_view->resize(1000, 560);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view.get()));
    }
    void cleanup()
    {
        m_link->stop();
        m_view.reset();
        m_link.reset();
        m_fake.reset();
        m_library.reset();
        m_dir.reset();
    }

    void entriesAndInitialSelection()
    {
        QCOMPARE(cats()->item(0)->text(), QStringLiteral("Favourites"));
        QCOMPARE(cats()->item(1)->text(), QStringLiteral("Recent"));
        QVERIFY(!(cats()->item(2)->flags() & Qt::ItemIsSelectable));
        QVERIFY(!(cats()->item(2)->flags() & Qt::ItemIsEnabled));
        QCOMPARE(cats()->item(3)->text(), QStringLiteral("All"));
        QCOMPARE(cats()->currentRow(), 3);
        QVERIFY(view().source() == BrowseView::Source::Browse);
        QCOMPARE(view().resultCount(), m_catalogue.size());
        QCOMPARE(view().categoryTitle(), QStringLiteral("CATEGORY (PROG)"));
        QCOMPARE(view().categoryNames().first(), QStringLiteral("All"));
        // The separator row is 9 high through the delegate.
        const QModelIndex sep = cats()->model()->index(2, 0);
        QCOMPARE(cats()->itemDelegate()->sizeHint(QStyleOptionViewItem(), sep).height(), 9);
    }

    void separatorSkippedByKeyboard()
    {
        cats()->setFocus();
        cats()->setCurrentRow(1);  // Recent
        QTest::keyClick(cats(), Qt::Key_Down);
        QCOMPARE(cats()->currentRow(), 3);  // All, not the separator
        QTest::keyClick(cats(), Qt::Key_Up);
        QCOMPARE(cats()->currentRow(), 1);
        QVERIFY(view().source() == BrowseView::Source::Recent);
        QTest::keyClick(cats(), Qt::Key_Up);
        QCOMPARE(cats()->currentRow(), 0);
        QVERIFY(view().source() == BrowseView::Source::Favourites);
    }

    void favouritesFilterAndOrder()
    {
        // Starred out of order: the view is catalogue order, never starring order.
        m_library->setFavourite(prog(B::B, 5), true);
        m_library->setFavourite(combi(B::C, 7), true);
        m_library->setFavourite(combi(B::A, 3), true);
        view().setSource(BrowseView::Source::Favourites);
        QCOMPARE(view().resultCount(), 3);
        QCOMPARE(view().results(), (QVector<A>{combi(B::A, 3), combi(B::C, 7), prog(B::B, 5)}));
        // Kind toggles AND.
        view().combisToggle()->setChecked(false);
        QCOMPARE(view().results(), (QVector<A>{prog(B::B, 5)}));
        view().combisToggle()->setChecked(true);
        // Search ANDs.
        view().setSearchText(QStringLiteral("B005"));
        QCOMPARE(view().results(), (QVector<A>{prog(B::B, 5)}));
        // Prev/Next and Enter follow the order shown (no link needed to check the highlight).
        view().setSearchText(QString());
        view().highlight(combi(B::A, 3));
        view().step(1);
        QCOMPARE(*view().highlighted(), combi(B::C, 7));
    }

    void recentOrderAfterSends()
    {
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        const QVector<A> order{prog(B::A, 10), combi(B::B, 2), prog(B::C, 99), combi(B::B, 2)};
        for (const A &a : order) {
            m_link->send(a);
            QTRY_COMPARE_WITH_TIMEOUT(sent.count(), int(&a - order.constData()) + 1, 3000);
        }
        view().setSource(BrowseView::Source::Recent);
        // Most recent first, not catalogue order; a repeat moves to the top, not twice.
        QCOMPARE(view().results(), (QVector<A>{combi(B::B, 2), prog(B::C, 99), prog(B::A, 10)}));
        QCOMPARE(model()->index(0, ResultModel::ColUsed).data().toString(), QStringLiteral("x2"));
        // Kind toggle and search combine.
        view().combisToggle()->setChecked(false);
        QCOMPARE(view().results(), (QVector<A>{prog(B::C, 99), prog(B::A, 10)}));
        view().combisToggle()->setChecked(true);
        view().setSearchText(QStringLiteral("A010"));
        QCOMPARE(view().results(), (QVector<A>{prog(B::A, 10)}));
    }

    void emptyStates()
    {
        view().setSource(BrowseView::Source::Favourites);
        QCOMPARE(view().resultCount(), 0);
        QCOMPARE(view().emptyText(), QStringLiteral("No favourites yet. Star a sound with Ctrl+D or the star."));
        view().setSource(BrowseView::Source::Recent);
        QCOMPARE(view().emptyText(), QStringLiteral("Nothing played yet. Sounds you send appear here."));

        // Entries exist but search or kinds hide them all.
        m_library->setFavourite(combi(B::A, 3), true);
        m_library->recordUse(combi(B::A, 3));
        view().setSource(BrowseView::Source::Favourites);
        QCOMPARE(view().resultCount(), 1);
        view().setSearchText(QStringLiteral("zzqq"));
        QCOMPARE(view().resultCount(), 0);
        QCOMPARE(view().emptyText(), QStringLiteral("No sounds match."));
        view().setSearchText(QString());
        view().combisToggle()->setChecked(false);
        QCOMPARE(view().emptyText(), QStringLiteral("No sounds match."));
        view().combisToggle()->setChecked(true);
        view().setSource(BrowseView::Source::Recent);
        view().setSearchText(QStringLiteral("zzqq"));
        QCOMPARE(view().emptyText(), QStringLiteral("No sounds match."));
        view().setSearchText(QString());
        // Any other category.
        view().setSource(BrowseView::Source::Browse);
        view().setSearchText(QStringLiteral("zzqq"));
        QCOMPARE(view().emptyText(), QStringLiteral("No sounds match."));
        view().setSearchText(QString());
        // Back to a populated view restores nothing odd.
        view().setSource(BrowseView::Source::Recent);
        QCOMPARE(view().resultCount(), 1);
    }

    void starClickKeepsRowUntilRebuild()
    {
        m_library->setFavourite(combi(B::A, 3), true);
        m_library->setFavourite(combi(B::A, 4), true);
        view().setSource(BrowseView::Source::Favourites);
        QCOMPARE(view().resultCount(), 2);
        view().highlight(combi(B::A, 3));
        const QRect cell = table()->visualRect(model()->index(0, ResultModel::ColStar));
        const QPoint pos = ResultDelegate::starHitRect(cell).center();
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(!m_library->isFavourite(combi(B::A, 3)));
        QCOMPARE(view().resultCount(), 2);  // the row stays, outline star
        QVERIFY(!model()->index(0, 0).data(ResultModel::FavouriteRole).toBool());
        QCOMPARE(*view().highlighted(), combi(B::A, 3));
        // A second click restores it in place.
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
        QVERIFY(m_library->isFavourite(combi(B::A, 3)));
        QCOMPARE(view().resultCount(), 2);
        // Unstar, then change a kind toggle: the rebuild drops it.
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
        QCOMPARE(view().resultCount(), 2);
        view().setSearchText(QStringLiteral("a"));
        view().setSearchText(QString());
        QCOMPARE(view().results(), (QVector<A>{combi(B::A, 4)}));
        // Same in Recent: only the star toggles, nothing else changes.
        m_library->recordUse(combi(B::A, 4));
        m_library->recordUse(prog(B::A, 1));
        view().setSource(BrowseView::Source::Recent);
        QCOMPARE(view().resultCount(), 2);
        const int row = model()->rowOf(combi(B::A, 4));
        const QPoint p2 = ResultDelegate::starHitRect(table()->visualRect(model()->index(row, ResultModel::ColStar))).center();
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, p2);
        QVERIFY(!m_library->isFavourite(combi(B::A, 4)));
        QCOMPARE(view().resultCount(), 2);
        QCOMPARE(model()->rowOf(combi(B::A, 4)), row);
    }

    void recentKeepsOrderAndHighlightOnSend()
    {
        m_library->recordUse(prog(B::A, 1));
        m_library->recordUse(prog(B::A, 2));
        view().setSource(BrowseView::Source::Recent);
        QCOMPARE(view().results(), (QVector<A>{prog(B::A, 2), prog(B::A, 1)}));
        view().highlight(prog(B::A, 1));
        m_library->recordUse(prog(B::A, 1));  // what a send does
        QCOMPARE(*view().highlighted(), prog(B::A, 1));  // highlight kept
        QCOMPARE(model()->index(model()->rowOf(prog(B::A, 1)), ResultModel::ColUsed).data().toString(), QStringLiteral("x2"));
        // Rebuilding shows the new order.
        view().setSource(BrowseView::Source::Browse);
        view().setSource(BrowseView::Source::Recent);
        QCOMPARE(view().results(), (QVector<A>{prog(B::A, 1), prog(B::A, 2)}));
    }

    void noMidiFromViewActions()
    {
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        m_fake->clearReceived();
        m_library->setFavourite(combi(B::A, 3), true);
        view().setSource(BrowseView::Source::Favourites);
        view().highlight(combi(B::A, 3));
        view().setSource(BrowseView::Source::Recent);
        cats()->setFocus();
        QTest::keyClick(cats(), Qt::Key_Down);
        QTest::keyClick(cats(), Qt::Key_Up);
        view().setSearchText(QStringLiteral("x"));
        view().combisToggle()->setChecked(false);
        view().toggleFavouriteHighlighted();
        QTest::qWait(150);
        QVERIFY(m_fake->received().isEmpty());
    }

    // R-T3.3b: the separator row is never the current row, by PgUp/PgDn/Home/End or by a click.
    void separatorNeverCurrent()
    {
        cats()->setFocus();
        const Qt::Key keys[] = {Qt::Key_PageUp, Qt::Key_PageDown, Qt::Key_Home, Qt::Key_End, Qt::Key_Up, Qt::Key_Down};
        for (int start : {0, 1, 3}) {
            for (Qt::Key k : keys) {
                cats()->setCurrentRow(start);
                for (int n = 0; n < 3; ++n) {
                    QTest::keyClick(cats(), k);
                    QVERIFY2(cats()->currentRow() != 2, qPrintable(QStringLiteral("key %1 from row %2").arg(int(k)).arg(start)));
                    QVERIFY(cats()->currentRow() >= 0);
                }
            }
        }
        // A mouse click on the separator changes nothing.
        cats()->setCurrentRow(1);
        QVERIFY(view().source() == BrowseView::Source::Recent);
        const QRect sep = cats()->visualItemRect(cats()->item(2));
        QVERIFY(sep.isValid() && sep.height() == 9);
        QTest::mouseClick(cats()->viewport(), Qt::LeftButton, Qt::NoModifier, sep.center());
        QCOMPARE(cats()->currentRow(), 1);
        QVERIFY(view().source() == BrowseView::Source::Recent);
        cats()->setCurrentRow(3);
        QTest::mouseClick(cats()->viewport(), Qt::LeftButton, Qt::NoModifier, sep.center());
        QCOMPARE(cats()->currentRow(), 3);
        QVERIFY(view().source() == BrowseView::Source::Browse);
        // Nothing sent (no link started here, so nothing could be; FakeX50 saw no bytes).
        QVERIFY(m_fake->received().isEmpty());
    }

    // R-T3.3b: Prev/Next follow the order on screen, do not wrap, and Enter sends the highlighted
    // row. The sends are observed through X50Link::sent (the byte sequences are MIDI.md section 3's
    // and are covered in test_x50_link; here the claim is which sound and how many).
    void prevNextNoWrapAndEnterOrder_data()
    {
        QTest::addColumn<int>("source");
        QTest::newRow("favourites") << int(BrowseView::Source::Favourites);
        QTest::newRow("recent") << int(BrowseView::Source::Recent);
    }
    void prevNextNoWrapAndEnterOrder()
    {
        QFETCH(int, source);
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        QVector<A> shown;
        if (source == int(BrowseView::Source::Favourites)) {
            m_library->setFavourite(prog(B::B, 5), true);
            m_library->setFavourite(combi(B::C, 7), true);
            m_library->setFavourite(combi(B::A, 3), true);
            shown = {combi(B::A, 3), combi(B::C, 7), prog(B::B, 5)};  // catalogue order
        } else {
            m_library->recordUse(prog(B::B, 5));
            m_library->recordUse(combi(B::C, 7));
            m_library->recordUse(combi(B::A, 3));
            shown = {combi(B::A, 3), combi(B::C, 7), prog(B::B, 5)};  // most recent first
        }
        view().setSource(static_cast<BrowseView::Source>(source));
        QCOMPARE(view().results(), shown);
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        const auto lastSent = [&sent]() { return sent.last().at(0).value<SoundAddress>(); };

        view().highlight(shown[0]);
        view().step(-1);  // before the first row: no wrap, nothing sent
        QTest::qWait(60);
        QCOMPARE(sent.count(), 0);
        QCOMPARE(*view().highlighted(), shown[0]);
        for (int i = 1; i < 3; ++i) {
            view().step(1);
            QTRY_COMPARE_WITH_TIMEOUT(sent.count(), i, 3000);
            QCOMPARE(lastSent(), shown[i]);
            QCOMPARE(*view().highlighted(), shown[i]);
        }
        view().step(1);  // after the last row: no wrap, nothing sent
        QTest::qWait(60);
        QCOMPARE(sent.count(), 2);
        QCOMPARE(*view().highlighted(), shown[2]);
        // Recent does not re-order while shown, so Prev still follows the screen order.
        QCOMPARE(view().results(), shown);
        view().step(-1);
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 3, 3000);
        QCOMPARE(lastSent(), shown[1]);
        QCOMPARE(view().results(), shown);
        // Enter on the table sends the highlighted row, in this view's order.
        view().highlight(shown[0]);
        table()->setFocus();
        QTest::keyClick(table(), Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(sent.count(), 4, 3000);
        QCOMPARE(lastSent(), shown[0]);
        QTest::keyClick(table(), Qt::Key_Down);  // highlight only
        QTest::qWait(60);
        QCOMPARE(sent.count(), 4);
    }

    // R-T3.3b: Ctrl+D on a row that is no longer a favourite stars it again; the row stays in place.
    void ctrlDOnFormerFavourite()
    {
        m_library->setFavourite(combi(B::A, 3), true);
        m_library->setFavourite(combi(B::A, 4), true);
        view().setSource(BrowseView::Source::Favourites);
        view().highlight(combi(B::A, 3));
        table()->setFocus();
        QTest::keyClick(table(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(!m_library->isFavourite(combi(B::A, 3)));
        QCOMPARE(view().resultCount(), 2);  // stays until a rebuild
        QCOMPARE(*view().highlighted(), combi(B::A, 3));
        QTest::keyClick(table(), Qt::Key_D, Qt::ControlModifier);  // the row is no longer a favourite: starred again
        QVERIFY(m_library->isFavourite(combi(B::A, 3)));
        QCOMPARE(view().resultCount(), 2);
        // From the search field too.
        QTest::keyClick(static_cast<QWidget *>(view().searchField()), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(!m_library->isFavourite(combi(B::A, 3)));
        QTest::keyClick(static_cast<QWidget *>(view().searchField()), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(m_library->isFavourite(combi(B::A, 3)));
        QVERIFY(m_fake->received().isEmpty());
    }

    // R-T3.3b: Ctrl+1..8 from Favourites and Recent put the highlighted sound in that slot; a slot
    // beyond the set size is refused with a status error and changes nothing.
    void ctrlDigitFromFavouritesAndRecent()
    {
        QString id;
        QVERIFY(m_library->createSet(QStringLiteral("Fri"), 6, &id).ok());
        m_library->setFavourite(combi(B::A, 3), true);
        m_library->recordUse(prog(B::B, 5));
        QStringList status;
        QList<bool> errors;
        connect(&view(), &BrowseView::statusMessage, this, [&](const QString &t, bool e) { status << t; errors << e; });

        view().setSource(BrowseView::Source::Favourites);
        view().highlight(combi(B::A, 3));
        table()->setFocus();
        QTest::keyClick(table(), Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(m_library->setById(id)->slot(0).address.value(), combi(B::A, 3));

        view().setSource(BrowseView::Source::Recent);
        view().highlight(prog(B::B, 5));
        QTest::keyClick(static_cast<QWidget *>(view().searchField()), Qt::Key_6, Qt::ControlModifier);
        QCOMPARE(m_library->setById(id)->slot(5).address.value(), prog(B::B, 5));
        QVERIFY(!errors.isEmpty() && !errors.last());

        QTest::keyClick(table(), Qt::Key_8, Qt::ControlModifier);  // the set has 6 slots
        QVERIFY(errors.last());
        QVERIFY(!m_library->setById(id)->slot(7).address.has_value());
        // The shortcut also works with the category list focused (the highlight is in the Recent view).
        cats()->setFocus();
        QTest::keyClick(cats(), Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(m_library->setById(id)->slot(1).address.value(), prog(B::B, 5));
        QVERIFY(m_fake->received().isEmpty());
    }

    void screenshotSmoke()
    {
        m_library->setFavourite(combi(B::A, 3), true);
        view().setSource(BrowseView::Source::Favourites);
        const QPixmap shot = view().grab();
        const qreal dpr = view().devicePixelRatioF();
        QCOMPARE(shot.size(), QSize(qRound(1000 * dpr), qRound(560 * dpr)));
        // The category list shows the amber star glyph of the Favourites row.
        const QImage img = cats()->grab().toImage();
        bool amber = false;
        const QRgb want = Theme::amber;
        for (int y = 0; y < img.height() && !amber; ++y)
            for (int x = 0; x < img.width(); ++x)
                if (img.pixel(x, y) == want) { amber = true; break; }
        QVERIFY2(amber, "no amber star pixel in the category list");
        // The Recent row (unselected while Favourites is current): its label paints `text` inside the
        // item rect, and the clock glyph paints `text-dim` (DESIGN 7.6), both to the right of the star
        // column and inside the row.
        const QRect recent = cats()->visualItemRect(cats()->item(1));
        const qreal cd = img.devicePixelRatio();
        const auto device = [cd](int v) { return qRound(v * cd); };
        const auto near = [](QRgb c, QRgb want, int tol) {
            return qAbs(qRed(c) - qRed(want)) <= tol && qAbs(qGreen(c) - qGreen(want)) <= tol && qAbs(qBlue(c) - qBlue(want)) <= tol;
        };
        bool textPx = false, clockPx = false;
        for (int y = device(recent.top()); y < device(recent.bottom() + 1); ++y) {
            for (int x = device(recent.left()); x < device(recent.right() + 1); ++x) {
                const QRgb c = img.pixel(x, y);
                const int lx = qRound(x / cd) - recent.left();  // logical offset inside the row
                if (lx >= Theme::s3 + 14 + Theme::s2 && near(c, Theme::text, 0))
                    textPx = true;
                if (lx >= Theme::s3 && lx < Theme::s3 + 14 && near(c, Theme::textDim, 24))
                    clockPx = true;
            }
        }
        QVERIFY2(textPx, "no exact `text` pixel in the Recent label rect");
        QVERIFY2(clockPx, "no `text-dim` pixel in the clock glyph box");
        // Empty state paints (Recent is empty).
        view().setSource(BrowseView::Source::Recent);
        const QImage empty = table()->viewport()->grab().toImage();
        bool dim = false;
        for (int y = 0; y < empty.height() && !dim; ++y)
            for (int x = 0; x < empty.width(); ++x)
                if (empty.pixel(x, y) != empty.pixel(0, 0)) { dim = true; break; }
        QVERIFY2(dim, "the empty-state text did not paint");
    }
};

QTEST_MAIN(TestFavouritesRecent)
#include "test_favourites_recent.moc"
