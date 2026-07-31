/*
 * ProjectSettingsDialog.h -- per-document project and integration coordinates.
 *
 * A plain QDialog, NOT a KConfigDialog. KConfigDialog exists to edit application
 * settings backed by a KConfigSkeleton in the user's config file; everything on
 * this sheet is per-document state that is serialised into the .issues file and
 * travels with it into git. Wiring it through KConfigSkeleton would mean either
 * storing document data in ~/.config or writing a fake skeleton, both of which
 * are worse than a form.
 *
 * Blank optional fields become ABSENT, never an empty string, so a document that
 * has never had a team name does not gain `"team" : ""` on save.
 */
#ifndef IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H
#define IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H

#include <QDialog>

#include <issueskit/IssueModel.h>

class QGroupBox;
class QLineEdit;

namespace ihaveissues
{

class ProjectSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    ProjectSettingsDialog(const issueskit::IssuesDocumentModel &model,
                          QWidget *parent = nullptr);
    ~ProjectSettingsDialog() override;

    //! The edited project block. Only meaningful after the dialog was accepted.
    const issueskit::ProjectInfo &project() const;

    //! The edited integrations. Only meaningful after the dialog was accepted.
    const issueskit::IntegrationSettings &integrations() const;

private Q_SLOTS:
    void commit();

private:
    void buildLayout();
    void populate();

    issueskit::ProjectInfo m_project;
    issueskit::IntegrationSettings m_integrations;

    QLineEdit *m_nameEdit;
    QLineEdit *m_summaryEdit;

    QGroupBox *m_githubGroup;
    QLineEdit *m_ownerEdit;
    QLineEdit *m_repositoryEdit;
    QLineEdit *m_defaultLabelsEdit;
    QLineEdit *m_defaultAssigneesEdit;
    QLineEdit *m_defaultMilestoneEdit;

    QGroupBox *m_azureGroup;
    QLineEdit *m_organizationEdit;
    QLineEdit *m_projectEdit;
    QLineEdit *m_teamEdit;
    QLineEdit *m_areaPathEdit;
    QLineEdit *m_iterationPathEdit;
    QLineEdit *m_workItemTypeEdit;
};

} // namespace ihaveissues

#endif // IHAVEISSUES_PROJECT_SETTINGS_DIALOG_H
