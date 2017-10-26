/* This file is part of KDevelop
Copyright 2009 Andreas Pakulat <apaku@gmx.de>
Copyright (C) 2008 Cédric Pasteur <cedric.pasteur@free.fr>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public License
along with this library; see the file COPYING.LIB.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.
*/

#include "sourceformattercontroller.h"

#include <QAction>
#include <QAbstractButton>
#include <QMimeDatabase>
#include <QRegExp>
#include <QStringList>
#include <QUrl>

#include <KActionCollection>
#include <KIO/StoredTransferJob>
#include <KLocalizedString>
#include <KMessageBox>
#include <KTextEditor/Command>
#include <KTextEditor/ConfigInterface>
#include <KTextEditor/Document>
#include <KTextEditor/Editor>
#include <KTextEditor/View>
#include <KTextEditor/MainWindow>
#include <KParts/MainWindow>

#include <interfaces/context.h>
#include <interfaces/contextmenuextension.h>
#include <interfaces/icore.h>
#include <interfaces/idocument.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/iplugincontroller.h>
#include <interfaces/iproject.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/isession.h>
#include <interfaces/isourceformatter.h>
#include <interfaces/iuicontroller.h>
#include <language/codegen/coderepresentation.h>
#include <language/interfaces/ilanguagesupport.h>
#include <project/projectmodel.h>
#include <util/path.h>

#include "core.h"
#include "debug.h"
#include "plugincontroller.h"
#include "sourceformatterjob.h"

namespace {

namespace Strings {
QString SourceFormatter() { return QStringLiteral("SourceFormatter"); }
QString UseDefault() { return QStringLiteral("UseDefault"); }
}

}

