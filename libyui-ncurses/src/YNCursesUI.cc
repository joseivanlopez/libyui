/*---------------------------------------------------------------------\
|                                                                      |
|                      __   __    ____ _____ ____                      |
|                      \ \ / /_ _/ ___|_   _|___ \                     |
|                       \ V / _` \___ \ | |   __) |                    |
|                        | | (_| |___) || |  / __/                     |
|                        |_|\__,_|____/ |_| |_____|                    |
|                                                                      |
|                               core system                            |
|                                                        (C) SuSE GmbH |
\----------------------------------------------------------------------/

   File:       YNCursesUI.cc

   Author:     Michael Andres <ma@suse.de>
   Maintainer: Michael Andres <ma@suse.de>

/-*/

#include "YNCursesUI.h"
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <langinfo.h>

#include <YUI.h>
#include <YEvent.h>
#include <YDialog.h>
#include <YCommandLine.h>
#include <YMacro.h>

#define YUILogComponent "ncurses"
#include <yui/YUILog.h>

//#include "NCPackageSelectorStart.h"


#define  YUILogComponent "ncurses"
#include <YUILog.h>
#include "NCstring.h"
#include "NCWidgetFactory.h"
#include "NCOptionalWidgetFactory.h"
#include "NCPackageSelectorPluginStub.h"

extern string language2encoding( string lang );


YUI * createUI( bool withThreads )
{
    if ( ! YNCursesUI::ui() )
	new YNCursesUI( withThreads );
    
    return YNCursesUI::ui();
}


YNCursesUI::YNCursesUI( bool withThreads )
    : YUI( withThreads )
{
    yuiMilestone() << "Start YNCursesUI" << endl;
    _ui = this;

    if ( getenv( "LANG" ) != NULL )
    {
	string language = getenv( "LANG" );
	string encoding =  nl_langinfo( CODESET );

        // setlocale ( LC_ALL, "" ) is called in WMFInterpreter::WFMInterpreter;

	// Explicitly set LC_CTYPE so that it won't be changed if setenv( LANG ) is called elsewhere.
        // (it's not enough to call setlocale( LC_CTYPE, .. ), set env. variable LC_CTYPE!)

	string locale = setlocale( LC_CTYPE, NULL );
	setenv( "LC_CTYPE", locale.c_str(), 1 );

	yuiMilestone() << "setenv LC_CTYPE: " << locale << " encoding: " << encoding << endl;
	
        // The encoding of a terminal (xterm, konsole etc.) can never change; the encoding
	// of the "real" console is changed in setConsoleFont(). 
	NCstring::setTerminalEncoding( encoding );
  
	app()->setLanguage( language, encoding );
    }

    try {
	NCurses::init();
    }
    catch ( NCursesError & err ) {
	yuiMilestone() << err << endl;
	::endwin();
	abort();
    }

    topmostConstructorHasFinished();
}

YNCursesUI::~YNCursesUI()
{
    //delete left-over dialogs (if any)
    YDialog::deleteAllDialogs();
    yuiMilestone() << "Stop YNCursesUI" << endl;
}


YNCursesUI * YNCursesUI::_ui = 0;

YWidgetFactory *
YNCursesUI::createWidgetFactory()
{
    NCWidgetFactory * factory = new NCWidgetFactory();
    YUI_CHECK_NEW( factory );
    
    return factory;
}



YOptionalWidgetFactory *
YNCursesUI::createOptionalWidgetFactory()
{
    NCOptionalWidgetFactory * factory = new NCOptionalWidgetFactory();
    YUI_CHECK_NEW( factory );
    
    return factory;
}


