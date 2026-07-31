/*
 * IssueEditDialog.cpp
 */
#include "IssueEditDialog.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <KLocalizedString>

#include <issueskit/IssueDate.h>
#include <issueskit/IssueEnums.h>
#include <issueskit/StringUtils.h>

using issueskit::Issue;
using issueskit::Timestamp;

namespace ihaveissues
{

namespace
{

/*! The date format used to hand a day between QDate and issueskit.
 *
 *  The stored value is midnight UTC of a calendar day. Rather than convert
 *  through QDateTime -- where a wrong time-spec argument silently shifts the day
 *  for anyone east or west of GMT -- both directions go through
 *  issueskit::IssueDate, which is fixed-UTC by construction. The QDateEdit then
 *  only ever deals in a bare QDate, which carries no time zone at all.
 */
const char *const kDayFormat = "yyyy-MM-dd";

QString dayString(Timestamp value)
{
    return QString::fromStdString(issueskit::IssueDate::ToString(value));
}

QPlainTextEdit *makeTextArea()
{
    auto *edit = new QPlainTextEdit();
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setTabChangesFocus(true);
    return edit;
}

//! Wraps a text area plus an optional hint line in a tab page.
QWidget *makeTextPage(QPlainTextEdit *edit, const QString &hint)
{
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(edit);
    if (!hint.isEmpty()) {
        auto *label = new QLabel(hint, page);
        label->setWordWrap(true);
        layout->addWidget(label);
    }
    return page;
}

} // unnamed namespace

IssueEditDialog::IssueEditDialog(const Issue &issue, QWidget *parent)
    : QDialog(parent)
    , m_draft(issue)
    , m_result(issue)
    , m_titleEdit(nullptr)
    , m_typeCombo(nullptr)
    , m_priorityCombo(nullptr)
    , m_statusCombo(nullptr)
    , m_resolutionCombo(nullptr)
    , m_reportedEdit(nullptr)
    , m_reportedByEdit(nullptr)
    , m_areaEdit(nullptr)
    , m_milestoneEdit(nullptr)
    , m_estimateEdit(nullptr)
    , m_labelsEdit(nullptr)
    , m_assigneesEdit(nullptr)
    , m_descriptionEdit(nullptr)
    , m_stepsEdit(nullptr)
    , m_environmentEdit(nullptr)
    , m_notesEdit(nullptr)
    , m_resolutionTextEdit(nullptr)
{
    setWindowTitle(i18nc("@title:window, followed by the issue number",
                         "Issue %1", QString::fromStdString(m_draft.DisplayNumber())));
    setModal(true);
    buildLayout();
    populate();
    resize(560, 640);
}

IssueEditDialog::~IssueEditDialog() = default;

void IssueEditDialog::buildLayout()
{
    m_titleEdit = new QLineEdit(this);

    m_typeCombo = new QComboBox(this);
    for (int i = 0; i < issueskit::kIssueTypeCount; i++) {
        m_typeCombo->addItem(QString::fromUtf8(
            issueskit::IssueTypeDisplayName(issueskit::IssueTypeAt(i))));
    }

    m_priorityCombo = new QComboBox(this);
    for (int i = 0; i < issueskit::kIssuePriorityCount; i++) {
        m_priorityCombo->addItem(QString::fromUtf8(
            issueskit::IssuePriorityDisplayName(issueskit::IssuePriorityAt(i))));
    }

    m_statusCombo = new QComboBox(this);
    for (int i = 0; i < issueskit::kIssueStatusCount; i++) {
        m_statusCombo->addItem(QString::fromUtf8(
            issueskit::IssueStatusDisplayName(issueskit::IssueStatusAt(i))));
    }

    // ResolutionKind has no default case, so the picker carries an explicit
    // "None" entry at index 0 that maps back to "unset".
    m_resolutionCombo = new QComboBox(this);
    m_resolutionCombo->addItem(i18nc("@item no resolution kind is set", "None"));
    for (int i = 0; i < issueskit::kResolutionKindCount; i++) {
        m_resolutionCombo->addItem(QString::fromUtf8(
            issueskit::ResolutionKindDisplayName(issueskit::ResolutionKindAt(i))));
    }

    m_reportedEdit = new QDateEdit(this);
    m_reportedEdit->setDisplayFormat(QString::fromLatin1(kDayFormat));
    m_reportedEdit->setCalendarPopup(true);

    m_reportedByEdit = new QLineEdit(this);
    m_areaEdit = new QLineEdit(this);
    m_milestoneEdit = new QLineEdit(this);
    m_estimateEdit = new QLineEdit(this);
    m_labelsEdit = new QLineEdit(this);
    m_assigneesEdit = new QLineEdit(this);

    auto *form = new QFormLayout();
    form->addRow(i18n("Title:"), m_titleEdit);
    form->addRow(i18n("Type:"), m_typeCombo);
    form->addRow(i18n("Priority:"), m_priorityCombo);
    form->addRow(i18n("Status:"), m_statusCombo);
    form->addRow(i18n("Resolution:"), m_resolutionCombo);
    form->addRow(i18n("Reported:"), m_reportedEdit);
    form->addRow(i18n("Reported by:"), m_reportedByEdit);
    form->addRow(i18n("Area:"), m_areaEdit);
    form->addRow(i18n("Milestone:"), m_milestoneEdit);
    form->addRow(i18n("Estimate:"), m_estimateEdit);
    form->addRow(i18n("Labels:"), m_labelsEdit);
    form->addRow(i18n("Assignees:"), m_assigneesEdit);

    auto *listHint = new QLabel(i18n("Separate multiple entries with commas."), this);
    listHint->setWordWrap(true);
    form->addRow(QString(), listHint);

    m_descriptionEdit = makeTextArea();
    m_stepsEdit = makeTextArea();
    m_environmentEdit = makeTextArea();
    m_notesEdit = makeTextArea();
    m_resolutionTextEdit = makeTextArea();

    auto *tabs = new QTabWidget(this);
    tabs->addTab(makeTextPage(m_descriptionEdit, QString()), i18n("Description"));
    tabs->addTab(makeTextPage(m_stepsEdit, i18n("One step per line.")),
                 i18n("Steps to Reproduce"));
    tabs->addTab(makeTextPage(m_environmentEdit, QString()), i18n("Environment"));
    tabs->addTab(makeTextPage(m_notesEdit, QString()), i18n("Notes / Investigation"));
    tabs->addTab(makeTextPage(m_resolutionTextEdit, QString()), i18n("Resolution"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &IssueEditDialog::commit);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    m_titleEdit->setFocus();
}

void IssueEditDialog::populate()
{
    m_titleEdit->setText(QString::fromStdString(m_draft.title));
    m_typeCombo->setCurrentIndex(static_cast<int>(m_draft.type));
    m_priorityCombo->setCurrentIndex(static_cast<int>(m_draft.priority));
    m_statusCombo->setCurrentIndex(static_cast<int>(m_draft.status));
    m_resolutionCombo->setCurrentIndex(
        m_draft.resolutionKind.has_value()
            ? static_cast<int>(*m_draft.resolutionKind) + 1
            : 0);

    m_reportedEdit->setDate(
        QDate::fromString(dayString(m_draft.reported), QString::fromLatin1(kDayFormat)));

    m_reportedByEdit->setText(QString::fromStdString(m_draft.reportedBy));
    m_areaEdit->setText(QString::fromStdString(m_draft.area));
    m_milestoneEdit->setText(m_draft.milestone.has_value()
                                 ? QString::fromStdString(*m_draft.milestone)
                                 : QString());
    m_estimateEdit->setText(m_draft.estimate.has_value()
                                ? QString::fromStdString(
                                      issueskit::FormatDouble(*m_draft.estimate))
                                : QString());
    m_labelsEdit->setText(QString::fromStdString(issueskit::Join(m_draft.labels, ", ")));
    m_assigneesEdit->setText(
        QString::fromStdString(issueskit::Join(m_draft.assignees, ", ")));

    m_descriptionEdit->setPlainText(QString::fromStdString(m_draft.description));
    m_stepsEdit->setPlainText(
        QString::fromStdString(issueskit::Join(m_draft.stepsToReproduce, "\n")));
    m_environmentEdit->setPlainText(QString::fromStdString(m_draft.environment));
    m_notesEdit->setPlainText(QString::fromStdString(m_draft.notes));
    m_resolutionTextEdit->setPlainText(QString::fromStdString(m_draft.resolution));
}

void IssueEditDialog::commit()
{
    Issue result = m_draft;

    result.title = m_titleEdit->text().toStdString();
    result.type = issueskit::IssueTypeAt(m_typeCombo->currentIndex());
    result.priority = issueskit::IssuePriorityAt(m_priorityCombo->currentIndex());

    const issueskit::IssueStatus previousStatus = result.status;
    result.status = issueskit::IssueStatusAt(m_statusCombo->currentIndex());

    const int resolutionIndex = m_resolutionCombo->currentIndex();
    if (resolutionIndex <= 0) {
        result.resolutionKind.reset();
    } else {
        result.resolutionKind = issueskit::ResolutionKindAt(resolutionIndex - 1);
    }

    // A QDate carries no time zone; IssueDate::Parse turns it into midnight UTC.
    // An unparsable value keeps the previous date rather than jumping to today.
    Timestamp reported = 0;
    const std::string reportedText =
        m_reportedEdit->date().toString(QString::fromLatin1(kDayFormat)).toStdString();
    if (issueskit::IssueDate::Parse(reportedText, reported)) {
        result.reported = reported;
    }

    result.reportedBy = m_reportedByEdit->text().toStdString();
    result.area = m_areaEdit->text().toStdString();

    const std::string milestone = issueskit::Trim(m_milestoneEdit->text().toStdString());
    if (milestone.empty()) {
        result.milestone.reset();
    } else {
        result.milestone = milestone;
    }

    // Unparsable numeric input clears the estimate rather than failing the save,
    // matching Double(estimateText) in the Apple editor.
    double estimate = 0.0;
    if (issueskit::ParseDouble(issueskit::Trim(m_estimateEdit->text().toStdString()),
                               estimate)) {
        result.estimate = estimate;
    } else {
        result.estimate.reset();
    }

    result.labels = issueskit::SplitTrimNonEmpty(m_labelsEdit->text().toStdString(), ',');
    result.assignees =
        issueskit::SplitTrimNonEmpty(m_assigneesEdit->text().toStdString(), ',');

    result.description = m_descriptionEdit->toPlainText().toStdString();
    result.stepsToReproduce =
        issueskit::SplitTrimNonEmpty(m_stepsEdit->toPlainText().toStdString(), '\n');
    result.environment = m_environmentEdit->toPlainText().toStdString();
    result.notes = m_notesEdit->toPlainText().toStdString();
    result.resolution = m_resolutionTextEdit->toPlainText().toStdString();

    result.updatedAt = issueskit::IssueDate::Now();

    // FIX relative to the Apple app: closedAt is stamped when the status becomes
    // Resolved and cleared when it moves off Resolved. No Apple code path ever
    // writes that field (its own gap list, item 2). The Haiku port does the same.
    const bool wasResolved = previousStatus == issueskit::kIssueStatusResolved;
    const bool isResolved = result.status == issueskit::kIssueStatusResolved;
    if (isResolved && !wasResolved) {
        result.closedAt = result.updatedAt;
    } else if (!isResolved) {
        result.closedAt.reset();
    }

    m_result = result;
    accept();
}

const Issue &IssueEditDialog::result() const
{
    return m_result;
}

} // namespace ihaveissues
