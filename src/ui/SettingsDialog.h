#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

#include "midi/X50Link.h"
#include "ui/ConnectionSettings.h"
#include "ui/ProbeChecklist.h"

class MidiBackend;
class Library;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSpinBox;

// Settings (SPEC 4.5, DESIGN 8): MIDI output and input port, the manual channel and Bank Map
// for when the X50 does not answer, and the probe checklist. Browse view only; never opened
// from the live view. Modal in the app (DESIGN 8); the tests drive it non-modally.
//
// "Test" and "Check X50" send only what the probe sends: Device Inquiry and Mode and State
// requests (read-only, docs/MIDI.md 5 and 6). Nothing is sent when the dialog opens or when a
// control changes.
//   Test       applies the chosen ports (closes and reopens them) and runs the probe on them.
//   Check X50  runs the probe on the ports already open (opens them first when none is).
//
// The manual channel and Bank Map are enabled only when the last probe could not read them
// (the X50 gave no answer); changing them applies them to the link at once. A channel the X50
// reported is shown (and its spin box disabled) rather than overwritten. Port choices and manual
// values are stored in "<configDir>/settings.ini" (ConnectionSettings) on Test, Check and close,
// not on every change. Test and Check are disabled while the link is busy, with the reason in the note.
// Neither the link nor the backend is owned.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    SettingsDialog(X50Link *link, MidiBackend *backend, const QString &configDir, QWidget *parent = nullptr);

    QComboBox *outputCombo() const { return m_output; }
    QComboBox *inputCombo() const { return m_input; }
    QPushButton *testButton() const { return m_test; }
    QPushButton *checkButton() const { return m_check; }
    QPushButton *closeButton() const { return m_close; }
    QSpinBox *channelSpin() const { return m_channel; }
    QComboBox *bankMapCombo() const { return m_bankMap; }
    ProbeChecklist *checklist() const { return m_list; }
    QLabel *statusWord() const { return m_statusWord; }
    // Connection problem of the last Test or Check (port busy, no port found); empty when none.
    QString note() const;
    // The manual Bank Map is editable (the channel too unless the X50 reported one).
    bool manualEnabled() const;
    // True when the last save of the settings file failed (read-only folder); the note says so.
    bool saveFailed() const { return m_saveFailed; }
    static QString saveFailedText();
    // "Last check" shows the library's unit.lastSeen when a probe confirmed the X50. Not owned.
    void setLibrary(const Library *library);
    // Pure: the "Bank Map: not reported" line, shared with the screenshot runner.
    static ProbeChecklist::Row bankMapNotReportedRow(BankMap effective);

    // Re-read the port lists from the backend (opening the dialog does this once).
    void refreshPorts();
    // Pure: the checklist lines of a probe result. `effective` is the Bank Map the link would
    // send with when the probe did not read one.
    static QVector<ProbeChecklist::Row> rowsFromProbe(const X50Link::ProbeResult &result, BankMap effective);
    // The six lines of SPEC 4.5 in the running state, text = the check's name.
    static QVector<ProbeChecklist::Row> runningRows();
    // Re-fit the dialog after the checklist changed (screenshots call it after editing the rows).
    void fitToContent();
    // Overrides the height limit (80% of the screen, never below the 560 minimum); the headless
    // screenshot's virtual screen is smaller than a real one. 0 restores the screen rule.
    void setHeightLimit(int pixels);

protected:
    // Saves the settings once, on the way out.
    void done(int result) override;

private:
    void applyPorts();
    void startTest();
    void startCheck();
    void onLinkChanged();
    void onManualChanged();
    void updateEnabled();
    void setNote(const QString &text);
    void clearNote();
    bool saveSettings();
    void updateLastCheck();

    X50Link *m_link;
    MidiBackend *m_backend;
    QString m_configDir;
    ConnectionSettings m_settings;
    const Library *m_library = nullptr;
    QComboBox *m_output = nullptr, *m_input = nullptr, *m_bankMap = nullptr;
    QSpinBox *m_channel = nullptr;
    QPushButton *m_test = nullptr, *m_check = nullptr, *m_close = nullptr;
    QLabel *m_statusWord = nullptr, *m_note = nullptr;
    QScrollArea *m_scroll = nullptr;
    ProbeChecklist *m_list = nullptr;
    int m_heightLimit = 0;
    bool m_building = false;
    // The manual values shown: the link's effective ones (a --channel wins over the saved value).
    // Only a user change is applied to the link and written to the settings.
    int m_shownChannel = 1;
    BankMap m_shownBankMap = BankMap::Gm;
    bool m_manualTouched = false;
    bool m_hasPorts = false;
    bool m_checkedThisSession = false;
    bool m_saveFailed = false;
};
