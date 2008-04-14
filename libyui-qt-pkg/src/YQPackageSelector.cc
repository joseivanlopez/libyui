/*---------------------------------------------------------------------\
  |								       |
  |		       __   __	  ____ _____ ____		       |
  |		       \ \ / /_ _/ ___|_   _|___ \		       |
  |			\ V / _` \___ \ | |   __) |		       |
  |			 | | (_| |___) || |  / __/		       |
  |			 |_|\__,_|____/ |_| |_____|		       |
  |								       |
  |				core system			       |
  |							 (C) SuSE GmbH |
  \----------------------------------------------------------------------/

  File:	      YQPackageSelector.cc
  See also:   YQPackageSelectorHelp.cc

  Author:     Stefan Hundhammer <sh@suse.de>

  Textdomain "qt-pkg"

  /-*/

#define CHECK_DEPENDENCIES_ON_STARTUP			1
#define DEPENDENCY_FEEDBACK_IF_OK			1
#define AUTO_CHECK_DEPENDENCIES_DEFAULT			true
#define ALWAYS_SHOW_PATCHES_VIEW_IF_PATCHES_AVAILABLE	0
#define GLOBAL_UPDATE_CONFIRMATION_THRESHOLD		20
#define  ENABLE_SOURCE_RPMS				0

#include <fstream>
#include <boost/bind.hpp>

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QAction>
#include <QShortcut>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QFileDialog>
#include <QLabel>
#include <QMap>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTimer>
#include <QMenu>

#define YUILogComponent "qt-pkg"
#include "YUILog.h"

#include "QY2LayoutUtils.h"

#include "YQPackageSelector.h"
#include "YQPkgChangeLogView.h"
#include "YQPkgChangesDialog.h"
#include "YQPkgConflictDialog.h"
#include "YQPkgConflictList.h"
#include "YQPkgDependenciesView.h"
#include "YQPkgDescriptionView.h"
#include "YQPkgDiskUsageList.h"
#include "YQPkgDiskUsageWarningDialog.h"
#include "YQPkgFileListView.h"
#include "YQPkgRepoFilterView.h"
#include "YQPkgRepoList.h"
#include "YQPkgLangList.h"
#include "YQPkgList.h"
#include "YQPkgPatchFilterView.h"
#include "YQPkgPatchList.h"
#include "YQPkgPatternList.h"
#include "YQPkgProductDialog.h"
#include "YQPkgRpmGroupTagsFilterView.h"
#include "YQPkgSearchFilterView.h"
#include "YQPkgStatusFilterView.h"
#include "YQPkgTechnicalDetailsView.h"
#include "YQPkgTextDialog.h"
#include "YQPkgUpdateProblemFilterView.h"
#include "YQPkgVersionsView.h"
#include "zypp/SysContent.h"

#include "QY2ComboTabWidget.h"
#include "YQDialog.h"
#include "YQApplication.h"
#include "utf8.h"
#include "YQUI.h"
#include "YEvent.h"
#include "YQi18n.h"


using std::max;
using std::string;
using std::map;
using std::pair;

#define DEFAULT_EXPORT_FILE_NAME	"user-packages.xml"
#define FAST_SOLVER			1



YQPackageSelector::YQPackageSelector( YWidget *		parent,
				      long		modeFlags )
    : YQPackageSelectorBase( parent, modeFlags )
{
    _showChangesDialog		= true;
    _autoDependenciesCheckBox	= 0;
    _detailsViews		= 0;
    _diskUsageList		= 0;
    _filters			= 0;
    _repoFilterView		= 0;
    _langList			= 0;
    _patternList		= 0;
    _pkgChangeLogView		= 0;
    _pkgDependenciesView	= 0;
    _pkgDescriptionView		= 0;
    _pkgFileListView		= 0;
    _pkgList			= 0;
    _pkgTechnicalDetailsView	= 0;
    _pkgVersionsView		= 0;
    _rpmGroupTagsFilterView	= 0;
    _searchFilterView		= 0;
    _statusFilterView		= 0;
    _updateProblemFilterView	= 0;
    _patchFilterView		= 0;
    _patchList			= 0;
    _excludeDevelPkgs		= 0;
    _excludeDebugInfoPkgs	= 0;


    if ( onlineUpdateMode() )	yuiMilestone() << "Online update mode" << endl;
    if ( updateMode() )		yuiMilestone() << "Update mode" << endl;


    basicLayout();
    addMenus();		// Only after all widgets are created!
    makeConnections();
    emit loadData();

    if ( _pkgList )
	_pkgList->clear();

    if ( _patchFilterView && onlineUpdateMode() )
    {
	if ( _filters && _patchFilterView && _patchList )
	{
	    _filters->showPage( _patchFilterView );
	    _patchList->filter();
	}
    }
    else if ( _repoFilterView && repoMode() )
    {
	if ( YQPkgRepoList::countEnabledRepositories() > 1 )
	{
	    _filters->showPage( _repoFilterView );
	    _repoFilterView->filter();
	}
	else if ( _searchFilterView )
	{
	    yuiMilestone() << "No multiple repositories - falling back to search mode" << endl;
	    _filters->showPage( _searchFilterView );
	    _searchFilterView->filter();
	    QTimer::singleShot( 0, _searchFilterView, SLOT( setFocus() ) );
	}
    }
    else if ( _updateProblemFilterView )
    {
	_filters->showPage( _updateProblemFilterView );
	_updateProblemFilterView->filter();

    }
    else if ( searchMode() && _searchFilterView )
    {
	_filters->showPage( _searchFilterView );
	_searchFilterView->filter();
	QTimer::singleShot( 0, _searchFilterView, SLOT( setFocus() ) );
    }
    else if ( summaryMode() && _statusFilterView )
    {
	_filters->showPage( _statusFilterView );
	_statusFilterView->filter();
    }
    else if ( _patternList )
    {
	_filters->showPage( _patternList );
	_patternList->filter();
    }


    if ( _diskUsageList )
	_diskUsageList->updateDiskUsage();

    yuiMilestone() << "PackageSelector init done" << endl;


#if CHECK_DEPENDENCIES_ON_STARTUP

    if ( ! testMode() )
    {
	// Fire up the first dependency check in the main loop.
	// Don't do this right away - wait until all initializations are finished.
	QTimer::singleShot( 0, this, SLOT( resolveDependencies() ) );

    }
#endif
}


