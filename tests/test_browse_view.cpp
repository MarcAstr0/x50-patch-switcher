// T3.2: BrowseView against FakeX50 and X50Link (headless). Expected MIDI bytes are typed
// from docs/MIDI.md: section 3 (bank select order and values for Bank Map GM, which is
// FakeX50's default; global channel 1 so n = 0) and section 4 (Mode Change).
// Registered twice in CMake (1x and QT_SCALE_FACTOR=2) for the screenshot smoke.
#include <algorithm>
#include <memory>

#include <QApplication>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include "core/Catalogue.h"
#include "core/Library.h"
#include "core/SearchQuery.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/DisplayStrip.h"
#include "ui/MainWindow.h"
#include "ui/ResultModel.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
using K = SoundAddress::Kind;
using B = SoundAddress::Bank;

SoundAddress combi(B b, int n) { return A(K::Combi, b, n); }

QByteArray H(std::initializer_list<int> list)
{
    QByteArray b;
    for (int v : list)
        b.append(static_cast<char>(v));
    return b;
}

int countStatus(const QList<QByteArray> &msgs, int status)
{
    int n = 0;
    for (const QByteArray &m : msgs)
        if (!m.isEmpty() && (static_cast<unsigned char>(m.at(0)) & 0xF0) == status)
            ++n;
    return n;
}
int programChanges(const FakeX50 &f) { return countStatus(f.received(), 0xC0); }
int bankSelects(const FakeX50 &f) { return countStatus(f.received(), 0xB0); }
} // namespace

class TestBrowseView : public QObject {
    Q_OBJECT

    Catalogue m_catalogue;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Library> m_library;
    std::unique_ptr<FakeX50> m_fake;
    std::unique_ptr<X50Link> m_link;
    std::unique_ptr<BrowseView> m_view;
    QStringList m_statuses;
    QList<bool> m_statusErrors;

    BrowseView &view() { return *m_view; }
    QLineEdit *search() { return m_view->searchField(); }
    QTableView *table() { return m_view->resultTable(); }
    ResultModel *model() { return m_view->resultModel(); }

    void key(QWidget *w, Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier) { QTest::keyClick(w, k, m); }
    void autoRepeat(QWidget *w, Qt::Key k, Qt::KeyboardModifiers m = Qt::NoModifier)
    {
        QKeyEvent ev(QEvent::KeyPress, k, m, QString(), true /*autorepeat*/, 1);
        QApplication::sendEvent(w, &ev);
    }
    void connectLink()
    {
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        m_fake->clearReceived();
    }
    // A query that gives a short, stable list of both kinds.
    void searchPiano() { view().setSearchText(QStringLiteral("piano")); }
    // A double click as the window system delivers it: press, release, double-click, release.
    void doubleClick(QWidget *viewport, const QPoint &pos)
    {
        const QPointF global = viewport->mapToGlobal(pos);
        auto send = [&](QEvent::Type type, Qt::MouseButtons buttons) {
            QMouseEvent ev(type, QPointF(pos), global, Qt::LeftButton, buttons, Qt::NoModifier);
            QApplication::sendEvent(viewport, &ev);
        };
        send(QEvent::MouseButtonPress, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, Qt::NoButton);
        send(QEvent::MouseButtonDblClick, Qt::LeftButton);
        send(QEvent::MouseButtonRelease, Qt::NoButton);
    }
    QPoint cellCenter(int row, int col = ResultModel::ColName)
    {
        return table()->visualRect(model()->index(row, col)).center();
    }

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
        m_statuses.clear();
        m_statusErrors.clear();
        connect(m_view.get(), &BrowseView::statusMessage, this, [this](const QString &t, bool e) {
            m_statuses << t;
            m_statusErrors << e;
        });
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

    // SPEC 4.1: search box, all three kinds on, whole catalogue in catalogue order (G2).
    void startState()
    {
        QCOMPARE(view().resultCount(), m_catalogue.size());
        QVERIFY(view().combisToggle()->isChecked() && view().programsToggle()->isChecked() && view().gmToggle()->isChecked());
        const QVector<SoundAddress> r = view().results();
        QVERIFY(std::is_sorted(r.begin(), r.end()));
        QCOMPARE(view().highlighted().has_value(), false);
        QVERIFY(!view().playButton()->isEnabled());
        QCOMPARE(view().categoryTitle(), QStringLiteral("CATEGORY (PROG)"));
        QCOMPARE(view().categoryNames().size(), 17);  // All + 16
        QCOMPARE(view().categoryNames().first(), QStringLiteral("All"));
    }

