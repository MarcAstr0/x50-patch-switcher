// T3.6: SetEditor inside BrowseView against Library, FakeX50 and X50Link (headless).
// Nothing here has a byte of its own: the only MIDI claim is that the set editor sends
// none (docs/MIDI.md section 3 lists the explicit actions; a set editor click, drag,
// label edit or Ctrl+digit is not one), so FakeX50 must have received nothing.
// Registered twice in CMake (1x and QT_SCALE_FACTOR=2) for the screenshot smoke.
#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QPixmap>
#include <QPushButton>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>

#include "core/Catalogue.h"
#include "core/Library.h"
#include "midi/FakeX50.h"
#include "midi/X50Link.h"
#include "ui/BrowseView.h"
#include "ui/ResultModel.h"
#include "ui/SetEditor.h"
#include "ui/SlotCell.h"
#include "ui/Theme.h"

namespace {
using A = SoundAddress;
using K = SoundAddress::Kind;
using B = SoundAddress::Bank;
SoundAddress combi(B b, int n) { return A(K::Combi, b, n); }
SoundAddress prog(B b, int n) { return A(K::Prog, b, n); }
} // namespace

class TestSetEditor : public QObject {
    Q_OBJECT

    Catalogue m_catalogue;
    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<Library> m_library;
    std::unique_ptr<FakeX50> m_fake;
    std::unique_ptr<X50Link> m_link;
    std::unique_ptr<BrowseView> m_view;
    QString m_setId;
    QStringList m_statuses;
    QList<bool> m_errors;

    SetEditor &ed() { return *m_view->setEditor(); }
    const SetModel &set() { return *m_library->setById(m_setId); }

    void drop(SlotCell *to, const QString &mimeType, const QByteArray &payload, Qt::DropAction action)
    {
        QMimeData mime;
        mime.setData(mimeType, payload);
        sendDrop(to, &mime, action);
    }
    static void sendDrop(SlotCell *to, const QMimeData *mime, Qt::DropAction action)
    {
        QDragEnterEvent enter(QPoint(5, 5), action, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(to, &enter);
        QVERIFY(enter.isAccepted());
        QDropEvent ev(QPointF(5, 5), action, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(to, &ev);
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
        m_library->createSet(QStringLiteral("Friday"), 8, &m_setId);
        m_library->setSlot(m_setId, 0, combi(B::A, 2));
        m_library->setSlot(m_setId, 1, prog(B::D, 104));
        m_library->setLabel(m_setId, 1, QStringLiteral("Verse EP"));
        m_fake = std::make_unique<FakeX50>();
        m_link = std::make_unique<X50Link>(m_fake.get());
        m_link->setPortNames(QStringLiteral("X50"));
        m_view = std::make_unique<BrowseView>(&m_catalogue, m_library.get(), m_link.get());
        // MainWindow records uses on `sent` (not BrowseView); do the same here.
        connect(m_link.get(), &X50Link::sent, m_library.get(), [this](const SoundAddress &a) { m_library->recordUse(a); });
        m_statuses.clear();
        m_errors.clear();
        connect(m_view.get(), &BrowseView::statusMessage, this, [this](const QString &t, bool e) {
            m_statuses << t;
            m_errors << e;
        });
        m_view->resize(1000, 700);
        m_view->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_view.get()));
        m_link->start();
        QTRY_COMPARE_WITH_TIMEOUT(m_link->state(), X50Link::State::Connected, 3000);
        m_fake->clearReceived();
        // Rebuild with the filled set visible.
        QVERIFY(ed().visibleCellCount() == 8);
    }
    void cleanup()
    {
        // Every test ends with nothing sent by the set editor.
        QVERIFY2(m_fake->received().isEmpty(), "the set editor must never send MIDI");
        m_link->stop();
        m_view.reset();
        m_link.reset();
        m_fake.reset();
        m_library.reset();
        m_dir.reset();
    }

    void showsTheCurrentSet()
    {
        QCOMPARE(ed().picker()->count(), 1);
        QCOMPARE(ed().picker()->currentText(), QStringLiteral("Friday"));
        QVERIFY(ed().size8Button()->isChecked() && !ed().size6Button()->isChecked());
        QVERIFY(!ed().cell(0)->isEmpty());
        QCOMPARE(ed().cell(0)->line2(), m_library->setById(m_setId)->slot(0).address->format());
        QCOMPARE(ed().cell(1)->line1(), QStringLiteral("Verse EP"));  // label first
        QVERIFY(ed().cell(1)->line2().startsWith(QStringLiteral("PROG D104 - ")));
        QVERIFY(ed().cell(2)->isEmpty());
    }

