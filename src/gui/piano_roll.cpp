#ifndef SINGLE_SOURCE_COMPILE

/*
 * piano_roll.cpp - implementation of piano-roll which is used for actual
 *                  writing of melodies
 *
 * Copyright (c) 2004-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * 
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#include "piano_roll.h"


#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QMdiArea>
#include <QtGui/QPainter>
#include <QtGui/QStyleOption>
#include <QtGui/QWheelEvent>


#ifndef __USE_XOPEN
#define __USE_XOPEN
#endif

#include <math.h>


#include "automatable_model_templates.h"
#include "clipboard.h"
#include "combobox.h"
#include "debug.h"
#include "detuning_helper.h"
#include "embed.h"
#include "gui_templates.h"
#include "instrument_track.h"
#include "main_window.h"
#include "midi.h"
#include "mmp.h"
#include "pattern.h"
#include "piano.h"
#include "pixmap_button.h"
#include "song.h"
#include "song_editor.h"
#include "templates.h"
#include "text_float.h"
#include "timeline.h"
#include "tool_button.h"
#include "tooltip.h"


typedef automationPattern::timeMap timeMap;


extern Keys whiteKeys[];	// defined in piano_widget.cpp


// some constants...
const int INITIAL_PIANOROLL_WIDTH = 640;
const int INITIAL_PIANOROLL_HEIGHT = 480;

const int SCROLLBAR_SIZE = 16;
const int PIANO_X = 0;

const int WHITE_KEY_WIDTH = 64;
const int BLACK_KEY_WIDTH = 41;
const int WHITE_KEY_SMALL_HEIGHT = 18;
const int WHITE_KEY_BIG_HEIGHT = 24;
const int BLACK_KEY_HEIGHT = 16;
const int C_KEY_LABEL_X = WHITE_KEY_WIDTH - 19;
const int KEY_LINE_HEIGHT = 12;
const int OCTAVE_HEIGHT = KEY_LINE_HEIGHT * KeysPerOctave;	// = 12 * 12;

const int PR_BOTTOM_MARGIN = SCROLLBAR_SIZE;
const int PR_TOP_MARGIN = 48;

// width of area used for resizing (the grip at the end of a note)
const int RESIZE_AREA_WIDTH = 4;

// width of line for setting volume/panning of note
const int NE_LINE_WIDTH = 3;

// key where to start
const int INITIAL_START_KEY = Key_C + Octave_3 * KeysPerOctave;


QPixmap * pianoRoll::s_whiteKeySmallPm = NULL;
QPixmap * pianoRoll::s_whiteKeyBigPm = NULL;
QPixmap * pianoRoll::s_blackKeyPm = NULL;
QPixmap * pianoRoll::s_toolDraw = NULL;
QPixmap * pianoRoll::s_toolErase = NULL;
QPixmap * pianoRoll::s_toolSelect = NULL;
QPixmap * pianoRoll::s_toolMove = NULL;
QPixmap * pianoRoll::s_toolOpen = NULL;

// used for drawing of piano
pianoRoll::pianoRollKeyTypes pianoRoll::prKeyOrder[] =
{
	PR_WHITE_KEY_SMALL, PR_BLACK_KEY, PR_WHITE_KEY_BIG, PR_BLACK_KEY,
	PR_WHITE_KEY_SMALL, PR_WHITE_KEY_SMALL, PR_BLACK_KEY, PR_WHITE_KEY_BIG,
	PR_BLACK_KEY, PR_WHITE_KEY_BIG, PR_BLACK_KEY, PR_WHITE_KEY_SMALL
} ;


const int DEFAULT_PR_PPT = KEY_LINE_HEIGHT * DEFAULT_STEPS_PER_TACT;


pianoRoll::pianoRoll( void ) :
	m_zoomingModel( new comboBoxModel( /* this */ ) ),
	m_quantizeModel( new comboBoxModel( /* this */ ) ),
	m_noteLenModel( new comboBoxModel( /* this */ ) ),
	m_pattern( NULL ),
	m_currentPosition(),
	m_recording( FALSE ),
	m_currentNote( NULL ),
	m_action( NONE ),
	m_moveStartKey( 0 ),
	m_moveStartTick( 0 ),
	m_notesEditHeight( 100 ),
	m_ppt( DEFAULT_PR_PPT ),
	m_lenOfNewNotes( midiTime( 0, DefaultTicksPerTact/4 ) ),
	m_startKey( INITIAL_START_KEY ),
	m_lastKey( 0 ),
	m_editMode( DRAW ),
	m_scrollBack( FALSE )
{
	// init pixmaps
	if( s_whiteKeySmallPm == NULL )
	{
		s_whiteKeySmallPm = new QPixmap( embed::getIconPixmap(
						"pr_white_key_small" ) );
	}
	if( s_whiteKeyBigPm == NULL )
	{
		s_whiteKeyBigPm = new QPixmap( embed::getIconPixmap(
							"pr_white_key_big" ) );
	}
	if( s_blackKeyPm == NULL )
	{
		s_blackKeyPm = new QPixmap( embed::getIconPixmap(
							"pr_black_key" ) );
	}
	if( s_toolDraw == NULL )
	{
		s_toolDraw = new QPixmap( embed::getIconPixmap(
							"edit_draw" ) );
	}
	if( s_toolErase == NULL )
	{
		s_toolErase= new QPixmap( embed::getIconPixmap(
							"edit_erase" ) );
	}
	if( s_toolSelect == NULL )
	{
		s_toolSelect = new QPixmap( embed::getIconPixmap(
							"edit_select" ) );
	}
	if( s_toolMove == NULL )
	{
		s_toolMove = new QPixmap( embed::getIconPixmap(
							"edit_move" ) );
	}
	if( s_toolOpen == NULL )
	{
		s_toolOpen = new QPixmap( embed::getIconPixmap(
							"automation" ) );
	}


	// add time-line
	m_timeLine = new timeLine( WHITE_KEY_WIDTH, 32, m_ppt,
					engine::getSong()->getPlayPos(
						song::Mode_PlayPattern ),
						m_currentPosition, this );
	connect( this, SIGNAL( positionChanged( const midiTime & ) ),
		m_timeLine, SLOT( updatePosition( const midiTime & ) ) );
	connect( m_timeLine, SIGNAL( positionChanged( const midiTime & ) ),
			this, SLOT( updatePosition( const midiTime & ) ) );


	m_toolBar = new QWidget( this );
	m_toolBar->setFixedHeight( 32 );
	m_toolBar->move( 0, 0 );
	m_toolBar->setAutoFillBackground( TRUE );
	QPalette pal;
	pal.setBrush( m_toolBar->backgroundRole(),
					embed::getIconPixmap( "toolbar_bg" ) );
	m_toolBar->setPalette( pal );

	QHBoxLayout * tb_layout = new QHBoxLayout( m_toolBar );
	tb_layout->setMargin( 0 );
	tb_layout->setSpacing( 0 );


	// init control-buttons at the top

	m_playButton = new toolButton( embed::getIconPixmap( "play" ),
				tr( "Play/pause current pattern (Space)" ),
					this, SLOT( play() ), m_toolBar );

	m_recordButton = new toolButton( embed::getIconPixmap( "record" ),
			tr( "Record notes from MIDI-device/channel-piano" ),
					this, SLOT( record() ), m_toolBar );

	m_stopButton = new toolButton( embed::getIconPixmap( "stop" ),
				tr( "Stop playing of current pattern (Space)" ),
					this, SLOT( stop() ), m_toolBar );

	m_playButton->setWhatsThis(
		tr( "Click here, if you want to play the current pattern. "
			"This is useful while editing it. The pattern is "
			"automatically looped when its end is reached." ) );
	m_recordButton->setWhatsThis(
		tr( "Click here, if you want to record notes from a MIDI-"
			"device or the virtual test-piano of the according "
			"channel-window to the current pattern. When recording "
			"all notes you play will be written to this pattern "
			"and you can play and edit them afterwards." ) );
	m_stopButton->setWhatsThis(
		tr( "Click here, if you want to stop playing of current "
			"pattern." ) );



	removeSelection();

	// init scrollbars
	m_leftRightScroll = new QScrollBar( Qt::Horizontal, this );
	m_leftRightScroll->setSingleStep( 1 );
	connect( m_leftRightScroll, SIGNAL( valueChanged( int ) ), this,
						SLOT( horScrolled( int ) ) );

	m_topBottomScroll = new QScrollBar( Qt::Vertical, this );
	m_topBottomScroll->setSingleStep( 1 );
	m_topBottomScroll->setPageStep( 20 );
	connect( m_topBottomScroll, SIGNAL( valueChanged( int ) ), this,
						SLOT( verScrolled( int ) ) );

	// init edit-buttons at the top
	m_drawButton = new toolButton( embed::getIconPixmap( "edit_draw" ),
					tr( "Draw mode (Shift+D)" ),
					this, SLOT( drawButtonToggled() ),
					m_toolBar );
	m_drawButton->setCheckable( TRUE );
	m_drawButton->setChecked( TRUE );

	m_eraseButton = new toolButton( embed::getIconPixmap( "edit_erase" ),
					tr( "Erase mode (Shift+E)" ),
					this, SLOT( eraseButtonToggled() ),
					m_toolBar );
	m_eraseButton->setCheckable( TRUE );

	m_selectButton = new toolButton( embed::getIconPixmap(
							"edit_select" ),
					tr( "Select mode (Shift+S)" ),
					this, SLOT( selectButtonToggled() ),
					m_toolBar );
	m_selectButton->setCheckable( TRUE );

	m_moveButton = new toolButton( embed::getIconPixmap( "edit_move" ),
					tr( "Move selection mode (Shift+M)" ),
					this, SLOT( moveButtonToggled() ),
					m_toolBar );
	m_moveButton->setCheckable( TRUE );

	QButtonGroup * tool_button_group = new QButtonGroup( this );
	tool_button_group->addButton( m_drawButton );
	tool_button_group->addButton( m_eraseButton );
	tool_button_group->addButton( m_selectButton );
	tool_button_group->addButton( m_moveButton );
	tool_button_group->setExclusive( TRUE );

	m_drawButton->setWhatsThis(
		tr( "If you click here, draw-mode will be activated. In this "
			"mode you can add, resize and move single notes. This "
			"is the default-mode which is used most of the time. "
			"You can also press 'Shift+D' on your keyboard to "
			"activate this mode." ) );
	m_eraseButton->setWhatsThis(
		tr( "If you click here, erase-mode will be activated. In this "
			"mode you can erase single notes. You can also press "
			"'Shift+E' on your keyboard to activate this mode." ) );
	m_selectButton->setWhatsThis(
		tr( "If you click here, select-mode will be activated. "
			"In this mode you can select notes. This is neccessary "
			"if you want to cut, copy, paste, delete or move "
			"notes. You can also press 'Shift+S' on your keyboard "
			"to activate this mode." ) );
	m_moveButton->setWhatsThis(
		tr( "If you click here, move-mode will be activated. In this "
			"mode you can move the notes you selected in select-"
			"mode. You can also press 'Shift+M' on your keyboard "
			"to activate this mode." ) );

	m_cutButton = new toolButton( embed::getIconPixmap( "edit_cut" ),
					tr( "Cut selected notes (Ctrl+X)" ),
					this, SLOT( cutSelectedNotes() ),
					m_toolBar );

	m_copyButton = new toolButton( embed::getIconPixmap( "edit_copy" ),
					tr( "Copy selected notes (Ctrl+C)" ),
					this, SLOT( copySelectedNotes() ),
					m_toolBar );

	m_pasteButton = new toolButton( embed::getIconPixmap( "edit_paste" ),
					tr( "Paste notes from clipboard "
								"(Ctrl+V)" ),
					this, SLOT( pasteNotes() ),
					m_toolBar );

	m_cutButton->setWhatsThis(
		tr( "If you click here, selected notes will be cut into the "
			"clipboard. You can paste them anywhere in any pattern "
			"by clicking on the paste-button." ) );
	m_copyButton->setWhatsThis(
		tr( "If you click here, selected notes will be copied into the "
			"clipboard. You can paste them anywhere in any pattern "
			"by clicking on the paste-button." ) );
	m_pasteButton->setWhatsThis(
		tr( "If you click here, the notes from the clipboard will be "
			"pasted at the first visible tact." ) );



	QLabel * zoom_lbl = new QLabel( m_toolBar );
	zoom_lbl->setPixmap( embed::getIconPixmap( "zoom" ) );

	// setup zooming-stuff
	for( int i = 0; i < 6; ++i )
	{
		m_zoomingModel->addItem( QString::number( 25 << i ) + "%" );
	}
	m_zoomingModel->setValue( m_zoomingModel->findText( "100%" ) );
	connect( m_zoomingModel, SIGNAL( dataChanged() ),
					this, SLOT( zoomingChanged() ) );
	m_zoomingComboBox = new comboBox( m_toolBar );
	m_zoomingComboBox->setModel( m_zoomingModel );
	m_zoomingComboBox->setFixedSize( 80, 22 );


	// setup quantize-stuff
	QLabel * quantize_lbl = new QLabel( m_toolBar );
	quantize_lbl->setPixmap( embed::getIconPixmap( "quantize" ) );

	for( int i = 0; i < 7; ++i )
	{
		m_quantizeModel->addItem( "1/" + QString::number( 1 << i ) );
	}
	m_quantizeModel->addItem( "1/192" );
	m_quantizeModel->setValue( m_quantizeModel->findText( "1/16" ) );
	m_quantizeComboBox = new comboBox( m_toolBar );
	m_quantizeComboBox->setModel( m_quantizeModel );
	m_quantizeComboBox->setFixedSize( 60, 22 );


	// setup note-len-stuff
	QLabel * note_len_lbl = new QLabel( m_toolBar );
	note_len_lbl->setPixmap( embed::getIconPixmap( "note" ) );

	m_noteLenModel->addItem( tr( "Last note" ), new QPixmap(
					embed::getIconPixmap( "edit_draw" ) ) );
	const QString pixmaps[] = { "whole", "half", "quarter", "eighth",
						"sixteenth", "thirtysecond" } ;
	for( int i = 0; i < 6; ++i )
	{
		m_noteLenModel->addItem( "1/" + QString::number( 1 << i ),
				new QPixmap( embed::getIconPixmap(
					QString( "note_" + pixmaps[i] ).
						toAscii().constData() ) ) );
	}
	m_noteLenModel->addItem( "1/192" );
	m_noteLenModel->setValue( 0 );
	m_noteLenComboBox = new comboBox( m_toolBar );
	m_noteLenComboBox->setModel( m_noteLenModel );
	m_noteLenComboBox->setFixedSize( 120, 22 );


	tb_layout->addSpacing( 5 );
	tb_layout->addWidget( m_playButton );
	tb_layout->addWidget( m_recordButton );
	tb_layout->addWidget( m_stopButton );
	tb_layout->addSpacing( 10 );
	tb_layout->addWidget( m_drawButton );
	tb_layout->addWidget( m_eraseButton );
	tb_layout->addWidget( m_selectButton );
	tb_layout->addWidget( m_moveButton );
	tb_layout->addSpacing( 10 );
	tb_layout->addWidget( m_cutButton );
	tb_layout->addWidget( m_copyButton );
	tb_layout->addWidget( m_pasteButton );
	tb_layout->addSpacing( 10 );
	m_timeLine->addToolButtons( m_toolBar );
	tb_layout->addSpacing( 15 );
	tb_layout->addWidget( zoom_lbl );
	tb_layout->addSpacing( 4 );
	tb_layout->addWidget( m_zoomingComboBox );
	tb_layout->addSpacing( 10 );
	tb_layout->addWidget( quantize_lbl );
	tb_layout->addSpacing( 4 );
	tb_layout->addWidget( m_quantizeComboBox );
	tb_layout->addSpacing( 10 );
	tb_layout->addWidget( note_len_lbl );
	tb_layout->addSpacing( 4 );
	tb_layout->addWidget( m_noteLenComboBox );
	tb_layout->addStretch();

	// setup our actual window
	setFocusPolicy( Qt::StrongFocus );
	setFocus();
	setWindowIcon( embed::getIconPixmap( "piano" ) );
	setCurrentPattern( NULL );

	setMouseTracking( TRUE );

	// add us to workspace
	if( engine::getMainWindow()->workspace() )
	{
		engine::getMainWindow()->workspace()->addSubWindow( this );
		parentWidget()->resize( INITIAL_PIANOROLL_WIDTH,
						INITIAL_PIANOROLL_HEIGHT );
		parentWidget()->hide();
	}
	else
	{
		resize( INITIAL_PIANOROLL_WIDTH, INITIAL_PIANOROLL_HEIGHT );
		hide();
	}
}




