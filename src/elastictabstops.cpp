#include "elastictabstops.h"

#include <QAbstractTextDocumentLayout>
#include <QEvent>
#include <QFontMetricsF>
#include <QLoggingCategory>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>

namespace ElasticTabstops {

Q_LOGGING_CATEGORY(log, "qtc.elastictabstops", QtWarningMsg)

QList<QList<qreal>> computeTabStops(const QList<QList<qreal>> &cellWidths,
                                    qreal padding,
                                    qreal minimumWidth)
{
    const qsizetype lineCount = cellWidths.size();
    qsizetype maxColumns = 0;
    for (const QList<qreal> &line : cellWidths)
        maxColumns = std::max(maxColumns, line.size());

    // First the width of every cell, then turn the widths into positions.
    QList<QList<qreal>> stops(lineCount);
    for (qsizetype i = 0; i < lineCount; ++i)
        stops[i].resize(cellWidths[i].size());

    for (qsizetype column = 0; column < maxColumns; ++column) {
        qsizetype line = 0;
        while (line < lineCount) {
            if (cellWidths[line].size() <= column) {
                ++line;
                continue;
            }
            const qsizetype blockStart = line;
            qreal blockWidth = minimumWidth;
            for (; line < lineCount && cellWidths[line].size() > column; ++line)
                blockWidth = std::max(blockWidth, cellWidths[line][column] + padding);
            for (qsizetype i = blockStart; i < line; ++i)
                stops[i][column] = blockWidth;
        }
    }

    for (QList<qreal> &line : stops) {
        qreal x = 0;
        for (qreal &stop : line) {
            x += stop;
            stop = x;
        }
    }
    return stops;
}

static const char HANDLER_PROPERTY[] = "_q_elasticTabstopsHandler";

bool relayoutBlock(QTextLayout *layout, const QList<QTextOption::Tab> &tabs)
{
    // Not laid out yet: QPlainTextDocumentLayout will lay it out with the
    // default options anyway, so there is nothing to keep.
    if (layout->lineCount() == 0)
        return false;

    QTextOption option = layout->textOption();
    if (option.tabs() == tabs)
        return false;
    option.setTabs(tabs);

    struct LineGeometry
    {
        QPointF position;
        qreal width;
    };
    QList<LineGeometry> lines;
    for (int i = 0; i < layout->lineCount(); ++i) {
        const QTextLine line = layout->lineAt(i);
        lines.append({line.position(), line.width()});
    }

    // Same procedure as QPlainTextDocumentLayout::layoutBlock(), but with the
    // existing line geometry instead of the document's text width.
    layout->setTextOption(option);
    layout->beginLayout();
    qreal y = lines.first().position.y();
    for (int i = 0;; ++i) {
        QTextLine line = layout->createLine();
        if (!line.isValid())
            break;
        const LineGeometry &geometry = lines.at(std::min<qsizetype>(i, lines.size() - 1));
        line.setLeadingIncluded(true);
        line.setLineWidth(geometry.width);
        line.setPosition(i < lines.size() ? geometry.position : QPointF(geometry.position.x(), y));
        y = line.position().y() + line.height();
    }
    layout->endLayout();
    return true;
}

Handler::Handler(QPlainTextEdit *editor)
    : m_editor(editor)
{
    // Not a child of the editor: QWidget deletes its children while it is
    // already half destroyed. QWidget::~QWidget emits destroyed() before the
    // QPointer is cleared, but subclasses (e.g. Qt Creator's TextEditorWidget)
    // may have deleted the document by then, so don't touch the editor at all.
    connect(editor, &QObject::destroyed, this, [this] {
        m_editor = nullptr;
        delete this;
    });
    editor->viewport()->installEventFilter(this);
    editor->setProperty(HANDLER_PROPERTY, QVariant::fromValue<QObject *>(this));
    qCDebug(log) << "Attached to" << editor;
    scheduleRepaint();
}

Handler::~Handler()
{
    if (m_editor) {
        m_editor->setProperty(HANDLER_PROPERTY, QVariant());
        m_editor->viewport()->removeEventFilter(this);
        restoreDefaultTabs();
    }
}

Handler *Handler::forEditor(const QPlainTextEdit *editor)
{
    return qobject_cast<Handler *>(editor->property(HANDLER_PROPERTY).value<QObject *>());
}

void Handler::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        restoreDefaultTabs();
    scheduleRepaint();
}

void Handler::setPadding(qreal spaces)
{
    m_paddingSpaces = spaces;
    scheduleRepaint();
}

void Handler::setMinimumWidth(qreal spaces)
{
    m_minimumWidthSpaces = spaces;
    scheduleRepaint();
}

