/*
 * eqcontrols.cpp - defination of EqControls class.
 *
 * Copyright (c) 2014 David French <dave/dot/french3/at/googlemail/dot/com>
 *
 * This file is part of LMMS - http://lmms.io
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

#include <QtXml/QDomElement>
#include "EqControls.h"
#include "EqEffect.h"




EqControls::EqControls( EqEffect *effect ) :
	EffectControls( effect ),
	m_effect( effect ),
	m_inGainModel( 1.0, 0.0, 2.0, 0.001, this, tr( "Input gain") ),
	m_outGainModel( 1.0, 0.0, 2.0, 0.001, this, tr( "Output gain" ) ),
	m_lowShelfGainModel(  0.0 , -40, 40, 0.001, this, tr( "Low shelf gain" ) ),
	m_para1GainModel(  0.0 , -40, 40, 0.001, this, tr( "Peak 1 gain" ) ),
	m_para2GainModel(  0.0 , -40, 40, 0.001, this, tr( "Peak 2 gain" ) ),
	m_para3GainModel(   0.0 , -40, 40, 0.001, this, tr( "Peak 3 gain" ) ),
	m_para4GainModel(   0.0 , -40, 40, 0.001, this, tr( "Peak 4 gain" ) ),
	m_highShelfGainModel(  0.0 , -40, 40, 0.001, this, tr( "High Shelf gain" ) ),
	m_hpResModel( 0.707,0.003, 10.0 , 0.001, this, tr( "HP res" )  ),
	m_lowShelfResModel( 1.4,0.0, 10.0 , 0.001, this , tr( "Low Shelf res" ) ),
	m_para1ResModel( 1.4 ,0.55, 10.0 , 0.001, this , tr( "Peak 1 res" ) ),
	m_para2ResModel( 1.4, 0.55, 10.0 , 0.001, this , tr( "Peak 2 res" ) ),
	m_para3ResModel( 1.4, 0.55, 10.0 , 0.001, this , tr( "Peak 3 res" ) ),
	m_para4ResModel( 1.4, 0.55, 10.0 , 0.001, this , tr( "Peak 4 res" ) ),
	m_highShelfResModel( 1.4, 0.001, 10.0 , 0.001, this , tr( "High Shelf res" ) ),
	m_lpResModel( 0.707,0.003, 10.0 , 0.001, this , tr( "LP res" ) ),
	m_hpFeqModel( 31.0, 30.0, 20000, 0.001, this , tr( "HP freq" ) ),
	m_lowShelfFreqModel( 80.0, 25.0, 20000, 0.001, this , tr( "Low Shelf freq" ) ),
	m_para1FreqModel( 120.0, 27.0, 20000, 0.001, this , tr( "Peak 1 freq" ) ),
	m_para2FreqModel( 250.0, 27.0, 20000, 0.001, this, tr( "Peak 2 freq" )  ),
	m_para3FreqModel( 2000.0, 27.0, 20000, 0.001, this , tr( "Peak 3 freq" ) ),
	m_para4FreqModel( 4000.0, 27.0, 20000, 0.001, this , tr( "Peak 4 freq" ) ),
	m_highShelfFreqModel( 12000.0, 27.0, 20000, 0.001, this , tr( "High shelf freq" ) ),
	m_lpFreqModel( 18000.0, 27.0, 20000, 0.001, this , tr( "LP freq" ) ),
	m_hpActiveModel( false, this , tr( "HP active" ) ),
	m_lowShelfActiveModel( false, this , tr( "Low shelf active" ) ),
	m_para1ActiveModel(false, this , tr( "Peak 1 active" ) ),
	m_para2ActiveModel( false, this , tr( "Peak 2 active" ) ),
	m_para3ActiveModel( false, this , tr( "Peak 3 active" ) ),
	m_para4ActiveModel( false, this , tr( "Peak 4 active" ) ),
	m_highShelfActiveModel( false, this , tr( "High shelf active" ) ),
	m_lpActiveModel( false, this , tr( "LP active" ) ),
	m_lp12Model( false, this , tr( "LP 12" ) ),
	m_lp24Model( false, this , tr( "LP 24" ) ),
	m_lp48Model( false, this , tr( "LP 48" ) ),
	m_hp12Model( false, this , tr( "HP 12" ) ),
	m_hp24Model( false, this , tr( "HP 24" ) ),
	m_hp48Model( false, this , tr( "HP 48" ) ),
	m_analyzeModel( true, this , tr( "Analyze enable" ) ),
	m_lpTypeModel( 0,0,2,this, tr( "low pass type") ) ,
	m_hpTypeModel( 0,0,2,this, tr( "high pass type") )
{
	m_hpFeqModel.setScaleLogarithmic( true );
	m_lowShelfFreqModel.setScaleLogarithmic( true );
	m_para1FreqModel.setScaleLogarithmic( true );
	m_para2FreqModel.setScaleLogarithmic( true );
	m_para3FreqModel.setScaleLogarithmic( true );
	m_para4FreqModel.setScaleLogarithmic( true );
	m_highShelfFreqModel.setScaleLogarithmic( true );
	m_lpFreqModel.setScaleLogarithmic( true );
	m_para1GainModel.setScaleLogarithmic( true );
	m_inPeakL = 0;
	m_inPeakR = 0;
	m_outPeakL = 0;
	m_outPeakR = 0;
	m_lowShelfPeakL = 0; m_lowShelfPeakR = 0;
	m_para1PeakL = 0; m_para1PeakR = 0;
	m_para2PeakL = 0; m_para2PeakR = 0;
	m_para3PeakL = 0; m_para3PeakR = 0;
	m_para4PeakL = 0; m_para4PeakR = 0;
	m_highShelfPeakL = 0; m_highShelfPeakR = 0;
	m_inProgress = false;
}




void EqControls::loadSettings( const QDomElement &_this )
{
	m_inGainModel.loadSettings( _this, "Inputgain" );
	m_outGainModel.loadSettings( _this, "Outputgain");
	m_lowShelfGainModel.loadSettings( _this , "Lowshelfgain" );
	m_para1GainModel.loadSettings( _this, "Peak1gain" );
	m_para2GainModel.loadSettings( _this, "Peak2gain" );
	m_para3GainModel.loadSettings( _this, "Peak3gain" );
	m_para4GainModel.loadSettings( _this, "Peak4gain" );
	m_highShelfGainModel.loadSettings( _this , "HighShelfgain");
	m_hpResModel.loadSettings( _this ,"HPres");
	m_lowShelfResModel.loadSettings( _this, "LowShelfres" );
	m_para1ResModel.loadSettings( _this ,"Peak1res" );
	m_para2ResModel.loadSettings( _this ,"Peak2res" );
	m_para3ResModel.loadSettings( _this ,"Peak3res" );
	m_para4ResModel.loadSettings( _this ,"Peak4res" );
	m_highShelfResModel.loadSettings( _this, "HighShelfres" );
	m_lpResModel.loadSettings( _this, "LPres");
	m_hpFeqModel.loadSettings( _this, "HPfreq" );
	m_lowShelfFreqModel.loadSettings( _this, "LowShelffreq" );
	m_para1FreqModel.loadSettings( _this, "Peak1freq" );
	m_para2FreqModel.loadSettings( _this, "Peak2freq" );
	m_para3FreqModel.loadSettings( _this, "Peak3freq" );
	m_para4FreqModel.loadSettings( _this, "Peak4freq" );
	m_highShelfFreqModel.loadSettings( _this, "Highshelffreq" );
	m_lpFreqModel.loadSettings( _this, "LPfreq" );
	m_hpActiveModel.loadSettings( _this, "HPactive" );
	m_lowShelfActiveModel.loadSettings( _this, "Lowshelfactive" );
	m_para1ActiveModel.loadSettings( _this, "Peak1active");
	m_para2ActiveModel.loadSettings( _this, "Peak2active");
	m_para3ActiveModel.loadSettings( _this, "Peak3active");
	m_para4ActiveModel.loadSettings( _this, "Peak4active");
	m_highShelfActiveModel.loadSettings( _this, "Highshelfactive" );
	m_lpActiveModel.loadSettings( _this, "LPactive" );
	m_lp12Model.loadSettings( _this , "LP12" );
	m_lp24Model.loadSettings( _this , "LP24" );
	m_lp48Model.loadSettings( _this , "LP48" );
	m_hp12Model.loadSettings( _this , "HP12" );
	m_hp24Model.loadSettings( _this , "HP24" );
	m_hp48Model.loadSettings( _this , "HP48" );
	m_analyzeModel.loadSettings( _this, "Analyzeenable");
	m_lpTypeModel.loadSettings( _this, "LP" );
	m_hpTypeModel.loadSettings( _this, "HP" );
}




void EqControls::saveSettings( QDomDocument &doc, QDomElement &parent )
{

	m_inGainModel.saveSettings( doc, parent, "Inputgain" );
	m_outGainModel.saveSettings( doc, parent, "Outputgain");
	m_lowShelfGainModel.saveSettings( doc, parent , "Lowshelfgain" );
	m_para1GainModel.saveSettings( doc, parent, "Peak1gain" );
	m_para2GainModel.saveSettings( doc, parent, "Peak2gain" );
	m_para3GainModel.saveSettings( doc, parent, "Peak3gain" );
	m_para4GainModel.saveSettings( doc, parent, "Peak4gain" );
	m_highShelfGainModel.saveSettings( doc, parent, "HighShelfgain");
	m_hpResModel.saveSettings( doc, parent ,"HPres");
	m_lowShelfResModel.saveSettings( doc, parent, "LowShelfres" );
	m_para1ResModel.saveSettings( doc, parent,"Peak1res" );
	m_para2ResModel.saveSettings( doc, parent,"Peak2res" );
	m_para3ResModel.saveSettings( doc, parent,"Peak3res" );
	m_para4ResModel.saveSettings( doc, parent,"Peak4res" );
	m_highShelfResModel.saveSettings( doc, parent, "HighShelfres" );
	m_lpResModel.saveSettings( doc, parent, "LPres");
	m_hpFeqModel.saveSettings( doc, parent, "HPfreq" );
	m_lowShelfFreqModel.saveSettings( doc, parent, "LowShelffreq" );
	m_para1FreqModel.saveSettings( doc, parent, "Peak1freq" );
	m_para2FreqModel.saveSettings( doc, parent, "Peak2freq" );
	m_para3FreqModel.saveSettings( doc, parent, "Peak3freq" );
	m_para4FreqModel.saveSettings( doc, parent, "Peak4freq" );
	m_highShelfFreqModel.saveSettings( doc, parent, "Highshelffreq" );
	m_lpFreqModel.saveSettings( doc, parent, "LPfreq" );
	m_hpActiveModel.saveSettings( doc, parent, "HPactive" );
	m_lowShelfActiveModel.saveSettings( doc, parent, "Lowshelfactive" );
	m_para1ActiveModel.saveSettings( doc, parent, "Peak1active" );
	m_para2ActiveModel.saveSettings( doc, parent, "Peak2active" );
	m_para3ActiveModel.saveSettings( doc, parent, "Peak3active" );
	m_para4ActiveModel.saveSettings( doc, parent, "Peak4active" );
	m_highShelfActiveModel.saveSettings( doc, parent, "Highshelfactive" );
	m_lpActiveModel.saveSettings( doc, parent, "LPactive" );
	m_lp12Model.saveSettings( doc, parent, "LP12" );
	m_lp24Model.saveSettings( doc, parent, "LP24" );
	m_lp48Model.saveSettings( doc, parent, "LP48" );
	m_hp12Model.saveSettings( doc, parent, "HP12" );
	m_hp24Model.saveSettings( doc, parent, "HP24" );
	m_hp48Model.saveSettings( doc, parent, "HP48" );
	m_analyzeModel.saveSettings( doc, parent, "Analyzeenable");
	m_lpTypeModel.saveSettings( doc, parent, "LP" );
	m_hpTypeModel.saveSettings( doc, parent, "HP" );
}
