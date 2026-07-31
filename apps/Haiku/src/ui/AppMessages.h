/*
 * AppMessages.h -- every BMessage `what` constant the app defines.
 *
 * Four-character codes, Be API convention. Kept in one header so a collision is
 * visible at a glance.
 */
#ifndef IHAVEISSUES_APP_MESSAGES_H
#define IHAVEISSUES_APP_MESSAGES_H

#include <SupportDefs.h>

// The application signature. Must match IHaveIssues.rdef and the Makefile.
#define kApplicationSignature "application/x-vnd.Druware-IHaveIssues"

// The MIME type of a .issues document.
#define kIssuesMimeType "application/x-vnd.Druware-issues"

enum {
	// File menu
	kMsgNewDocument				= 'nwdc',
	kMsgOpenDocument			= 'opdc',
	kMsgSaveDocument			= 'save',
	kMsgSaveDocumentAs			= 'svas',
	kMsgExportMarkdown			= 'expm',
	kMsgImportMarkdown			= 'impm',

	// File panel replies. B_SAVE_REQUESTED carries a "purpose" int32 naming
	// which panel replied, because one window owns three save panels.
	kMsgSavePanelDone			= 'svpd',
	kMsgOpenPanelDone			= 'oppd',

	// Save-panel purposes, sent as "purpose" in kMsgSavePanelDone.
	kPurposeSaveDocument		= 'pssd',
	kPurposeExportMarkdown		= 'psem',

	// Open-panel purposes, sent as "purpose" in kMsgOpenPanelDone.
	kPurposeOpenDocument		= 'poop',
	kPurposeImportMarkdown		= 'poim',

	// Issue actions
	kMsgAddIssue				= 'adis',
	kMsgEditIssue				= 'edis',
	kMsgDeleteIssue				= 'dlis',
	kMsgIssueSelected			= 'isse',
	kMsgIssueInvoked			= 'issi',
	//! From IssueEditWindow. Carries "issue" (a string of JSON).
	kMsgIssueEdited				= 'issd',

	// Project menu
	kMsgShowProjectSettings		= 'prst',
	kMsgShowGitHubSync			= 'ghsy',
	//! From ProjectSettingsWindow. Carries the edited fields.
	kMsgProjectSettingsSaved	= 'prsv',

	// GitHub sync window
	kMsgSyncStart				= 'gsst',
	kMsgSyncTokenRemove			= 'gsrm',
	kMsgSyncDone				= 'gsdn',
	kMsgSyncClose				= 'gscl',
	//! From GitHubSyncWindow. Carries "issues" (a string of JSON).
	kMsgSyncIssuesUpdated		= 'gsiu',

	// Dialog buttons. The destructive confirmations (delete an issue, replace a
	// document with an import) use BAlert::Go() synchronously instead, so they
	// need no message of their own.
	kMsgCancel					= 'canc',
	kMsgSave					= 'sved',

	//! A document window telling the app it is going away.
	kMsgWindowClosed			= 'wncl'
};

#endif // IHAVEISSUES_APP_MESSAGES_H
