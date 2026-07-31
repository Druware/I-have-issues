/*
 * ProjectSettingsDialog.cpp
 */
#include "ProjectSettingsDialog.h"

#include <optional>
#include <string>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#include <KLocalizedString>

#include <issueskit/StringUtils.h>

using issueskit::AzureDevOpsIntegration;
using issueskit::GitHubIntegration;
using issueskit::IntegrationSettings;
using issueskit::IssuesDocumentModel;
using issueskit::ProjectInfo;

namespace ihaveissues
{

namespace
{

//! An optional field is unset when blank, never an empty string.
std::optional<std::string> optionalText(const QString &text)
{
    const std::string trimmed = issueskit::Trim(text.toStdString());
    if (trimmed.empty()) {
        return std::optional<std::string>();
    }
    return trimmed;
}

} // unnamed namespace

ProjectSettingsDialog::ProjectSettingsDialog(const IssuesDocumentModel &model,
                                             QWidget *parent)
    : QDialog(parent)
    , m_project(model.project)
    , m_integrations(model.integrations)
    , m_nameEdit(nullptr)
    , m_summaryEdit(nullptr)
    , m_githubGroup(nullptr)
    , m_ownerEdit(nullptr)
    , m_repositoryEdit(nullptr)
    , m_defaultLabelsEdit(nullptr)
    , m_defaultAssigneesEdit(nullptr)
    , m_defaultMilestoneEdit(nullptr)
    , m_azureGroup(nullptr)
    , m_organizationEdit(nullptr)
    , m_projectEdit(nullptr)
    , m_teamEdit(nullptr)
    , m_areaPathEdit(nullptr)
    , m_iterationPathEdit(nullptr)
    , m_workItemTypeEdit(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Project Settings"));
    setModal(true);
    buildLayout();
    populate();
    resize(520, 620);
}

ProjectSettingsDialog::~ProjectSettingsDialog() = default;

void ProjectSettingsDialog::buildLayout()
{
    m_nameEdit = new QLineEdit(this);
    m_summaryEdit = new QLineEdit(this);

    auto *projectGroup = new QGroupBox(i18n("Project"), this);
    auto *projectForm = new QFormLayout(projectGroup);
    projectForm->addRow(i18n("Name:"), m_nameEdit);
    projectForm->addRow(i18n("Summary:"), m_summaryEdit);

    // A checkable QGroupBox is the Qt spelling of "this whole block is behind a
    // toggle": unchecking it disables every child on its own.
    m_githubGroup = new QGroupBox(i18n("Configure GitHub"), this);
    m_githubGroup->setCheckable(true);
    m_ownerEdit = new QLineEdit(m_githubGroup);
    m_repositoryEdit = new QLineEdit(m_githubGroup);
    m_defaultLabelsEdit = new QLineEdit(m_githubGroup);
    m_defaultAssigneesEdit = new QLineEdit(m_githubGroup);
    m_defaultMilestoneEdit = new QLineEdit(m_githubGroup);

    auto *githubHint = new QLabel(
        i18n("The personal access token is kept in the system wallet and is never "
             "saved into this document."),
        m_githubGroup);
    githubHint->setWordWrap(true);

    auto *githubForm = new QFormLayout(m_githubGroup);
    githubForm->addRow(i18n("Owner:"), m_ownerEdit);
    githubForm->addRow(i18n("Repository:"), m_repositoryEdit);
    githubForm->addRow(i18n("Default labels:"), m_defaultLabelsEdit);
    githubForm->addRow(i18n("Default assignees:"), m_defaultAssigneesEdit);
    githubForm->addRow(i18n("Default milestone:"), m_defaultMilestoneEdit);
    githubForm->addRow(QString(), githubHint);

    m_azureGroup = new QGroupBox(i18n("Configure Azure DevOps"), this);
    m_azureGroup->setCheckable(true);
    m_organizationEdit = new QLineEdit(m_azureGroup);
    m_projectEdit = new QLineEdit(m_azureGroup);
    m_teamEdit = new QLineEdit(m_azureGroup);
    m_areaPathEdit = new QLineEdit(m_azureGroup);
    m_iterationPathEdit = new QLineEdit(m_azureGroup);
    m_workItemTypeEdit = new QLineEdit(m_azureGroup);

    // The format carries these fields and Project Settings edits them, but no
    // Azure DevOps sync exists on any platform -- see the README.
    auto *azureHint = new QLabel(
        i18n("These coordinates are stored in the document. This application does "
             "not sync to Azure DevOps."),
        m_azureGroup);
    azureHint->setWordWrap(true);

    auto *azureForm = new QFormLayout(m_azureGroup);
    azureForm->addRow(i18n("Organization:"), m_organizationEdit);
    azureForm->addRow(i18n("Project:"), m_projectEdit);
    azureForm->addRow(i18n("Team:"), m_teamEdit);
    azureForm->addRow(i18n("Area path:"), m_areaPathEdit);
    azureForm->addRow(i18n("Iteration path:"), m_iterationPathEdit);
    azureForm->addRow(i18n("Default work item type:"), m_workItemTypeEdit);
    azureForm->addRow(QString(), azureHint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProjectSettingsDialog::commit);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(projectGroup);
    layout->addWidget(m_githubGroup);
    layout->addWidget(m_azureGroup);
    layout->addStretch(1);
    layout->addWidget(buttons);

    m_nameEdit->setFocus();
}

void ProjectSettingsDialog::populate()
{
    m_nameEdit->setText(QString::fromStdString(m_project.name));
    m_summaryEdit->setText(QString::fromStdString(m_project.summary));

    // DEVIATION from Apple: the Apple sheet pre-seeds blank GitHub coordinates
    // from legacy UserDefaults keys left by a pre-document build. KDE has no such
    // legacy state, so the fields simply start blank. Haiku does the same.
    const std::optional<GitHubIntegration> &github = m_integrations.github;
    m_githubGroup->setChecked(github.has_value());
    if (github.has_value()) {
        m_ownerEdit->setText(QString::fromStdString(github->owner));
        m_repositoryEdit->setText(QString::fromStdString(github->repository));
        m_defaultLabelsEdit->setText(
            QString::fromStdString(issueskit::Join(github->defaultLabels, ", ")));
        m_defaultAssigneesEdit->setText(
            QString::fromStdString(issueskit::Join(github->defaultAssignees, ", ")));
        m_defaultMilestoneEdit->setText(
            github->defaultMilestone.has_value()
                ? QString::fromStdString(*github->defaultMilestone)
                : QString());
    }

    const std::optional<AzureDevOpsIntegration> &azure = m_integrations.azureDevOps;
    m_azureGroup->setChecked(azure.has_value());
    if (azure.has_value()) {
        m_organizationEdit->setText(QString::fromStdString(azure->organization));
        m_projectEdit->setText(QString::fromStdString(azure->project));
        m_teamEdit->setText(azure->team.has_value()
                                ? QString::fromStdString(*azure->team)
                                : QString());
        m_areaPathEdit->setText(azure->areaPath.has_value()
                                    ? QString::fromStdString(*azure->areaPath)
                                    : QString());
        m_iterationPathEdit->setText(azure->iterationPath.has_value()
                                         ? QString::fromStdString(*azure->iterationPath)
                                         : QString());
        m_workItemTypeEdit->setText(QString::fromStdString(azure->defaultWorkItemType));
    } else {
        m_workItemTypeEdit->setText(QStringLiteral("Issue"));
    }
}

void ProjectSettingsDialog::commit()
{
    m_project.name = issueskit::Trim(m_nameEdit->text().toStdString());
    m_project.summary = m_summaryEdit->text().toStdString();

    if (m_githubGroup->isChecked()) {
        GitHubIntegration integration;
        integration.owner = issueskit::Trim(m_ownerEdit->text().toStdString());
        integration.repository = issueskit::Trim(m_repositoryEdit->text().toStdString());
        integration.defaultLabels = issueskit::SplitTrimNonEmpty(
            m_defaultLabelsEdit->text().toStdString(), ',');
        integration.defaultAssignees = issueskit::SplitTrimNonEmpty(
            m_defaultAssigneesEdit->text().toStdString(), ',');
        integration.defaultMilestone = optionalText(m_defaultMilestoneEdit->text());
        m_integrations.github = integration;
    } else {
        m_integrations.github.reset();
    }

    if (m_azureGroup->isChecked()) {
        AzureDevOpsIntegration integration;
        integration.organization =
            issueskit::Trim(m_organizationEdit->text().toStdString());
        integration.project = issueskit::Trim(m_projectEdit->text().toStdString());
        integration.team = optionalText(m_teamEdit->text());
        integration.areaPath = optionalText(m_areaPathEdit->text());
        integration.iterationPath = optionalText(m_iterationPathEdit->text());
        const std::optional<std::string> workItemType =
            optionalText(m_workItemTypeEdit->text());
        integration.defaultWorkItemType =
            workItemType.has_value() ? *workItemType : std::string("Issue");
        m_integrations.azureDevOps = integration;
    } else {
        m_integrations.azureDevOps.reset();
    }

    accept();
}

const ProjectInfo &ProjectSettingsDialog::project() const
{
    return m_project;
}

const IntegrationSettings &ProjectSettingsDialog::integrations() const
{
    return m_integrations;
}

} // namespace ihaveissues