    void searchFocusedOnStart()
    {
        // Focus is recorded on the window even when it is not the active one (offscreen).
        QCOMPARE(view().focusWidget(), search());
    }

    void typingFilters()
    {
        QTest::keyClicks(search(), QStringLiteral("piano"));
        const int expected = SearchQuery::parse(QStringLiteral("piano")).search(m_catalogue).size();
        QVERIFY(expected > 10);
        QCOMPARE(view().resultCount(), expected);
        for (const SoundAddress &a : view().results())
            QVERIFY2(m_catalogue.effectiveName(a).contains(QStringLiteral("iano"), Qt::CaseInsensitive) ||
                         m_catalogue.effectiveCategory(a).contains(QStringLiteral("piano"), Qt::CaseInsensitive),
                     qPrintable(a.format()));
        QTest::keyClicks(search(), QStringLiteral("zzqq"));
        QCOMPARE(view().resultCount(), 0);
        for (int i = 0; i < 4; ++i)
            key(search(), Qt::Key_Backspace);
        QCOMPARE(view().resultCount(), expected);
        // An address token and cat: work through the same box.
        view().setSearchText(QStringLiteral("combi c096"));
        QCOMPARE(view().results(), QVector<SoundAddress>{combi(B::C, 96)});
        view().setSearchText(QString());
        QCOMPARE(view().resultCount(), m_catalogue.size());
    }

