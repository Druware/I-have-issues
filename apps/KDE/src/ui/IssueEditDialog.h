/*
 * IssueEditDialog.h -- the add/edit sheet.
 *
 * Edits a local copy. Cancel discards it; Save builds the result and the caller
 * (MainWindow) owns persistence, exactly as the Apple IssueEditView hands its
 * draft back through onSave.
 *
 * The five long-text sections live in a QTabWidget rather than one very tall
 * form, for the same reason the Haiku editor uses a BTabView: a desktop dialog
 * that is 1200 points tall is worse than four tabs. Every field from the Apple
 * IssueEditView is present.
 */
#ifndef IHAVEISSUES_ISSUE_EDIT_DIALOG_H
#define IHAVEISSUES_ISSUE_EDIT_DIALOG_H

#include <QDialog>

#include <issueskit/IssueModel.h>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

namespace ihaveissues
{

class IssueEditDialog : public QDialog
{
    Q_OBJECT

public:
    IssueEditDialog(const issueskit::Issue &issue, QWidget *parent = nullptr);
    ~IssueEditDialog() override;

    //! The edited issue. Only meaningful after the dialog was accepted.
    const issueskit::Issue &result() const;

private Q_SLOTS:
    //! Builds m_result from the widgets, then accepts.
    void commit();

private:
    void buildLayout();
    void populate();

    issueskit::Issue m_draft;
    issueskit::Issue m_result;

    QLineEdit *m_titleEdit;
    QComboBox *m_typeCombo;
    QComboBox *m_priorityCombo;
    QComboBox *m_statusCombo;
    QComboBox *m_resolutionCombo;
    QDateEdit *m_reportedEdit;
    QLineEdit *m_reportedByEdit;
    QLineEdit *m_areaEdit;
    QLineEdit *m_milestoneEdit;
    QLineEdit *m_estimateEdit;
    QLineEdit *m_labelsEdit;
    QLineEdit *m_assigneesEdit;

    QPlainTextEdit *m_descriptionEdit;
    QPlainTextEdit *m_stepsEdit;
    QPlainTextEdit *m_environmentEdit;
    QPlainTextEdit *m_notesEdit;
    QPlainTextEdit *m_resolutionTextEdit;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_ISSUE_EDIT_DIALOG_H