void
YQPackageSelector::basicLayout()
{
    QVBoxLayout *layout = new QVBoxLayout();
    setLayout(layout);
    layoutMenuBar(this);

    QSplitter * outer_splitter = new QSplitter( Qt::Horizontal, this );
    Q_CHECK_PTR( outer_splitter );
    outer_splitter->setObjectName( "outer_splitter" );

    layout->addWidget(outer_splitter);

    QWidget * left_pane	 = layoutLeftPane ( outer_splitter );

    QWidget * right_pane = layoutRightPane( outer_splitter );

    outer_splitter->setStretchFactor(outer_splitter->indexOf(left_pane), 0);
    outer_splitter->setStretchFactor(outer_splitter->indexOf(right_pane), 1);
    layout->addStretch();
    
}


QWidget *
YQPackageSelector::layoutLeftPane( QWidget *parent )
{
    QSplitter * splitter = new QSplitter( Qt::Vertical, parent );
    Q_CHECK_PTR( splitter );
    splitter->setObjectName( "leftpanesplitter" );

    QWidget * upper_vbox = new QWidget( splitter );
    QVBoxLayout *layout = new QVBoxLayout(upper_vbox);
    upper_vbox->setLayout(layout);

    Q_CHECK_PTR( upper_vbox );
    layoutFilters( upper_vbox );

    _diskUsageList = new YQPkgDiskUsageList( splitter );

    splitter->setStretchFactor(splitter->indexOf(upper_vbox), 1);
    splitter->setStretchFactor(splitter->indexOf( _diskUsageList ), 2);

    return splitter;
}


void
YQPackageSelector::layoutFilters( QWidget *parent )
{
    _filters = new QY2ComboTabWidget( _( "Fi&lter:" ), parent );
    Q_CHECK_PTR( _filters );
    parent->layout()->addWidget(_filters);

    //
    // Update problem view
    //

    if ( updateMode() )
    {
	if ( YQPkgUpdateProblemFilterView::haveProblematicPackages()
	     || testMode() )
	{
	    _updateProblemFilterView = new YQPkgUpdateProblemFilterView( parent);
	    Q_CHECK_PTR( _updateProblemFilterView );
	    _filters->addPage( _( "Update Problems" ), _updateProblemFilterView );
	}
    }


    //
    // Patches view
    //

    if ( onlineUpdateMode()
#if ALWAYS_SHOW_PATCHES_VIEW_IF_PATCHES_AVAILABLE
	 || ! zyppPool().empty<zypp::Patch>()
#endif
	 )
	addPatchFilterView();


    //
    // Patterns view
    //

    if ( ! zyppPool().empty<zypp::Pattern>() || testMode() )
    {
	_patternList = new YQPkgPatternList( parent, true );
	Q_CHECK_PTR( _patternList );
	_filters->addPage( _( "Patterns" ), _patternList );

	connect( _patternList,		SIGNAL( statusChanged()			),
		 this,			SLOT  ( autoResolveDependencies()	) );

	connect( this,			SIGNAL( refresh()			),
		 _patternList,		SLOT  ( updateItemStates()		) );

	if ( _pkgConflictDialog )
	{
	    connect( _pkgConflictDialog, SIGNAL( updatePackages()		),
		     _patternList,	 SLOT  ( updateItemStates()		) );
	}
    }


    //
    // RPM group tags view
    //

    _rpmGroupTagsFilterView = new YQPkgRpmGroupTagsFilterView( parent );
    Q_CHECK_PTR( _rpmGroupTagsFilterView );
    _filters->addPage( _( "Package Groups" ), _rpmGroupTagsFilterView );

    connect( this,			SIGNAL( loadData() ),
	     _rpmGroupTagsFilterView,	SLOT  ( filter()   ) );


    //
    // Languages view
    //
    _langList = new YQPkgLangList( parent );
    Q_CHECK_PTR( _langList );

    _filters->addPage( _( "Languages" ), _langList );
    _langList->setSizePolicy( QSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored ) ); // hor/vert

    connect( _langList,		SIGNAL( statusChanged()			),
	     this,		SLOT  ( autoResolveDependencies()	) );

    connect( this,		SIGNAL( refresh()			),
	     _langList,		SLOT  ( updateItemStates()		) );


    //
    // Repository view
    //

    _repoFilterView = new YQPkgRepoFilterView( parent );
    Q_CHECK_PTR( _repoFilterView );
    _filters->addPage( _( "Repositories" ), _repoFilterView );


    //
    // Package search view
    //

    _searchFilterView = new YQPkgSearchFilterView( parent );
    Q_CHECK_PTR( _searchFilterView );
    _filters->addPage( _( "Search" ), _searchFilterView );


    //
    // Status change view
    //

    _statusFilterView = new YQPkgStatusFilterView( parent );
    Q_CHECK_PTR( _statusFilterView );
    _filters->addPage( _( "Installation Summary" ), _statusFilterView );


