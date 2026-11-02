#include "elastictabstops.h"

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/dialogs/ioptionspage.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/editormanager/ieditor.h>

#include <extensionsystem/iplugin.h>

#include <texteditor/texteditor.h>
#include <texteditor/texteditorconstants.h>

#include <utils/aspects.h>
#include <utils/layoutbuilder.h>

#include <QAction>

using namespace Core;
using namespace Utils;

namespace ElasticTabstops::Internal {

const char SETTINGS_PAGE_ID[] = "ElasticTabstops.Settings";
const char TOGGLE_ACTION_ID[] = "ElasticTabstops.Toggle";

class Settings final : public AspectContainer
{
public:
    Settings()
    {
        setSettingsGroup("ElasticTabstops");
        setAutoApply(false);

        enabled.setSettingsKey("Enabled");
        enabled.setDefaultValue(true);
        enabled.setLabelText("Enable elastic tabstops");
        enabled.setToolTip("Tab characters align with the corresponding cells of the adjacent "
                           "lines instead of jumping to fixed positions.");

        padding.setSettingsKey("Padding");
        padding.setDefaultValue(2);
        padding.setRange(0, 32);
        padding.setLabelText("Cell padding:");
        padding.setSuffix(" spaces");
        padding.setToolTip("Space between the end of a cell's text and the next tab stop.");

        minimumWidth.setSettingsKey("MinimumWidth");
        minimumWidth.setDefaultValue(0);
        minimumWidth.setRange(0, 64);
        minimumWidth.setLabelText("Minimum cell width:");
        minimumWidth.setSuffix(" spaces");
        minimumWidth.setSpecialValueText("Tab size");
        minimumWidth.setToolTip("Width of empty cells, e.g. indentation. \"Tab size\" uses the "
                                "editor's normal tab width.");

        maximumScanLines.setSettingsKey("MaximumScanLines");
        maximumScanLines.setDefaultValue(2000);
        maximumScanLines.setRange(10, 1000000);
        maximumScanLines.setLabelText("Lines to scan around the visible area:");
        maximumScanLines.setToolTip("Limits how far a column block is followed outside the "
                                    "visible area. Larger values are more accurate in large, "
                                    "tab-heavy files but slower.");

        setLayouter([this] {
            using namespace Layouting;
            return Column {
                enabled,
                Form {
                    padding, br,
                    minimumWidth, br,
                    maximumScanLines, br,
                },
                st,
            };
        });

        readSettings();
    }

    BoolAspect enabled{this};
    IntegerAspect padding{this};
    IntegerAspect minimumWidth{this};
    IntegerAspect maximumScanLines{this};
};

static Settings &settings()
{
    static Settings theSettings;
    return theSettings;
}

class SettingsPage final : public IOptionsPage
{
public:
    SettingsPage()
    {
        setId(SETTINGS_PAGE_ID);
        setDisplayName("Elastic Tabstops");
        setCategory(TextEditor::Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
        setSettingsProvider([] { return &settings(); });
    }
};

static void applySettings(Handler *handler)
{
    const Settings &s = settings();
    handler->setPadding(s.padding());
    handler->setMinimumWidth(s.minimumWidth());
    handler->setMaximumScanLines(int(s.maximumScanLines()));
    handler->setEnabled(s.enabled());
}

static void attachToEditor(IEditor *editor)
{
    auto textEditor = qobject_cast<TextEditor::BaseTextEditor *>(editor);
    if (!textEditor)
        return;
    TextEditor::TextEditorWidget *widget = textEditor->editorWidget();
    if (!widget || Handler::forEditor(widget))
        return;

    auto handler = new Handler(widget);
    applySettings(handler);
    QObject::connect(&settings(), &AspectContainer::changed, handler, [handler] {
        applySettings(handler);
    });
}

class ElasticTabstopsPlugin final : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "ElasticTabstops.json")

public:
    void initialize() final
    {
        static SettingsPage settingsPage;

        ActionBuilder(this, TOGGLE_ACTION_ID)
            .setText("Elastic Tabstops")
            .setCheckable(true)
            .setChecked(settings().enabled())
            .addToContainer(Constants::M_EDIT_ADVANCED, Constants::G_EDIT_FORMAT)
            .bindContextAction(&m_toggleAction)
            .addOnToggled(this, [](bool on) {
                if (settings().enabled() == on)
                    return;
                settings().enabled.setValue(on);
                settings().writeSettings();
            });
        settings().enabled.addOnChanged(this, [this] {
            m_toggleAction->setChecked(settings().enabled());
        });

        connect(EditorManager::instance(), &EditorManager::editorOpened, this, &attachToEditor);
    }

    void extensionsInitialized() final
    {
        for (IEditor *editor : EditorManager::visibleEditors())
            attachToEditor(editor);
    }

private:
    QAction *m_toggleAction = nullptr;
};

} // namespace ElasticTabstops::Internal

#include "elastictabstopsplugin.moc"