pianoRoll::~pianoRoll()
{
}


void pianoRoll::setCurrentPattern( pattern * _new_pattern )
{
	m_pattern = _new_pattern;
	m_currentPosition = 0;
	m_startKey = INITIAL_START_KEY;

	if( validPattern() == FALSE )
	{
		//resizeEvent( NULL );
		setWindowTitle( tr( "Piano-Roll - no pattern" ) );

		update();
		return;
	}


	const noteVector & notes = m_pattern->notes();
	int central_key = 0;
	if( notes.empty() == FALSE )
	{
		// determine the central key so that we can scroll to it
		int total_notes = 0;
		for( noteVector::const_iterator it = notes.begin();
						it != notes.end(); ++it )
		{
			if( ( *it )->length() > 0 )
			{
				central_key += ( *it )->key();
				++total_notes;
			}
		}

		if( total_notes > 0 )
		{
			central_key = central_key / total_notes -
					( KeysPerOctave * NumOctaves -
						m_totalKeysToScroll ) / 2;
			m_startKey = tLimit( central_key, 0,
						NumOctaves * KeysPerOctave );
		}
	}
	// resizeEvent() does the rest for us (scrolling, range-checking
	// of start-notes and so on...)
	resizeEvent( NULL );

	// remove all connections to other channel-tracks
	disconnect( this, SLOT( recordNote( const note & ) ) );

	// and now connect to noteDone()-signal of channel so that
	// we receive note-off-events from it's midi-port for recording it
	connect( m_pattern->getInstrumentTrack(),
			SIGNAL( noteDone( const note & ) ),
			this, SLOT( recordNote( const note & ) ) );

	setWindowTitle( tr( "Piano-Roll - %1" ).arg( m_pattern->name() ) );

	update();
}




void pianoRoll::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	mainWindow::saveWidgetState( this, _this );
}




void pianoRoll::loadSettings( const QDomElement & _this )
{
	mainWindow::restoreWidgetState( this, _this );
}