#if 0
    // DEBUG

    _filters->addPage( _( "Keywords"   ), new QLabel( "Keywords\nfilter\n\nfor future use", 0 ) );
    _filters->addPage( _( "MIME Types" ), new QLabel( "MIME Types\nfilter\n\nfor future use" , 0 ) );
#endif

}


QWidget *
YQPackageSelector::layoutRightPane( QWidget *parent )
{
    QWidget * right_pane_vbox = new QWidget( parent );

    QVBoxLayout *layout = new QVBoxLayout( right_pane_vbox );

    Q_CHECK_PTR( right_pane_vbox );

    QSplitter * splitter = new QSplitter( Qt::Vertical, right_pane_vbox );
    Q_CHECK_PTR( splitter );
    layout->addWidget(splitter);

    Q_CHECK_PTR( splitter );
    layoutPkgList( splitter );

    layoutDetailsViews( splitter );

    layoutButtons( right_pane_vbox );

    return right_pane_vbox;
}


void
YQPackageSelector::layoutPkgList( QWidget *parent )
{
    _pkgList= new YQPkgList( parent );
    Q_CHECK_PTR( _pkgList );

    connect( _pkgList,	SIGNAL( statusChanged()		  ),
	     this,	SLOT  ( autoResolveDependencies() ) );
}


void
YQPackageSelector::layoutDetailsViews( QWidget *parent )
{
    bool haveInstalledPkgs = YQPkgList::haveInstalledPkgs();


    _detailsViews = new QTabWidget( parent );
    Q_CHECK_PTR( _detailsViews );

    //
    // Description
    //

    _pkgDescriptionView = new YQPkgDescriptionView( _detailsViews );
    Q_CHECK_PTR( _pkgDescriptionView );

    _detailsViews->addTab( _pkgDescriptionView, _( "D&escription" ) );
    _detailsViews->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) ); // hor/vert

    connect( _pkgList,			SIGNAL( currentItemChanged    ( ZyppSel ) ),
	     _pkgDescriptionView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );

    //
    // Technical details
    //

    _pkgTechnicalDetailsView = new YQPkgTechnicalDetailsView( _detailsViews );
    Q_CHECK_PTR( _pkgTechnicalDetailsView );

    _detailsViews->addTab( _pkgTechnicalDetailsView, _( "&Technical Data" ) );

    connect( _pkgList,			SIGNAL( currentItemChanged    ( ZyppSel ) ),
	     _pkgTechnicalDetailsView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );


    //
    // Dependencies
    //

    _pkgDependenciesView = new YQPkgDependenciesView( _detailsViews );
    Q_CHECK_PTR( _pkgDependenciesView );

    _detailsViews->addTab( _pkgDependenciesView, _( "Dependencies" ) );
    _detailsViews->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) ); // hor/vert

    connect( _pkgList,			SIGNAL( currentItemChanged    ( ZyppSel ) ),
	     _pkgDependenciesView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );



    //
    // Versions
    //

    _pkgVersionsView = new YQPkgVersionsView( _detailsViews,
					      true );	// userCanSwitchVersions
    Q_CHECK_PTR( _pkgVersionsView );

    _detailsViews->addTab( _pkgVersionsView, _( "&Versions" ) );

    connect( _pkgList,		SIGNAL( currentItemChanged    ( ZyppSel ) ),
	     _pkgVersionsView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );


    //
    // File List
    //

    if ( haveInstalledPkgs )	// file list information is only available for installed pkgs
    {
	_pkgFileListView = new YQPkgFileListView( _detailsViews );
	Q_CHECK_PTR( _pkgFileListView );

	_detailsViews->addTab( _pkgFileListView, _( "File List" ) );
	_detailsViews->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) ); // hor/vert

	connect( _pkgList,		SIGNAL( currentItemChanged    ( ZyppSel ) ),
		 _pkgFileListView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );
    }


    //
    // Change Log
    //

    if ( haveInstalledPkgs )	// change log information is only available for installed pkgs
    {
	_pkgChangeLogView = new YQPkgChangeLogView( _detailsViews );
	Q_CHECK_PTR( _pkgChangeLogView );

	_detailsViews->addTab( _pkgChangeLogView, _( "Change Log" ) );
	_detailsViews->setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) ); // hor/vert

	connect( _pkgList,		SIGNAL( currentItemChanged    ( ZyppSel ) ),
		 _pkgChangeLogView,	SLOT  ( showDetailsIfVisible( ZyppSel ) ) );
    }
}


void
YQPackageSelector::layoutButtons( QWidget *parent )
{
    QWidget * button_box = new QWidget( parent );
    Q_CHECK_PTR( button_box );
    parent->layout()->addWidget( button_box );
    
    QHBoxLayout *layout = new QHBoxLayout(button_box);
    button_box->setLayout(layout);
    
    // Button: Dependency check
    // Translators: Please keep this short!
    _checkDependenciesButton = new QPushButton( _( "Chec&k" ), button_box );
    layout->addWidget(_checkDependenciesButton);

    Q_CHECK_PTR( _checkDependenciesButton );
    _checkDependenciesButton->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) ); // hor/vert
    _normalButtonBackground = _checkDependenciesButton->palette().color(QPalette::Background);;

    connect( _checkDependenciesButton,	SIGNAL( clicked() ),
	     this,			SLOT  ( manualResolvePackageDependencies() ) );


    // Checkbox: Automatically check dependencies for every package status change?
    // Translators: Please keep this short!
    _autoDependenciesCheckBox = new QCheckBox( _( "A&utocheck" ), button_box );
    Q_CHECK_PTR( _autoDependenciesCheckBox );
    layout->addWidget(_autoDependenciesCheckBox);

    _autoDependenciesCheckBox->setChecked( AUTO_CHECK_DEPENDENCIES_DEFAULT );

    layout->addStretch();

    QPushButton * cancel_button = new QPushButton( _( "&Cancel" ), button_box );
    Q_CHECK_PTR( cancel_button );
    layout->addWidget(cancel_button);

    cancel_button->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) ); // hor/vert

    connect( cancel_button, SIGNAL( clicked() ),
	     this,	    SLOT  ( reject()   ) );


    QPushButton * accept_button = new QPushButton( _( "&Accept" ), button_box );
    Q_CHECK_PTR( accept_button );
    layout->addWidget(accept_button);
    accept_button->setSizePolicy( QSizePolicy( QSizePolicy::Fixed, QSizePolicy::Fixed ) ); // hor/vert

    connect( accept_button, SIGNAL( clicked() ),
	     this,	    SLOT  ( accept()   ) );

    button_box->setFixedHeight( button_box->sizeHint().height() );
}


