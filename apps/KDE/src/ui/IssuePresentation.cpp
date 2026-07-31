/*
 * IssuePresentation.cpp
 */
#include "IssuePresentation.h"

#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QRect>
#include <QStringList>

#include <KColorScheme>

using issueskit::IssuePriority;
using issueskit::IssueStatus;
using issueskit::IssueType;

namespace ihaveissues
{

namespace
{

const int kFallbackIconSize = 16;

/*! Returns the first name the icon theme actually provides.
 *
 *  VERIFY: the Breeze icon names below. They were chosen because they are
 *  long-standing entries in the Breeze icon set, but an icon theme is data, not
 *  API, and a name that has been renamed simply falls through to the next
 *  candidate and finally to the painted badge. Nothing breaks; the icons just
 *  get plainer. Check what actually resolves and tighten the lists.
 */
QIcon firstThemeIcon(const QStringList &names)
{
    for (const QString &name : names) {
        if (QIcon::hasThemeIcon(name)) {
            return QIcon::fromTheme(name);
        }
    }
    return QIcon();
}

//! A tinted disc with a single letter -- the no-icon-theme fallback.
QIcon paintedBadge(const QString &letter, const QColor &tint)
{
    QPixmap pixmap(kFallbackIconSize, kFallbackIconSize);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tint);
    painter.drawEllipse(0, 0, kFallbackIconSize - 1, kFallbackIconSize - 1);

    QFont font = QGuiApplication::font();
    font.setBold(true);
    font.setPixelSize(kFallbackIconSize - 6);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, kFallbackIconSize, kFallbackIconSize),
                     Qt::AlignCenter, letter);
    painter.end();

    return QIcon(pixmap);
}

/*! One KColorScheme foreground role, resolved against the current palette.
 *
 *  VERIFY: KColorScheme lives in KConfigWidgets and is included as
 *  <KColorScheme>. The ForegroundRole names used here -- NormalText,
 *  InactiveText, LinkText, VisitedText, NegativeText, NeutralText, PositiveText
 *  -- are the long-standing set.
 */
QColor schemeColor(KColorScheme::ForegroundRole role)
{
    const KColorScheme scheme(QPalette::Active, KColorScheme::View);
    return scheme.foreground(role).color();
}

} // unnamed namespace

QString issueTypeBadgeLetter(IssueType type)
{
    switch (type) {
    case issueskit::kIssueTypeBug:
        return QStringLiteral("B");
    case issueskit::kIssueTypeFeature:
        return QStringLiteral("F");
    case issueskit::kIssueTypeTask:
        return QStringLiteral("T");
    case issueskit::kIssueTypeQuestion:
        return QStringLiteral("?");
    }
    return QStringLiteral("T");
}

QColor issueTypeTint(IssueType type)
{
    // Apple used pink/purple/blue/teal. The nearest semantic roles KDE offers
    // are negative/visited/link/positive, which keeps the four types visually
    // distinct while still tracking the user's colour scheme.
    switch (type) {
    case issueskit::kIssueTypeBug:
        return schemeColor(KColorScheme::NegativeText);
    case issueskit::kIssueTypeFeature:
        return schemeColor(KColorScheme::VisitedText);
    case issueskit::kIssueTypeTask:
        return schemeColor(KColorScheme::LinkText);
    case issueskit::kIssueTypeQuestion:
        return schemeColor(KColorScheme::PositiveText);
    }
    return schemeColor(KColorScheme::LinkText);
}

QIcon issueTypeIcon(IssueType type)
{
    QStringList names;
    switch (type) {
    case issueskit::kIssueTypeBug:
        names << QStringLiteral("tools-report-bug") << QStringLiteral("bug");
        break;
    case issueskit::kIssueTypeFeature:
        names << QStringLiteral("draw-star") << QStringLiteral("rating")
              << QStringLiteral("starred");
        break;
    case issueskit::kIssueTypeTask:
        names << QStringLiteral("view-task") << QStringLiteral("view-list-details");
        break;
    case issueskit::kIssueTypeQuestion:
        names << QStringLiteral("dialog-question") << QStringLiteral("help-contents");
        break;
    }

    const QIcon themed = firstThemeIcon(names);
    if (!themed.isNull()) {
        return themed;
    }
    return paintedBadge(issueTypeBadgeLetter(type), issueTypeTint(type));
}

QColor issuePriorityTint(IssuePriority priority)
{
    // A monotone ramp: grey, normal, amber, red. Apple's yellow/orange pair
    // collapses to KColorScheme's single NeutralText, so medium takes the plain
    // text colour and high takes the neutral one -- the ordering the user reads
    // off the list is preserved, which is what the indicator is for.
    switch (priority) {
    case issueskit::kIssuePriorityLow:
        return schemeColor(KColorScheme::InactiveText);
    case issueskit::kIssuePriorityMedium:
        return schemeColor(KColorScheme::NormalText);
    case issueskit::kIssuePriorityHigh:
        return schemeColor(KColorScheme::NeutralText);
    case issueskit::kIssuePriorityCritical:
        return schemeColor(KColorScheme::NegativeText);
    }
    return schemeColor(KColorScheme::NormalText);
}

QIcon issueStatusIcon(IssueStatus status)
{
    QStringList names;
    switch (status) {
    case issueskit::kIssueStatusOpen:
        names << QStringLiteral("media-playback-stop") << QStringLiteral("dialog-information");
        break;
    case issueskit::kIssueStatusInProgress:
        names << QStringLiteral("chronometer") << QStringLiteral("clock");
        break;
    case issueskit::kIssueStatusBlocked:
        names << QStringLiteral("dialog-cancel") << QStringLiteral("process-stop");
        break;
    case issueskit::kIssueStatusResolved:
        names << QStringLiteral("dialog-ok-apply") << QStringLiteral("checkmark");
        break;
    }
    return firstThemeIcon(names);
}

} // namespace ihaveissues