namespace KDevelop
{

class SourceFormatterControllerPrivate
{
public:
    // GUI actions
    QAction* formatTextAction;
    QAction* formatFilesAction;
    QAction* formatLine;
    QList<KDevelop::ProjectBaseItem*> prjItems;
    QList<QUrl> urls;
    bool enabled = true;
};

QString SourceFormatterController::kateModeLineConfigKey()
{
    return QStringLiteral("ModelinesEnabled");
}

QString SourceFormatterController::kateOverrideIndentationConfigKey()
{
    return QStringLiteral("OverrideKateIndentation");
}

QString SourceFormatterController::styleCaptionKey()
{
    return QStringLiteral("Caption");
}

QString SourceFormatterController::styleContentKey()
{
    return QStringLiteral("Content");
}

QString SourceFormatterController::styleMimeTypesKey()
{
    return QStringLiteral("MimeTypes");
}

QString SourceFormatterController::styleSampleKey()
{
    return QStringLiteral("StyleSample");
}

SourceFormatterController::SourceFormatterController(QObject *parent)
    : ISourceFormatterController(parent)
    , d(new SourceFormatterControllerPrivate)
{
    setObjectName(QStringLiteral("SourceFormatterController"));
    setComponentName(QStringLiteral("kdevsourceformatter"), i18n("Source Formatter"));
    setXMLFile(QStringLiteral("kdevsourceformatter.rc"));

    if (Core::self()->setupFlags() & Core::NoUi) return;

    d->formatTextAction = actionCollection()->addAction(QStringLiteral("edit_reformat_source"));
    d->formatTextAction->setText(i18n("&Reformat Source"));
    d->formatTextAction->setToolTip(i18n("Reformat source using AStyle"));
    d->formatTextAction->setWhatsThis(i18n("Source reformatting functionality using <b>astyle</b> library."));
    connect(d->formatTextAction, &QAction::triggered, this, &SourceFormatterController::beautifySource);

    d->formatLine = actionCollection()->addAction(QStringLiteral("edit_reformat_line"));
    d->formatLine->setText(i18n("Reformat Line"));
    d->formatLine->setToolTip(i18n("Reformat current line using AStyle"));
    d->formatLine->setWhatsThis(i18n("Source reformatting of line under cursor using <b>astyle</b> library."));
    connect(d->formatLine, &QAction::triggered, this, &SourceFormatterController::beautifyLine);

    d->formatFilesAction = actionCollection()->addAction(QStringLiteral("tools_astyle"));
    d->formatFilesAction->setText(i18n("Reformat Files..."));
    d->formatFilesAction->setToolTip(i18n("Format file(s) using the current theme"));
    d->formatFilesAction->setWhatsThis(i18n("Formatting functionality using <b>astyle</b> library."));
    connect(d->formatFilesAction, &QAction::triggered, this, static_cast<void(SourceFormatterController::*)()>(&SourceFormatterController::formatFiles));

    // connect to both documentActivated & documentClosed,
    // otherwise we miss when the last document was closed
    connect(Core::self()->documentController(), &IDocumentController::documentActivated,
            this, &SourceFormatterController::updateFormatTextAction);
    connect(Core::self()->documentController(), &IDocumentController::documentClosed,
            this, &SourceFormatterController::updateFormatTextAction);
    // Use a queued connection, because otherwise the view is not yet fully set up
    connect(Core::self()->documentController(), &IDocumentController::documentLoaded,
            this, &SourceFormatterController::documentLoaded, Qt::QueuedConnection);

    updateFormatTextAction();
}

void SourceFormatterController::documentLoaded( IDocument* doc )
{
    // NOTE: explicitly check this here to prevent crashes on shutdown
    //       when this slot gets called (note: delayed connection)
    //       but the text document was already destroyed
    //       there have been unit tests that failed due to that...
    if (!doc->textDocument()) {
        return;
    }
    const auto url = doc->url();
    const auto mime = QMimeDatabase().mimeTypeForUrl(url);
    adaptEditorIndentationMode(doc->textDocument(), formatterForUrl(url, mime), url);
}

void SourceFormatterController::initialize()
{
}

SourceFormatterController::~SourceFormatterController()
{
}

ISourceFormatter* SourceFormatterController::formatterForUrl(const QUrl &url)
{
    QMimeType mime = QMimeDatabase().mimeTypeForUrl(url);
    return formatterForUrl(url, mime);
}

KConfigGroup SourceFormatterController::configForUrl(const QUrl& url) const
{
    auto core = KDevelop::Core::self();
    auto project = core->projectController()->findProjectForUrl(url);
    if (project) {
        auto config = project->projectConfiguration()->group(Strings::SourceFormatter());
        if (config.isValid() && !config.readEntry(Strings::UseDefault(), true)) {
            return config;
        }
    }

    return core->activeSession()->config()->group( Strings::SourceFormatter() );
}

KConfigGroup SourceFormatterController::sessionConfig() const
{
    return KDevelop::Core::self()->activeSession()->config()->group( Strings::SourceFormatter() );
}

KConfigGroup SourceFormatterController::globalConfig() const
{
    return KSharedConfig::openConfig()->group( Strings::SourceFormatter() );
}

ISourceFormatter* SourceFormatterController::findFirstFormatterForMimeType(const QMimeType& mime ) const
{
    static QHash<QString, ISourceFormatter*> knownFormatters;
    if (knownFormatters.contains(mime.name()))
        return knownFormatters[mime.name()];

    QList<IPlugin*> plugins = Core::self()->pluginController()->allPluginsForExtension( QStringLiteral("org.kdevelop.ISourceFormatter") );
    foreach( IPlugin* p, plugins) {
        ISourceFormatter *iformatter = p->extension<ISourceFormatter>();
        QSharedPointer<SourceFormatter> formatter(createFormatterForPlugin(iformatter));
        if( formatter->supportedMimeTypes().contains(mime.name()) ) {
            knownFormatters[mime.name()] = iformatter;
            return iformatter;
        }
    }
    knownFormatters[mime.name()] = nullptr;
    return nullptr;
}

static void populateStyleFromConfigGroup(SourceFormatterStyle* s, const KConfigGroup& stylegrp)
{
    s->setCaption( stylegrp.readEntry( SourceFormatterController::styleCaptionKey(), QString() ) );
    s->setContent( stylegrp.readEntry( SourceFormatterController::styleContentKey(), QString() ) );
    s->setMimeTypes( stylegrp.readEntry<QStringList>( SourceFormatterController::styleMimeTypesKey(), QStringList() ) );
    s->setOverrideSample( stylegrp.readEntry( SourceFormatterController::styleSampleKey(), QString() ) );
}

SourceFormatter* SourceFormatterController::createFormatterForPlugin(ISourceFormatter *ifmt) const
{
    SourceFormatter* formatter = new SourceFormatter();
    formatter->formatter = ifmt;

    // Inserted a new formatter. Now fill it with styles
    foreach( const KDevelop::SourceFormatterStyle& style, ifmt->predefinedStyles() ) {
        formatter->styles[ style.name() ] = new SourceFormatterStyle(style);
    }
    KConfigGroup grp = globalConfig();
    if( grp.hasGroup( ifmt->name() ) ) {
        KConfigGroup fmtgrp = grp.group( ifmt->name() );
        foreach( const QString& subgroup, fmtgrp.groupList() ) {
            SourceFormatterStyle* s = new SourceFormatterStyle( subgroup );
            KConfigGroup stylegrp = fmtgrp.group( subgroup );
            populateStyleFromConfigGroup(s, stylegrp);
            formatter->styles[ s->name() ] = s;
        }
    }
    return formatter;
}

ISourceFormatter* SourceFormatterController::formatterForUrl(const QUrl& url, const QMimeType& mime)
{
    if (!d->enabled || !isMimeTypeSupported(mime)) {
        return nullptr;
    }

    const auto formatter = configForUrl(url).readEntry(mime.name(), QString());

    if( formatter.isEmpty() )
    {
        return findFirstFormatterForMimeType( mime );
    }

    QStringList formatterinfo = formatter.split( QStringLiteral("||"), QString::SkipEmptyParts );

    if( formatterinfo.size() != 2 ) {
        qCDebug(SHELL) << "Broken formatting entry for mime:" << mime.name() << "current value:" << formatter;
        return nullptr;
    }

    return Core::self()->pluginControllerInternal()->extensionForPlugin<ISourceFormatter>( QStringLiteral("org.kdevelop.ISourceFormatter"), formatterinfo.at(0) );
}

bool SourceFormatterController::isMimeTypeSupported(const QMimeType& mime)
{
    if( findFirstFormatterForMimeType( mime ) ) {
        return true;
    }
    return false;
}

QString SourceFormatterController::indentationMode(const QMimeType& mime)
{
    if (mime.inherits(QStringLiteral("text/x-c++src")) || mime.inherits(QStringLiteral("text/x-chdr")) ||
        mime.inherits(QStringLiteral("text/x-c++hdr")) || mime.inherits(QStringLiteral("text/x-csrc")) ||
        mime.inherits(QStringLiteral("text/x-java")) || mime.inherits(QStringLiteral("text/x-csharp"))) {
        return QStringLiteral("cstyle");
    }
    return QStringLiteral("none");
}

QString SourceFormatterController::addModelineForCurrentLang(QString input, const QUrl& url, const QMimeType& mime)
{
    if( !isMimeTypeSupported(mime) )
        return input;

    QRegExp kateModelineWithNewline(QStringLiteral("\\s*\\n//\\s*kate:(.*)$"));

    // If there already is a modeline in the document, adapt it while formatting, even
    // if "add modeline" is disabled.
    if (!configForUrl(url).readEntry(SourceFormatterController::kateModeLineConfigKey(), false) &&
            kateModelineWithNewline.indexIn( input ) == -1 )
        return input;

    ISourceFormatter* fmt = formatterForUrl(url, mime);
    ISourceFormatter::Indentation indentation = fmt->indentation(url);

    if( !indentation.isValid() )
        return input;

    QString output;
    QTextStream os(&output, QIODevice::WriteOnly);
    QTextStream is(&input, QIODevice::ReadOnly);

    Q_ASSERT(fmt);


    QString modeline(QStringLiteral("// kate: ")
                   + QStringLiteral("indent-mode ") + indentationMode(mime) + QStringLiteral("; "));

    if(indentation.indentWidth) // We know something about indentation-width
        modeline.append(QStringLiteral("indent-width %1; ").arg(indentation.indentWidth));

    if(indentation.indentationTabWidth != 0) // We know something about tab-usage
    {
        modeline.append(QStringLiteral("replace-tabs %1; ").arg(QLatin1String((indentation.indentationTabWidth == -1) ? "on" : "off")));
        if(indentation.indentationTabWidth > 0)
            modeline.append(QStringLiteral("tab-width %1; ").arg(indentation.indentationTabWidth));
    }

    qCDebug(SHELL) << "created modeline: " << modeline << endl;

    QRegExp kateModeline(QStringLiteral("^\\s*//\\s*kate:(.*)$"));

    bool modelinefound = false;
    QRegExp knownOptions(QStringLiteral("\\s*(indent-width|space-indent|tab-width|indent-mode|replace-tabs)"));
    while (!is.atEnd()) {
        QString line = is.readLine();
        // replace only the options we care about
        if (kateModeline.indexIn(line) >= 0) { // match
            qCDebug(SHELL) << "Found a kate modeline: " << line << endl;
            modelinefound = true;
            QString options = kateModeline.cap(1);
            QStringList optionList = options.split(QLatin1Char(';'), QString::SkipEmptyParts);

            os <<  modeline;
            foreach(QString s, optionList) {
                if (knownOptions.indexIn(s) < 0) { // unknown option, add it
                    if(s.startsWith(QLatin1Char(' ')))
                        s=s.mid(1);
                    os << s << ";";
                    qCDebug(SHELL) << "Found unknown option: " << s << endl;
                }
            }
            os << endl;
        } else
            os << line << endl;
    }

    if (!modelinefound)
        os << modeline << endl;
    return output;
}

void SourceFormatterController::cleanup()
{
}

void SourceFormatterController::updateFormatTextAction()
{
    bool enabled = false;

    IDocument* doc = KDevelop::ICore::self()->documentController()->activeDocument();
    if (doc) {
        QMimeType mime = QMimeDatabase().mimeTypeForUrl(doc->url());
        if (isMimeTypeSupported(mime))
            enabled = true;
    }

    d->formatLine->setEnabled(enabled);
    d->formatTextAction->setEnabled(enabled);
}

void SourceFormatterController::beautifySource()
{
    IDocument* idoc = KDevelop::ICore::self()->documentController()->activeDocument();
    if (!idoc)
        return;
    KTextEditor::View* view = idoc->activeTextView();
    if (!view)
        return;
    KTextEditor::Document* doc = view->document();
    // load the appropriate formatter
    const auto url = idoc->url();
    const auto mime = QMimeDatabase().mimeTypeForUrl(url);
    ISourceFormatter* formatter = formatterForUrl(url, mime);
        if( !formatter ) {
            qCDebug(SHELL) << "no formatter available for" << mime.name();
            return;
        }

    // Ignore the modeline, as the modeline will be changed anyway
    adaptEditorIndentationMode(doc, formatter, url, true);

    bool has_selection = view->selection();

    if (has_selection) {
        QString original = view->selectionText();

        QString output = formatter->formatSource(view->selectionText(), url, mime,
                                                doc->text(KTextEditor::Range(KTextEditor::Cursor(0,0),view->selectionRange().start())),
                                                doc->text(KTextEditor::Range(view->selectionRange().end(), doc->documentRange().end())));

        //remove the final newline character, unless it should be there
        if (!original.endsWith(QLatin1Char('\n'))  && output.endsWith(QLatin1Char('\n')))
            output.resize(output.length() - 1);
        //there was a selection, so only change the part of the text related to it

        // We don't use KTextEditor::Document directly, because CodeRepresentation transparently works
        // around a possible tab-replacement incompatibility between kate and kdevelop
        DynamicCodeRepresentation::Ptr code( dynamic_cast<DynamicCodeRepresentation*>( KDevelop::createCodeRepresentation( IndexedString( doc->url() ) ).data() ) );
        Q_ASSERT( code );
        code->replace( view->selectionRange(), original, output );
    } else {
        formatDocument(idoc, formatter, mime);
    }
}

void SourceFormatterController::beautifyLine()
{
    KDevelop::IDocumentController *docController = KDevelop::ICore::self()->documentController();
    KDevelop::IDocument *doc = docController->activeDocument();
    if (!doc || !doc->isTextDocument())
        return;
    KTextEditor::Document *tDoc = doc->textDocument();
    KTextEditor::View* view = doc->activeTextView();
    if (!view)
        return;
    // load the appropriate formatter
    const auto url = doc->url();
    const auto mime = QMimeDatabase().mimeTypeForUrl(url);
    ISourceFormatter* formatter = formatterForUrl(url, mime);
    if( !formatter ) {
        qCDebug(SHELL) << "no formatter available for" << mime.name();
        return;
    }

    const KTextEditor::Cursor cursor = view->cursorPosition();
    const QString line = tDoc->line(cursor.line());
    const QString prev = tDoc->text(KTextEditor::Range(0, 0, cursor.line(), 0));
    const QString post = QLatin1Char('\n') + tDoc->text(KTextEditor::Range(KTextEditor::Cursor(cursor.line() + 1, 0), tDoc->documentEnd()));

    const QString formatted = formatter->formatSource(line, doc->url(), mime, prev, post);

    // We don't use KTextEditor::Document directly, because CodeRepresentation transparently works
    // around a possible tab-replacement incompatibility between kate and kdevelop
    DynamicCodeRepresentation::Ptr code(dynamic_cast<DynamicCodeRepresentation*>( KDevelop::createCodeRepresentation( IndexedString( doc->url() ) ).data() ) );
    Q_ASSERT( code );
    code->replace( KTextEditor::Range(cursor.line(), 0, cursor.line(), line.length()), line, formatted );

    // advance cursor one line
    view->setCursorPosition(KTextEditor::Cursor(cursor.line() + 1, 0));
}

void SourceFormatterController::formatDocument(KDevelop::IDocument* doc, ISourceFormatter* formatter, const QMimeType& mime)
{
    Q_ASSERT(doc);
    Q_ASSERT(formatter);

    qCDebug(SHELL) << "Running" << formatter->name() << "on" << doc->url();

    // We don't use KTextEditor::Document directly, because CodeRepresentation transparently works
    // around a possible tab-replacement incompatibility between kate and kdevelop
    CodeRepresentation::Ptr code = KDevelop::createCodeRepresentation( IndexedString( doc->url() ) );

    KTextEditor::Cursor cursor = doc->cursorPosition();
    QString text = formatter->formatSource(code->text(), doc->url(), mime);
    text = addModelineForCurrentLang(text, doc->url(), mime);
    code->setText(text);

    doc->setCursorPosition(cursor);
}

void SourceFormatterController::settingsChanged()
{
    foreach (KDevelop::IDocument* doc, ICore::self()->documentController()->openDocuments()) {
        adaptEditorIndentationMode(doc->textDocument(), formatterForUrl(doc->url()), doc->url());
    }
}

/**
* Kate commands:
* Use spaces for indentation:
*   "set-replace-tabs 1"
* Use tabs for indentation (eventually mixed):
*   "set-replace-tabs 0"
* Indent width:
* 	 "set-indent-width X"
* Tab width:
*   "set-tab-width X"
* */

void SourceFormatterController::adaptEditorIndentationMode(KTextEditor::Document *doc, ISourceFormatter *formatter,
                                                           const QUrl& url, bool ignoreModeline)
{
    if (!formatter || !configForUrl(url).readEntry(SourceFormatterController::kateOverrideIndentationConfigKey(), false) || !doc)
        return;

    qCDebug(SHELL) << "adapting mode for" << url;

    QRegExp kateModelineWithNewline(QStringLiteral("\\s*\\n//\\s*kate:(.*)$"));

    // modelines should always take precedence
    if( !ignoreModeline && kateModelineWithNewline.indexIn( doc->text() ) != -1 )
    {
        qCDebug(SHELL) << "ignoring because a kate modeline was found";
        return;
    }

    ISourceFormatter::Indentation indentation = formatter->indentation(url);
    if(indentation.isValid())
    {
        struct CommandCaller {
            explicit CommandCaller(KTextEditor::Document* _doc) : doc(_doc), editor(KTextEditor::Editor::instance()) {
                Q_ASSERT(editor);
            }
            void operator()(const QString& cmd) {
                KTextEditor::Command* command = editor->queryCommand( cmd );
                Q_ASSERT(command);
                QString msg;
                qCDebug(SHELL) << "calling" << cmd;
                foreach(KTextEditor::View* view, doc->views())
                    if (!command->exec(view, cmd, msg))
                        qCWarning(SHELL) << "setting indentation width failed: " << msg;
            }

            KTextEditor::Document* doc;
            KTextEditor::Editor* editor;
        } call(doc);

        if( indentation.indentWidth ) // We know something about indentation-width
            call( QStringLiteral("set-indent-width %1").arg(indentation.indentWidth ) );

        if( indentation.indentationTabWidth != 0 ) // We know something about tab-usage
        {
            call( QStringLiteral("set-replace-tabs %1").arg( (indentation.indentationTabWidth == -1) ? 1 : 0 ) );
            if( indentation.indentationTabWidth > 0 )
                call( QStringLiteral("set-tab-width %1").arg(indentation.indentationTabWidth ) );
        }
    }else{
        qCDebug(SHELL) << "found no valid indentation";
    }
}

void SourceFormatterController::formatFiles()
{
    if (d->prjItems.isEmpty() && d->urls.isEmpty())
        return;

    //get a list of all files in this folder recursively
    QList<KDevelop::ProjectFolderItem*> folders;
    foreach (KDevelop::ProjectBaseItem *item, d->prjItems) {
        if (!item)
            continue;
        if (item->folder())
            folders.append(item->folder());
        else if (item->file())
            d->urls.append(item->file()->path().toUrl());
        else if (item->target()) {
            foreach(KDevelop::ProjectFileItem *f, item->fileList())
            d->urls.append(f->path().toUrl());
        }
    }

    while (!folders.isEmpty()) {
        KDevelop::ProjectFolderItem *item = folders.takeFirst();
        foreach(KDevelop::ProjectFolderItem *f, item->folderList())
        folders.append(f);
        foreach(KDevelop::ProjectTargetItem *f, item->targetList()) {
            foreach(KDevelop::ProjectFileItem *child, f->fileList())
            d->urls.append(child->path().toUrl());
        }
        foreach(KDevelop::ProjectFileItem *f, item->fileList())
        d->urls.append(f->path().toUrl());
    }

    auto win = ICore::self()->uiController()->activeMainWindow()->window();

    QMessageBox msgBox(QMessageBox::Question, i18n("Reformat files?"),
                       i18n("Reformat all files in the selected folder?"),
                       QMessageBox::Ok|QMessageBox::Cancel, win);
    msgBox.setDefaultButton(QMessageBox::Cancel);
    auto okButton = msgBox.button(QMessageBox::Ok);
    okButton->setText(i18n("Reformat"));
    msgBox.exec();

    if (msgBox.clickedButton() == okButton) {
        auto formatterJob = new SourceFormatterJob(this);
        formatterJob->setFiles(d->urls);
        ICore::self()->runController()->registerJob(formatterJob);
    }
}

KDevelop::ContextMenuExtension SourceFormatterController::contextMenuExtension(KDevelop::Context* context, QWidget* parent)
{
    Q_UNUSED(parent);

    KDevelop::ContextMenuExtension ext;
    d->urls.clear();
    d->prjItems.clear();

    if (context->hasType(KDevelop::Context::EditorContext))
    {
        if (d->formatTextAction->isEnabled())
            ext.addAction(KDevelop::ContextMenuExtension::EditGroup, d->formatTextAction);
    } else if (context->hasType(KDevelop::Context::FileContext)) {
        KDevelop::FileContext* filectx = static_cast<KDevelop::FileContext*>(context);
        d->urls = filectx->urls();
        ext.addAction(KDevelop::ContextMenuExtension::EditGroup, d->formatFilesAction);
    } else if (context->hasType(KDevelop::Context::CodeContext)) {
    } else if (context->hasType(KDevelop::Context::ProjectItemContext)) {
        KDevelop::ProjectItemContext* prjctx = static_cast<KDevelop::ProjectItemContext*>(context);
        d->prjItems = prjctx->items();
        if (!d->prjItems.isEmpty()) {
            ext.addAction(KDevelop::ContextMenuExtension::ExtensionGroup, d->formatFilesAction);
        }
    }
    return ext;
}

SourceFormatterStyle SourceFormatterController::styleForUrl(const QUrl& url, const QMimeType& mime)
{
    const auto formatter = configForUrl(url).readEntry(mime.name(), QString()).split(QStringLiteral("||"), QString::SkipEmptyParts);
    if( formatter.count() == 2 )
    {
        SourceFormatterStyle s( formatter.at( 1 ) );
        KConfigGroup fmtgrp = globalConfig().group( formatter.at(0) );
        if( fmtgrp.hasGroup( formatter.at(1) ) ) {
            KConfigGroup stylegrp = fmtgrp.group( formatter.at(1) );
            populateStyleFromConfigGroup(&s, stylegrp);
        }
        return s;
    }
    return SourceFormatterStyle();
}

void SourceFormatterController::disableSourceFormatting(bool disable)
{
    d->enabled = !disable;
}

bool SourceFormatterController::sourceFormattingEnabled()
{
    return d->enabled;
}

}