void
YQPackageSelector::layoutMenuBar( QWidget *parent )
{
    _menuBar = new QMenuBar( parent );
    Q_CHECK_PTR( _menuBar );

    parent->layout()->addWidget(_menuBar);

    _fileMenu		= 0;
    _viewMenu		= 0;
    _pkgMenu		= 0;
    _patchMenu		= 0;
    _extrasMenu		= 0;
    _repositoryMenu 	= 0;
    _helpMenu		= 0;

}


void
YQPackageSelector::addMenus()
{
    //
    // File menu
    //

    _fileMenu = new QMenu( _menuBar );
    Q_CHECK_PTR( _fileMenu );
    QAction *action = _menuBar->addMenu( _fileMenu );
    action->setText(( "&File" ));

    _fileMenu->addAction( _( "&Import..." ),	this, SLOT( pkgImport() ) );
    _fileMenu->addAction( _( "&Export..." ),	this, SLOT( pkgExport() ) );

    _fileMenu->addSeparator();

    _fileMenu->addAction( _( "E&xit -- Discard Changes" ), this, SLOT( reject() ) );
    _fileMenu->addAction( _( "&Quit -- Save Changes"	 ), this, SLOT( accept() ) );


    if ( _pkgList )
    {
	//
	// View menu
	//

	_viewMenu = new QMenu( _menuBar );
	Q_CHECK_PTR( _viewMenu );
	QAction *action = _menuBar->addMenu( _viewMenu );
	action->setText(_( "&View" ));

        // Translators: This is about packages ending in "-devel", so don't translate that "-devel"!
	_showDevelAction = _viewMenu->addAction( _( "Show -de&vel Packages" ),
						 this, SLOT( pkgExcludeDevelChanged( bool ) ), Qt::Key_F7 );
  _showDevelAction->setCheckable(true);
	_showDevelAction->setChecked(true);

	_excludeDevelPkgs = new YQPkgObjList::ExcludeRule( _pkgList, QRegExp( ".*-devel(-\\d+bit)?$" ), _pkgList->nameCol() );
	Q_CHECK_PTR( _excludeDevelPkgs );
	_excludeDevelPkgs->enable( false );

	// Translators: This is about packages ending in "-debuginfo", so don't translate that "-debuginfo"!
	_showDebugAction = _viewMenu->addAction( _( "Show -&debuginfo Packages" ),
						 this, SLOT( pkgExcludeDebugChanged( bool ) ), Qt::Key_F8 );
  _showDebugAction->setCheckable(true);
	_showDebugAction->setChecked(true);
	_excludeDebugInfoPkgs = new YQPkgObjList::ExcludeRule( _pkgList, QRegExp( ".*-debuginfo$" ), _pkgList->nameCol() );
	Q_CHECK_PTR( _excludeDebugInfoPkgs );
	_excludeDebugInfoPkgs->enable( false );


	//
	// Package menu
	//

	_pkgMenu = new QMenu( _menuBar );
	Q_CHECK_PTR( _pkgMenu );
	action = _menuBar->addMenu( _pkgMenu );
	action->setText(_( "&Package" ));

	_pkgMenu->addAction(_pkgList->actionSetCurrentInstall);
	_pkgMenu->addAction(_pkgList->actionSetCurrentDontInstall);
	_pkgMenu->addAction(_pkgList->actionSetCurrentKeepInstalled);
	_pkgMenu->addAction(_pkgList->actionSetCurrentDelete);
	_pkgMenu->addAction(_pkgList->actionSetCurrentUpdate);
	_pkgMenu->addAction(_pkgList->actionSetCurrentTaboo);

#if ENABLE_SOURCE_RPMS
	_pkgMenu->addSeparator();

	_pkgMenu->addAction(_pkgList->actionInstallSourceRpm);
	_pkgMenu->addAction(_pkgList->actionDontInstallSourceRpm);
#endif

	_pkgMenu->addSeparator();
	QMenu * submenu = _pkgList->addAllInListSubMenu( _pkgMenu );
	Q_CHECK_PTR( submenu );

#if ENABLE_SOURCE_RPMS
	submenu->addSeparator();

	_pkgMenu->addAction(_pkgList->actionInstallListSourceRpms);
	_pkgMenu->addAction(_pkgList->actionDontInstallListSourceRpms);
#endif

	//
	// Submenu for all packages
	//

	submenu = new QMenu( _pkgMenu );
	Q_CHECK_PTR( submenu );

	// Translators: Unlike the "all in this list" submenu, this submenu
	// refers to all packages globally, not only to those that are
	// currently visible in the packages list.
	action = _pkgMenu->addMenu( submenu );
	action->setText(_( "All Packages" ));

	submenu->addAction( _( "Update if newer version available" ),
			    this, SLOT( globalUpdatePkg() ) );

	submenu->addAction( _( "Update unconditionally" ),
			    this, SLOT( globalUpdatePkgForce() ) );
    }


    if ( _patchList )
    {
	//
	// Patch menu
	//

	_patchMenu = new QMenu( _menuBar );
	Q_CHECK_PTR( _patchMenu );
	action = _menuBar->addMenu( _patchMenu );
	action->setText(_( "&Patch" ));

	_patchMenu->addAction(_patchList->actionSetCurrentInstall);
	_patchMenu->addAction(_patchList->actionSetCurrentDontInstall);
	_patchMenu->addAction(_patchList->actionSetCurrentKeepInstalled);

#if ENABLE_DELETING_PATCHES
	_patchMenu->addAction(_patchList->actionSetCurrentDelete);
#endif
	_patchMenu->addAction(_patchList->actionSetCurrentUpdate);
	_patchMenu->addAction(_patchList->actionSetCurrentTaboo);

	_patchMenu->addSeparator();
	_patchList->addAllInListSubMenu( _patchMenu );
    }

    // Repository menu
    _repositoryMenu = new QMenu( _menuBar );
    Q_CHECK_PTR( _repositoryMenu );
    action = _menuBar->addMenu( _repositoryMenu );
    action->setText(_( "&Repositories" ));
    _repositoryMenu->addAction( _( "Repository &Manager..." ), this, SLOT( repoManager() ), Qt::CTRL + Qt::Key_M );

    //
    // Extras menu
    //
    _extrasMenu = new QMenu( _menuBar );
    Q_CHECK_PTR( _extrasMenu );
    action = _menuBar->addMenu( _extrasMenu );
    action->setText(_( "&Extras" ));
    
    _extrasMenu->addAction( _( "Show &Products" 		  ), this, SLOT( showProducts()    ) );
    _extrasMenu->addAction( _( "Show &Automatic Package Changes" ), this, SLOT( showAutoPkgList() ), Qt::CTRL + Qt::Key_A );
    _extrasMenu->addAction( _( "&Verify System"                  ), this, SLOT( verifySystem()    ) );

    _extrasMenu->addSeparator();

    // Translators: This is about packages ending in "-devel", so don't translate that "-devel"!
    _extrasMenu->addAction( _( "Install All Matching -&devel Packages" ), this, SLOT( installDevelPkgs() ) );

    // Translators: This is about packages ending in "-debuginfo", so don't translate that "-debuginfo"!
    _extrasMenu->addAction( _( "Install All Matching -de&buginfo Packages" ), this, SLOT( installDebugInfoPkgs() ) );

    _extrasMenu->addSeparator();

    if ( _pkgConflictDialog )
	_extrasMenu->addAction( _( "Generate Dependency Resolver &Test Case" ),
				_pkgConflictDialog, SLOT( askCreateSolverTestCase() ) );

    if ( _actionResetIgnoredDependencyProblems )
	_extrasMenu->addAction(_actionResetIgnoredDependencyProblems);


#ifdef FIXME
    if ( _patchList )
	_extrasMenu->addAction(_patchList->actionShowRawPatchInfo);
#endif


    //
    // Help menu
    //

    _helpMenu = new QMenu( _menuBar );
    Q_CHECK_PTR( _helpMenu );
    _menuBar->addSeparator();
    action = _menuBar->addMenu( _helpMenu );
    action->setText(_( "&Help" ));

    // Note: The help functions and their texts are moved out
    // to a separate source file YQPackageSelectorHelp.cc

    // Menu entry for help overview
    _helpMenu->addAction( _( "&Overview" ), this, SLOT( help()		), Qt::Key_F1 );

    // Menu entry for help about used symbols ( icons )
    _helpMenu->addAction( _( "&Symbols" ), this, SLOT( symbolHelp()	), Qt::SHIFT + Qt::Key_F1 );

    // Menu entry for keyboard help
    _helpMenu->addAction( _( "&Keys" ), this, SLOT( keyboardHelp() )		      );
}


