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

  File:	      YQInputField.cc

  Author:     Stefan Hundhammer <sh@suse.de>

  Textdomain "packages-qt"

/-*/


#include <qlineedit.h>
#define y2log_component "qt-ui"
#include <ycp/y2log.h>

using std::max;

#include "utf8.h"
#include "YQUI.h"
#include "YEvent.h"
#include "QY2CharValidator.h"
#include "YQInputField.h"
#include "YQi18n.h"
#include "YQSignalBlocker.h"
#include "YQWidgetCaption.h"

// Include low-level X headers AFTER Qt headers:
// X.h pollutes the global namespace (!!!) with pretty useless #defines
// like "Above", "Below" etc. that clash with some Qt headers.
#include <X11/X.h>		// CapsLock detection
#include <X11/Xlib.h>		// CapsLock detection
#include <X11/keysym.h>		// CapsLock detection

using std::string;



YQInputField::YQInputField( YWidget * 		parent,
			    const string &	label,
			    bool		passwordMode )
    : QVBox( (QWidget *) parent->widgetRep() )
    , YInputField( parent, label, passwordMode )
    , _validator(0)
    , _displayingCapsLockWarning( false )
{
    setWidgetRep( this );

    setSpacing( YQWidgetSpacing );
    setMargin( YQWidgetMargin );

    _caption = new YQWidgetCaption( this, label );
    YUI_CHECK_NEW( _caption );
    
    _qt_lineEdit = new YQRawLineEdit( this );
    YUI_CHECK_NEW( _qt_lineEdit );

    _caption->setBuddy( _qt_lineEdit );

    connect( _qt_lineEdit, SIGNAL( textChanged( const QString & ) ),
	     this,         SLOT  ( changed    ( const QString & ) ) );

    if ( passwordMode )
    {
	_qt_lineEdit->setEchoMode( QLineEdit::Password );

	connect( _qt_lineEdit,	SIGNAL( capsLockActivated() ),
		 this,		SLOT  ( displayCapsLockWarning() ) );

	connect( _qt_lineEdit,	SIGNAL( capsLockDeactivated() ),
		 this,		SLOT  ( clearCapsLockWarning() ) );
    }
}


string YQInputField::value()
{
    return toUTF8( _qt_lineEdit->text() );
}


void YQInputField::setValue( const string & newText )
{
    QString text = fromUTF8( newText );

    if ( isValidText( text ) )
    {
	YQSignalBlocker sigBlocker( _qt_lineEdit );
	_qt_lineEdit->setText( text );
    }
    else
    {
	y2error( "%s \"%s\": Rejecting invalid value \"%s\"",
		 widgetClass(), debugLabel().c_str(), newText.c_str() );
    }
}


void YQInputField::setEnabled( bool enabled )
{
    _qt_lineEdit->setEnabled( enabled );
    _caption->setEnabled( enabled );
    YWidget::setEnabled( enabled );
}


int YQInputField::preferredWidth()
{
    int minSize	  = shrinkable() ? 30 : 200;
    int hintWidth = _caption->isShown()
	? _caption->sizeHint().width() + 2 * YQWidgetMargin
	: 0;

    return max( minSize, hintWidth );
}


int YQInputField::preferredHeight()
{
    return sizeHint().height();
}


void YQInputField::setSize( int newWidth, int newHeight )
{
    resize( newWidth, newHeight );
}


void YQInputField::setLabel( const string & label )
{
    _caption->setText( label );
    YInputField::setLabel( label );
}


bool YQInputField::isValidText( const QString & txt ) const
{
    if ( ! _validator )
	return true;

    int pos = 0;
    QString text( txt );	// need a non-const QString &

    return _validator->validate( text, pos ) == QValidator::Acceptable;
}


