/* Copyright (c) 2012, STANISLAW ADASZEWSKI
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of STANISLAW ADASZEWSKI nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL STANISLAW ADASZEWSKI BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include <QGraphicsScene>
#include <QFileDialog>
#include <QOpenGLWidget>
#include <QSplitter>

#include "mainwindow.h"

#include "nodegraph/qneblock.h"
#include "nodegraph/qnodeseditor.h"
#include "nodegraph/qneport.h"

#include "nodes/distanceopnode.h"
#include "gridscene.h"

#include "qtimelineanimated.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    m_framebuffer = new QFramebuffer;
    m_framebuffer->resize(800, 600);

    connect(m_framebuffer, SIGNAL(destroyed(QObject*)), this, SLOT(cancelFlipbook()));

    QAction *quitAct = new QAction(tr("&Quit"), this);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));
    connect(quitAct, SIGNAL(triggered()), qApp, SLOT(quit()));

    QAction *loadAct = new QAction(tr("&Load"), this);
    loadAct->setShortcuts(QKeySequence::Open);
    loadAct->setStatusTip(tr("Open a file"));
    connect(loadAct, SIGNAL(triggered()), this, SLOT(loadFile()));

    QAction *saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcuts(QKeySequence::Save);
    saveAct->setStatusTip(tr("Save a file"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveFile()));

    QAction *addAct = new QAction(tr("&Add"), this);
    addAct->setStatusTip(tr("Add a block"));
    connect(addAct, SIGNAL(triggered()), this, SLOT(addBlock()));


    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(addAct);
    fileMenu->addAction(loadAct);
    fileMenu->addAction(saveAct);
    fileMenu->addSeparator();
    fileMenu->addAction(quitAct);


    QAction *flipbookAct = new QAction(tr("&Flipbook"), this);
    addAct->setStatusTip(tr("Flipbook a preview animation"));
    connect(flipbookAct, SIGNAL(triggered()), this, SLOT(startFlipbook()));

    m_cancelFlipbookAct = new QAction(tr("&Cancel Flipbook"), this);
    addAct->setStatusTip(tr("Cancel running flipbook"));
    connect(m_cancelFlipbookAct , SIGNAL(triggered()), this, SLOT(cancelFlipbook()));

    m_cancelFlipbookAct->setEnabled(false);

    renderMenu = menuBar()->addMenu(tr("&Render"));
    renderMenu->addAction(flipbookAct);
    renderMenu->addAction(m_cancelFlipbookAct );

    setWindowTitle(tr("Node Editor"));

    QWidget* window = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(window);
//    layout->setMargin(0);

    QSplitter* splitter = new QSplitter(window);
    // Don't try and resize the gl widgets until user
    splitter->setOpaqueResize(false);

    scene = new GridScene(-1000, -1000, 2000, 2000);

    QFormLayout* settingsLayout = new QFormLayout;

//    QVBoxLayout* rhs_layout = new QVBoxLayout;
//    rhs_layout->

//    QWidget* rhs = new QWidget(this);
//    rhs->setLayout(  );

    view = new QGraphicsView(splitter);
    view->setScene(scene);
    //view->setViewport(new QOpenGLWidget);

    view->setRenderHint(QPainter::Antialiasing, true);
    view->setRenderHint(QPainter::HighQualityAntialiasing, true);

    nodeEditor = new QNodeGraph(this);
    nodeEditor->install(scene);

    connect(nodeEditor, SIGNAL(graphChanged()), this, SLOT(graphUpdated()) );

    m_glViewport = 0;
    m_glViewport = new TestGLWidget(splitter);
    m_glViewport->setMinimumWidth(640);
    m_glViewport->setMinimumHeight(480);

    connect(m_glViewport, SIGNAL(initializedGL()), this, SLOT(initializeGL()));

    // Add Viewport
    splitter->addWidget(m_glViewport);

    // Add right hand side
    splitter->addWidget(view);

    m_timeline = new QAnimatedTimeline;
    m_timeline->setStartFrame(0);
    m_timeline->setEndFrame(200);

    connect(m_timeline, SIGNAL(timeUpdated(float)), m_glViewport, SLOT(updateTime(float)));

    layout->addWidget(splitter);
    layout->addWidget(m_timeline);
    window->setLayout(layout);

    setCentralWidget(window);

    m_updateTimer = startTimer(30);
    m_drawTimer = startTimer(30);

    m_flipbooking = false;
}

void MainWindow::initializeGL()
{
    OptixScene* optixscene = m_glViewport->m_optixScene;
    connect(optixscene, SIGNAL(frameReady()), this, SLOT(dumpFrame()));
}

void MainWindow::dumpFrame()
{
    int currentFrame = m_timeline->getTime();

//    if(currentFrame == m_timeline->getEndFrame())
//    {
//        qDebug() << "Finished";
//        m_flipbooking = false;
//        return;
//    }

    // Cancel if the framebuffer was closed
    if(!m_framebuffer->isVisible())
    {
        m_flipbooking = false;
        m_cancelFlipbookAct->setEnabled(false);
    }

    if(m_flipbooking)
    {
        m_timeline->setTime( currentFrame + 1 );

        OptixScene* optixscene = m_glViewport->m_optixScene;
        unsigned long elementSize, width, height;
        float* buffer = optixscene->getBufferContents( optixscene->outputBuffer(), &elementSize, &width, &height );

        unsigned int totalbytes = width * height * elementSize;
        uchar* bufferbytes = new uchar[totalbytes];
        for(int i = 0; i < totalbytes; i++)
        {
            bufferbytes[i] = static_cast<uchar>(buffer[i] * 255);
        }

        QImage imageFrame;

        switch(elementSize / sizeof(float))
        {
        case 1:
            imageFrame = QImage(bufferbytes, width, height, QImage::Format_Grayscale8);
            break;
        case 3:
            imageFrame = QImage(bufferbytes, width, height, QImage::Format_RGB32);
            break;
        case 4:
            imageFrame = QImage(bufferbytes, width, height, QImage::Format_RGBA8888);
            break;
        default:
            qWarning("Invalid QImage type");
            return;
        }

        QTransform flipY;
        flipY.scale(1, -1);
//        flipY.rotate(90, Qt::Axis::YAxis);
        imageFrame = imageFrame.transformed(flipY);

        QImage finalImage(imageFrame.size(), imageFrame.format());
        finalImage.fill( Qt::black );

        QPainter p(&finalImage);
        p.drawImage(0, 0, imageFrame);

        // Set current frame to this one
        int currentFrame = m_framebuffer->addFrame( finalImage );
        m_framebuffer->setFrame( currentFrame );

        m_framebuffer->setBufferSize( imageFrame.width(), imageFrame.height() );

        m_cancelFlipbookAct->setEnabled(true);
    }
}

void MainWindow::startFlipbook()
{
    // Reuse existing window
    if(m_framebuffer->isVisible())
    {
        m_framebuffer->clearFrames();
    }

    m_framebuffer->show();

    m_timeline->setTime( m_timeline->getStartFrame() );
    m_glViewport->updateTime( m_timeline->getTime() ); // Force update
    m_flipbooking = true;
}

void MainWindow::cancelFlipbook()
{
    m_flipbooking = false;

    m_cancelFlipbookAct->setEnabled(false);
}

void MainWindow::keyPressEvent(QKeyEvent* _event)
{

}

void MainWindow::timerEvent(QTimerEvent *_event)
{
    if(_event->timerId() == m_updateTimer)
    {
      //if (isExposed())
      if(m_glViewport)
      {
          m_glViewport->update();
      }
    }
}

void MainWindow::graphUpdated()
{
    std::string src = nodeEditor->parseGraph().c_str();
    qDebug() << src.c_str();
    m_glViewport->m_optixScene->createGeometry( src );
}

void MainWindow::timeUpdated(float _t)
{
    qDebug() << "Time : " << _t;
}

MainWindow::~MainWindow()
{
    killTimer(m_drawTimer);
    killTimer(m_updateTimer);
}

void MainWindow::saveFile()
{
	QString fname = QFileDialog::getSaveFileName();
	if (fname.isEmpty())
		return;

	QFile f(fname);
	f.open(QFile::WriteOnly);
	QDataStream ds(&f);
    nodeEditor->save(ds);
}

void MainWindow::loadFile()
{
	QString fname = QFileDialog::getOpenFileName();
	if (fname.isEmpty())
		return;

	QFile f(fname);
	f.open(QFile::ReadOnly);
	QDataStream ds(&f);
    nodeEditor->load(ds);
}

void MainWindow::addBlock()
{
   DistanceOpNode *c = new DistanceOpNode("Union", scene, 0);
//	static const char* names[] = {"Vin", "Voutsadfasdf", "Imin", "Imax", "mul", "add", "sub", "div", "Conv", "FFT"};
//	for (int i = 0; i < 4 + rand() % 3; i++)
//	{
//		b->addPort(names[rand() % 10], rand() % 2, 0, 0);
//        b->setPos(view->sceneRect().center().toPoint());
//	}
}