void
YQPackageSelector::connectFilter( QWidget * filter,
				  QWidget * pkgList,
				  bool hasUpdateSignal )
{
    if ( ! filter  )	return;
    if ( ! pkgList )	return;

    if ( _filters )
    {
	connect( _filters,	SIGNAL( currentChanged(QWidget *) ),
		 filter,	SLOT  ( filterIfVisible()	     ) );
    }

    connect( this,	SIGNAL( refresh()	  ),
	     filter,	SLOT  ( filterIfVisible() ) );

    connect( filter,	SIGNAL( filterStart()	),
	     pkgList,	SLOT  ( clear()		) );

    connect( filter,	SIGNAL( filterMatch( ZyppSel, ZyppPkg ) ),
	     pkgList,	SLOT  ( addPkgItem ( ZyppSel, ZyppPkg ) ) );

    connect( filter,	SIGNAL( filterFinished()  ),
	     pkgList,	SLOT  ( selectSomething() ) );

    connect( filter,	SIGNAL( filterFinished()       ),
	     pkgList,	SLOT  ( logExcludeStatistics() ) );


    if ( hasUpdateSignal )
    {
	connect( filter,		SIGNAL( updatePackages()   ),
		 pkgList,		SLOT  ( updateItemStates() ) );

	if ( _diskUsageList )
	{
	    connect( filter,		SIGNAL( updatePackages()  ),
		     _diskUsageList,	SLOT  ( updateDiskUsage() ) );
	}
    }
}


