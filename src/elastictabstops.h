// Elastic tabstops for QPlainTextEdit-based editors.
// See https://nick-gravgaard.com/elastic-tabstops/
//
// This file has no Qt Creator dependencies, so the engine can be used (and
// tested) with any QPlainTextEdit.

#pragma once

#include <QFont>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QTextOption>

QT_BEGIN_NAMESPACE
class QTextBlock;
class QTextLayout;
QT_END_NAMESPACE

namespace ElasticTabstops {

// The pure algorithm.
//
// cellWidths[i] holds the text widths of the tab-terminated cells of line i
// (i.e. one entry per tab character; the text after the last tab is not a
// cell). Returns, for every line, the absolute x positions of its tab stops.
//
// A "column block" is a run of consecutive lines that all have a cell in
// column k. All cells in a column block get the same width:
//     max(widest text in the block + padding, minimumWidth)
QList<QList<qreal>> computeTabStops(const QList<QList<qreal>> &cellWidths,
                                    qreal padding,
                                    qreal minimumWidth);

// Applies elastic tabstops to a QPlainTextEdit. Deletes itself together with
// the editor; deleting it earlier restores the normal tab stops.
//
// QPlainTextDocumentLayout lays out each block with the document's default
// QTextOption, so there is no supported way to give individual blocks their
// own tab stops. Instead, right before each paint, the handler re-lays out the
// visible blocks with per-block tab positions, keeping the line geometry Qt
// chose. QPlainTextDocumentLayout only lays a block out again when its layout
// was cleared (i.e. after an edit), so the result sticks for painting, hit
// testing and cursor positioning until the next change.
class Handler : public QObject
{
    Q_OBJECT

public:
    explicit Handler(QPlainTextEdit *editor);
    ~Handler() override;

    QPlainTextEdit *editor() const { return m_editor; }
    static Handler *forEditor(const QPlainTextEdit *editor);

    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // Padding added to the right of each cell's text, in multiples of the
    // width of a space.
    void setPadding(qreal spaces);
    // Minimum cell width in multiples of the width of a space. 0 means "use
    // the editor's normal tab stop distance", so indentation looks the same
    // as with fixed tabs.
    void setMinimumWidth(qreal spaces);
    // How many lines above/below the visible area are examined to find the
    // extent of the column blocks.
    void setMaximumScanLines(int lines);

    // Recomputes and applies the tab stops for the visible blocks. Returns
    // true if any block had to be re-laid out.
    bool apply();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    const QList<qreal> &cellWidths(const QTextBlock &block);
    void restoreDefaultTabs();
    void scheduleRepaint();

    QPointer<QPlainTextEdit> m_editor;
    bool m_enabled = true;
    qreal m_paddingSpaces = 2;
    qreal m_minimumWidthSpaces = 0;
    int m_maximumScanLines = 2000;

    struct CacheEntry
    {
        QString text;
        QList<qreal> widths;
    };
    QHash<int, CacheEntry> m_cache; // keyed by block number
    QFont m_cacheFont;
};

// Re-lays out a block's QTextLayout with the given tab stops, keeping the
// position and width of the existing lines. Returns false if the block
// already had these tabs.
bool relayoutBlock(QTextLayout *layout, const QList<QTextOption::Tab> &tabs);

} // namespace ElasticTabstops