inline void pianoRoll::drawNoteRect( QPainter & _p, int _x, int _y,
					int _width, const bool _is_selected,
						const bool _is_step_note )
{
	++_x;
	++_y;
	_width -= 2;

	if( _width <= 0 )
	{
		_width = 2;
	}

	QColor current_color( 0xFF, 0xB0, 0x00 );
	if( _is_step_note == TRUE )
	{
		current_color.setRgb( 0, 255, 0 );
	}
	else if( _is_selected == TRUE )
	{
		current_color.setRgb( 0x00, 0x40, 0xC0 );
	}
	_p.fillRect( _x, _y, _width, KEY_LINE_HEIGHT - 2, current_color );

	_p.setPen( QColor( 0xFF, 0xDF, 0x20 ) );
	_p.drawLine( _x, _y, _x + _width, _y );
	_p.drawLine( _x, _y, _x, _y + KEY_LINE_HEIGHT - 2 );

	_p.setPen( QColor( 0xFF, 0x9F, 0x00 ) );
	_p.drawLine( _x + _width, _y, _x + _width, _y + KEY_LINE_HEIGHT - 2 );
	_p.drawLine( _x, _y + KEY_LINE_HEIGHT - 2, _x + _width,
						_y + KEY_LINE_HEIGHT - 2 );

	_p.setPen( QColor( 0xFF, 0xFF, 0x40 ) );
	if( _width > 2 )
	{
		_p.drawLine( _x + _width - 3, _y + 2, _x + _width - 3,
						_y + KEY_LINE_HEIGHT - 4 );
	}
	_p.drawLine( _x + _width - 1, _y + 2, _x + _width - 1,
						_y + KEY_LINE_HEIGHT - 4 );
	_p.drawLine( _x + _width - 2, _y + 2, _x + _width - 2,
						_y + KEY_LINE_HEIGHT - 4 );
}




inline void pianoRoll::drawDetuningInfo( QPainter & _p, note * _n, Uint16 _x,
								Uint16 _y )
{
	Uint16 middle_y = _y + KEY_LINE_HEIGHT / 2;
	_p.setPen( QColor( 0xFF, 0xDF, 0x20 ) );

	timeMap & map = _n->detuning()->getAutomationPattern()->getTimeMap();
	timeMap::iterator it = map.end();
	do
	{
		--it;
		Sint32 pos_ticks = -it.key();
		if( pos_ticks > _n->length() )
		{
			break;
		}
		Uint16 pos_x = _x + pos_ticks * m_ppt / DefaultTicksPerTact;

		const int level = it.value();

		Uint16 pos_y = middle_y - level * KEY_LINE_HEIGHT / 10;

		_p.drawLine( pos_x - 1, pos_y, pos_x + 1, pos_y );
		_p.drawLine( pos_x, pos_y - 1, pos_x, pos_y + 1 );
	} while( it != map.begin() );
}




void pianoRoll::removeSelection( void )
{
	m_selectStartTick = 0;
	m_selectedTick = 0;
	m_selectStartKey = 0;
	m_selectedKeys = 0;
}




void pianoRoll::closeEvent( QCloseEvent * _ce )
{
	QApplication::restoreOverrideCursor();
	if( parentWidget() )
	{
		parentWidget()->hide();
	}
	else
	{
		hide();
	}
	_ce->ignore();
}




