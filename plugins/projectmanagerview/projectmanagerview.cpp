/* This file is part of KDevelop
    Copyright 2005 Roberto Raggi <roberto@kdevelop.org>
    Copyright 2007 Andreas Pakulat <apaku@gmx.de>
    Copyright 2008 Aleix Pol <aleixpol@gmail.com>

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

#include "projectmanagerview.h"

#include <QtCore/QDebug>
#include <QtGui/QHeaderView>
#include <QtGui/QVBoxLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QStandardItem>
#include <QtGui/QStackedWidget>
#include <QtGui/QToolButton>

#include <kxmlguiwindow.h>
#include <kiconloader.h>
#include <kmenu.h>
#include <kdebug.h>
#include <kurl.h>
#include <klocale.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <kfadewidgeteffect.h>
#include <kcombobox.h>

#include <interfaces/icore.h>
#include <interfaces/iprojectcontroller.h>
#include <interfaces/iuicontroller.h>
#include <interfaces/idocumentcontroller.h>
#include <interfaces/iproject.h>
#include <project/interfaces/ibuildsystemmanager.h>
#include <project/interfaces/iprojectbuilder.h>
#include <project/projectmodel.h>

#include "tests/common/modeltest.h"
#include "projectproxymodel.h"
#include "projectbuildsetwidget.h"
#include "projectmanagerviewplugin.h"
#include "projecttreeview.h"

using namespace KDevelop;

class ProjectManagerPrivate
{
public:
    ProjectManagerViewPlugin *mplugin;
    ProjectTreeView *m_projectOverview;
    ProjectBuildSetWidget* m_buildView;
    KComboBox* m_detailSwitcher;
//     KLineEdit* m_filters;
    QStackedWidget* m_detailStack;
    QStringList m_cachedFileList;
    QToolButton* hideDetailsButton;
    ProjectProxyModel* m_modelFilter;

    void fileDirty( const QString &fileName )
    {
        Q_UNUSED(fileName)
    }
    void fileCreated( const QString &fileName )
    {
        Q_UNUSED(fileName)
    }
    void fileDeleted( const QString &fileName )
    {
        Q_UNUSED(fileName)
    }

    void openUrl( const KUrl& url )
    {
        mplugin->core()->documentController()->openDocument( url );
    }

    /*void filtersChanged()
    {
        m_modelFilter->setFilterWildcard(m_filters->text());
    }*/
};