void YQInputField::setValidChars( const string & newValidChars )
{
    if ( _validator )
    {
	_validator->setValidChars( fromUTF8( newValidChars ) );
    }
    else
    {
	_validator = new QY2CharValidator( fromUTF8( newValidChars ), this );
	_qt_lineEdit->setValidator( _validator );

	// No need to delete the validator in the destructor - Qt will take
	// care of that since it's a QObject with a parent!
    }

    if ( ! isValidText( _qt_lineEdit->text() ) )
    {
	y2error( "Old value \"%s\" of %s \"%s\" invalid according to ValidChars \"%s\" - deleting",
		 (const char *) _qt_lineEdit->text(),
		 widgetClass(), debugLabel().c_str(),
		 newValidChars.c_str() );
	_qt_lineEdit->setText( "" );
    }

    YInputField::setValidChars( newValidChars );
}

void YQInputField::setInputMaxLength( int len )
{
    _qt_lineEdit->setMaxLength( len );
    YInputField::setInputMaxLength( len );
}

bool YQInputField::setKeyboardFocus()
{
    _qt_lineEdit->setFocus();
    _qt_lineEdit->selectAll();

    return true;
}


void YQInputField::changed( const QString & )
{
    if ( notify() )
	YQUI::ui()->sendEvent( new YWidgetEvent( this, YEvent::ValueChanged ) );
}


void YQInputField::displayCapsLockWarning()
{
    y2milestone( "warning" );
    if ( _displayingCapsLockWarning )
	return;

    if ( _qt_lineEdit->echoMode() == QLineEdit::Normal )
	return;

    // Translators: This is a very short warning that the CapsLock key
    // is active while trying to type in a password field. This warning
    // replaces the normal label (caption) of that password field while
    // CapsLock is active, so please keep it short. Please don't translate it
    // at all if the term "CapsLock" can reasonably expected to be understood
    // by the target audience.
    //
    // In particular, please don't translate this to death in German.
    // Simply leave it.

    _caption->setText( _( "CapsLock!" ) );
    _displayingCapsLockWarning = true;
}


void YQInputField::clearCapsLockWarning()
{
    y2milestone( "warning off " );
    if ( ! _displayingCapsLockWarning )
	return;

    if ( _qt_lineEdit->echoMode() == QLineEdit::Normal )
	return;

    _caption->setText( label() );
    _displayingCapsLockWarning = false;
}


bool YQRawLineEdit::x11Event( XEvent * event )
{
    // Qt (3.x) does not have support for the CapsLock key.
    // All other modifiers (Shift, Control, Meta) are propagated via
    // Qt's events, but for some reason, CapsLock is not.
    //
    // So let's examine the raw X11 event here to check for the
    // CapsLock status. All events are really handled on the parent class
    // (QWidget) level, though. We only peek into the modifier states.

    if ( event )
    {
	bool oldCapsLockActive = _capsLockActive;

	switch ( event->type )
	{
	    case KeyPress:
		_capsLockActive = (bool) ( event->xkey.state & LockMask );
		break;

	    case KeyRelease:

		_capsLockActive = (bool) ( event->xkey.state & LockMask );

		if ( _capsLockActive && oldCapsLockActive )
		{
		    KeySym key = XLookupKeysym( &(event->xkey), 0 );

		    if ( key == XK_Caps_Lock ||
			 key == XK_Shift_Lock  )
		    {
			y2milestone( "CapsLock released" );
			_capsLockActive = false;
		    }
		}

		y2debug( "Key event; caps lock: %s", _capsLockActive ? "on" : "off" );
		break;

	    case ButtonPress:
	    case ButtonRelease:
		_capsLockActive = (bool) ( event->xbutton.state & LockMask );
		break;

	    case EnterNotify:
		_capsLockActive = (bool) ( event->xcrossing.state & LockMask );
		break;

	    case LeaveNotify:
	    case FocusOut:
		_capsLockActive = false;
		emit capsLockDeactivated();
		break;

	    default:
		break;
	}

	if ( oldCapsLockActive != _capsLockActive )
	{
	    y2milestone( "Emitting warning" );

	    if ( _capsLockActive )
		emit capsLockActivated();
	    else
		emit capsLockDeactivated();
	}
    }

    return false; // handle this event at the Qt level
}


#include "YQInputField.moc"