void pianoRoll::keyPressEvent( QKeyEvent * _ke )
{
	if( validPattern() && _ke->modifiers() == Qt::NoModifier )
	{
		int key_num = pianoView::getKeyFromScancode(
						_ke->nativeScanCode() ) +
				( DefaultOctave - 1 ) * KeysPerOctave;

		if( _ke->isAutoRepeat() == FALSE && key_num > -1 )
		{
			m_pattern->getInstrumentTrack()->
					getPiano()->handleKeyPress( key_num );
		}
	}
	switch( _ke->key() )
	{
		case Qt::Key_Up:
			m_topBottomScroll->setValue(
					m_topBottomScroll->value() - 1 );
			break;

		case Qt::Key_Down:
			m_topBottomScroll->setValue(
					m_topBottomScroll->value() + 1 );
			break;

		case Qt::Key_Left:
		{
			if( ( m_timeLine->pos() -= 16 ) < 0 )
			{
				m_timeLine->pos().setTact( 0 );
				m_timeLine->pos().setTicks( 0 );
			}
			m_timeLine->updatePosition();
			break;
		}
		case Qt::Key_Right:
		{
			m_timeLine->pos() += 16;
			m_timeLine->updatePosition();
			break;
		}

		case Qt::Key_C:
			if( _ke->modifiers() & Qt::ControlModifier )
			{
				copySelectedNotes();
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_X:
			if( _ke->modifiers() & Qt::ControlModifier )
			{
				cutSelectedNotes();
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_V:
			if( _ke->modifiers() & Qt::ControlModifier )
			{
				pasteNotes();
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_A:
			if( _ke->modifiers() & Qt::ControlModifier )
			{
				m_selectButton->setChecked( TRUE );
				selectAll();
				update();
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_D:
			if( _ke->modifiers() & Qt::ShiftModifier )
			{
				m_drawButton->setChecked( TRUE );
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_E:
			if( _ke->modifiers() & Qt::ShiftModifier )
			{
				m_eraseButton->setChecked( TRUE );
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_S:
			if( _ke->modifiers() & Qt::ShiftModifier )
			{
				m_selectButton->setChecked( TRUE );
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_M:
			if( _ke->modifiers() & Qt::ShiftModifier )
			{
				m_moveButton->setChecked( TRUE );
			}
			else
			{
				_ke->ignore();
			}
			break;

		case Qt::Key_Delete:
			deleteSelectedNotes();
			break;

		case Qt::Key_Space:
			if( engine::getSong()->playing() )
			{
				stop();
			}
			else
			{
				play();
			}
			break;

		case Qt::Key_Home:
			m_timeLine->pos().setTact( 0 );
			m_timeLine->pos().setTicks( 0 );
			m_timeLine->updatePosition();
			break;

		case Qt::Key_Control:
			if( mouseOverNote() )
			{
				m_editMode = OPEN;
				QApplication::changeOverrideCursor(
						QCursor( Qt::ArrowCursor ) );
				update();
			}

		default:
			_ke->ignore();
			break;
	}
}




void pianoRoll::keyReleaseEvent( QKeyEvent * _ke )
{
	if( validPattern() && _ke->modifiers() == Qt::NoModifier )
	{
		int key_num = pianoView::getKeyFromScancode(
						_ke->nativeScanCode() ) +
				( DefaultOctave - 1 ) * KeysPerOctave;

		if( _ke->isAutoRepeat() == FALSE && key_num > -1 )
		{
			m_pattern->getInstrumentTrack()->
					getPiano()->handleKeyRelease( key_num );
		}
	}
	switch( _ke->key() )
	{
		case Qt::Key_Control:
			if( m_editMode == OPEN )
			{
				m_editMode = DRAW;
				update();
			}
	}
	_ke->ignore();
}




void pianoRoll::leaveEvent( QEvent * _e )
{
	while( QApplication::overrideCursor() != NULL )
	{
		QApplication::restoreOverrideCursor();
	}

	QWidget::leaveEvent( _e );
}




void pianoRoll::mousePressEvent( QMouseEvent * _me )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	if( m_editMode == OPEN && noteUnderMouse() )
	{
		noteUnderMouse()->editDetuningPattern();
		return;
	}

	if( _me->y() > PR_TOP_MARGIN )
	{
		bool play_note = TRUE;
		volume vol = DefaultVolume;

		bool edit_note = ( _me->y() > height() -
					PR_BOTTOM_MARGIN - m_notesEditHeight );

		int key_num = getKey( _me->y() );

		int x = _me->x();

		if( x > WHITE_KEY_WIDTH )
		{
			// set, move or resize note

			x -= WHITE_KEY_WIDTH;

			// get tick in which the user clicked
			int pos_ticks = x * DefaultTicksPerTact / m_ppt +
							m_currentPosition;


			// get note-vector of current pattern
			const noteVector & notes = m_pattern->notes();

			// will be our iterator in the following loop
			noteVector::const_iterator it = notes.begin();

			// loop through whole note-vector...
			while( it != notes.end() )
			{
				midiTime len = ( *it )->length();
				if( len < 0 )
				{
					len = 4;
				}
				// and check whether the user clicked on an
				// existing note or an edit-line
				if( pos_ticks >= ( *it )->pos() &&
						len > 0 &&
					(
					( edit_note == FALSE &&
					pos_ticks <= ( *it )->pos() + len &&
					( *it )->key() == key_num )
					||
					( edit_note == TRUE &&
					pos_ticks <= ( *it )->pos() +
							NE_LINE_WIDTH *
							DefaultTicksPerTact /
								m_ppt )
					)
					)
				{
					break;
				}
				++it;
			}

			// first check whether the user clicked in note-edit-
			// area
			if( edit_note == TRUE )
			{
				if( it != notes.end() )
				{
					vol = 2 * ( -_me->y() + height() -
							PR_BOTTOM_MARGIN );
					( *it )->setVolume( vol );
					m_currentNote = *it;
					m_action = CHANGE_NOTE_VOLUME;
					key_num = ( *it )->key();
				}
				else
				{
					play_note = FALSE;
				}
			}
			// left button??
			else if( _me->button() == Qt::LeftButton &&
							m_editMode == DRAW )
			{
				// did it reach end of vector because
				// there's no note??
				if( it == notes.end() )
				{
					m_pattern->setType(
						pattern::MelodyPattern );

					// then set new note
					// +32 to quanitize the note correctly when placing notes with
					// the mouse.  We do this here instead of in note.quantized
					// because live notes should still be quantized at the half.
					midiTime note_pos( pos_ticks - (quantization() / 2) );
					midiTime note_len( newNoteLen() );
		
					note new_note( note_len, note_pos, key_num );

					note * created_new_note =
						m_pattern->addNote( new_note );

					// reset it so that it can be used for
					// ops (move, resize) after this
					// code-block
					it = notes.begin();
					while( it != notes.end() &&
						*it != created_new_note )
					{
						++it;
					}
				}

				m_currentNote = *it;

				// clicked at the "tail" of the note?
				if( pos_ticks*m_ppt/DefaultTicksPerTact >
					( m_currentNote->pos() +
					m_currentNote->length() )*m_ppt/
							DefaultTicksPerTact -
							RESIZE_AREA_WIDTH &&
						m_currentNote->length() > 0 )
				{
					// then resize the note
					m_action = RESIZE_NOTE;

					// set resize-cursor
					QCursor c( Qt::SizeHorCursor );
					QApplication::setOverrideCursor( c );
					play_note = FALSE;
				}
				else
				{
					// otherwise move it
					m_action = MOVE_NOTE;
					int aligned_x = (int)( (float)( (
							m_currentNote->pos() -
							m_currentPosition ) *
							m_ppt ) /
							DefaultTicksPerTact );
					m_moveXOffset = x - aligned_x - 1;
					// set move-cursor
					QCursor c( Qt::SizeAllCursor );
					QApplication::setOverrideCursor( c );
				}

				engine::getSong()->setModified();
			}
			else if( ( _me->button() == Qt::RightButton &&
							m_editMode == DRAW ) ||
					m_editMode == ERASE )
			{
				// erase single note

				play_note = FALSE;
				if( it != notes.end() )
				{
					if( ( *it )->length() > 0 )
					{
						m_pattern->removeNote( *it );
					}
					else
					{
						( *it )->setLength( 0 );
						m_pattern->dataChanged();
					}
					engine::getSong()->setModified();
				}
			}
			else if( _me->button() == Qt::LeftButton &&
							m_editMode == SELECT )
			{

				// select an area of notes

				m_selectStartTick = pos_ticks;
				m_selectedTick = 0;
				m_selectStartKey = key_num;
				m_selectedKeys = 1;
				m_action = SELECT_NOTES;

				play_note = FALSE;
			}
			else if( _me->button() == Qt::RightButton &&
							m_editMode == SELECT )
			{
				// when clicking right in select-move, we
				// switch to move-mode
				m_moveButton->setChecked( TRUE );
				play_note = FALSE;

			}
			else if( _me->button() == Qt::LeftButton &&
							m_editMode == MOVE )
			{

				// move selection (including selected notes)

				// save position where move-process began
				m_moveStartTick = pos_ticks;
				m_moveStartKey = key_num;

				m_action = MOVE_SELECTION;

				play_note = FALSE;
				engine::getSong()->setModified();
			}
			else if( _me->button() == Qt::RightButton &&
							m_editMode == MOVE )
			{
				// when clicking right in select-move, we
				// switch to draw-mode
				m_drawButton->setChecked( TRUE );
				play_note = FALSE;
			}

			update();
		}

		// was there an action where should be played the note?
		if( play_note == TRUE && m_recording == FALSE &&
					engine::getSong()->playing() == FALSE )
		{
			m_lastKey = key_num;
			m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( NOTE_ON, 0, key_num,
							vol * 127 / 100 ),
								midiTime() );
		}
	}
}




void pianoRoll::mouseReleaseEvent( QMouseEvent * _me )
{
	if( validPattern() == TRUE )
	{
		if( m_action == CHANGE_NOTE_VOLUME && m_currentNote != NULL )
		{
			m_pattern->getInstrumentTrack()->processInEvent(
				midiEvent( NOTE_OFF, 0,
					m_currentNote->key(), 0 ), midiTime() );
		}
		else
		{
			m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( NOTE_OFF, 0, m_lastKey, 0 ),
								midiTime() );
		}
	}

	m_currentNote = NULL;

	m_action = NONE;

	if( m_editMode == DRAW )
	{
		QApplication::restoreOverrideCursor();
	}
}




void pianoRoll::mouseMoveEvent( QMouseEvent * _me )
{
	if( validPattern() == FALSE )
	{
		update();
		return;
	}

	if( _me->y() > PR_TOP_MARGIN )
	{
		bool edit_note = ( _me->y() > height() -
					PR_BOTTOM_MARGIN - m_notesEditHeight );

		int key_num = getKey( _me->y() );
		int x = _me->x();

		// is the calculated key different from current key?
		// (could be the user just moved the cursor one pixel up/down
		// but is still on the same key)
		if( key_num != m_lastKey &&
			m_action != CHANGE_NOTE_VOLUME &&
			m_action != MOVE_SELECTION &&
			edit_note == FALSE &&
			_me->buttons() & Qt::LeftButton )
		{
			m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( NOTE_OFF, 0, m_lastKey, 0 ),
								midiTime() );
			if( _me->buttons() & Qt::LeftButton &&
				m_action != RESIZE_NOTE &&
				m_action != SELECT_NOTES &&
				m_action != MOVE_SELECTION &&
				m_recording == FALSE &&
				engine::getSong()->playing() == FALSE )
			{
				m_lastKey = key_num;
				m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( NOTE_ON, 0, key_num,
						DefaultVolume * 127 / 100 ),
								midiTime() );
			}
		}
		if( _me->x() <= WHITE_KEY_WIDTH )
		{
			update();
			return;
		}
		x -= WHITE_KEY_WIDTH;

		// Volume Bars
		if( ( edit_note == TRUE || m_action == CHANGE_NOTE_VOLUME ) &&
				_me->buttons() & Qt::LeftButton )
		{
			// Use nearest-note when changing volume so the bars can
			// be "scribbled"
			int pos_ticks = ( x * DefaultTicksPerTact ) / m_ppt +
						m_currentPosition;

			// get note-vector of current pattern
			const noteVector & notes = m_pattern->notes();

			// will be our iterator in the following loop
			noteVector::const_iterator it = notes.begin();
			
			note * shortNote = NULL;
			
			// Max "snap length" 1/8 note on either side
			int shortDistance = DefaultTicksPerTact/8;

			// loop through vector to find nearest note
			while( it != notes.end() )
			{
				int tmp = abs( pos_ticks - (int)( (*it)->pos() ) );
			
				if( tmp < shortDistance && (*it)->length().getTicks() > 0 )
				{
					shortDistance = tmp;
					shortNote = *it ;

				}
				++it;

			}

			if( shortNote != m_currentNote && 
					engine::getSong()->playing() == FALSE )
			{
				if( m_currentNote != NULL ) {
					m_pattern->getInstrumentTrack()->processInEvent(
						midiEvent( NOTE_OFF, 0,
						m_currentNote->key(), 0 ), midiTime() );
				}
				
				if( shortNote != NULL ) {
					m_lastKey = shortNote->key();

					m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( NOTE_ON, 0,
					shortNote->key(), shortNote->getVolume() ), midiTime() );
				}
			}
			m_currentNote = shortNote;

			if( m_currentNote != NULL ) {
				volume vol = tLimit<int>( 2 * ( -_me->y() +
								height() -
							PR_BOTTOM_MARGIN ),
								MinVolume,
								MaxVolume );
				m_currentNote->setVolume( vol );
				m_pattern->dataChanged();
				m_pattern->getInstrumentTrack()->processInEvent(
					midiEvent( KEY_PRESSURE, 0, m_lastKey,
							vol * 127 / 100 ),
								midiTime() );
			}
		}
		else if( m_currentNote != NULL &&
			_me->buttons() & Qt::LeftButton && m_editMode == DRAW )
		{
			int key_num = getKey( _me->y() );
			
			if( m_action == MOVE_NOTE )
			{
				x -= m_moveXOffset;
			}
			int pos_ticks = x * DefaultTicksPerTact / m_ppt +
							m_currentPosition;
			if( m_action == MOVE_NOTE )
			{
				// moving note
				if( pos_ticks < 0 )
				{
					pos_ticks = 0;
				}
				m_currentNote->setPos( midiTime(
							pos_ticks ) );
				m_currentNote->setKey( key_num );

				// we moved the note so the note has to be
				// moved properly according to new starting-
				// time in the note-array of pattern
				m_currentNote = m_pattern->rearrangeNote(
								m_currentNote );
			}
			else
			{
				// resizing note
				int ticks_diff = pos_ticks -
							m_currentNote->pos();
				if( ticks_diff <= 0 )
				{
					ticks_diff = 1;
				}
				m_currentNote->setLength( midiTime(
							ticks_diff ) );
				m_currentNote->quantizeLength( quantization() );
				m_lenOfNewNotes = m_currentNote->length();
				m_pattern->dataChanged();
			}

			engine::getSong()->setModified();

		}
		else if( _me->buttons() == Qt::NoButton && m_editMode == DRAW )
		{
			// set move- or resize-cursor

			// get tick in which the cursor is posated
			int pos_ticks = ( x * DefaultTicksPerTact ) / m_ppt +
							m_currentPosition;

			// get note-vector of current pattern
			const noteVector & notes = m_pattern->notes();

			// will be our iterator in the following loop
			noteVector::const_iterator it = notes.begin();

			// loop through whole note-vector...
			while( it != notes.end() )
			{
				// and check whether the cursor is over an
				// existing note
				if( pos_ticks >= ( *it )->pos() &&
			    		pos_ticks <= ( *it )->pos() +
							( *it )->length() &&
					( *it )->key() == key_num &&
					( *it )->length() > 0 )
				{
					break;
				}
				++it;
			}

			// did it reach end of vector because there's
			// no note??
			if( it != notes.end() )
			{
				if( _me->modifiers() & Qt::ControlModifier )
				{
					m_editMode = OPEN;
					QApplication::changeOverrideCursor(
						QCursor( Qt::ArrowCursor ) );
				}
				// cursor at the "tail" of the note?
				else if( ( *it )->length() > 0 &&
					pos_ticks*m_ppt/DefaultTicksPerTact >
						( ( *it )->pos() +
						( *it )->length() )*m_ppt/
							DefaultTicksPerTact -
							RESIZE_AREA_WIDTH )
				{
					if( QApplication::overrideCursor() )
					{
	if( QApplication::overrideCursor()->shape() != Qt::SizeHorCursor )
						{
				while( QApplication::overrideCursor() != NULL )
				{
					QApplication::restoreOverrideCursor();
				}

				QCursor c( Qt::SizeHorCursor );
				QApplication::setOverrideCursor( c );
						}
					}
					else
					{
						QCursor c( Qt::SizeHorCursor );
						QApplication::setOverrideCursor(
									c );
					}
				}
				else
				{
					if( QApplication::overrideCursor() )
					{
	if( QApplication::overrideCursor()->shape() != Qt::SizeAllCursor )
						{
				while( QApplication::overrideCursor() != NULL )
				{
					QApplication::restoreOverrideCursor();
				}

						QCursor c( Qt::SizeAllCursor );
						QApplication::setOverrideCursor(
									c );
						}
					}
					else
					{
						QCursor c( Qt::SizeAllCursor );
						QApplication::setOverrideCursor(
									c );
					}
				}
			}
			else
			{
				// the cursor is over no note, so restore cursor
				while( QApplication::overrideCursor() != NULL )
				{
					QApplication::restoreOverrideCursor();
				}
			}
		}
		else if( _me->buttons() & Qt::LeftButton &&
						m_editMode == SELECT &&
						m_action == SELECT_NOTES )
		{

			// change size of selection

			if( x < 0 && m_currentPosition > 0 )
			{
				x = 0;
				QCursor::setPos( mapToGlobal( QPoint(
								WHITE_KEY_WIDTH,
								_me->y() ) ) );
				if( m_currentPosition >= 4 )
				{
					m_leftRightScroll->setValue(
							m_currentPosition - 4 );
				}
				else
				{
					m_leftRightScroll->setValue( 0 );
				}
			}
			else if( x > width() - WHITE_KEY_WIDTH )
			{
				x = width() - WHITE_KEY_WIDTH;
				QCursor::setPos( mapToGlobal( QPoint( width(),
								_me->y() ) ) );
				m_leftRightScroll->setValue( m_currentPosition +
									4 );
			}

			// get tick in which the cursor is posated
			int pos_ticks = x * DefaultTicksPerTact / m_ppt +
							m_currentPosition;

			m_selectedTick = pos_ticks -
							m_selectStartTick;
			if( (int) m_selectStartTick + m_selectedTick <
									0 )
			{
				m_selectedTick = -static_cast<int>(
							m_selectStartTick );
			}
			m_selectedKeys = key_num - m_selectStartKey;
			if( key_num <= m_selectStartKey )
			{
				--m_selectedKeys;
			}
		}
		else if( _me->buttons() & Qt::LeftButton &&
					m_editMode == MOVE &&
					m_action == MOVE_SELECTION )
		{
			// move selection + selected notes

			// do horizontal move-stuff
			int pos_ticks = x * DefaultTicksPerTact / m_ppt +
							m_currentPosition;
			int ticks_diff = pos_ticks -
							m_moveStartTick;
			if( m_selectedTick > 0 )
			{
				if( (int) m_selectStartTick +
							ticks_diff < 0 )
				{
					ticks_diff = -m_selectStartTick;
				}
			}
			else
			{
				if( (int) m_selectStartTick +
					m_selectedTick + ticks_diff <
									0 )
				{
					ticks_diff = -(
							m_selectStartTick +
							m_selectedTick );
				}
			}
			m_selectStartTick += ticks_diff;

			int tact_diff = ticks_diff / DefaultTicksPerTact;
			ticks_diff = ticks_diff % DefaultTicksPerTact;


			// do vertical move-stuff
			int key_diff = key_num - m_moveStartKey;

			if( m_selectedKeys > 0 )
			{
				if( m_selectStartKey + key_diff < -1 )
				{
					key_diff = -m_selectStartKey - 1;
				}
				else if( m_selectStartKey + m_selectedKeys +
						key_diff >= KeysPerOctave *
								NumOctaves )
				{
					key_diff = KeysPerOctave * NumOctaves -
							( m_selectStartKey +
							m_selectedKeys ) - 1;
				}
			}
			else
			{
				if( m_selectStartKey + m_selectedKeys +
								key_diff < -1 )
				{
					key_diff = -( m_selectStartKey +
							m_selectedKeys ) - 1;
				}
				else if( m_selectStartKey + key_diff >=
						KeysPerOctave * NumOctaves )
				{
					key_diff = KeysPerOctave * NumOctaves -
							m_selectStartKey - 1;
				}
			}
			m_selectStartKey += key_diff;


			for( noteVector::iterator it =
						m_selNotesForMove.begin();
				it != m_selNotesForMove.end(); ++it )
			{
				int note_tact = ( *it )->pos().getTact() +
								tact_diff;
				int note_ticks = ( *it )->pos().getTicks() +
								ticks_diff;
				// ensure note_ticks range
				if( note_ticks >> 6 )
				{
					note_tact += note_ticks >> 6;
					note_ticks &= 63;
				}
				midiTime new_note_pos( note_tact,
							note_ticks );
				( *it )->setPos( new_note_pos );
				( *it )->setKey( ( *it )->key() + key_diff );
				*it = m_pattern->rearrangeNote( *it, FALSE );
			}

			m_moveStartTick = pos_ticks;
			m_moveStartKey = key_num;
		}
		else if( m_editMode == OPEN && !( mouseOverNote()
				&& _me->modifiers() & Qt::ControlModifier ) )
		{
			m_editMode = DRAW;
		}
	}
	else
	{
		if( _me->buttons() & Qt::LeftButton &&
					m_editMode == SELECT &&
					m_action == SELECT_NOTES )
		{

			int x = _me->x() - WHITE_KEY_WIDTH;
			if( x < 0 && m_currentPosition > 0 )
			{
				x = 0;
				QCursor::setPos( mapToGlobal( QPoint(
							WHITE_KEY_WIDTH,
							_me->y() ) ) );
				if( m_currentPosition >= 4 )
				{
					m_leftRightScroll->setValue(
							m_currentPosition - 4 );
				}
				else
				{
					m_leftRightScroll->setValue( 0 );
				}
			}
			else if( x > width() - WHITE_KEY_WIDTH )
			{
				x = width() - WHITE_KEY_WIDTH;
				QCursor::setPos( mapToGlobal( QPoint( width(),
							_me->y() ) ) );
				m_leftRightScroll->setValue( m_currentPosition +
									4 );
			}

			// get tick in which the cursor is posated
			int pos_ticks = x * DefaultTicksPerTact / m_ppt +
							m_currentPosition;

			m_selectedTick = pos_ticks -
							m_selectStartTick;
			if( (int) m_selectStartTick + m_selectedTick <
									0 )
			{
				m_selectedTick = -static_cast<int>(
							m_selectStartTick );
			}


			int key_num = getKey( _me->y() );
			int visible_keys = ( height() - PR_TOP_MARGIN -
						PR_BOTTOM_MARGIN -
						m_notesEditHeight ) /
							KEY_LINE_HEIGHT + 2;
			const int s_key = m_startKey - 1;

			if( key_num <= s_key )
			{
				QCursor::setPos( mapToGlobal( QPoint( _me->x(),
							height() -
							PR_BOTTOM_MARGIN -
							m_notesEditHeight ) ) );
				m_topBottomScroll->setValue(
					m_topBottomScroll->value() + 1 );
				key_num = s_key;
			}
			else if( key_num >= s_key + visible_keys )
			{
				QCursor::setPos( mapToGlobal( QPoint( _me->x(),
							PR_TOP_MARGIN ) ) );
				m_topBottomScroll->setValue(
					m_topBottomScroll->value() - 1 );
				key_num = s_key + visible_keys;
			}

			m_selectedKeys = key_num - m_selectStartKey;
			if( key_num <= m_selectStartKey )
			{
				--m_selectedKeys;
			}
		}
		QApplication::restoreOverrideCursor();
	}

	update();
}




void pianoRoll::paintEvent( QPaintEvent * _pe )
{
	QStyleOption opt;
	opt.initFrom( this );
	QPainter p( this );
	style()->drawPrimitive( QStyle::PE_Widget, &opt, &p, this );

	// set font-size to 8
	p.setFont( pointSize<8>( p.font() ) );

	// y_offset is used to align the piano-keys on the key-lines
	int y_offset = 0;

	// calculate y_offset according to first key
	switch( prKeyOrder[m_startKey % KeysPerOctave] )
	{
		case PR_BLACK_KEY: y_offset = KEY_LINE_HEIGHT/4; break;
		case PR_WHITE_KEY_BIG: y_offset = KEY_LINE_HEIGHT/2; break;
		case PR_WHITE_KEY_SMALL:
			if( prKeyOrder[( ( m_startKey + 1 ) %
					KeysPerOctave)] != PR_BLACK_KEY )
			{
				y_offset = KEY_LINE_HEIGHT / 2;
			}
			break;
	}

	// start drawing at the bottom
	int key_line_y = height() - PR_BOTTOM_MARGIN - m_notesEditHeight - 1;
	// used for aligning black-keys later
	int first_white_key_height = WHITE_KEY_SMALL_HEIGHT;
	// key-counter - only needed for finding out whether the processed 
	// key is the first one
	int keys_processed = 0;

	int key = m_startKey;

	// draw all white keys...
	for( int y = key_line_y + 1 + y_offset; y > PR_TOP_MARGIN;
			key_line_y -= KEY_LINE_HEIGHT, ++keys_processed )
	{
		// check for white key that is only half visible on the 
		// bottom of piano-roll
		if( keys_processed == 0 &&
			prKeyOrder[m_startKey % KeysPerOctave] ==
								PR_BLACK_KEY )
		{
			// draw it!
			p.drawPixmap( PIANO_X, y - WHITE_KEY_SMALL_HEIGHT,
							*s_whiteKeySmallPm );
			// update y-pos
			y -= WHITE_KEY_SMALL_HEIGHT / 2;
			// move first black key down (we didn't draw whole 
			// white key so black key needs to be lifted down)
			// (default for first_white_key_height = 
			// WHITE_KEY_SMALL_HEIGHT, so WHITE_KEY_SMALL_HEIGHT/2
			// is smaller)
			first_white_key_height = WHITE_KEY_SMALL_HEIGHT / 2;
		}
		// check whether to draw a big or a small white key
		if( prKeyOrder[key % KeysPerOctave] == PR_WHITE_KEY_SMALL )
		{
			// draw a small one...
			p.drawPixmap( PIANO_X, y - WHITE_KEY_SMALL_HEIGHT,
							*s_whiteKeySmallPm );
			// update y-pos
			y -= WHITE_KEY_SMALL_HEIGHT;

		}
		else if( prKeyOrder[key % KeysPerOctave] ==
							PR_WHITE_KEY_BIG )
		{
			// draw a big one...
			p.drawPixmap( PIANO_X, y-WHITE_KEY_BIG_HEIGHT,
							*s_whiteKeyBigPm );
			// if a big white key has been the first key,
			// black keys needs to be lifted up
			if( keys_processed == 0 )
			{
				first_white_key_height = WHITE_KEY_BIG_HEIGHT;
			}
			// update y-pos
			y -= WHITE_KEY_BIG_HEIGHT;
		}
		// label C-keys...
		if( static_cast<Keys>( key % KeysPerOctave ) == Key_C )
		{
			p.setPen( QColor( 240, 240, 240 ) );
			p.drawText( C_KEY_LABEL_X + 1, y+14, "C" +
					QString::number( static_cast<int>( key /
							KeysPerOctave ) ) );
			p.setPen( QColor( 0, 0, 0 ) );
			p.drawText( C_KEY_LABEL_X, y + 13, "C" +
					QString::number( static_cast<int>( key /
							KeysPerOctave ) ) );
			p.setPen( QColor( 0x4F, 0x4F, 0x4F ) );
		}
		else
		{
			p.setPen( QColor( 0x3F, 0x3F, 0x3F ) );
		}
		// draw key-line
		p.drawLine( WHITE_KEY_WIDTH, key_line_y, width(), key_line_y );
		++key;
	}

	// reset all values, because now we're going to draw all black keys
	key = m_startKey;
	keys_processed = 0;
	int white_cnt = 0;

	// and go!
	for( int y = height() - PR_BOTTOM_MARGIN - m_notesEditHeight + y_offset;
					y > PR_TOP_MARGIN; ++keys_processed )
	{
		// check for black key that is only half visible on the bottom
		// of piano-roll
		if( keys_processed == 0
		    // current key may not be a black one
		    && prKeyOrder[key % KeysPerOctave] != PR_BLACK_KEY
		    // but the previous one must be black (we must check this
		    // because there might be two white keys (E-F)
		    && prKeyOrder[( key - 1 ) % KeysPerOctave] ==
								PR_BLACK_KEY )
		{
			// draw the black key!
			p.drawPixmap( PIANO_X, y - BLACK_KEY_HEIGHT / 2,
								*s_blackKeyPm );
			// is the one after the start-note a black key??
			if( prKeyOrder[( key + 1 ) % KeysPerOctave] !=
								PR_BLACK_KEY )
			{
				// no, then move it up!
				y -= KEY_LINE_HEIGHT / 2;
			}
		}
		// current key black?
		if( prKeyOrder[key % KeysPerOctave] == PR_BLACK_KEY )
		{
			// then draw it (calculation of y very complicated,
			// but that's the only working solution, sorry...)
			p.drawPixmap( PIANO_X, y - ( first_white_key_height -
					WHITE_KEY_SMALL_HEIGHT ) -
					WHITE_KEY_SMALL_HEIGHT/2 - 1 -
					BLACK_KEY_HEIGHT, *s_blackKeyPm );

			// update y-pos
			y -= WHITE_KEY_BIG_HEIGHT;
			// reset white-counter
			white_cnt = 0;
		}
		else
		{
			// simple workaround for increasing x if there were 
			// two white keys (e.g. between E and F)
			++white_cnt;
			if( white_cnt > 1 )
			{
				y -= WHITE_KEY_BIG_HEIGHT/2;
			}
		}

		++key;
	}


	// erase the area below the piano, because there might be keys that 
	// should be only half-visible
	p.fillRect( QRect( 0, height() - PR_BOTTOM_MARGIN - m_notesEditHeight,
			WHITE_KEY_WIDTH, m_notesEditHeight ),
			QColor( 0, 0, 0 ) );


	// set clipping area, because we are not allowed to paint over
	// keyboard...
	p.setClipRect( WHITE_KEY_WIDTH, PR_TOP_MARGIN,
				width() - WHITE_KEY_WIDTH,
				height() - PR_TOP_MARGIN - PR_BOTTOM_MARGIN );

	// draw vertical raster
	int tact_16th = m_currentPosition / ( DefaultTicksPerTact / 16 );
	const int offset = ( m_currentPosition % (DefaultTicksPerTact/16) ) *
					m_ppt / DefaultTicksPerTact;

	for( int x = WHITE_KEY_WIDTH - offset; x < width();
			x += m_ppt / DEFAULT_STEPS_PER_TACT, ++tact_16th )
	{
		if( x >= WHITE_KEY_WIDTH )
		{
			// every tact-start needs to be a bright line
			if( tact_16th % 16 == 0 )
			{
	 			p.setPen( QColor( 0x7F, 0x7F, 0x7F ) );
			}
			// normal line
			else if( tact_16th % 4 == 0 )
			{
				p.setPen( QColor( 0x5F, 0x5F, 0x5F ) );
			}
			// weak line
			else
			{
				p.setPen( QColor( 0x3F, 0x3F, 0x3F ) );
			}
			p.drawLine( x, PR_TOP_MARGIN, x, height() -
							PR_BOTTOM_MARGIN );
		}
	}



	// following code draws all notes in visible area + volume-lines

	// setup selection-vars
	int sel_pos_start = m_selectStartTick;
	int sel_pos_end = m_selectStartTick+m_selectedTick;
	if( sel_pos_start > sel_pos_end )
	{
		qSwap<int>( sel_pos_start, sel_pos_end );
	}

	int sel_key_start = m_selectStartKey - m_startKey + 1;
	int sel_key_end = sel_key_start + m_selectedKeys;
	if( sel_key_start > sel_key_end )
	{
		qSwap<int>( sel_key_start, sel_key_end );
	}

	int y_base = height() - PR_BOTTOM_MARGIN - m_notesEditHeight - 1;
	if( validPattern() == TRUE )
	{
		p.setClipRect( WHITE_KEY_WIDTH, PR_TOP_MARGIN,
				width() - WHITE_KEY_WIDTH,
				height() - PR_TOP_MARGIN );

		const noteVector & notes = m_pattern->notes();

		const int visible_keys = ( height() - PR_TOP_MARGIN -
					PR_BOTTOM_MARGIN - m_notesEditHeight ) /
							KEY_LINE_HEIGHT + 2;

		for( noteVector::const_iterator it = notes.begin();
						it != notes.end(); ++it )
		{
			Sint32 len_ticks = ( *it )->length();

			if( len_ticks == 0 )
			{
				continue;
			}
			else if( len_ticks < 0 )
			{
				len_ticks = 4;
			}

			const int key = ( *it )->key() - m_startKey + 1;

			Sint32 pos_ticks = ( *it )->pos();

			int note_width = len_ticks * m_ppt /
							DefaultTicksPerTact;
			const int x = ( pos_ticks - m_currentPosition ) *
						m_ppt / DefaultTicksPerTact;
			// skip this note if not in visible area at all
			if( !( x + note_width >= 0 &&
					x <= width() - WHITE_KEY_WIDTH ) )
			{
				continue;
			}

			// is the note in visible area?
			if( key > 0 && key <= visible_keys )
			{
				bool is_selected = FALSE;
				// if we're in move-mode, we may only draw notes
				// in selected area, that have originally been
				// selected and not notes that are now in
				// selection because the user moved it...
				if( m_editMode == MOVE )
				{
					if( qFind( m_selNotesForMove.begin(),
							m_selNotesForMove.end(),
							*it ) !=
						m_selNotesForMove.end() )
					{
						is_selected = TRUE;
					}
				}
				else if( key > sel_key_start &&
					key <= sel_key_end &&
					pos_ticks >= sel_pos_start &&
					pos_ticks + len_ticks <=
								sel_pos_end )
				{
					is_selected = TRUE;
				}

				// we've done and checked all, lets draw the
				// note
				drawNoteRect( p, x + WHITE_KEY_WIDTH,
						y_base - key * KEY_LINE_HEIGHT,
								note_width,
								is_selected,
							( *it )->length() < 0 );
			}
			// draw volume-line of note
			QColor color = QColor::fromHsv( 120, 221, 
					tMin(255, 60 + ( *it )->getVolume() ) );
			p.setPen( QPen( color,
							NE_LINE_WIDTH ) );
			p.drawLine( x + WHITE_KEY_WIDTH,
					height() - PR_BOTTOM_MARGIN -
						( *it )->getVolume() / 2,
					x + WHITE_KEY_WIDTH,
					height() - PR_BOTTOM_MARGIN );
			p.drawLine( x + WHITE_KEY_WIDTH-1,
					height() - PR_BOTTOM_MARGIN -
						( *it )->getVolume() / 2+1,
					x + WHITE_KEY_WIDTH + 1,
					height()-PR_BOTTOM_MARGIN -
						(*it)->getVolume() / 2 + 1 );
			if( ( *it )->hasDetuningInfo() )
			{
				drawDetuningInfo( p, *it,
					x + WHITE_KEY_WIDTH,
					y_base - key * KEY_LINE_HEIGHT );
			}
		}
	}
	else
	{
		QFont f = p.font();
		f.setBold( TRUE );
		p.setFont( pointSize<14>( f ) );
		p.setPen( QColor( 0, 255, 0 ) );
		p.drawText( WHITE_KEY_WIDTH + 20, PR_TOP_MARGIN + 40,
				tr( "Please open a pattern by double-clicking "
								"on it!" ) );
	}

	p.setClipRect( WHITE_KEY_WIDTH, PR_TOP_MARGIN, width() -
				WHITE_KEY_WIDTH, height() - PR_TOP_MARGIN -
					m_notesEditHeight - PR_BOTTOM_MARGIN );

	// now draw selection-frame
	int x = ( ( sel_pos_start - m_currentPosition ) * m_ppt ) /
							DefaultTicksPerTact;
	int w = ( ( ( sel_pos_end - m_currentPosition ) * m_ppt ) /
						DefaultTicksPerTact ) - x;
	int y = (int) y_base - sel_key_start * KEY_LINE_HEIGHT;
	int h = (int) y_base - sel_key_end * KEY_LINE_HEIGHT - y;
	p.setPen( QColor( 0, 64, 192 ) );
	p.drawRect( x + WHITE_KEY_WIDTH, y, w, h );

	// TODO: Get this out of paint event
	int l = ( validPattern() == TRUE )? (int) m_pattern->length() : 0;

	// reset scroll-range
	if( m_leftRightScroll->maximum() != l )
	{
		m_leftRightScroll->setRange( 0, l );
		m_leftRightScroll->setPageStep( l );
	}

	// horizontal line for the key under the cursor
	if( validPattern() == TRUE )
	{
		int key_num = getKey( mapFromGlobal( QCursor::pos() ).y() );
		p.fillRect( 10, height() + 3 - PR_BOTTOM_MARGIN -
				m_notesEditHeight - KEY_LINE_HEIGHT *
					( key_num - m_startKey + 1 ),
				width() - 10, KEY_LINE_HEIGHT - 7,
							QColor( 64, 64, 64 ) );
	}

	const QPixmap * cursor = NULL;
	// draw current edit-mode-icon below the cursor
	switch( m_editMode )
	{
		case DRAW: cursor = s_toolDraw; break;
		case ERASE: cursor = s_toolErase; break;
		case SELECT: cursor = s_toolSelect; break;
		case MOVE: cursor = s_toolMove; break;
		case OPEN: cursor = s_toolOpen; break;
	}
	p.drawPixmap( mapFromGlobal( QCursor::pos() ) + QPoint( 8, 8 ),
								*cursor );
}




// responsible for moving/resizing scrollbars after window-resizing
void pianoRoll::resizeEvent( QResizeEvent * )
{
	m_leftRightScroll->setGeometry( WHITE_KEY_WIDTH, height() -
								SCROLLBAR_SIZE,
					width()-WHITE_KEY_WIDTH,
							SCROLLBAR_SIZE );
	m_topBottomScroll->setGeometry( width() - SCROLLBAR_SIZE, PR_TOP_MARGIN,
						SCROLLBAR_SIZE,
						height() - PR_TOP_MARGIN -
						SCROLLBAR_SIZE );

	int total_pixels = OCTAVE_HEIGHT * NumOctaves - ( height() -
					PR_TOP_MARGIN - PR_BOTTOM_MARGIN -
							m_notesEditHeight );
	m_totalKeysToScroll = total_pixels * KeysPerOctave / OCTAVE_HEIGHT;

	m_topBottomScroll->setRange( 0, m_totalKeysToScroll );

	if( m_startKey > m_totalKeysToScroll )
	{
		m_startKey = m_totalKeysToScroll;
	}
	m_topBottomScroll->setValue( m_totalKeysToScroll - m_startKey );

	engine::getSong()->getPlayPos( song::Mode_PlayPattern
					).m_timeLine->setFixedWidth( width() );
	m_toolBar->setFixedWidth( width() );
	update();
}




void pianoRoll::wheelEvent( QWheelEvent * _we )
{
	_we->accept();
	if( engine::getMainWindow()->isCtrlPressed() == TRUE )
	{
		if( _we->delta() > 0 )
		{
			m_ppt = tMin( m_ppt * 2, KEY_LINE_HEIGHT *
						DEFAULT_STEPS_PER_TACT * 8 );
		}
		else if( m_ppt >= 72 )
		{
			m_ppt /= 2;
		}
		// update combobox with zooming-factor
		m_zoomingModel->setValue(
				m_zoomingModel->findText( QString::number(
					static_cast<int>( m_ppt * 100 /
						DEFAULT_PR_PPT ) ) +"%" ) );
		// update timeline
		m_timeLine->setPixelsPerTact( m_ppt );
		update();
	}
	else if( engine::getMainWindow()->isShiftPressed() )
	{
		m_leftRightScroll->setValue( m_leftRightScroll->value() -
							_we->delta() * 2 / 15 );
	}
	else
	{
		m_topBottomScroll->setValue( m_topBottomScroll->value() -
							_we->delta() / 30 );
	}
}




int pianoRoll::getKey( int _y ) const
{
	int key_line_y = height() - PR_BOTTOM_MARGIN - m_notesEditHeight - 1;
	// pressed key on piano
	int key_num = ( key_line_y - _y ) / KEY_LINE_HEIGHT;
	key_num += m_startKey;

	// some range-checking-stuff
	if( key_num < 0 )
	{
		key_num = 0;
	}

	if( key_num >= KeysPerOctave * NumOctaves )
	{
		key_num = KeysPerOctave * NumOctaves - 1;
	}

	return( key_num );
}




void pianoRoll::play( void )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	if( engine::getSong()->playing() )
	{
		if( engine::getSong()->playMode() != song::Mode_PlayPattern )
		{
			engine::getSong()->stop();
			engine::getSong()->playPattern( m_pattern );
			m_playButton->setIcon( embed::getIconPixmap(
								"pause" ) );
		}
		else
		{
			engine::getSong()->pause();
			m_playButton->setIcon( embed::getIconPixmap( "play" ) );
		}
	}
	else if( engine::getSong()->paused() )
	{
		engine::getSong()->resumeFromPause();
		m_playButton->setIcon( embed::getIconPixmap( "pause" ) );
	}
	else
	{
		m_playButton->setIcon( embed::getIconPixmap( "pause" ) );
		engine::getSong()->playPattern( m_pattern );
	}
}