    // DESIGN 7.9 (D3): header buttons 84/96/106/84, size toggles 36 x 36, the slot grid flush with the
    // header row (no inset), cells 62 high, 8 gaps, 8 below the header, 12 above the panel bottom.
    void headerAndGridGeometry()
    {
        QApplication::processEvents();
        QWidget *panel = &ed();
        QCOMPARE(panel->height(), Theme::browseSetEditorHeight);
        const auto button = [this](const QString &text) {
            for (QPushButton *b : ed().findChildren<QPushButton *>())
                if (b->text() == text)
                    return b;
            return static_cast<QPushButton *>(nullptr);
        };
        QPushButton *nb = button(QStringLiteral("New")), *rb = button(QStringLiteral("Rename")),
                    *db = button(QStringLiteral("Duplicate")), *xb = button(QStringLiteral("Delete"));
        QVERIFY(nb && rb && db && xb);
        QCOMPARE(nb->width(), 84);
        QCOMPARE(rb->width(), 96);
        QCOMPARE(db->width(), 106);
        QCOMPARE(xb->width(), 84);
        for (QWidget *w : {static_cast<QWidget *>(nb), static_cast<QWidget *>(rb), static_cast<QWidget *>(db),
                           static_cast<QWidget *>(xb), static_cast<QWidget *>(ed().picker())})
            QCOMPARE(w->height(), 36);
        QCOMPARE(ed().size6Button()->size(), QSize(36, 36));
        QCOMPARE(ed().size8Button()->size(), QSize(36, 36));
        ed().size6Button()->setChecked(true);  // the checked state has the thicker border: size must not move
        QCOMPARE(ed().size6Button()->size(), QSize(36, 36));
        ed().size6Button()->setChecked(false);

        const QRect picker = ed().picker()->geometry();
        const QRect last = ed().size8Button()->geometry();
        const QRect c0 = ed().cell(0)->geometry(), c3 = ed().cell(3)->geometry(), c4 = ed().cell(4)->geometry();
        QCOMPARE(c0.left(), picker.left());          // flush with the header row, no 4 px inset
        QCOMPARE(c3.right(), last.right());
        QCOMPARE(c0.top(), picker.bottom() + 1 + 8);  // header bottom + 8
        QCOMPARE(c0.height(), 62);
        QCOMPARE(c4.height(), 62);
        QCOMPARE(c4.top() - (c0.bottom() + 1), 8);
        QCOMPARE(ed().height() - (c4.bottom() + 1), 12);  // panel bottom - 12
        QCOMPARE(ed().cell(1)->geometry().left() - (c0.right() + 1), 8);
    }

    void clickSelectsButSendsNothing()
    {
        QTest::mouseClick(ed().cell(0), Qt::LeftButton);
        QCOMPARE(ed().selectedSlot(), 0);
        QVERIFY(ed().cell(0)->isSelected());
        QTest::mouseClick(ed().cell(5), Qt::LeftButton);
        QCOMPARE(ed().selectedSlot(), 5);
        QVERIFY(!ed().cell(0)->isSelected());
    }

    void currentSoundMarksItsCell()
    {
        ed().setCurrentSound(combi(B::A, 2));
        QVERIFY(ed().cell(0)->isCurrent());
        QVERIFY(!ed().cell(1)->isCurrent());
        ed().setCurrentSound(std::nullopt);
        QVERIFY(!ed().cell(0)->isCurrent());
    }

