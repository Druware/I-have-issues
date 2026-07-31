/*
 * IssueItemDelegate.cpp
 */
#include "IssueItemDelegate.h"

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QIcon>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QStyle>
#include <QStyleOptionViewItem>

#include "IssueListModel.h"
#include "IssuePresentation.h"

namespace ihaveissues
{

namespace
{

const int kHorizontalMargin = 6;
const int kVerticalMargin = 3;
const int kIconSize = 16;
const int kPriorityDotSize = 10;
const int kGap = 6;

//! The "#NNN" line is drawn at this fraction of the list font size.
const qreal kSmallFontFactor = 0.85;

QFont smallFont(const QFont &base)
{
    QFont font(base);
    const qreal size = base.pointSizeF();
    if (size > 0.0) {
        font.setPointSizeF(size * kSmallFontFactor);
    } else if (base.pixelSize() > 0) {
        font.setPixelSize(qMax(1, static_cast<int>(base.pixelSize() * kSmallFontFactor)));
    }
    return font;
}

} // unnamed namespace

IssueItemDelegate::IssueItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

IssueItemDelegate::~IssueItemDelegate() = default;

void IssueItemDelegate::paint(QPainter *painter,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    if (!index.data(IssueListModel::IsIssueRole).toBool()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    // The style paints the background, focus rect and selection; the text and
    // the decoration are drawn below, so both are cleared out of the option.
    opt.text.clear();
    opt.icon = QIcon();
    opt.features &= ~QStyleOptionViewItem::HasDecoration;

    const QWidget *widget = opt.widget;
    QStyle *style = widget != nullptr ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    const bool selected = (opt.state & QStyle::State_Selected) != 0;
    const QColor textColor = selected
        ? opt.palette.color(QPalette::HighlightedText)
        : opt.palette.color(QPalette::Text);
    const QColor dimColor = selected
        ? opt.palette.color(QPalette::HighlightedText)
        : opt.palette.color(QPalette::Disabled, QPalette::Text);

    const QRect content = opt.rect.adjusted(kHorizontalMargin, kVerticalMargin,
                                            -kHorizontalMargin, -kVerticalMargin);
    if (!content.isValid()) {
        return;
    }

    painter->save();

    // Leading type icon.
    const auto type = static_cast<issueskit::IssueType>(
        index.data(IssueListModel::TypeRole).toInt());
    const QRect iconRect(content.left(),
                         content.top() + (content.height() - kIconSize) / 2,
                         kIconSize, kIconSize);
    issueTypeIcon(type).paint(painter, iconRect, Qt::AlignCenter, QIcon::Normal);

    // Trailing priority indicator.
    const auto priority = static_cast<issueskit::IssuePriority>(
        index.data(IssueListModel::PriorityRole).toInt());
    const QRect dotRect(content.right() - kPriorityDotSize,
                        content.top() + (content.height() - kPriorityDotSize) / 2,
                        kPriorityDotSize, kPriorityDotSize);
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(issuePriorityTint(priority));
    painter->drawEllipse(dotRect);
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Text block between the two.
    const int textLeft = iconRect.right() + 1 + kGap;
    int textRight = dotRect.left() - kGap;
    if (textRight < textLeft + 10) {
        textRight = textLeft + 10;
    }
    const int textWidth = textRight - textLeft;

    QFont titleFont(opt.font);
    titleFont.setBold(true);
    const QFont numberFont = smallFont(opt.font);

    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics numberMetrics(numberFont);

    const QRect titleRect(textLeft, content.top(), textWidth, titleMetrics.height());
    const QRect numberRect(textLeft, titleRect.bottom() + 1, textWidth,
                           numberMetrics.height());

    const QString title = index.data(IssueListModel::TitleRole).toString();
    painter->setFont(titleFont);
    painter->setPen(textColor);
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(title, Qt::ElideRight, textWidth));

    painter->setFont(numberFont);
    painter->setPen(dimColor);
    painter->drawText(numberRect, Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(IssueListModel::NumberRole).toString());

    painter->restore();
}

QSize IssueItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    const QSize base = QStyledItemDelegate::sizeHint(option, index);
    if (!index.data(IssueListModel::IsIssueRole).toBool()) {
        return base;
    }

    QFont titleFont(option.font);
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics numberMetrics(smallFont(option.font));

    const int height = titleMetrics.height() + 1 + numberMetrics.height()
        + 2 * kVerticalMargin;
    return QSize(base.width(), qMax(base.height(), height));
}

} // namespace ihaveissues
