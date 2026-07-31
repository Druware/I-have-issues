/*
 * ProjectSettingsWindow.cpp
 */
#include "ProjectSettingsWindow.h"

#include <Box.h>
#include <Button.h>
#include <CheckBox.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <TextControl.h>

#include "AppMessages.h"
#include <issueskit/IssuesJsonCoder.h>
#include <issueskit/StringUtils.h>

using namespace issueskit;

namespace ihaveissues {

namespace {

const uint32 kMsgToggleGitHub = 'tggh';
const uint32 kMsgToggleAzure = 'tgaz';

} // unnamed namespace


ProjectSettingsWindow::ProjectSettingsWindow(BWindow* parent,
	const IssuesDocumentModel& model, const BMessenger& target)
	:
	BWindow(BRect(0, 0, 520, 620), "Project Settings", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS | B_NOT_ZOOMABLE),
	fModel(model),
	fTarget(target)
{
	SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
	if (parent != NULL) {
		AddToSubset(parent);
		CenterIn(parent->Frame());
	} else {
		CenterOnScreen();
	}

	_BuildLayout();
	_Populate();
	_UpdateEnabledState();
}


ProjectSettingsWindow::~ProjectSettingsWindow()
{
}


void
ProjectSettingsWindow::_BuildLayout()
{
	fNameControl = new BTextControl("name", "Name:", "", NULL);
	fSummaryControl = new BTextControl("summary", "Summary:", "", NULL);

	fGitHubBox = new BCheckBox("githubBox", "Configure GitHub",
		new BMessage(kMsgToggleGitHub));
	fOwnerControl = new BTextControl("owner", "Owner:", "", NULL);
	fRepositoryControl = new BTextControl("repository", "Repository:", "",
		NULL);
	fDefaultLabelsControl = new BTextControl("defaultLabels", "Default labels:",
		"", NULL);
	fDefaultAssigneesControl = new BTextControl("defaultAssignees",
		"Default assignees:", "", NULL);
	fDefaultMilestoneControl = new BTextControl("defaultMilestone",
		"Default milestone:", "", NULL);

	fAzureBox = new BCheckBox("azureBox", "Configure Azure DevOps",
		new BMessage(kMsgToggleAzure));
	fOrganizationControl = new BTextControl("organization", "Organization:", "",
		NULL);
	fProjectControl = new BTextControl("project", "Project:", "", NULL);
	fTeamControl = new BTextControl("team", "Team:", "", NULL);
	fAreaPathControl = new BTextControl("areaPath", "Area path:", "", NULL);
	fIterationPathControl = new BTextControl("iterationPath",
		"Iteration path:", "", NULL);
	fWorkItemTypeControl = new BTextControl("workItemType",
		"Default work item type:", "", NULL);

	fCancelButton = new BButton("cancel", "Cancel", new BMessage(kMsgCancel));
	fSaveButton = new BButton("save", "Save", new BMessage(kMsgSave));

	BBox* projectBox = new BBox("projectBox");
	projectBox->SetLabel("Project");
	BLayoutBuilder::Grid<>(projectBox, B_USE_SMALL_SPACING,
			B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS, B_USE_BIG_INSETS, B_USE_SMALL_INSETS,
			B_USE_SMALL_INSETS)
		.AddTextControl(fNameControl, 0, 0)
		.AddTextControl(fSummaryControl, 0, 1);

	BBox* githubBox = new BBox("githubBox");
	githubBox->SetLabel("GitHub");
	BLayoutBuilder::Grid<>(githubBox, B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS, B_USE_BIG_INSETS, B_USE_SMALL_INSETS,
			B_USE_SMALL_INSETS)
		.Add(fGitHubBox, 0, 0, 2, 1)
		.AddTextControl(fOwnerControl, 0, 1)
		.AddTextControl(fRepositoryControl, 0, 2)
		.AddTextControl(fDefaultLabelsControl, 0, 3)
		.AddTextControl(fDefaultAssigneesControl, 0, 4)
		.AddTextControl(fDefaultMilestoneControl, 0, 5)
		.Add(new BStringView("githubHint",
			"The personal access token is kept in the system key store and is "
			"never saved into this document."), 0, 6, 2, 1);

	BBox* azureBox = new BBox("azureBox");
	azureBox->SetLabel("Azure DevOps");
	BLayoutBuilder::Grid<>(azureBox, B_USE_SMALL_SPACING, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_SMALL_INSETS, B_USE_BIG_INSETS, B_USE_SMALL_INSETS,
			B_USE_SMALL_INSETS)
		.Add(fAzureBox, 0, 0, 2, 1)
		.AddTextControl(fOrganizationControl, 0, 1)
		.AddTextControl(fProjectControl, 0, 2)
		.AddTextControl(fTeamControl, 0, 3)
		.AddTextControl(fAreaPathControl, 0, 4)
		.AddTextControl(fIterationPathControl, 0, 5)
		.AddTextControl(fWorkItemTypeControl, 0, 6);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_SMALL_SPACING)
		.SetInsets(B_USE_WINDOW_INSETS)
		.Add(projectBox)
		.Add(githubBox)
		.Add(azureBox)
		.AddGlue()
		.AddGroup(B_HORIZONTAL, B_USE_SMALL_SPACING)
			.AddGlue()
			.Add(fCancelButton)
			.Add(fSaveButton)
			.End()
		.End();

	fSaveButton->MakeDefault(true);
	fNameControl->MakeFocus(true);
}