void
YQPackageSelector::makeConnections()
{
    connect( this, SIGNAL( resolvingStarted()	),
	     this, SLOT	 ( animateCheckButton() ) );

    connect( this, SIGNAL( resolvingFinished()	),
	     this, SLOT	 ( restoreCheckButton() ) );

    connectFilter( _updateProblemFilterView,	_pkgList, false );
    connectFilter( _patternList,		_pkgList );
    connectFilter( _langList,		_pkgList );
    connectFilter( _repoFilterView,		_pkgList, false );
    connectFilter( _rpmGroupTagsFilterView,	_pkgList, false );
    //FIXMEconnectFilter( _langList,			_pkgList );
    connectFilter( _statusFilterView,		_pkgList, false );
    connectFilter( _searchFilterView,		_pkgList, false );

    if ( _searchFilterView && _pkgList )
    {
	connect( _searchFilterView,	SIGNAL( message( const QString & ) ),
		 _pkgList,		SLOT  ( message( const QString & ) ) );
    }

    if ( _repoFilterView && _pkgList )
    {
	connect( _repoFilterView,	SIGNAL( filterNearMatch	 ( ZyppSel, ZyppPkg ) ),
		 _pkgList,		SLOT  ( addPkgItemDimmed ( ZyppSel, ZyppPkg ) ) );
    }

    if ( _pkgList && _diskUsageList )
    {

	connect( _pkgList,		SIGNAL( statusChanged()	  ),
		 _diskUsageList,	SLOT  ( updateDiskUsage() ) );
    }

    connectPatchList();



    //
    // Connect package conflict dialog
    //

    if ( _pkgConflictDialog )
    {
	if (_pkgList )
	{
	    connect( _pkgConflictDialog,	SIGNAL( updatePackages()   ),
		     _pkgList,			SLOT  ( updateItemStates() ) );
	}

	if ( _patternList )
	{
	    connect( _pkgConflictDialog,	SIGNAL( updatePackages()   ),
		     _patternList,		SLOT  ( updateItemStates() ) );
	}


	if ( _diskUsageList )
	{
	    connect( _pkgConflictDialog,	SIGNAL( updatePackages()   ),
		     _diskUsageList,		SLOT  ( updateDiskUsage()  ) );
	}
    }


    //
    // Connect package versions view
    //

    if ( _pkgVersionsView && _pkgList )
    {
	connect( _pkgVersionsView,	SIGNAL( candidateChanged( ZyppObj ) ),
		 _pkgList,		SLOT  ( updateItemData()    ) );
    }


    //
    // Hotkey to enable "patches" filter view on the fly
    //

    QShortcut * accel = new QShortcut( Qt::Key_F2, this, SLOT( hotkeyInsertPatchFilterView() ) );
    Q_CHECK_PTR( accel );

    //
    // Update actions just before opening menus
    //

    if ( _pkgMenu && _pkgList )
    {
	connect( _pkgMenu,	SIGNAL( aboutToShow()	),
		 _pkgList,	SLOT  ( updateActions() ) );
    }

    if ( _patchMenu && _patchList )
    {
	connect( _patchMenu,	SIGNAL( aboutToShow()	),
		 _patchList,	SLOT  ( updateActions() ) );
    }
}


void
YQPackageSelector::animateCheckButton()
{
    if ( _checkDependenciesButton )
    {
	QPalette p = _checkDependenciesButton->palette();
	p.setColor(QPalette::Background, QColor( 0xE0, 0xE0, 0xF8 ));
	_checkDependenciesButton->setPalette(p);
	_checkDependenciesButton->repaint();
    }
}


void
YQPackageSelector::restoreCheckButton()
{
    if ( _checkDependenciesButton )
    {
        QPalette p = _checkDependenciesButton->palette();
        p.setColor(QPalette::Background, _normalButtonBackground);
        _checkDependenciesButton->setPalette(p);
    }
}


void
YQPackageSelector::autoResolveDependencies()
{
    if ( _autoDependenciesCheckBox && ! _autoDependenciesCheckBox->isChecked() )
	return;

    resolveDependencies();
}


int
YQPackageSelector::manualResolvePackageDependencies()
{
    if ( ! _pkgConflictDialog )
    {
	yuiError() << "No package conflict dialog existing" << endl;
	return QDialog::Accepted;
    }

    YQUI::ui()->busyCursor();
    int result = _pkgConflictDialog->solveAndShowConflicts();
    YQUI::ui()->normalCursor();

#if DEPENDENCY_FEEDBACK_IF_OK

    if ( result == QDialog::Accepted )
    {
	QMessageBox::information( this, "",
				  _( "All package dependencies are OK." ),
				  QMessageBox::Ok );
    }
#endif

    return result;
}


void
YQPackageSelector::addPatchFilterView()
{
    if ( ! _patchFilterView )
    {
	_patchFilterView = new YQPkgPatchFilterView( this );
	Q_CHECK_PTR( _patchFilterView );
	_filters->addPage( _( "Patches" ), _patchFilterView );

	_patchList = _patchFilterView->patchList();
	Q_CHECK_PTR( _patchList );

	connectPatchList();
    }
}


void
YQPackageSelector::hotkeyInsertPatchFilterView()
{
    if ( ! _patchFilterView )
    {
	yuiMilestone() << "Activating patches filter view" << endl;

	addPatchFilterView();
	connectPatchList();

	_filters->showPage( _patchFilterView );
	_pkgList->clear();
	_patchList->filter();
    }
}


void
YQPackageSelector::connectPatchList()
{
    if ( _pkgList && _patchList )
    {
	connectFilter( _patchList, _pkgList );

	connect( _patchList, SIGNAL( filterMatch   ( const QString &, const QString &, FSize ) ),
		 _pkgList,   SLOT  ( addPassiveItem( const QString &, const QString &, FSize ) ) );

	connect( _patchList,		SIGNAL( statusChanged()			),
		 this,			SLOT  ( autoResolveDependencies()	) );

	if ( _pkgConflictDialog )
	{
	    connect( _pkgConflictDialog,SIGNAL( updatePackages()		),
		     _patchList,	SLOT  ( updateItemStates()		) );
	}

	connect( this,			SIGNAL( refresh()			),
		 _patchList,		SLOT  ( updateItemStates()		) );

    }
}


