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
