#include <QtGui>

#include "mainwindow.h"
#include <iostream>
#include <cstring>
#include <QString>
#include <QTextStream>
#include <QDebug>
#include <QTextStream>
#include "compiletimeconstants.h"
#include "vtkCamera.h"


MainWindow::MainWindow() : imageInfo(""), workPath(QDir::home()), allowOpen(false)
{
	ui = new Ui::MainWindow;
	ui->setupUi(this); //set up user interface

    seedPointManager=0;
	imagePairManager=0;
	viewManager=0;
	drawManager=0;
	segmenter=0;



	//Setup About Qt Dialog
	connect(ui->actionAboutQt,SIGNAL(triggered()),qApp,SLOT(aboutQt()));


	setWindowTitle(PROGRAM_NAME);

}

MainWindow::~MainWindow()
{
	delete ui;

	if(imagePairManager!=0)
        delete imagePairManager;

    if(seedPointManager!=0)
        delete seedPointManager;

	if(viewManager!=0)
        delete viewManager;

	if(drawManager!=0)
        delete drawManager;

	if(segmenter!=0)
		delete segmenter;
}

void MainWindow::on_actionOpen_Image_triggered()
{
	QString newImagePath;

	std::cerr << "Opening image" << std::endl;

	//check okay to open image
	if(imagePairManager==NULL || (!imagePairManager->segblockModified()) || allowOpen)
	{
		newImagePath = QFileDialog::getOpenFileName(this,
							tr("Open Image"),
							workPath.absolutePath(),
							tr("VTK Structured points (*.vtk)"));

		if(!newImagePath.isEmpty())
		{
			imageInfo.setFile(newImagePath);

			//Setup the ImagePairManager
			if(imagePairManager!=0)
				delete imagePairManager;

			imagePairManager = new ImagePairManager();

			if(!(imagePairManager->loadImage(imageInfo)))
			{
				qDebug() << "Failed to open" << imageInfo.absoluteFilePath();
				return;
			}

			//setup SeedPointManager
            if(seedPointManager!=0)
                delete seedPointManager;
            seedPointManager = new SeedPointManager(imagePairManager->getZDim());

			//setup LayoutManager
			if(viewManager!=0)
				delete viewManager;
			viewManager = new ViewManager(imagePairManager, seedPointManager, ui->qvtkWidget,ui->blockingAlphadoubleSpinBox, ui->segmentationAlphadoubleSpinBox);

			//setup drawManager
			if(drawManager!=0)
				delete drawManager;
            drawManager = new DrawManager(imagePairManager,ui->drawSizeSpinBox,ui->drawOnComboBox);

			//setup segmenter
			if(segmenter!=0)
				delete segmenter;
            segmenter = new Segmenter(seedPointManager,imagePairManager);



			sliceControlSetup();
			contrastControlSetup();
			toolbarSetup();
			segmentationControlSetup();

			//setup statusbar update from viewmanager
			connect(viewManager,SIGNAL(mouseHasMoved()), this, SLOT(updateStatusBar()));
			connect(viewManager,SIGNAL(mouseEntersWidget()), this, SLOT(updateStatusBar()));
			connect(viewManager,SIGNAL(mouseLeavesWidget()), this, SLOT(updateStatusBar()));

			//setup seedPointLine view being told about the seed point being change
            connect(seedPointManager,SIGNAL(seedPointChanged(int,int,int)), this, SLOT(seedPointChanged()));
			connect(viewManager,SIGNAL(sliceChanged(int)), this,SLOT(seedPointChanged()));

			//setup zoom control
			connect(ui->actionZoom_in,SIGNAL(triggered()), viewManager,SLOT(zoomIn()));
			connect(ui->actionZoom_out,SIGNAL(triggered()), viewManager,SLOT(zoomOut()));

			//setup so on segmentation completion we redraw
			connect(segmenter,SIGNAL(segmentationDone(int)), viewManager, SLOT(update()));

			//setup on drawing complete we redraw
			connect(drawManager,SIGNAL(drawingDone()),viewManager,SLOT(update()));

			//Update the work path to the location of the new image
			workPath.setPath(imageInfo.absolutePath());

			setWindowModified(false); //because we've loaded new image nothing can be modified

			//allow debug information to be shown from menu
			connect(ui->actionDump_debug,SIGNAL(triggered()),viewManager,SLOT(debugDump()));

			//allow load/save segblock menus now
			ui->actionLoad_Segmentation->setEnabled(true);
			ui->actionSave_Segmentation->setEnabled(true);


			//set window title
			QString newWindowTitle(PROGRAM_NAME);
			newWindowTitle.append(" - ").append(imageInfo.fileName());
			setWindowTitle(newWindowTitle);

			allowOpen=false;
		}

	}
	else
	{
		// offer to save stuff TODO
		qDebug() << "Need to offer to save segblock!!";

		//now try to open again.
		allowOpen=true;
		on_actionOpen_Image_triggered();

	}
}