    void newRenameDuplicateDelete()
    {
        ed().setTextPrompt([](const QString &, const QString &, const QString &) { return std::optional<QString>(QStringLiteral("Saturday")); });
        QTest::mouseClick(ed().newButton(), Qt::LeftButton);
        QCOMPARE(m_library->sets().size(), 2);
        QCOMPARE(m_library->currentSet()->name(), QStringLiteral("Saturday"));
        QCOMPARE(ed().picker()->count(), 2);
        QCOMPARE(ed().picker()->currentText(), QStringLiteral("Saturday"));

        ed().setTextPrompt([](const QString &, const QString &initial, const QString &) { Q_UNUSED(initial); return std::optional<QString>(QStringLiteral("Sunday")); });
        QTest::mouseClick(ed().renameButton(), Qt::LeftButton);
        QCOMPARE(m_library->currentSet()->name(), QStringLiteral("Sunday"));
        QCOMPARE(ed().picker()->currentText(), QStringLiteral("Sunday"));

        // A cancelled prompt changes nothing.
        ed().setTextPrompt([](const QString &, const QString &, const QString &) { return std::optional<QString>(); });
        QTest::mouseClick(ed().renameButton(), Qt::LeftButton);
        QCOMPARE(m_library->currentSet()->name(), QStringLiteral("Sunday"));

        QTest::mouseClick(ed().duplicateButton(), Qt::LeftButton);
        QCOMPARE(m_library->sets().size(), 3);
        QCOMPARE(m_library->sets().at(2).name(), QStringLiteral("Sunday copy"));
        QCOMPARE(m_library->currentSet()->name(), QStringLiteral("Sunday"));  // not made current

        // Switching sets through the picker updates Library.
        ed().picker()->setCurrentIndex(0);
        QCOMPARE(m_library->currentSetId(), m_setId);
        QVERIFY(!ed().cell(0)->isEmpty());

        // Delete asks; "no" keeps the set, "yes" deletes it.
        bool asked = false;
        ed().setConfirm([&](const QString &, const QString &) { asked = true; return false; });
        QTest::mouseClick(ed().deleteButton(), Qt::LeftButton);
        QVERIFY(asked);
        QCOMPARE(m_library->sets().size(), 3);
        ed().setConfirm([](const QString &, const QString &) { return true; });
        QTest::mouseClick(ed().deleteButton(), Qt::LeftButton);
        QCOMPARE(m_library->sets().size(), 2);
        QVERIFY(m_library->setById(m_setId) == nullptr);
        QCOMPARE(ed().picker()->count(), 2);
    }

    void resizeAndRefusal()
    {
        QTest::mouseClick(ed().size6Button(), Qt::LeftButton);
        QCOMPARE(set().size(), 6);
        QCOMPARE(ed().visibleCellCount(), 6);
        QVERIFY(!ed().cell(6)->isVisible());
        QVERIFY(ed().size6Button()->isChecked() && !ed().size8Button()->isChecked());
        QTest::mouseClick(ed().size8Button(), Qt::LeftButton);
        QCOMPARE(set().size(), 8);
        QVERIFY(ed().cell(7)->isVisible());

        // A filled slot 7 makes the resize to 6 refuse, with the model's message.
        QVERIFY(m_library->setSlot(m_setId, 6, combi(B::A, 5)).ok());
        const QString refusal = m_library->resizeSet(m_setId, 6).error;
        QVERIFY(!refusal.isEmpty());
        m_statuses.clear();
        m_errors.clear();
        QTest::mouseClick(ed().size6Button(), Qt::LeftButton);
        QCOMPARE(set().size(), 8);
        QVERIFY(ed().size8Button()->isChecked() && !ed().size6Button()->isChecked());  // toggle snapped back
        QVERIFY(!m_statuses.isEmpty());
        QCOMPARE(m_statuses.last(), refusal);
        QVERIFY(m_errors.last());
    }

    void dragReorderSwaps()
    {
        // Drop slot 1 (index 0) on slot 2 (index 1): occupied, so they swap, labels travel.
        drop(ed().cell(1), SlotMime::slot(), "0", Qt::MoveAction);
        QCOMPARE(*set().slot(0).address, prog(B::D, 104));
        QCOMPARE(set().slot(0).label, QStringLiteral("Verse EP"));
        QCOMPARE(*set().slot(1).address, combi(B::A, 2));
        QCOMPARE(set().slot(1).label, QString());
        // Onto an empty slot: moves.
        drop(ed().cell(4), SlotMime::slot(), "1", Qt::MoveAction);
        QVERIFY(set().slot(1).isEmpty());
        QCOMPARE(*set().slot(4).address, combi(B::A, 2));
        QVERIFY(ed().cell(4)->line1() == m_catalogue.effectiveName(combi(B::A, 2)));
    }