void Handler::setMaximumScanLines(int lines)
{
    m_maximumScanLines = lines;
    scheduleRepaint();
}

const QList<qreal> &Handler::cellWidths(const QTextBlock &block)
{
    const QString text = block.text();
    CacheEntry &entry = m_cache[block.blockNumber()];
    if (!entry.widths.isEmpty() && entry.text == text)
        return entry.widths;

    entry.text = text;
    entry.widths.clear();
    const QFontMetricsF metrics(m_cacheFont);
    qsizetype cellStart = 0;
    for (qsizetype tab = text.indexOf(QLatin1Char('\t')); tab >= 0;
         tab = text.indexOf(QLatin1Char('\t'), cellStart)) {
        entry.widths.append(metrics.horizontalAdvance(text.mid(cellStart, tab - cellStart)));
        cellStart = tab + 1;
    }
    return entry.widths;
}

bool Handler::apply()
{
    if (!m_editor || !m_enabled)
        return false;

    QTextDocument *document = m_editor->document();
    QAbstractTextDocumentLayout *documentLayout = document->documentLayout();
    if (document->defaultFont() != m_cacheFont) {
        m_cache.clear();
        m_cacheFont = document->defaultFont();
    }

    const QTextBlock firstVisible = m_editor->cursorForPosition(QPoint(0, 0)).block();
    const QTextBlock lastVisible
        = m_editor->cursorForPosition(QPoint(0, m_editor->viewport()->height() - 1)).block();
    if (!firstVisible.isValid() || !lastVisible.isValid()
        || lastVisible.blockNumber() < firstVisible.blockNumber()) {
        return false;
    }

    // Column blocks can extend beyond the visible area: they end at the first
    // line without any tab.
    const auto hasTab = [](const QTextBlock &block) {
        return block.text().contains(QLatin1Char('\t'));
    };
    QTextBlock start = firstVisible;
    for (int i = 0; i < m_maximumScanLines && hasTab(start); ++i) {
        const QTextBlock previous = start.previous();
        if (!previous.isValid() || !hasTab(previous))
            break;
        start = previous;
    }
    QTextBlock end = lastVisible;
    for (int i = 0; i < m_maximumScanLines && hasTab(end); ++i) {
        const QTextBlock next = end.next();
        if (!next.isValid() || !hasTab(next))
            break;
        end = next;
    }

    QList<QTextBlock> blocks;
    QList<QList<qreal>> widths;
    for (QTextBlock block = start; block.isValid(); block = block.next()) {
        blocks.append(block);
        widths.append(cellWidths(block));
        if (block == end)
            break;
    }

    const QFontMetricsF metrics(m_cacheFont);
    const qreal space = metrics.horizontalAdvance(QLatin1Char(' '));
    const qreal minimumWidth = m_minimumWidthSpaces > 0
                                   ? m_minimumWidthSpaces * space
                                   : document->defaultTextOption().tabStopDistance();
    const QList<QList<qreal>> stops = computeTabStops(widths, m_paddingSpaces * space,
                                                      minimumWidth);

    bool changed = false;
    const int firstNumber = firstVisible.blockNumber();
    const int lastNumber = lastVisible.blockNumber();
    for (qsizetype i = 0; i < blocks.size(); ++i) {
        const QTextBlock &block = blocks[i];
        if (block.blockNumber() < firstNumber || block.blockNumber() > lastNumber
            || !block.isVisible()) {
            continue;
        }
        documentLayout->blockBoundingRect(block); // Makes sure the block is laid out.
        QList<QTextOption::Tab> tabs;
        tabs.reserve(stops[i].size());
        for (qreal position : stops[i])
            tabs.append(QTextOption::Tab(position, QTextOption::LeftTab));
        if (relayoutBlock(block.layout(), tabs)) {
            qCDebug(log) << "Block" << block.blockNumber() << "tab stops" << stops[i];
            changed = true;
        }
    }
    return changed;
}

void Handler::restoreDefaultTabs()
{
    QTextDocument *document = m_editor->document();
    const QList<QTextOption::Tab> defaultTabs = document->defaultTextOption().tabs();
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next())
        relayoutBlock(block.layout(), defaultTabs);
}

void Handler::scheduleRepaint()
{
    if (m_editor)
        m_editor->viewport()->update();
}

bool Handler::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Paint && m_editor && watched == m_editor->viewport()) {
        // Changing the tabs of one line can move the tab stops of its
        // neighbours, which may lie outside the area being repainted.
        if (apply()
            && static_cast<QPaintEvent *>(event)->rect() != m_editor->viewport()->rect()) {
            scheduleRepaint();
        }
    }
    return QObject::eventFilter(watched, event);
}

} // namespace ElasticTabstops