YApplication *
YNCursesUI::createApplication()
{
    NCApplication * app = new NCApplication();
    YUI_CHECK_NEW( app );

    return app;
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::idleLoop
//	METHOD TYPE : void
//
//	DESCRIPTION :
//
void YNCursesUI::idleLoop( int fd_ycp )
{

  int    timeout = 5;
  struct timeval tv;
  fd_set fdset;
  int    retval;

  do {
    tv.tv_sec  = timeout;
    tv.tv_usec = 0;

    FD_ZERO( &fdset );
    FD_SET( 0,      &fdset );
    FD_SET( fd_ycp, &fdset );

    retval = select( fd_ycp+1, &fdset, 0, 0, &tv );

    if ( retval < 0 ) {
      if ( errno != EINTR )
	  yuiError() << "idleLoop error in select() (" << errno << ')' << endl;
    } else if ( retval != 0 ) {
      //do not throw here, as current dialog may not necessarily exist yet
      //if we have threads
      YDialog *currentDialog = YDialog::currentDialog( false );

      if (currentDialog) {
        NCDialog * ncd = static_cast<NCDialog *>( currentDialog );
        if( ncd ) {
	  extern NCBusyIndicator* NCBusyIndicatorObject;
	  if (NCBusyIndicatorObject)
  	    NCBusyIndicatorObject->handler(0);

	  ncd->idleInput();
        }
      }
    } // else no input within timeout sec.
  } while ( !FD_ISSET( fd_ycp, &fdset ) );
}

///////////////////////////////////////////////////////////////////


#define ONCREATE yuiDebug() << endl
//#define ONCREATE


///////////////////////////////////////////////////////////////////
//
// package selection
//
///////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::packageSelectorPlugin()
//	METHOD TYPE : NCPackageSelectorPlugin
//
//	DESCRIPTION : Create the package selector plugin
//
NCPackageSelectorPluginStub * YNCursesUI::packageSelectorPlugin()
{
    static NCPackageSelectorPluginStub * plugin = 0;

    if ( ! plugin )
    {
	plugin = new NCPackageSelectorPluginStub();

	// This is a deliberate memory leak: If an application requires a
	// PackageSelector, it is a package selection application by
	// definition. In this case, the ncurses_pkg plugin is intentionally
	// kept open to avoid repeated start-up cost of the plugin and libzypp.
    }

    return plugin;
}


///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::runPkgSelection
//	METHOD TYPE : YEvent *
//
//	DESCRIPTION : Implementation of UI builtin RunPkgSelection() which
//		      has to be called after OpenDialog( `PackageSelector() ).
//
YEvent * YNCursesUI::runPkgSelection( YWidget * selector )
{
    YEvent * event = 0;
    
    YDialog *dialog = YDialog::currentDialog();
    NCPackageSelectorPluginStub * plugin = packageSelectorPlugin();
    
    if ( !dialog )
    {
	yuiError() << "ERROR package selection: No dialog rexisting." << endl;
	return 0;
    }
    if ( !selector )
    {
	yuiError() << "ERROR package selection: No package selector existing." << endl;
	return 0;
    }
    // FIXME - remove debug logging
    yuiMilestone() << "Dump current dialog in YNCursesUI::runPkgSelection" << endl;
    // debug: dump the widget tree
    dialog->dumpDialogWidgetTree();	

    if ( plugin )
    {
	event = plugin->runPkgSelection( dialog, selector );
    }

    return event;
}


///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::init_title
//	METHOD TYPE : void
//
//	DESCRIPTION :
//
void YNCursesUI::init_title()
{
    string title_ti( "YaST" );
    string module;
    char hostname_ti[256];
    hostname_ti[0] = hostname_ti[255] = '\0';

    // get command line args, usually in the form 
    // '/usr/lib/whatever/y2base' 'module_name' 'selected_ui'
    // (e.g. 'y2base' 'lan' 'ncurses') -> we need 'lan' item
    YCommandLine *cmdline = new YCommandLine();
    YUI_CHECK_NEW( cmdline );
    module = (*cmdline)[1];

    // check for valid hostname, suppress "(none)" as hostname
    if ( gethostname( hostname_ti, 255 ) != -1
	 && hostname_ti[0]
	 && hostname_ti[0] != '(' )
    {
	if ( !module.empty() )
        {
	    title_ti += " - ";
	    title_ti += module;
        }
	title_ti += " @ ";
	title_ti += hostname_ti;
    }
    NCurses::SetTitle( title_ti );
}

///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::want_colors()
//	METHOD TYPE : bool
//
//	DESCRIPTION :
//
bool YNCursesUI::want_colors()
{
  if ( getenv( "Y2NCURSES_BW" ) != NULL ) {
    yuiMilestone() << "Y2NCURSES_BW is set - won't use colors" << endl;
    return false;
  }
  return true;
}


///////////////////////////////////////////////////////////////////
//
//
//	METHOD NAME : YNCursesUI::setConsoleFont
//
//	DESCRIPTION : UI::setConsoleFont() is called in Console.ycp.
//		      The terminal encoding must be set correctly.
//
/**
 * This doesn't belong here, but it is so utterly entangled with member
 * variables that are not exported at all (sic!) that it's not really feasible
 * to extract the relevant parts. 
 **/ 
void YNCursesUI::setConsoleFont( const string & console_magic,
				 const string & font,
				 const string & screen_map,
				 const string & unicode_map,
				 const string & encoding )
{
    string cmd( "setfont" );
    cmd += " -C " + myTerm;
    cmd += " " + font;
    if ( !screen_map.empty() )
	cmd += " -m " + screen_map;
    if ( !unicode_map.empty() )
	cmd += " -u " + unicode_map;

    yuiMilestone() << cmd << endl;
    int ret = system( (cmd + " >/dev/null 2>&1").c_str() );

    // setfont returns error if called e.g. on a xterm -> return
    if ( ret ) {
	yuiError() << cmd.c_str() << " returned " << ret << endl;
	Refresh();
	return;
    }
    // go on in case of a "real" console
    cmd = "(echo -en \"\\033";
    if ( console_magic.length() )
	cmd += console_magic;
    else
	cmd += "(B";
    cmd += "\" >" + myTerm + ")";
    yuiMilestone() << cmd << endl;
    ret = system( (cmd + " >/dev/null 2>&1").c_str() );
    if ( ret ) {
	yuiError() << cmd.c_str() << " returned " << ret << endl;
    }

    // set terminal encoding for console
    // (setConsoleFont() in Console.ycp has passed the encoding as last
    // argument but this encoding was not correct; now Console.ycp passes the
    // language) if the encoding is NOT UTF-8 set the console encoding
    // according to the language
    
    if ( NCstring::terminalEncoding() != "UTF-8" )
    {
	string language = YUI::app()->language();
	string::size_type pos = language.find( '.' );

	if ( pos != string::npos )
	{
	    language.erase( pos );
	}
	pos = language.find( '_' );
	if ( pos != string::npos )
	{
	    language.erase( pos );
	}

	string code = language2encoding( language );

	yuiMilestone() << "setConsoleFont( ENCODING:  " << code << " )" << endl;
    
	if ( NCstring::setTerminalEncoding( code ) )
	{
	    Redraw();
	}
	else
	{    
	    Refresh();
	}
    }
    else
    {
	Refresh();
    }
}