void pianoRoll::record( void )
{
	if( engine::getSong()->playing() )
	{
		stop();
	}
	if( m_recording == TRUE || validPattern() == FALSE )
	{
		return;
	}

	m_recording = TRUE;
	engine::getSong()->playPattern( m_pattern, FALSE );
}




void pianoRoll::stop( void )
{
	engine::getSong()->stop();
	m_playButton->setIcon( embed::getIconPixmap( "play" ) );
	m_playButton->update();
	m_recording = FALSE;
	m_scrollBack = TRUE;
}




void pianoRoll::recordNote( const note & _n )
{
	if( m_recording == TRUE && validPattern() == TRUE )
	{
		note n( _n.length(), engine::getSong()->getPlayPos(
				song::Mode_PlayPattern ) - _n.length(),
				_n.key(), _n.getVolume(), _n.getPanning() );
		n.quantizeLength( quantization() );
		m_pattern->addNote( n );
		update();
		engine::getSong()->setModified();
	}
}




void pianoRoll::horScrolled( int _new_pos )
{
	m_currentPosition = _new_pos;
	emit positionChanged( m_currentPosition );
	update();
}




void pianoRoll::verScrolled( int _new_pos )
{
	// revert value
	m_startKey = m_totalKeysToScroll - _new_pos;

	update();
}