    // SPEC 4.1: "Highlight never sends."
    void highlightSendsNothing()
    {
        connectLink();
        searchPiano();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        key(search(), Qt::Key_Down);
        key(search(), Qt::Key_Down);
        QVERIFY(view().highlighted().has_value());
        // The query already highlighted row 0 (R-T3.2); two Downs move to row 2.
        QCOMPARE(view().highlighted().value(), view().results().at(2));
        key(search(), Qt::Key_Up);
        QCOMPARE(view().highlighted().value(), view().results().at(1));
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, cellCenter(3));
        QCOMPARE(view().highlighted().value(), view().results().at(3));
        key(table(), Qt::Key_Down);
        key(table(), Qt::Key_PageDown);  // native paging in the list, not Next
        view().highlight(view().results().at(2));
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 0);
        QVERIFY(view().playButton()->isEnabled());
    }

    // MIDI.md 4 (Mode Change to COMBI PLAY, the fake starts in PROG PLAY) then 3 (CC0 3F,
    // CC32 02, PC 96 for combination bank C with Bank Map GM).
    void enterSendsExactlyOneSelect()
    {
        connectLink();
        view().setSearchText(QStringLiteral("combi c096"));
        key(search(), Qt::Key_Down);
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        key(search(), Qt::Key_Return);
        QTRY_COMPARE(sent.count(), 1);
        const QList<QByteArray> got = m_fake->received();
        QCOMPARE(got.size(), 4);
        QCOMPARE(got.at(0), H({0xF0, 0x42, 0x30, 0x7A, 0x4E, 0x00, 0xF7}));
        QCOMPARE(got.at(1), H({0xB0, 0x00, 0x3F}));
        QCOMPARE(got.at(2), H({0xB0, 0x20, 0x02}));
        QCOMPARE(got.at(3), H({0xC0, 0x60}));
        // Enter in the list sends too; the keypad Enter as well.
        m_fake->clearReceived();
        key(table(), Qt::Key_Enter, Qt::KeypadModifier);
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(programChanges(*m_fake), 1);
    }

    void doubleClickSendsExactlyOneSelect()
    {
        connectLink();
        searchPiano();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        doubleClick(table()->viewport(), cellCenter(2));
        QTRY_COMPARE(sent.count(), 1);
        QTest::qWait(60);
        QCOMPARE(sent.count(), 1);
        QCOMPARE(programChanges(*m_fake), 1);
        QCOMPARE(bankSelects(*m_fake), 2);
        QCOMPARE(sent.first().first().value<SoundAddress>(), view().results().at(2));
    }

    void playSendsExactlyOneSelect()
    {
        connectLink();
        searchPiano();
        view().highlight(view().results().at(4));
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        view().playButton()->click();
        QTRY_COMPARE(sent.count(), 1);
        QTest::qWait(60);
        QCOMPARE(programChanges(*m_fake), 1);
        QCOMPARE(m_link->currentSound().value(), view().results().at(4));
    }

    // Enter and Play with nothing highlighted do nothing but say so.
    void nothingHighlightedSendsNothing()
    {
        connectLink();
        key(search(), Qt::Key_Return);
        QTest::qWait(30);
        QVERIFY(m_fake->received().isEmpty());
        QVERIFY(!m_statuses.isEmpty());
    }

    // SPEC 4.1 is silent on wrap; SPEC 4.2 (live) says no wrap. Browse stops at the ends.
    void prevNextStepAndStopAtEnds()
    {
        connectLink();
        searchPiano();
        const QVector<SoundAddress> r = view().results();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        view().highlight(r.at(2));
        key(search(), Qt::Key_Right, Qt::ControlModifier);
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(sent.last().first().value<SoundAddress>(), r.at(3));
        QCOMPARE(view().highlighted().value(), r.at(3));
        key(search(), Qt::Key_Left, Qt::ControlModifier);
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(sent.last().first().value<SoundAddress>(), r.at(2));
        // The buttons do the same.
        view().nextButton()->click();
        QTRY_COMPARE(sent.count(), 3);
        QCOMPARE(sent.last().first().value<SoundAddress>(), r.at(3));
        view().prevButton()->click();
        QTRY_COMPARE(sent.count(), 4);
        // Ends: no send, no wrap, highlight stays.
        view().highlight(r.last());
        m_fake->clearReceived();
        key(search(), Qt::Key_Right, Qt::ControlModifier);
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(view().highlighted().value(), r.last());
        view().highlight(r.first());
        key(search(), Qt::Key_Left, Qt::ControlModifier);
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(view().highlighted().value(), r.first());
        QCOMPARE(sent.count(), 4);
    }

    // SPEC 4.1: PgUp/PgDn step when the list does not have focus; in the list they page.
    void pageKeysStepOutsideTheList()
    {
        connectLink();
        searchPiano();
        const QVector<SoundAddress> r = view().results();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        view().highlight(r.at(1));
        key(search(), Qt::Key_PageDown);
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(sent.last().first().value<SoundAddress>(), r.at(2));
        key(search(), Qt::Key_PageUp);
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(sent.last().first().value<SoundAddress>(), r.at(1));
        key(view().playButton(), Qt::Key_PageDown);
        QTRY_COMPARE(sent.count(), 3);
        m_fake->clearReceived();
        key(table(), Qt::Key_PageDown);
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 3);
    }

    // A held key delivers auto-repeat events: none of them sends. Each real key event
    // sends at most once (the link coalesces sends that arrive during a Mode Change wait).
    void autoRepeatDoesNotBurst()
    {
        connectLink();
        searchPiano();
        const QVector<SoundAddress> r = view().results();
        view().highlight(r.at(0));
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        for (int i = 0; i < 20; ++i) {
            autoRepeat(search(), Qt::Key_Right, Qt::ControlModifier);
            autoRepeat(search(), Qt::Key_PageDown);
            autoRepeat(search(), Qt::Key_Return);
            autoRepeat(table(), Qt::Key_Return);
        }
        QTest::qWait(120);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 0);
        QCOMPARE(view().highlighted().value(), r.at(0));
        // Three separate presses in a row: three highlight steps, never more than three sends.
        key(search(), Qt::Key_Right, Qt::ControlModifier);
        key(search(), Qt::Key_Right, Qt::ControlModifier);
        key(search(), Qt::Key_Right, Qt::ControlModifier);
        QCOMPARE(view().highlighted().value(), r.at(3));
        QTest::qWait(250);
        QVERIFY(programChanges(*m_fake) >= 1 && programChanges(*m_fake) <= 3);
        QVERIFY(sent.count() <= 3);
        QCOMPARE(m_link->currentSound().value(), r.at(3));
    }

    // Sends are recorded on success only (Library recents); the Used column follows.
    void recentsCountUpdatesAfterSend()
    {
        connectLink();
        view().setSearchText(QStringLiteral("combi c096"));
        view().highlight(combi(B::C, 96));
        QCOMPARE(m_library->recentCount(combi(B::C, 96)), 0);
        QCOMPARE(model()->index(0, ResultModel::ColUsed).data().toString(), QString());
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        view().playButton()->click();
        QTRY_COMPARE(sent.count(), 1);
        QCOMPARE(m_library->recentCount(combi(B::C, 96)), 1);
        QCOMPARE(model()->index(0, ResultModel::ColUsed).data().toString(), QStringLiteral("x1"));
        view().playButton()->click();
        QTRY_COMPARE(sent.count(), 2);
        QCOMPARE(model()->index(0, ResultModel::ColUsed).data().toString(), QStringLiteral("x2"));
        // The model's current-sound marker follows the link.
        QVERIFY(model()->index(0, 0).data(ResultModel::CurrentRole).toBool());
    }

    void failedSendIsNotRecorded()
    {
        // Link never started: every send fails and says so, nothing is recorded.
        view().setSearchText(QStringLiteral("combi c096"));
        view().highlight(combi(B::C, 96));
        QSignalSpy failed(m_link.get(), &X50Link::sendFailed);
        view().playButton()->click();
        QCOMPARE(failed.count(), 1);
        QCOMPARE(m_library->recentCount(combi(B::C, 96)), 0);
        QVERIFY(m_statusErrors.last());
        QVERIFY2(m_statuses.last().contains(QStringLiteral("Could not send COMBI C096")), qPrintable(m_statuses.last()));
        QVERIFY(m_fake->received().isEmpty());
    }

    // SPEC 4.1: Star toggles favourite (Ctrl+D or click).
    void ctrlDTogglesFavourite()
    {
        searchPiano();
        const SoundAddress a = view().results().at(1);
        view().highlight(a);
        QVERIFY(!m_library->isFavourite(a));
        key(search(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(m_library->isFavourite(a));
        QVERIFY(model()->index(1, 0).data(ResultModel::FavouriteRole).toBool());
        key(table(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(!m_library->isFavourite(a));
        view().starButton()->click();
        QVERIFY(m_library->isFavourite(a));
        // Clicking the star in the row toggles it too, and does not send.
        const int row = 1;
        QTest::mouseClick(table()->viewport(), Qt::LeftButton, Qt::NoModifier, cellCenter(row, ResultModel::ColStar));
        QVERIFY(!m_library->isFavourite(a));
        QVERIFY(m_fake->received().isEmpty());
        // Nothing highlighted: nothing happens.
        view().clearHighlight();
        key(search(), Qt::Key_D, Qt::ControlModifier);
        QVERIFY(m_library->favourites().isEmpty());
    }

    // SPEC 4.1: Ctrl+1..8 puts the highlighted sound in that slot of the current set.
    void ctrlNumberAddsToSlot()
    {
        QString id;
        QVERIFY(m_library->createSet(QStringLiteral("Friday"), 8, &id).ok());
        searchPiano();
        const SoundAddress a = view().results().at(2);
        view().highlight(a);
        key(search(), Qt::Key_3, Qt::ControlModifier);
        QVERIFY(m_library->currentSet()->slot(2).address.has_value());
        QCOMPARE(m_library->currentSet()->slot(2).address.value(), a);
        QVERIFY(!m_statusErrors.last());
        QVERIFY2(m_statuses.last().contains(QStringLiteral("slot 3")), qPrintable(m_statuses.last()));
        key(table(), Qt::Key_8, Qt::ControlModifier);
        QCOMPARE(m_library->currentSet()->slot(7).address.value(), a);
        // The context-menu path.
        view().highlight(view().results().at(5));
        view().addHighlightedToSlot(1);
        QCOMPARE(m_library->currentSet()->slot(0).address.value(), view().results().at(5));
        // A 6-slot set has no slot 7: refused, said in the status line.
        QVERIFY(m_library->createSet(QStringLiteral("Small"), 6).ok());
        key(search(), Qt::Key_7, Qt::ControlModifier);
        QVERIFY(m_statusErrors.last());
        QVERIFY(!m_library->currentSet()->slot(5).address.has_value());
    }

    void ctrlNumberWithoutSetOrHighlight()
    {
        searchPiano();
        view().highlight(view().results().at(0));
        key(search(), Qt::Key_1, Qt::ControlModifier);  // no set exists
        QVERIFY(m_statusErrors.last());
        QVERIFY(m_library->sets().isEmpty());
        QVERIFY(m_library->createSet(QStringLiteral("S"), 6).ok());
        view().clearHighlight();
        key(search(), Qt::Key_1, Qt::ControlModifier);
        QVERIFY(!m_library->currentSet()->slot(0).address.has_value());
    }

    // SPEC 4.2/4.1: F5 asks for the live view (nothing listens until M4).
    void f5RequestsLive()
    {
        QSignalSpy live(m_view.get(), &BrowseView::liveRequested);
        key(search(), Qt::Key_F5);
        key(table(), Qt::Key_F5);
        autoRepeat(search(), Qt::Key_F5);
        view().liveButton()->click();
        QCOMPARE(live.count(), 3);
    }

    void kindToggles()
    {
        QCOMPARE(view().resultCount(), m_catalogue.size());
        view().gmToggle()->setChecked(false);
        for (const SoundAddress &a : view().results())
            QVERIFY(a.bank() != B::G && a.bank() != B::Gd);
        QCOMPARE(view().categoryTitle(), QStringLiteral("CATEGORY (PROG)"));
        view().combisToggle()->setChecked(false);
        for (const SoundAddress &a : view().results())
            QVERIFY(a.kind() == K::Prog && a.bank() != B::G);
        QCOMPARE(view().resultCount(), 512);
        view().combisToggle()->setChecked(true);
        view().programsToggle()->setChecked(false);
        QCOMPARE(view().resultCount(), 384);
        QCOMPARE(view().categoryTitle(), QStringLiteral("CATEGORY (COMBI)"));
        QCOMPARE(view().categoryNames().size(), 17);
        QVERIFY(view().categoryNames().contains(QStringLiteral("BassSplits")));
        view().programsToggle()->setChecked(true);
        view().combisToggle()->setChecked(false);
        view().gmToggle()->setChecked(true);
        QCOMPARE(view().resultCount(), 512 + 128 + 9);
        view().programsToggle()->setChecked(false);
        QCOMPARE(view().resultCount(), 128 + 9);
        view().gmToggle()->setChecked(false);
        QCOMPARE(view().resultCount(), 0);
    }

    void categoryFilter()
    {
        QVERIFY(view().setCategory(QStringLiteral("Strings")));
        QVERIFY(view().resultCount() > 20);
        for (const SoundAddress &a : view().results())
            QCOMPARE(m_catalogue.effectiveCategory(a), QStringLiteral("Strings"));
        // Combined with the search text.
        view().setSearchText(QStringLiteral("pad"));
        int expected = 0;
        for (const Catalogue::Entry &e : m_catalogue.entries())
            if (m_catalogue.effectiveCategory(e.address) == QStringLiteral("Strings") &&
                m_catalogue.effectiveName(e.address).contains(QStringLiteral("pad"), Qt::CaseInsensitive))
                ++expected;
        QVERIFY(expected > 0);
        QCOMPARE(view().resultCount(), expected);
        view().setSearchText(QString());
        QVERIFY(view().setCategory(QString()));
        QCOMPARE(view().resultCount(), m_catalogue.size());
        // Clicking the list row filters too.
        view().categoryList()->setCurrentRow(1);
        QCOMPARE(view().category(), view().categoryNames().at(1));
        QVERIFY(view().resultCount() < m_catalogue.size());
        // Switching to combinations only changes the list; a category the new list lacks resets to All.
        QVERIFY(view().setCategory(QStringLiteral("Bass")));
        view().programsToggle()->setChecked(false);
        view().gmToggle()->setChecked(false);
        QCOMPARE(view().category(), QString());
        QVERIFY(!view().setCategory(QStringLiteral("nonsense")));
        // Category order is first appearance in the catalogue, not alphabetical (Decisions log).
        view().programsToggle()->setChecked(true);
        const QStringList names = view().categoryNames();
        QStringList sorted = names.mid(1);
        std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
        QVERIFY(names.mid(1) != sorted);
    }

    // The highlight survives typing while the sound is still in the results, else clears.
    void highlightKeptWhileFiltering()
    {
        view().setSearchText(QStringLiteral("piano"));
        view().highlight(combi(B::C, 96));
        view().setSearchText(QStringLiteral("pian"));
        QCOMPARE(view().highlighted().value(), combi(B::C, 96));
        view().setSearchText(QStringLiteral("strings"));
        QVERIFY(view().highlighted().has_value() == view().results().contains(combi(B::C, 96)));
    }

    // SPEC 4.4: the strip follows the link (own sends and panel changes); BrowseView marks the row.
    void windowFollowsLink()
    {
        MainWindow w(&m_catalogue, m_library.get(), m_link.get());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(!w.displayStrip()->isConnected());
        QCOMPARE(w.displayStrip()->texts().known, false);
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        QTRY_VERIFY(w.displayStrip()->isConnected());
        w.browseView()->setSearchText(QStringLiteral("combi c096"));
        w.browseView()->highlight(combi(B::C, 96));
        w.browseView()->playButton()->click();
        QTRY_COMPARE(w.displayStrip()->texts().bar, QStringLiteral("096: Piano &Strings"));
        QVERIFY(w.statusText().startsWith(QStringLiteral("Sent COMBI C096")));
        QVERIFY(!w.statusIsError());
        // A change on the X50's panel.
        w.browseView()->setSearchText(QString());
        m_fake->panelSelect(combi(B::A, 2));
        QTRY_COMPARE(w.displayStrip()->texts().bar, QStringLiteral("002: Vintage Pad"));
        const int row = w.browseView()->resultModel()->rowOf(combi(B::A, 2));
        QVERIFY(row >= 0);
        QVERIFY(w.browseView()->resultModel()->index(row, 0).data(ResultModel::CurrentRole).toBool());
        // Unplugged: the strip says so.
        m_fake->unplug(true);
        QTRY_VERIFY(!w.displayStrip()->isConnected());
        QVERIFY(w.statusIsError());
    }

    // R-T1.5: load errors, load warnings and save failures reach the status line, never a dialog.
    void libraryErrorsReachTheStatusLine()
    {
        {
            MainWindow w(&m_catalogue, m_library.get(), nullptr);
            QVERIFY(w.statusText().isEmpty());
            emit m_library->saveFailed(QStringLiteral("disk full"));
            QVERIFY(w.statusIsError());
            QVERIFY2(w.statusText().contains(QStringLiteral("disk full")), qPrintable(w.statusText()));
        }
        QTemporaryDir bad;
        QFile f(bad.filePath(QStringLiteral("library.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        Library broken;
        QVERIFY(!broken.load(bad.path()));
        MainWindow w(&m_catalogue, &broken, nullptr);
        QVERIFY(w.statusIsError());
        QCOMPARE(w.statusText(), broken.loadError());
        QVERIFY(!w.statusText().isEmpty());
    }

    // Screenshot smoke (1x and, via the second ctest entry, 2x) and the DESIGN 7.1 minimum size.
    void screenshotSmoke()
    {
        MainWindow w(&m_catalogue, m_library.get(), m_link.get());
        w.resize(1180, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.browseView()->setSearchText(QStringLiteral("piano"));
        w.browseView()->highlight(combi(B::B, 64));
        const QPixmap shot = w.grab();
        const qreal dpr = w.devicePixelRatioF();
        QCOMPARE(shot.size(), QSize(qRound(1180 * dpr), qRound(760 * dpr)));
        const QImage img = shot.toImage();
        QSet<QRgb> colours;
        for (int y = 0; y < img.height(); y += 7)
            for (int x = 0; x < img.width(); x += 7)
                colours.insert(img.pixel(x, y));
        QVERIFY2(colours.size() > 8, "the browse view rendered blank");
        // Background colour at the window corner is bg-browse.
        QCOMPARE(img.pixel(2, img.height() - 2), Theme::bgBrowse);
    }

    // DESIGN 7.1: at the minimum window the result list still shows 5 rows under its header.
    void minimumSizeKeepsFiveRows()
    {
        MainWindow w(&m_catalogue, m_library.get(), m_link.get());
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        w.resize(w.minimumSize());
        QTest::qWait(30);
        QCOMPARE(w.size(), w.minimumSize());
        const int viewport = w.browseView()->resultTable()->viewport()->height();
        QVERIFY2(viewport >= 5 * Theme::resultRowHeight, qPrintable(QString::number(viewport)));
        // DESIGN 7.7 and the mockup: the list's 1 px top border plus the header are 28 high.
        QCOMPARE(w.browseView()->resultTable()->horizontalHeader()->height() + Theme::borderControl, Theme::resultHeaderHeight);
    }

    // R-T3.2 decision: a changed result set highlights its first row; the highlight never
    // sends, Enter does, once. Typing alone sends nothing.
    void typingHighlightsFirstRowAndSendsNothing()
    {
        connectLink();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        QTest::keyClicks(search(), QStringLiteral("piano"));
        QTest::qWait(60);
        QVERIFY(view().resultCount() > 0);
        QCOMPARE(view().highlighted().value(), view().results().first());
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 0);
    }
    void typeThenEnterSendsFirstRowOnce()
    {
        connectLink();
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        QTest::keyClicks(search(), QStringLiteral("piano"));
        const SoundAddress first = view().results().first();
        key(search(), Qt::Key_Return);
        QTRY_COMPARE(sent.count(), 1);
        QTest::qWait(60);
        QCOMPARE(sent.count(), 1);
        QCOMPARE(sent.first().first().value<SoundAddress>(), first);
        QCOMPARE(programChanges(*m_fake), 1);
    }
    void kindsAndCategoryAlsoHighlightFirstRow()
    {
        view().setKinds(true, false, false);
        QCOMPARE(view().highlighted().value(), view().results().first());
        view().clearHighlight();
        QVERIFY(view().setCategory(QStringLiteral("Keyboard")));
        QVERIFY(view().resultCount() > 0);
        QCOMPARE(view().highlighted().value(), view().results().first());
    }
    // A highlighted sound that is still in the new results stays highlighted.
    void stillPresentHighlightStays()
    {
        searchPiano();
        const SoundAddress third = view().results().at(2);
        view().highlight(third);
        view().setSearchText(QStringLiteral("pian"));
        QCOMPARE(view().highlighted().value(), third);
    }
    // No results: nothing highlighted, nothing sent.
    void emptyResultsHighlightNothing()
    {
        searchPiano();
        QVERIFY(view().highlighted().has_value());
        view().setSearchText(QStringLiteral("zzqq"));
        QCOMPARE(view().resultCount(), 0);
        QVERIFY(!view().highlighted().has_value());
    }

    // R-T3.2 review: PgDn with the focus in a list pages natively and never steps (SPEC 4.1).
    void pageDownInCategoryListAtLastRowSendsNothing()
    {
        connectLink();
        QListWidget *cats = view().categoryList();
        cats->setFocus();
        cats->setCurrentRow(cats->count() - 1);
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        m_fake->clearReceived();
        key(cats, Qt::Key_PageDown);
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 0);
        QCOMPARE(cats->currentRow(), cats->count() - 1);
    }
    void pageDownInTableAfterEndSendsNothing()
    {
        connectLink();
        searchPiano();
        table()->setFocus();
        key(table(), Qt::Key_End);
        QSignalSpy sent(m_link.get(), &X50Link::sent);
        m_fake->clearReceived();
        key(table(), Qt::Key_PageDown);
        QTest::qWait(60);
        QVERIFY(m_fake->received().isEmpty());
        QCOMPARE(sent.count(), 0);
    }

    // DESIGN 7.3, 7.5, 7.7 against docs/design/mockup-browse.png: sizes of the toolbar controls
    // and the result columns.
    void toolbarAndColumnGeometry()
    {
        for (QToolButton *t : {view().combisToggle(), view().programsToggle(), view().gmToggle()}) {
            QCOMPARE(t->width(), Theme::kindToggleWidth);
            QCOMPARE(t->height(), Theme::browseToolbarHeight);
        }
        view().combisToggle()->setChecked(false);  // the unchecked state has a thinner border
        QCOMPARE(view().combisToggle()->width(), Theme::kindToggleWidth);
        QCOMPARE(view().combisToggle()->height(), Theme::browseToolbarHeight);
        view().combisToggle()->setChecked(true);
        QCOMPARE(search()->height(), Theme::browseToolbarHeight);
        QCOMPARE(view().liveButton()->height(), Theme::browseToolbarHeight);
        QCOMPARE(view().settingsButton()->height(), Theme::browseToolbarHeight);
        QCOMPARE(view().readNamesButton()->height(), Theme::browseToolbarHeight);
        QHeaderView *h = table()->horizontalHeader();
        QCOMPARE(h->sectionSize(ResultModel::ColStar), Theme::resultCellPadding + 36);  // DESIGN 7.7: Star 36
        QCOMPARE(h->sectionSize(ResultModel::ColAddress), 112);
        QCOMPARE(h->sectionSize(ResultModel::ColEdited), 60);
        QCOMPARE(h->sectionSize(ResultModel::ColCategory), 140);
        QCOMPARE(h->sectionSize(ResultModel::ColUsed), 44 + Theme::resultCellPadding);
        // The toolbar sits 12 px above the main area; the list starts right under it.
        QCOMPARE(table()->mapTo(&view(), QPoint(0, 0)).y(), Theme::browseToolbarHeight + Theme::browseGap);
    }

    // The window geometry and the category splitter are remembered between runs
    // (QSettings file in the config dir, never the registry).
    void layoutRoundTripInConfigDir()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        {
            MainWindow w(&m_catalogue, m_library.get(), nullptr);
            w.restoreLayout(dir.path(), QRect(0, 0, 3000, 2000));  // nothing saved yet: defaults
            QCOMPARE(w.size(), QSize(Theme::browseDefaultWidth, Theme::browseDefaultHeight));
            w.resize(1234, 777);
            w.move(40, 50);
            w.show();
            QVERIFY(QTest::qWaitForWindowExposed(&w));
            w.browseView()->setCategoryWidth(230);
            QTest::qWait(30);
            w.close();  // saves
        }
        QVERIFY(QFile::exists(dir.filePath(MainWindow::layoutFileName())));
        MainWindow w2(&m_catalogue, m_library.get(), nullptr);
        w2.restoreLayout(dir.path(), QRect(0, 0, 3000, 2000));  // the offscreen screen is only 800x600
        QCOMPARE(w2.size(), QSize(1234, 777));
        QCOMPARE(w2.pos(), QPoint(40, 50));
        w2.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w2));
        QTest::qWait(30);
        QCOMPARE(w2.size(), QSize(1234, 777));
        QCOMPARE(w2.browseView()->categoryWidth(), 230);
    }
    void layoutIgnoresGarbageFile()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QFile f(dir.filePath(MainWindow::layoutFileName()));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[window]\nwidth=abc\nheight=-5\nx=1\n[browse]\ncategoryWidth=99999\n");
        f.close();
        MainWindow w(&m_catalogue, m_library.get(), nullptr);
        w.restoreLayout(dir.path());
        QCOMPARE(w.size(), QSize(Theme::browseDefaultWidth, Theme::browseDefaultHeight));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(w.browseView()->categoryWidth() <= Theme::browseCategoryMaxWidth);
    }
    // Pure clamp (DESIGN 7.1 minimum wins; the window stays on the screen).
    void clampToScreen()
    {
        const QRect avail(0, 0, 1366, 728);
        const QSize min(1000, 700);
        QCOMPARE(MainWindow::clampToScreen(QRect(100, 50, 1180, 760), avail, min), QRect(100, 0, 1180, 728));
        QCOMPARE(MainWindow::clampToScreen(QRect(1000, 600, 800, 500), avail, min), QRect(366, 28, 1000, 700));
        QCOMPARE(MainWindow::clampToScreen(QRect(-500, -90, 1100, 710), avail, min), QRect(0, 0, 1100, 710));
        // A screen smaller than the minimum: the minimum wins, the origin stays at the corner.
        QCOMPARE(MainWindow::clampToScreen(QRect(10, 10, 1500, 900), QRect(0, 0, 800, 600), min), QRect(0, 0, 1000, 700));
        // A secondary screen with a non-zero origin.
        QCOMPARE(MainWindow::clampToScreen(QRect(2000, 100, 1100, 710), QRect(1920, 0, 1920, 1040), min),
                 QRect(2000, 100, 1100, 710));
    }
};

QTEST_MAIN(TestBrowseView)
#include "test_browse_view.moc"
