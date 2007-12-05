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

  File:	      YQApplication.h

  Author:     Stefan Hundhammer <sh@suse.de>

/-*/


#ifndef YQApplication_h
#define YQApplication_h

#include <qobject.h>
#include <qtranslator.h>
#include <qfont.h>

#include "QY2Settings.h"
#include "YApplication.h"


class YQApplication: public QObject, public YApplication
{
    Q_OBJECT
    
protected:

    friend class YQUI;
    
    /**
     * Constructor.
     *
     * Use YUI::app() to get the singleton for this class.
     **/
    YQApplication();

    /**
     * Destructor.
     **/
    virtual ~YQApplication();

    
public:
    
    /**
     * Set language and encoding for the locale environment ($LANG).
     *
     * Load UI-internal translations (e.g. for predefined dialogs like
     * file selection) and fonts.
     *
     * 'language' is the ISO short code ("de_DE", "en_US", ...).
     *
     * 'encoding' an (optional) encoding ("utf8", ...) that will be appended if
     *  present.
     *
     * Reimplemented from YApplication.
     **/
    virtual void setLanguage( const string & language,
			      const string & encoding = string() );
    
    /**
     * Load translations for Qt's predefined dialogs like file selection box
     * etc.
     **/
    void loadPredefinedQtTranslations();

    /**
     * Set fonts according to the specified language and encoding.
     *
     * This is most important for some Asian languages that have overlaps in
     * the Unicode table, like Japanese vs. Chinese.
     **/
    void setLangFonts( const string & language,
		       const string & encoding = string() );

    /**
     * Returns the application's default font.
     **/
    const QFont & currentFont();

    /**
     * Returns the application's default bold font.
     **/
    const QFont & boldFont();

    /**
     * Returns the application's heading font.
     **/
    const QFont & headingFont();

    /**
     * Delete the fonts so they will be reloaded upon their next usage.
     **/
    void deleteFonts();

    /**
     * Determine good fonts based on defaultsize geometry and set
     * _auto_normal_font_size and _auto_heading_font_size accordingly.
     * Caches the values, so it's safe to call this repeatedly.
     **/
    void pickAutoFonts();

    /**
     * Returns 'true' if the UI  automatically picks fonts, disregarding Qt
     * standard settings.
     *
     * This makes sense during system installation system where the display DPI
     * cannot reliably be retrieved and thus Qt uses random font sizes based on
     * that random DPI.
     **/
    bool autoFonts() const { return _autoFonts; }

    /**
     * Set whether or not fonts should automatically be picked.
     **/
    void setAutoFonts( bool useAutoFonts );

    
protected:

    /**
     * Constructs a key for the language specific font file:
     *     "font[lang]"
     * for
     *     font[de_DE] = "Sans Serif"
     *     font[zh] = "ChineseSpecial, something"
     *     font[ja_JP] = "JapaneseSpecial, something"
     *     font = "Sans Serif"
     **/
    QString fontKey( const QString & lang );


    //
    // Data members
    //
    
    // Fonts
    
    QFont * _currentFont;
    QFont * _headingFont;
    QFont * _boldFont;

    /**
     * Font family or list of font families to use ("Sans Serif" etc.)
     **/
    QString _fontFamily;

    /**
     * Language-specific font settings
     **/
    QY2Settings	* _langFonts;

    /**
     * Translator for the predefined Qt dialogs
     **/
    QTranslator * _qtTranslations;

    /**
     * For auto fonts
     **/
    bool _autoFonts;
    int  _autoNormalFontSize;
    int  _autoHeadingFontSize;

    
};


#endif // YQApplication_h