    void dropFromResults()
    {
        m_view->setSearchText(QStringLiteral("piano"));
        QVERIFY(m_view->resultCount() > 0);
        const SoundAddress first = m_view->resultModel()->address(0);
        const QMimeData *mime = nullptr;
        std::unique_ptr<QMimeData> owned(m_view->resultModel()->mimeData({m_view->resultModel()->index(0, 0)}));
        mime = owned.get();
        QVERIFY(mime && mime->hasFormat(SlotMime::sound()));
        sendDrop(ed().cell(3), mime, Qt::CopyAction);
        QCOMPARE(set().slot(3).address.value(), first);
        QVERIFY(ed().cell(3)->line2() == first.format());
        // Dropping onto a labelled slot keeps its label (Library semantics).
        drop(ed().cell(1), SlotMime::sound(), prog(B::A, 1).format().toUtf8(), Qt::CopyAction);
        QCOMPARE(set().slot(1).label, QStringLiteral("Verse EP"));
        QCOMPARE(*set().slot(1).address, prog(B::A, 1));
    }

    void clearWithDelete()
    {
        QTest::mouseClick(ed().cell(0), Qt::LeftButton);
        QTest::keyClick(ed().cell(0), Qt::Key_Delete);
        QVERIFY(set().slot(0).isEmpty());
        QVERIFY(ed().cell(0)->isEmpty());
        // Del on an empty slot is harmless.
        QTest::keyClick(ed().cell(0), Qt::Key_Delete);
        QVERIFY(set().slot(0).isEmpty());
    }

    void labelEditPersists()
    {
        ed().cell(0)->setFocus();
        QTest::keyClick(ed().cell(0), Qt::Key_F2);
        QVERIFY(ed().cell(0)->isEditing());
        QLineEdit *e = ed().cell(0)->editor();
        e->setText(QStringLiteral("Intro pad"));
        QTest::keyClick(e, Qt::Key_Return);
        QVERIFY(!ed().cell(0)->isEditing());
        QCOMPARE(set().slot(0).label, QStringLiteral("Intro pad"));
        QCOMPARE(ed().cell(0)->line1(), QStringLiteral("Intro pad"));

        // Esc cancels.
        QTest::keyClick(ed().cell(0), Qt::Key_F2);
        ed().cell(0)->editor()->setText(QStringLiteral("nope"));
        QTest::keyClick(ed().cell(0)->editor(), Qt::Key_Escape);
        QCOMPARE(set().slot(0).label, QStringLiteral("Intro pad"));

        // Emptied text clears the label; an empty slot cannot be edited.
        QTest::keyClick(ed().cell(0), Qt::Key_F2);
        ed().cell(0)->editor()->setText(QString());
        QTest::keyClick(ed().cell(0)->editor(), Qt::Key_Return);
        QCOMPARE(set().slot(0).label, QString());
        ed().editLabel(5);
        QVERIFY(!ed().cell(5)->isEditing());

        // Persisted: flush and reload.
        QVERIFY(ed().setLabel(0, QStringLiteral("Saved")));
        m_library->flush();
        Library again;
        again.load(m_dir->path());
        QCOMPARE(again.setById(m_setId)->slot(0).label, QStringLiteral("Saved"));
    }

    void doubleClickEditsLabel()
    {
        QTest::mouseDClick(ed().cell(1), Qt::LeftButton);
        QVERIFY(ed().cell(1)->isEditing());
        QCOMPARE(ed().cell(1)->editor()->text(), QStringLiteral("Verse EP"));
        QTest::keyClick(ed().cell(1)->editor(), Qt::Key_Escape);
    }

    void ctrlDigitIsReflectedAtOnce()
    {
        m_view->setSearchText(QStringLiteral("piano"));
        QVERIFY(m_view->highlight(m_view->resultModel()->address(0)));
        const SoundAddress h = *m_view->highlighted();
        QTest::keyClick(m_view->searchField(), Qt::Key_3, Qt::ControlModifier);
        QCOMPARE(set().slot(2).address.value(), h);
        QCOMPARE(ed().cell(2)->line2(), h.format());
        QVERIFY(!ed().cell(2)->isEmpty());
    }