void pianoRoll::drawButtonToggled( void )
{
	m_editMode = DRAW;
	removeSelection();
	update();
}




void pianoRoll::eraseButtonToggled( void )
{
	m_editMode = ERASE;
	removeSelection();
	update();
}




void pianoRoll::selectButtonToggled( void )
{
	m_editMode = SELECT;
	removeSelection();
	update();
}




void pianoRoll::moveButtonToggled( void )
{
	m_editMode = MOVE;
	m_selNotesForMove.clear();
	getSelectedNotes( m_selNotesForMove );
	update();
}




void pianoRoll::selectAll( void )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	const noteVector & notes = m_pattern->notes();

	// if first_time = TRUE, we HAVE to set the vars for select
	bool first_time = TRUE;

	for( noteVector::const_iterator it = notes.begin(); it != notes.end();
									++it )
	{
		Uint32 len_ticks = ( *it )->length();

		if( len_ticks > 0 )
		{
			const int key = ( *it )->key();

			Uint32 pos_ticks = ( *it )->pos();
			if( key <= m_selectStartKey || first_time )
			{
				// if we move start-key down, we have to add 
				// the difference between old and new start-key
				// to m_selectedKeys, otherwise the selection
				// is just moved down...
				m_selectedKeys += m_selectStartKey
								- ( key - 1 );
				m_selectStartKey = key - 1;
			}
			if( key >= m_selectedKeys+m_selectStartKey ||
								first_time )
			{
				m_selectedKeys = key - m_selectStartKey;
			}
			if( pos_ticks < m_selectStartTick ||
								first_time )
			{
				m_selectStartTick = pos_ticks;
			}
			if( pos_ticks + len_ticks >
				m_selectStartTick + m_selectedTick ||
								first_time )
			{
				m_selectedTick = pos_ticks +
							len_ticks -
							m_selectStartTick;
			}
			first_time = FALSE;
		}
	}
}