void MainWindow::on_actionSlice_up_triggered()
{
    if(viewManager!=0)
        viewManager->ChangeSlice( viewManager->getCurrentSlice() +1);
}


void MainWindow::on_actionSlice_down_triggered()
{
    if(viewManager!=0)
        viewManager->ChangeSlice( viewManager->getCurrentSlice() -1);
}


void MainWindow::on_actionAbout_triggered()
{
	QString about;
	QTextStream aboutTS(&about);

	aboutTS << "<h2>" << PROGRAM_NAME << "</h2>" <<
	"<p><b>Version:</b> " << PROGRAM_VERSION << "</p>" <<
	"<p>" << PROGRAM_NAME << " is a program for structure identification in medical images.</p>" <<
	"<p><b>Authors</b>:<br>" << PROGRAM_AUTHORS << "</p>";

	QMessageBox::about(this,tr("About ") + PROGRAM_NAME, about);
}




void MainWindow::contrastControlSetup()
{

	if(imagePairManager!=0)
	{
		ui->minIntensitySlider->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->maxIntensitySlider->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->minIntensitySpinBox->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->maxIntensitySpinBox->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
	}


	//Doing this change should cause the valueChanged() signal to be emitted by the sliders
	ui->minIntensitySlider->setValue(static_cast<int>(imagePairManager->getMinimumIntensity()));
	ui->maxIntensitySlider->setValue(static_cast<int>(imagePairManager->getMaximumIntensity()));

}

void MainWindow::changeContrast()
{
    if(viewManager!=0)
    {
        viewManager->setConstrast(static_cast<double>(ui->minIntensitySlider->value()), static_cast<double>(ui->maxIntensitySlider->value()));
    }
}



void MainWindow::on_minIntensitySlider_valueChanged(int value)
{
	//prevent minimum being > maximum
	if(value > ui->maxIntensitySpinBox->value())
		ui->minIntensitySpinBox->setValue(ui->maxIntensitySpinBox->value());

   changeContrast();
}

void MainWindow::on_maxIntensitySlider_valueChanged(int value)
{
	//prevent maximum being < minimum
	if(value < ui->minIntensitySpinBox->value())
		ui->maxIntensitySpinBox->setValue(ui->minIntensitySpinBox->value());

	changeContrast();
}


void MainWindow::on_sliceSlider_valueChanged(int value)
{
	if(viewManager!=0)
        viewManager->ChangeSlice(value);
}

void MainWindow::on_actionHandTool_triggered()
{
    qDebug() << "Hand tool activated";

    //disable other connections
    //disable seedTool connection
    disconnect(viewManager,
		SIGNAL(viewLeftClicked(int,int,int)),
        seedPointManager,
		SLOT(setSeedPoint(int,int,int))
		);

    //disable penTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(draw(int,int,int))
		);

    //disable eraseTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(erase(int,int,int))
		);

}

void MainWindow::on_actionPenTool_triggered()
{
    qDebug() << "Pen tool activated";

    //disable other connections
    //disable seedTool connection
    disconnect(viewManager,
		SIGNAL(viewLeftClicked(int,int,int)),
        seedPointManager,
		SLOT(setSeedPoint(int,int,int))
		);

    //disable eraseTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(erase(int,int,int))
		);

    //enable connection
    connect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(draw(int,int,int))
		);
}

void MainWindow::on_actionCrosshairTool_triggered()
{
    qDebug() << "Crosshair tool activated";

    //disable other connections
    //disable seedTool connection
    disconnect(viewManager,
		SIGNAL(viewLeftClicked(int,int,int)),
        seedPointManager,
		SLOT(setSeedPoint(int,int,int))
		);

    //disable penTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(draw(int,int,int))
		);

    //disable eraseTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(erase(int,int,int))
		);



    //enable connection
    connect(viewManager,
		SIGNAL(viewLeftClicked(int,int,int)),
        seedPointManager,
		SLOT(setSeedPoint(int,int,int))
		);

}

void MainWindow::on_actionEraseTool_triggered()
{
    qDebug() << "Erase tool activated";

    //disable seedTool connection
    disconnect(viewManager,
		SIGNAL(viewLeftClicked(int,int,int)),
        seedPointManager,
		SLOT(setSeedPoint(int,int,int))
		);

    //disable penTool connection
    disconnect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(draw(int,int,int))
		);

    //enable connection
    connect(viewManager,
		SIGNAL(dragEvent(int,int,int)),
		drawManager,
		SLOT(erase(int,int,int))
		);



}