void
YQPackageSelector::pkgExport()
{
    QString filename = YQApplication::askForSaveFileName( QString( DEFAULT_EXPORT_FILE_NAME ),	// startsWith
							  QString( "*.xml;;*" ),		// filter
							  _( "Save Package List" ) );

    if ( ! filename.isEmpty() )
    {
	zypp::syscontent::Writer writer;
	const zypp::ResPool & pool = zypp::getZYpp()->pool();

	// The ZYPP obfuscated C++ contest proudly presents:

	for_each( pool.begin(), pool.end(),
		  boost::bind( &zypp::syscontent::Writer::addIf,
			       boost::ref( writer ),
			       _1 ) );
	// Yuck. What a mess.
	//
	// Does anybody seriously believe this kind of thing is easier to read,
	// let alone use? Get real. This is an argument in favour of all C++
	// haters. And it's one that is really hard to counter.
	//
	// -sh 2006-12-13

	try
	{
	    std::ofstream exportFile( toUTF8( filename ).c_str() );
	    exportFile.exceptions( std::ios_base::badbit | std::ios_base::failbit );
	    exportFile << writer;

	    yuiMilestone() << "Package list exported to " << filename << endl;
	}
	catch ( std::exception & exception )
	{
	    yuiWarning() << "Error exporting package list to " << filename << endl;

	    // The export might have left over a partially written file.
	    // Try to delete it. Don't care if it doesn't exist and unlink() fails.
	    QFile::remove(filename);

	    // Post error popup
	    QMessageBox::warning( this,						// parent
				  _( "Error" ),					// caption
				  _( "Error exporting package list to %1" ).arg( filename ),
				  QMessageBox::Ok | QMessageBox::Default,	// button0
				  Qt::NoButton,			// button1
				  Qt::NoButton );			// button2
	}
    }
}


void
YQPackageSelector::pkgImport()
{
    QString filename =	QFileDialog::getOpenFileName( this, _( "Load Package List" ), DEFAULT_EXPORT_FILE_NAME,		// startsWi
						      "*.xml+;;*"// filter
						      );

    if ( ! filename.isEmpty() )
    {
	yuiMilestone() << "Importing package list from " << filename << endl;

	try
	{
	    std::ifstream importFile( toUTF8( filename ).c_str() );
	    zypp::syscontent::Reader reader( importFile );

	    //
	    // Put reader contents into maps
	    //

	    typedef zypp::syscontent::Reader::Entry	ZyppReaderEntry;
	    typedef std::pair<string, ZyppReaderEntry>	ImportMapPair;

	    map<string, ZyppReaderEntry> importPkg;
	    map<string, ZyppReaderEntry> importPatterns;

	    for ( zypp::syscontent::Reader::const_iterator it = reader.begin();
		  it != reader.end();
		  ++ it )
	    {
		string kind = it->kind();

		if      ( kind == "package" ) 	importPkg.insert     ( ImportMapPair( it->name(), *it ) );
		else if ( kind == "pattern" )	importPatterns.insert( ImportMapPair( it->name(), *it ) );
	    }

	    yuiDebug() << "Found "        << importPkg.size()
		       <<" packages and " << importPatterns.size()
		       << " patterns in " << filename
		       << endl;


	    //
	    // Set status of all patterns and packages according to import map
	    //

	    for ( ZyppPoolIterator it = zyppPatternsBegin();
		  it != zyppPatternsEnd();
		  ++it )
	    {
		ZyppSel selectable = *it;
		importSelectable( *it, importPatterns.find( selectable->name() ) != importPatterns.end(), "pattern" );
	    }

	    for ( ZyppPoolIterator it = zyppPkgBegin();
		  it != zyppPkgEnd();
		  ++it )
	    {
		ZyppSel selectable = *it;
		importSelectable( *it, importPkg.find( selectable->name() ) != importPkg.end(), "package" );
	    }


	    //
	    // Display result
	    //

	    emit refresh();

	    if ( _statusFilterView )
	    {
		// Switch to "Installation Summary" filter view

		_filters->showPage( _statusFilterView );
		_statusFilterView->filter();
	    }

	}
	catch ( const zypp::Exception & exception )
	{
	    yuiWarning() << "Error reading package list from " << filename << endl;

	    // Post error popup
	    QMessageBox::warning( this,						// parent
				  _( "Error" ),					// caption
				  _( "Error loading package list from %1" ).arg( filename ),
				  QMessageBox::Ok | QMessageBox::Default,	// button0
				  QMessageBox::NoButton,			// button1
				  QMessageBox::NoButton );			// button2
	}
    }
}


