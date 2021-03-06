//--
// This file is part of Sonic Pi: http://sonic-pi.net
// Full project source: https://github.com/samaaron/sonic-pi
// License: https://github.com/samaaron/sonic-pi/blob/master/LICENSE.md
//
// Copyright (C) 2016 by Adrian Cheater
// All rights reserved.
//
// Permission is granted for use, copying, modification, and
// distribution of modified versions of this work as long as this
// notice is included.
//++

#include "scope.h"

#include <QPaintEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <QIcon>
#include <QTimer>
#include <QPainter>
#include <QDebug>
#include <qwt_text_label.h>
#include <cmath>

ScopePanel::ScopePanel( const QString& name, double* sample_x, double* sample_y, int num_samples, QWidget* parent ) : QWidget(parent), name(name), plot(QwtText(name),this) 
{
#if QWT_VERSION >= 0x60100
  plot_curve.setPaintAttribute( QwtPlotCurve::PaintAttribute::FilterPoints );
#endif

  plot_curve.setRawSamples( sample_x, sample_y, num_samples );
  setXRange( 0, num_samples, false );
  setYRange( -1, 1, true );
  setPen(QPen(QColor("deeppink"), 2));

  plot_curve.attach(&plot);

  QSizePolicy sp(QSizePolicy::MinimumExpanding,QSizePolicy::Expanding);
  plot.setSizePolicy(sp);

  QVBoxLayout* layout = new QVBoxLayout();
  layout->addWidget(&plot);
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);
  setLayout(layout);
}

const QString& ScopePanel::getName() { return name; }

void ScopePanel::setYRange( float min, float max, bool showLabel )
{
  plot.setAxisScale( QwtPlot::Axis::yLeft, min, max );
  plot.enableAxis( QwtPlot::Axis::yLeft, showLabel );
  defaultShowY = showLabel;
}

void ScopePanel::setXRange( float min, float max, bool showLabel )
{
  plot.setAxisScale( QwtPlot::Axis::xBottom, min, max );
  plot.enableAxis( QwtPlot::Axis::xBottom, showLabel );
  defaultShowX = showLabel;
}

void ScopePanel::setPen( QPen pen )
{
  plot_curve.setPen( pen );
}

ScopePanel::~ScopePanel()
{
}

bool ScopePanel::setAxesVisible(bool b)
{
  plot.enableAxis(QwtPlot::Axis::yLeft,b && defaultShowY );
  plot.enableAxis(QwtPlot::Axis::xBottom,b && defaultShowX );
  if( b )
  {
    plot.setTitle(QwtText(name));
  } else
  {
    plot.setTitle(QwtText(""));
  }
  return b;
}

void ScopePanel::refresh( )
{
  if( !plot.isVisible() ) return;
  plot.replot();
}

Scope::Scope( QWidget* parent ) : QWidget(parent), paused( false ), emptyFrames(0)
{
  //lissajous("Lissajous", sample[0]+(4096-1024), sample[1]+(4096-1024), 1024, this ), left("Left",sample_x,sample[0],4096,this), right("Right",sample_x,sample[1],4096, this)
  panels.push_back( std::shared_ptr<ScopePanel>(new ScopePanel("Lissajous", sample[0]+(4096-1024), sample[1]+(4096-1024), 1024, this ) ) );
  panels.push_back( std::shared_ptr<ScopePanel>(new ScopePanel("Left",sample_x,sample[0],4096,this) ) );
  panels.push_back( std::shared_ptr<ScopePanel>(new ScopePanel("Right",sample_x,sample[1],4096, this) ) );
  panels[0]->setPen(QPen(QColor("deeppink"), 1));
  panels[0]->setXRange( -1, 1, true );

  for( unsigned int i = 0; i < 4096; ++i ) sample_x[i] = i;
  QTimer *scopeTimer = new QTimer(this);
  connect(scopeTimer, SIGNAL(timeout()), this, SLOT(refreshScope()));
  scopeTimer->start(20);

  QVBoxLayout* layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0,0,0,0);
  layout->addWidget(panels[0].get());
  layout->addWidget(panels[1].get());
  layout->addWidget(panels[2].get());
  setLayout(layout);
}

Scope::~Scope()
{
}

std::vector<QString> Scope::getScopeNames() const
{
  std::vector<QString> names;
  for( auto scope : panels )
  {
    names.push_back(scope->getName());
  }
  return names;
}

bool Scope::enableScope( const QString& name, bool on )
{
  for( auto scope : panels )
  {
    if( scope->getName() == name )
    {
      scope->setVisible(on);
      return on;
    }
  }
  return true;
}

bool Scope::setScopeAxes(bool on)
{
  for( auto scope : panels )
  {
    scope->setAxesVisible(on);
  }
  return on;
}

void Scope::togglePause() {
  paused = !paused;
}

void Scope::resetScope()
{
  shmClient.reset(new server_shared_memory_client(4556));
  shmReader = shmClient->get_scope_buffer_reader(0);
}

void Scope::refreshScope() {
  if( paused ) return;
  if( !isVisible() ) return;

  if( !shmReader.valid() )
  {
    resetScope();
  }

  unsigned int frames;
  if( shmReader.pull( frames ) )
  {
    emptyFrames = 0;
    float* data = shmReader.data();
    for( unsigned int j = 0; j < 2; ++j )
    {
      unsigned int offset = shmReader.max_frames() * j;
      for( unsigned int i = 0; i < 4096 - frames; ++i )
      {
        sample[j][i] = sample[j][i+frames];
      }

      for( unsigned int i = 0; i < frames; ++i )
      {
        sample[j][4096-frames+i] = data[i+offset];
      }
    }
    for( auto scope : panels )
    {
      scope->refresh();
    }
  } else
  {
    ++emptyFrames;
    if( emptyFrames > 10 )
    {
      resetScope();
      emptyFrames = 0;
    }
  }
}