ProjectManagerView::ProjectManagerView( ProjectManagerViewPlugin *plugin, QWidget *parent )
        : QWidget( parent ),
        d(new ProjectManagerPrivate)
{
    setWindowTitle(i18n("Projects"));

    m_syncAction = new KAction(this);
    m_syncAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    m_syncAction->setText(i18n("Locate Current Document"));
    m_syncAction->setToolTip(i18n("Locates the current document in the project tree and selects it."));
    m_syncAction->setIcon(KIcon("dirsync"));
    connect(m_syncAction, SIGNAL(triggered(bool)), this, SLOT(locateCurrentDocument()));
    addAction(m_syncAction);
    updateSyncAction();

    d->mplugin = plugin;
    QVBoxLayout *vbox = new QVBoxLayout( this );
    vbox->setMargin( 0 );

    d->m_projectOverview = new ProjectTreeView( plugin, this );
    d->m_projectOverview->setWhatsThis( i18n( "Project Overview" ) );
    vbox->addWidget( d->m_projectOverview );
    connect(d->m_projectOverview, SIGNAL(activateUrl(const KUrl&)), this, SLOT(openUrl(const KUrl&)));

//     d->m_filters = new KLineEdit(this);
//     d->m_filters->setClearButtonShown(true);
//     connect(d->m_filters, SIGNAL(returnPressed()), this, SLOT(filtersChanged()));
//     vbox->addWidget( d->m_filters );

    QHBoxLayout* hbox = new QHBoxLayout();
    vbox->addLayout( hbox );

    d->m_detailSwitcher = new KComboBox( false, this );
    d->m_detailSwitcher->insertItem( 0, i18n( "Buildset" ) );
    hbox->addWidget( d->m_detailSwitcher );

    d->hideDetailsButton = new QToolButton( this );
    d->hideDetailsButton->setIcon( KIcon( "arrow-down-double.png" ) );
    connect( d->hideDetailsButton, SIGNAL( clicked() ),
             this, SLOT( switchDetailView() ) );
    hbox->addWidget( d->hideDetailsButton );

    d->m_detailStack = new QStackedWidget( this );
    vbox->addWidget( d->m_detailStack );

    connect( d->m_detailSwitcher, SIGNAL( activated( int ) ),
             d->m_detailStack, SLOT( setCurrentIndex( int ) ) );

    d->m_buildView = new ProjectBuildSetWidget( this, d->m_detailStack );
//     d->m_buildView->setItemDelegate( delegate );
    d->m_buildView->setWhatsThis( i18n( "Build Items:" ) );
    d->m_detailStack->insertWidget( 0, d->m_buildView );

    QStandardItemModel *overviewModel = d->mplugin->core()->projectController()->projectModel();
    d->m_modelFilter = new ProjectProxyModel( this );
    d->m_modelFilter->setSourceModel(overviewModel);
    d->m_modelFilter->setDynamicSortFilter(true);
    ModelTest* test = new ModelTest( overviewModel );
    d->m_projectOverview->setModel( d->m_modelFilter );
    d->m_projectOverview->setSortingEnabled(true);
    d->m_projectOverview->sortByColumn( 0, Qt::AscendingOrder );
//     d->m_projectOverview->setModel( overviewModel );c
    setWindowIcon( SmallIcon( "kdevelop" ) ); //FIXME
    setWindowTitle( i18n( "Project Manager" ) );
    setWhatsThis( i18n( "Project Manager" ) );
    connect( KDevelop::ICore::self()->documentController(), SIGNAL(documentClosed(KDevelop::IDocument*) ),
             SLOT(updateSyncAction()));
    connect( KDevelop::ICore::self()->documentController(), SIGNAL(documentOpened(KDevelop::IDocument*) ),
             SLOT(updateSyncAction()));
}

void ProjectManagerView::updateSyncAction()
{
    m_syncAction->setEnabled( !KDevelop::ICore::self()->documentController()->openDocuments().isEmpty() );
}

ProjectManagerView::~ProjectManagerView()
{
    delete d;
}

ProjectManagerViewPlugin *ProjectManagerView::plugin() const
{
    return d->mplugin;
}

QList<KDevelop::ProjectBaseItem*> ProjectManagerView::selectedItems() const
{
    QList<KDevelop::ProjectBaseItem*> items;
    foreach( const QModelIndex &idx, d->m_projectOverview->selectionModel()->selectedIndexes() )
    {
        KDevelop::ProjectBaseItem* item =
                d->mplugin->core()->projectController()->projectModel()->item( d->m_modelFilter->mapToSource(idx) );
        if( item )
            items << item;
        else
            kDebug(9511) << "adding an unknown item";
    }
    return items;
}

void ProjectManagerView::switchDetailView()
{
    KFadeWidgetEffect* animation = new KFadeWidgetEffect( this );
    if( d->m_detailStack->isHidden() )
    {
        d->hideDetailsButton->setIcon( KIcon( "arrow-down-double" ) );
        d->m_detailStack->show();
    }
    else
    {
        d->hideDetailsButton->setIcon( KIcon( "arrow-up-double" ) );
        d->m_detailStack->hide();
    }
    animation->start();
}

void ProjectManagerView::locateCurrentDocument()
{
    KDevelop::IDocument *doc = ICore::self()->documentController()->activeDocument();

    foreach (IProject* proj, ICore::self()->projectController()->projects()) {
        foreach (KDevelop::ProjectFileItem* item, proj->filesForUrl(doc->url())) {
            QModelIndex index = d->m_modelFilter->indexFromItem(item);
            if (index.isValid()) {
                d->m_projectOverview->setCurrentIndex(index);
                d->m_projectOverview->expand(index);
                d->m_projectOverview->scrollTo(index);
                return;
            }
        }
    }
}

#include "projectmanagerview.moc"