void
ProjectSettingsWindow::_Populate()
{
	fNameControl->SetText(fModel.project.name.c_str());
	fSummaryControl->SetText(fModel.project.summary.c_str());

	// DEVIATION from Apple: the Apple sheet pre-seeds blank GitHub coordinates
	// from legacy UserDefaults keys left by a pre-document build. Haiku has no
	// such legacy state, so the fields simply start blank.
	const std::optional<GitHubIntegration>& github = fModel.integrations.github;
	fGitHubBox->SetValue(github.has_value() ? B_CONTROL_ON : B_CONTROL_OFF);
	if (github.has_value()) {
		fOwnerControl->SetText(github->owner.c_str());
		fRepositoryControl->SetText(github->repository.c_str());
		fDefaultLabelsControl->SetText(
			Join(github->defaultLabels, ", ").c_str());
		fDefaultAssigneesControl->SetText(
			Join(github->defaultAssignees, ", ").c_str());
		fDefaultMilestoneControl->SetText(
			github->defaultMilestone.has_value()
				? github->defaultMilestone->c_str() : "");
	}

	const std::optional<AzureDevOpsIntegration>& azure
		= fModel.integrations.azureDevOps;
	fAzureBox->SetValue(azure.has_value() ? B_CONTROL_ON : B_CONTROL_OFF);
	if (azure.has_value()) {
		fOrganizationControl->SetText(azure->organization.c_str());
		fProjectControl->SetText(azure->project.c_str());
		fTeamControl->SetText(azure->team.has_value()
			? azure->team->c_str() : "");
		fAreaPathControl->SetText(azure->areaPath.has_value()
			? azure->areaPath->c_str() : "");
		fIterationPathControl->SetText(azure->iterationPath.has_value()
			? azure->iterationPath->c_str() : "");
		fWorkItemTypeControl->SetText(azure->defaultWorkItemType.c_str());
	} else {
		fWorkItemTypeControl->SetText("Issue");
	}
}


void
ProjectSettingsWindow::_UpdateEnabledState()
{
	bool github = fGitHubBox->Value() == B_CONTROL_ON;
	fOwnerControl->SetEnabled(github);
	fRepositoryControl->SetEnabled(github);
	fDefaultLabelsControl->SetEnabled(github);
	fDefaultAssigneesControl->SetEnabled(github);
	fDefaultMilestoneControl->SetEnabled(github);

	bool azure = fAzureBox->Value() == B_CONTROL_ON;
	fOrganizationControl->SetEnabled(azure);
	fProjectControl->SetEnabled(azure);
	fTeamControl->SetEnabled(azure);
	fAreaPathControl->SetEnabled(azure);
	fIterationPathControl->SetEnabled(azure);
	fWorkItemTypeControl->SetEnabled(azure);
}


std::optional<std::string>
ProjectSettingsWindow::_Optional(const char* text)
{
	// An optional field is unset when blank, never an empty string.
	std::string trimmed = Trim(text != NULL ? text : "");
	if (trimmed.empty())
		return std::optional<std::string>();
	return trimmed;
}


void
ProjectSettingsWindow::_Save()
{
	fModel.project.name = Trim(fNameControl->Text());
	fModel.project.summary = fSummaryControl->Text();

	if (fGitHubBox->Value() == B_CONTROL_ON) {
		GitHubIntegration integration;
		integration.owner = Trim(fOwnerControl->Text());
		integration.repository = Trim(fRepositoryControl->Text());
		integration.defaultLabels
			= SplitTrimNonEmpty(fDefaultLabelsControl->Text(), ',');
		integration.defaultAssignees
			= SplitTrimNonEmpty(fDefaultAssigneesControl->Text(), ',');
		integration.defaultMilestone
			= _Optional(fDefaultMilestoneControl->Text());
		fModel.integrations.github = integration;
	} else {
		fModel.integrations.github.reset();
	}

	if (fAzureBox->Value() == B_CONTROL_ON) {
		AzureDevOpsIntegration integration;
		integration.organization = Trim(fOrganizationControl->Text());
		integration.project = Trim(fProjectControl->Text());
		integration.team = _Optional(fTeamControl->Text());
		integration.areaPath = _Optional(fAreaPathControl->Text());
		integration.iterationPath = _Optional(fIterationPathControl->Text());
		std::optional<std::string> workItemType
			= _Optional(fWorkItemTypeControl->Text());
		integration.defaultWorkItemType = workItemType.has_value()
			? *workItemType : std::string("Issue");
		fModel.integrations.azureDevOps = integration;
	} else {
		fModel.integrations.azureDevOps.reset();
	}

	// The whole edited document goes back as JSON and the document window copies
	// only `project` and `integrations` out of it. This window is modal to that
	// window, so nothing else can have changed the model meanwhile.
	std::string json;
	IssuesError error;
	if (!IssuesJsonCoder::Encode(fModel, json, error))
		return;

	BMessage message(kMsgProjectSettingsSaved);
	message.AddString("document", json.c_str());
	fTarget.SendMessage(&message);

	PostMessage(B_QUIT_REQUESTED);
}


void
ProjectSettingsWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgToggleGitHub:
		case kMsgToggleAzure:
			_UpdateEnabledState();
			break;

		case kMsgSave:
			_Save();
			break;

		case kMsgCancel:
			PostMessage(B_QUIT_REQUESTED);
			break;

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

} // namespace ihaveissues
