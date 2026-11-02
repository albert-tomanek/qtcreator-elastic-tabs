#include "elastictabstops.h"

#include <QPlainTextEdit>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>

using namespace ElasticTabstops;

class tst_ElasticTabstops : public QObject
{
    Q_OBJECT

private slots:
    void computeSingleColumn();
    void computeColumnBlocksBreak();
    void computeMinimumWidth();
    void editorAlignsColumns();
    void editorFollowsEdits();
    void editorRestoresWhenDisabled();
    void editorDestroyedAfterItsDocument();

private:
    static qreal xOf(QPlainTextEdit &editor, int blockNumber, int column);
    static void render(QPlainTextEdit &editor);
};

void tst_ElasticTabstops::computeSingleColumn()
{
    const auto stops = computeTabStops({{10}, {20}, {}, {5}}, 2, 0);
    QCOMPARE(stops, (QList<QList<qreal>>{{22}, {22}, {}, {7}}));
}

void tst_ElasticTabstops::computeColumnBlocksBreak()
{
    // Column 1 only exists in lines 0 and 2, so those are separate blocks.
    const auto stops = computeTabStops({{10, 5}, {20}, {3, 8}}, 2, 0);
    QCOMPARE(stops, (QList<QList<qreal>>{{22, 29}, {22}, {22, 32}}));
}

void tst_ElasticTabstops::computeMinimumWidth()
{
    const auto stops = computeTabStops({{0, 0, 30}, {0, 0}}, 2, 16);
    QCOMPARE(stops, (QList<QList<qreal>>{{16, 32, 64}, {16, 32}}));
}

qreal tst_ElasticTabstops::xOf(QPlainTextEdit &editor, int blockNumber, int column)
{
    const QTextBlock block = editor.document()->findBlockByNumber(blockNumber);
    return block.layout()->lineAt(0).cursorToX(column);
}

void tst_ElasticTabstops::render(QPlainTextEdit &editor)
{
    editor.grab(); // Delivers a paint event to the viewport.
}

static void setup(QPlainTextEdit &editor)
{
    QFont font(QStringLiteral("monospace"));
    font.setStyleHint(QFont::Monospace);
    editor.setFont(font);
    editor.setLineWrapMode(QPlainTextEdit::NoWrap);
    editor.setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * 4);
    editor.resize(600, 400);
    editor.show();
}

void tst_ElasticTabstops::editorAlignsColumns()
{
    QPlainTextEdit editor;
    setup(editor);
    editor.setPlainText(QStringLiteral("a\tb\n"
                                       "a much longer cell\tc\n"
                                       "no tabs here\n"
                                       "x\ty"));
    auto handler = new Handler(&editor);
    handler->setPadding(2);
    render(editor);

    const qreal bX = xOf(editor, 0, 2);
    const qreal cX = xOf(editor, 1, 19);
    QCOMPARE(bX, cX);
    const qreal space = QFontMetricsF(editor.font()).horizontalAdvance(QLatin1Char(' '));
    QVERIFY(qAbs(cX - xOf(editor, 1, 0) - 20 * space) < 1); // 18 chars + 2 padding

    // The line without tabs ends the column block.
    const qreal yX = xOf(editor, 3, 2);
    QVERIFY(yX < cX);
    QVERIFY2(qAbs(yX - xOf(editor, 3, 0) - editor.tabStopDistance()) < 1, qPrintable(QString("y at %1, tab %2, space %3").arg(yX).arg(editor.tabStopDistance()).arg(space))); // minimum width = tab size
}

void tst_ElasticTabstops::editorFollowsEdits()
{
    QPlainTextEdit editor;
    setup(editor);
    editor.setPlainText(QStringLiteral("a\tb\nshort\tc"));
    new Handler(&editor);
    render(editor);
    QCOMPARE(xOf(editor, 0, 2), xOf(editor, 1, 6));

    // Grow the first line's cell past the second one's.
    QTextCursor cursor(editor.document()->firstBlock());
    cursor.insertText(QStringLiteral("very very long "));
    render(editor);
    render(editor);
    const int bColumn = editor.document()->firstBlock().text().indexOf(QLatin1Char('b'));
    QCOMPARE(xOf(editor, 0, bColumn), xOf(editor, 1, 6));
    QVERIFY(xOf(editor, 1, 6) > xOf(editor, 1, 5) + 5);
}

void tst_ElasticTabstops::editorRestoresWhenDisabled()
{
    QPlainTextEdit editor;
    setup(editor);
    editor.setPlainText(QStringLiteral("a\tb\nthis is long\tc"));
    const qreal fixedX = [&] {
        render(editor);
        return xOf(editor, 0, 2);
    }();

    auto handler = new Handler(&editor);
    render(editor);
    QVERIFY(xOf(editor, 0, 2) > fixedX);

    handler->setEnabled(false);
    render(editor);
    QCOMPARE(xOf(editor, 0, 2), fixedX);

    handler->setEnabled(true);
    render(editor);
    QVERIFY(xOf(editor, 0, 2) > fixedX);

    delete handler;
    QCOMPARE(xOf(editor, 0, 2), fixedX);
}

void tst_ElasticTabstops::editorDestroyedAfterItsDocument()
{
    // Like Qt Creator's TextEditorWidget: the document is gone by the time
    // QWidget::~QWidget emits destroyed().
    class Editor : public QPlainTextEdit
    {
    public:
        Editor()
        {
            m_document = new QTextDocument;
            m_document->setDocumentLayout(new QPlainTextDocumentLayout(m_document));
            setDocument(m_document);
        }
        ~Editor() override { delete m_document; }

    private:
        QTextDocument *m_document;
    };

    auto editor = new Editor;
    setup(*editor);
    editor->setPlainText(QStringLiteral("a\tb\nlonger\tc"));
    new Handler(editor);
    render(*editor);
    delete editor; // Must not touch the deleted document.
}

QTEST_MAIN(tst_ElasticTabstops)

#include "tst_elastictabstops.moc"