// returns vector with pointers to all selected notes
void pianoRoll::getSelectedNotes( noteVector & _selected_notes )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	int sel_pos_start = m_selectStartTick;
	int sel_pos_end = sel_pos_start + m_selectedTick;
	if( sel_pos_start > sel_pos_end )
	{
		qSwap<int>( sel_pos_start, sel_pos_end );
	}

	int sel_key_start = m_selectStartKey;
	int sel_key_end = sel_key_start + m_selectedKeys;
	if( sel_key_start > sel_key_end )
	{
		qSwap<int>( sel_key_start, sel_key_end );
	}

	const noteVector & notes = m_pattern->notes();

	for( noteVector::const_iterator it = notes.begin(); it != notes.end();
									++it )
	{
		Sint32 len_ticks = ( *it )->length();

		if( len_ticks > 0 )
		{
			int key = ( *it )->key();
			Sint32 pos_ticks = ( *it )->pos();

			if( key > sel_key_start &&
				key <= sel_key_end &&
				pos_ticks >= sel_pos_start &&
				pos_ticks+len_ticks <= sel_pos_end )
			{
				_selected_notes.push_back( *it );
			}
		}
	}
}




void pianoRoll::copy_to_clipboard( const noteVector & _notes ) const
{
	multimediaProject mmp( multimediaProject::ClipboardData );
	QDomElement note_list = mmp.createElement( "note-list" );
	mmp.content().appendChild( note_list );

	midiTime start_pos( _notes.front()->pos().getTact(), 0 );
	for( noteVector::const_iterator it = _notes.begin(); it != _notes.end();
									++it )
	{
		note clip_note( **it );
		clip_note.setPos( clip_note.pos( start_pos ) );
		clip_note.saveState( mmp, note_list );
	}

	QMimeData * clip_content = new QMimeData;
	clip_content->setData( clipboard::mimeType(), mmp.toString().toUtf8() );
	QApplication::clipboard()->setMimeData( clip_content,
							QClipboard::Clipboard );
}