void MainWindow::sliceControlSetup()
{
	if(imagePairManager!=0)
	{
		ui->sliceSlider->setRange(imagePairManager->getZMin(),imagePairManager->getZMax());
		ui->sliceSpinBox->setRange(imagePairManager->getZMin(),imagePairManager->getZMax());

		connect(viewManager,SIGNAL(sliceChanged(int)),ui->sliceSlider,SLOT(setValue(int)));

    }
}

void MainWindow::toolbarSetup()
{
    //enable the toolbar buttons to be used
    ui->toolBar->setEnabled(true);

    //setup mutually exclusive behaviour
    toolbarActions = new QActionGroup(this);

    toolbarActions->addAction(ui->actionHandTool);
    toolbarActions->addAction(ui->actionPenTool);
    toolbarActions->addAction(ui->actionCrosshairTool);
    toolbarActions->addAction(ui->actionEraseTool);

    //set default action
    ui->actionHandTool->setChecked(true);
}

void MainWindow::segmentationControlSetup()
{
	if(imagePairManager!=0)
	{
		ui->minSegIntensitySlider->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->minSegIntensitySpinBox->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->maxSegIntensitySlider->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
		ui->maxSegIntensitySpinBox->setRange(static_cast<int>(imagePairManager->getMinimumIntensity()),static_cast<int>(imagePairManager->getMaximumIntensity()));
	}


	//Doing this change should cause the valueChanged() signal to be emitted by the sliders
	ui->minSegIntensitySlider->setValue(static_cast<int>(imagePairManager->getMinimumIntensity()));
	ui->maxSegIntensitySlider->setValue(static_cast<int>(imagePairManager->getMaximumIntensity()));
}

void MainWindow::on_minSegIntensitySlider_valueChanged(int value)
{
    //make sure min is not > max
    if(value > ui->maxSegIntensitySlider->value())
	    ui->minSegIntensitySlider->setValue( ui->maxSegIntensitySlider->value());

}

void MainWindow::on_maxSegIntensitySlider_valueChanged(int value)
{
	//make sure max is not < min
	if(value < ui->minSegIntensitySlider->value())
		ui->maxSegIntensitySlider->setValue( ui->minSegIntensitySlider->value());
}

void MainWindow::on_doSegmentation2D_clicked()
{
	if(segmenter!=0)
		segmenter->doSegmentation2D(viewManager->getCurrentSlice(), ui->minSegIntensitySlider->value(), ui->maxSegIntensitySlider->value());
}

void MainWindow::updateStatusBar()
{
	if(viewManager!=0 && viewManager->mouseIsOverWidget())
	{
		QString message("");
		QTextStream messageStream(&message);
		messageStream << "X:" << viewManager->getLastMousePosX() << " Y:"<< viewManager->getLastMousePosY() << " Z:" << viewManager->getLastMousePosZ() << " Intensity:" << viewManager->getLastMouseIntensity();

		ui->statusbar->showMessage(*(messageStream.string()),0);
	}
	else
		ui->statusbar->showMessage("");
}

void MainWindow::seedPointChanged()
{
	int x=0;
	int y=0;
    seedPointManager->getSeedPoint(viewManager->getCurrentSlice(),x,y);

	QString seedPoint("(");
	seedPoint += QString::number(x);
	seedPoint +=",";
	seedPoint += QString::number(y);
	seedPoint +=")";
	ui->seedPointLineEdit->setText(seedPoint);
}

void MainWindow::on_doSegmentation3D_clicked()
{
	if(segmenter!=0)
        segmenter->doSegmentation3D(viewManager->getCurrentSlice(), ui->minSegIntensitySlider->value(), ui->maxSegIntensitySlider->value());
}

void MainWindow::on_actionClear_Drawing_triggered()
{
    if(imagePairManager!=0 && viewManager!=0)
    {
        imagePairManager->setAll(viewManager->getCurrentSlice(), ImagePairManager::BLOCKING, ImagePairManager::BACKGROUND);
        viewManager->update();
    }
}

void MainWindow::on_actionClear_Segmentation_triggered()
{
    if(imagePairManager!=0 && viewManager!=0)
    {
        imagePairManager->setAll(viewManager->getCurrentSlice(), ImagePairManager::SEGMENTATION, ImagePairManager::BACKGROUND);
        viewManager->update();
    }
}

void MainWindow::on_actionLoad_Segmentation_triggered()
{
    QFileInfo loadPath;

    if(imagePairManager==0)
        return;

    //Check if we need to save our current segblock first
    if(imagePairManager->segblockModified())
    {
        //TO DO
        qDebug() << "Offer to save before load!";

    }

    imagePairManager->loadSegblock(loadPath);


}

void MainWindow::on_actionSave_Segmentation_triggered()
{
    QFileInfo savePath;

    if(imagePairManager!=0)
    {
        //TO DO

        imagePairManager->saveSegblock(savePath);
    }


}