    // Key isolation (CLAUDE.md: MIDI only on an explicit action). Keys typed in the label
    // editor or on a focused cell are not browse shortcuts. No bytes involved: the claim is
    // that FakeX50 received nothing (cleanup() asserts it too) and the slots did not change.
    void expectKeysInert(QWidget *target)
    {
        m_view->setSearchText(QStringLiteral("piano"));
        QVERIFY(m_view->highlight(m_view->resultModel()->address(0)));
        const SetModel before = set();
        int live = 0;
        connect(m_view.get(), &BrowseView::liveRequested, this, [&live]() { ++live; });
        const struct { Qt::Key key; Qt::KeyboardModifiers mods; } keys[] = {
            {Qt::Key_PageDown, Qt::NoModifier}, {Qt::Key_PageUp, Qt::NoModifier},
            {Qt::Key_Right, Qt::ControlModifier}, {Qt::Key_Left, Qt::ControlModifier},
            {Qt::Key_1, Qt::ControlModifier}, {Qt::Key_3, Qt::ControlModifier},
            {Qt::Key_D, Qt::ControlModifier}, {Qt::Key_F5, Qt::NoModifier},
        };
        for (const auto &k : keys) {
            QTest::keyClick(target, k.key, k.mods);
            QVERIFY(m_fake->received().isEmpty());
        }
        QCOMPARE(live, 0);
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(set().slot(i).address.has_value(), before.slot(i).address.has_value());
            if (before.slot(i).address)
                QCOMPARE(*set().slot(i).address, *before.slot(i).address);
            QCOMPARE(set().slot(i).label, before.slot(i).label);
        }
        QVERIFY(!m_library->isFavourite(*m_view->highlighted()));
    }

    void keysInLabelEditorAreIsolated()
    {
        QTest::keyClick(ed().cell(0), Qt::Key_F2);
        QVERIFY(ed().cell(0)->isEditing());
        expectKeysInert(ed().cell(0)->editor());
        QVERIFY(ed().cell(0)->isEditing());
        // Typing still works and Return still commits.
        QLineEdit *e = ed().cell(0)->editor();
        e->setText(QString());
        QTest::keyClicks(e, QStringLiteral("Hi 5"));
        QCOMPARE(e->text(), QStringLiteral("Hi 5"));
        QTest::keyClick(e, Qt::Key_Return);
        QVERIFY(!ed().cell(0)->isEditing());
        QCOMPARE(set().slot(0).label, QStringLiteral("Hi 5"));
        QVERIFY(m_fake->received().isEmpty());
    }

    void keysOnFocusedCellAreIsolated()
    {
        ed().cell(2)->setFocus();
        QVERIFY(ed().cell(2)->hasFocus());
        expectKeysInert(ed().cell(2));
        QTest::keyClick(ed().cell(2), Qt::Key_Return);
        QTest::keyClick(ed().cell(2), Qt::Key_Enter);
        QVERIFY(m_fake->received().isEmpty());
        QVERIFY(set().slot(2).isEmpty());
    }

    void dragPayloadAndThreshold()
    {
        SlotCell *c = ed().cell(1);
        std::unique_ptr<QMimeData> mime(c->makeDragMime());
        QVERIFY(mime);
        QCOMPARE(mime->data(SlotMime::slot()), QByteArray("1"));
        // No press yet: no drag.
        QVERIFY(!c->dragWanted(QPoint(60, 40)));
        QTest::mousePress(c, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QVERIFY(!c->dragWanted(QPoint(10, 10 + QApplication::startDragDistance() - 1)));
        QVERIFY(c->dragWanted(QPoint(10, 10 + QApplication::startDragDistance())));
        // An empty cell never drags.
        SlotCell *empty = ed().cell(5);
        QVERIFY(empty->isEmpty());
        QVERIFY(!empty->makeDragMime());
        QTest::mousePress(empty, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
        QVERIFY(!empty->dragWanted(QPoint(100, 40)));
    }

    void resultTableDragSetup()
    {
        auto *table = m_view->findChild<QTableView *>(QStringLiteral("resultList"));
        QVERIFY(table);
        QVERIFY(table->dragEnabled());
        QCOMPARE(table->dragDropMode(), QAbstractItemView::DragOnly);
    }

    void shrinkClampsSelection()
    {
        ed().selectSlot(7);
        QCOMPARE(ed().selectedSlot(), 7);
        QVERIFY(ed().resizeCurrent(6));
        QCOMPARE(ed().selectedSlot(), 5);
        QVERIFY(ed().cell(5)->isSelected());
    }

    void screenshotSmoke()
    {
        m_library->setSlot(m_setId, 2, combi(B::C, 96));
        ed().setCurrentSound(combi(B::C, 96));
        ed().selectSlot(1);
        QApplication::processEvents();
        QPixmap shot = ed().grab();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.width() > 0 && shot.deviceIndependentSize().height() == Theme::browseSetEditorHeight);
        m_library->resizeSet(m_setId, 6);
        QApplication::processEvents();
        QVERIFY(!ed().grab().isNull());
        QCOMPARE(ed().visibleCellCount(), 6);
    }
};

QTEST_MAIN(TestSetEditor)
#include "test_set_editor.moc"