void pianoRoll::copySelectedNotes( void )
{
	noteVector selected_notes;
	getSelectedNotes( selected_notes );

	if( selected_notes.empty() == FALSE )
	{
		copy_to_clipboard( selected_notes );

		textFloat::displayMessage( tr( "Notes copied" ),
				tr( "All selected notes were copied to the "
								"clipboard." ),
				embed::getIconPixmap( "edit_copy" ), 2000 );
	}
}




void pianoRoll::cutSelectedNotes( void )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	noteVector selected_notes;
	getSelectedNotes( selected_notes );

	if( selected_notes.empty() == FALSE )
	{
		copy_to_clipboard( selected_notes );

		engine::getSong()->setModified();

		for( noteVector::iterator it = selected_notes.begin();
					it != selected_notes.end(); ++it )
		{
			// note (the memory of it) is also deleted by
			// pattern::removeNote(...) so we don't have to do that
			m_pattern->removeNote( *it );
		}
	}

	update();
	engine::getSongEditor()->update();
}




void pianoRoll::pasteNotes( void )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	QString value = QApplication::clipboard()
				->mimeData( QClipboard::Clipboard )
						->data( clipboard::mimeType() );

	if( !value.isEmpty() )
	{
		multimediaProject mmp( value, FALSE );

		QDomNodeList list = mmp.elementsByTagName(
							note::classNodeName() );
		for( int i = 0; !list.item( i ).isNull(); ++i )
		{
			note cur_note;
			cur_note.restoreState( list.item( i ).toElement() );
			cur_note.setPos( cur_note.pos() + m_currentPosition );
			m_pattern->addNote( cur_note );
		}

		// we only have to do the following lines if we pasted at
		// least one note...
		engine::getSong()->setModified();
		update();
		engine::getSongEditor()->update();
	}
}




void pianoRoll::deleteSelectedNotes( void )
{
	if( validPattern() == FALSE )
	{
		return;
	}

	noteVector selected_notes;
	getSelectedNotes( selected_notes );

	const bool update_after_delete = !selected_notes.empty();

	while( selected_notes.empty() == FALSE )
	{
		m_pattern->removeNote( selected_notes.front() );
		selected_notes.erase( selected_notes.begin() );
	}

	if( update_after_delete == TRUE )
	{
		engine::getSong()->setModified();
		update();
		engine::getSongEditor()->update();
	}
}




void pianoRoll::updatePosition( const midiTime & _t )
{
	if( ( engine::getSong()->playing() &&
			engine::getSong()->playMode() ==
					song::Mode_PlayPattern ) ||
							m_scrollBack == TRUE )
	{
		const int w = width() - WHITE_KEY_WIDTH;
		if( _t > m_currentPosition + w * DefaultTicksPerTact / m_ppt )
		{
			m_leftRightScroll->setValue( _t.getTact() *
							DefaultTicksPerTact );
		}
		else if( _t < m_currentPosition )
		{
			midiTime t = tMax( _t - w * DefaultTicksPerTact *
					DefaultTicksPerTact / m_ppt, 0 );
			m_leftRightScroll->setValue( t.getTact() *
							DefaultTicksPerTact );
		}
		m_scrollBack = FALSE;
	}
}




void pianoRoll::zoomingChanged( void )
{
	const QString & zfac = m_zoomingModel->currentText();
	m_ppt = zfac.left( zfac.length() - 1 ).toInt() * DEFAULT_PR_PPT / 100;
#ifdef LMMS_DEBUG
	assert( m_ppt > 0 );
#endif
	m_timeLine->setPixelsPerTact( m_ppt );
	update();

}




int pianoRoll::quantization( void ) const
{
	return( DefaultTicksPerTact / m_quantizeModel->currentText().right(
				m_quantizeModel->currentText().length() -
								2 ).toInt() );
}




midiTime pianoRoll::newNoteLen( void ) const
{
	if( m_noteLenModel->value() == 0 )
	{
		return( m_lenOfNewNotes );
	}
	return( DefaultTicksPerTact / m_noteLenModel->currentText().right(
				m_noteLenModel->currentText().length() -
								2 ).toInt() );
}




bool pianoRoll::mouseOverNote( void )
{
	return( validPattern()
		&& noteIteratorUnderMouse() != m_pattern->notes().end() );
}




note * pianoRoll::noteUnderMouse( void )
{
	return( *noteIteratorUnderMouse() );
}




noteVector::const_iterator pianoRoll::noteIteratorUnderMouse( void )
{
	QPoint pos = mapFromGlobal( QCursor::pos() );

	// get note-vector of current pattern
	const noteVector & notes = m_pattern->notes();

	if( pos.x() <= WHITE_KEY_WIDTH || pos.x() > width() - SCROLLBAR_SIZE
		|| pos.y() < PR_TOP_MARGIN
		|| pos.y() > height() - PR_BOTTOM_MARGIN - m_notesEditHeight )
	{
		return( notes.end() );
	}

	int key_num = getKey( pos.y() );
	int pos_ticks = ( pos.x() - WHITE_KEY_WIDTH ) * DefaultTicksPerTact /
						m_ppt + m_currentPosition;

	// will be our iterator in the following loop
	noteVector::const_iterator it = notes.begin();

	// loop through whole note-vector...
	while( it != notes.end() )
	{
		// and check whether the cursor is over an
		// existing note
		if( pos_ticks >= ( *it )->pos() &&
	    		pos_ticks <= ( *it )->pos() + ( *it )->length() &&
			( *it )->key() == key_num && ( *it )->length() > 0 )
		{
			break;
		}
		++it;
	}

	return( it );
}



#ifdef BUILD_LINUX
bool pianoRoll::x11Event( XEvent * _xe )
{
	if( validPattern() )
	{
/*		return( m_pattern->getInstrumentTrack()->getPianoWidget()
							->x11Event( _xe ) );*/
	}
	return( FALSE );
}
#endif



#include "piano_roll.moc"


#endif