void
YQPackageSelector::importSelectable( ZyppSel		selectable,
				     bool		isWanted,
				     const char * 	kind )
{
    ZyppStatus oldStatus = selectable->status();
    ZyppStatus newStatus = oldStatus;

    if ( isWanted )
    {
	//
	// Make sure this selectable does not get installed
	//

	switch ( oldStatus )
	{
	    case S_Install:
	    case S_AutoInstall:
	    case S_KeepInstalled:
	    case S_Protected:
	    case S_Update:
	    case S_AutoUpdate:
		newStatus = oldStatus;
		break;

	    case S_Del:
	    case S_AutoDel:
		newStatus = S_KeepInstalled;
		yuiDebug() << "Keeping " << kind << " " << selectable->name() << endl;
		break;

	    case S_NoInst:
	    case S_Taboo:

		if ( selectable->hasCandidateObj() )
		{
		    newStatus = S_Install;
		    yuiDebug() << "Adding " << kind << " " <<  selectable->name() << endl;
		}
		else
		{
		    yuiDebug() << "Can't add " << kind << " " << selectable->name()
			       << ": No candidate" << endl;
		}
		break;
	}
    }
    else // ! isWanted
    {
	//
	// Make sure this selectable does not get installed
	//

	switch ( oldStatus )
	{
	    case S_Install:
	    case S_AutoInstall:
	    case S_KeepInstalled:
	    case S_Protected:
	    case S_Update:
	    case S_AutoUpdate:
		newStatus = S_Del;
		yuiDebug() << "Deleting " << kind << " " << selectable->name() << endl;
		break;

	    case S_Del:
	    case S_AutoDel:
	    case S_NoInst:
	    case S_Taboo:
		newStatus = oldStatus;
		break;
	}
    }

    if ( oldStatus != newStatus )
	selectable->setStatus( newStatus );
}


void
YQPackageSelector::globalUpdatePkg( bool force )
{
    if ( ! _pkgList )
	return;

    int count = _pkgList->globalSetPkgStatus( S_Update, force,
					      true ); // countOnly
    yuiMilestone() << count << " pkgs found for update" << endl;

    if ( count >= GLOBAL_UPDATE_CONFIRMATION_THRESHOLD )
    {
	if ( QMessageBox::question( this, "",	// caption
				    // Translators: %1 is the number of affected packages
				    _( "%1 packages will be updated" ).arg( count ),
				    _( "&Continue" ), _( "C&ancel" ),
				    0,		// defaultButtonNumber (from 0)
				    1 )		// escapeButtonNumber
	     == 1 )	// "Cancel"?
	{
	    return;
	}
    }

    (void) _pkgList->globalSetPkgStatus( S_Update, force,
					 false ); // countOnly

    if ( _statusFilterView )
    {
	_filters->showPage( _statusFilterView );
	_statusFilterView->clear();
	_statusFilterView->showTransactions();
	_statusFilterView->filter();
    }
}


void
YQPackageSelector::showProducts()
{
    YQPkgProductDialog::showProductDialog();
}

void
YQPackageSelector::installDevelPkgs()
{
    installSubPkgs( "-devel" );
}


void
YQPackageSelector::installDebugInfoPkgs()
{
    installSubPkgs( "-debuginfo" );
}

void
YQPackageSelector::pkgExcludeDebugChanged( bool on )
{
    if ( _viewMenu && _pkgList )
    {
        if ( _excludeDebugInfoPkgs )
            _excludeDebugInfoPkgs->enable( ! on );

        _pkgList->applyExcludeRules();
    }
}

void
YQPackageSelector::pkgExcludeDevelChanged( bool on )
{
    if ( _viewMenu && _pkgList )
    {
        if ( _excludeDevelPkgs )
            _excludeDevelPkgs->enable( ! on );

        _pkgList->applyExcludeRules();
    }
}

void
YQPackageSelector::installSubPkgs( const QString suffix )
{
    // Find all matching packages and put them into a QMap

    QMap<QString, ZyppSel> subPkgs;

    for ( ZyppPoolIterator it = zyppPkgBegin();
	  it != zyppPkgEnd();
	  ++it )
    {
	QString name = (*it)->name().c_str();

	if ( name.endsWith( suffix ) )
	{
	    subPkgs[ name ] = *it;

	    yuiDebug() << "Found subpackage: " << name << endl;
	}
    }


    // Now go through all packages and look if there is a corresponding subpackage in the QMap

    for ( ZyppPoolIterator it = zyppPkgBegin();
	  it != zyppPkgEnd();
	  ++it )
    {
	QString name = (*it)->name().c_str();

	if ( subPkgs.contains( name + suffix ) )
	{
	    QString subPkgName( name + suffix );
	    ZyppSel subPkg = subPkgs[ subPkgName ];

	    switch ( (*it)->status() )
	    {
		case S_AutoDel:
		case S_NoInst:
		case S_Protected:
		case S_Taboo:
		case S_Del:
		    // Don't install the subpackage
		    yuiMilestone() << "Ignoring unwanted subpackage " << subPkgName << endl;
		    break;

		case S_AutoInstall:
		case S_Install:
		case S_KeepInstalled:

		    // Install the subpackage, but don't try to update it

		    if ( ! subPkg->installedObj() )
		    {
			subPkg->setStatus( S_Install );
			yuiMilestone() << "Installing subpackage " << subPkgName << endl;
		    }
		    break;


		case S_Update:
		case S_AutoUpdate:

		    // Install or update the subpackage

		    if ( ! subPkg->installedObj() )
		    {
			subPkg->setStatus( S_Install );
			yuiMilestone() << "Installing subpackage " << subPkgName << endl;
		    }
		    else
		    {
			subPkg->setStatus( S_Update );
			yuiMilestone() << "Updating subpackage " << subPkgName << endl;
		    }
		    break;

		    // Intentionally omitting 'default' branch so the compiler can
		    // catch unhandled enum states
	    }
	}
    }


    if ( _filters && _statusFilterView )
    {
	_filters->showPage( _statusFilterView );
	_statusFilterView->filter();
    }

    YQPkgChangesDialog::showChangesDialog( _( "Added Subpackages:" ),
					   QRegExp( ".*" + suffix + "$" ),
					   _( "&OK" ),
					   QString::null,	// rejectButtonLabel
					   true );		// showIfEmpty
}


#include "YQPackageSelector.moc"
