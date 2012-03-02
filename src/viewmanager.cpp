#include "viewmanager.h"
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <QDebug>
#include <vtkCellPicker.h>
#include <QVTKInteractor.h>
#include <vtkImageActor.h>
#include <vtkLookupTable.h>
#include <vtkImageMapToColors.h>


ViewManager::ViewManager(ImagePairManager* imagePairManager, QVTKWidget* qvtkWidget) : scaleStep(10), dragOn(false), mouseX(0), mouseY(0), mouseZ(0), mouseIntensity(0), mouseOverWidget(false)
{
    this->imagePairManager = imagePairManager;
    this->qvtkWidget = qvtkWidget;

	//setup original image
	imageViewer = vtkImageViewer2::New();
    imageViewer->SetInput(imagePairManager->original);
	qvtkWidget->SetRenderWindow(imageViewer->GetRenderWindow());
	imageViewer->SetupInteractor(qvtkWidget->GetRenderWindow()->GetInteractor());

    //setup zoom control
    maxScale= ( imagePairManager->getYDim() )*( imagePairManager->getYSpacing() )/2.0;
    minScale=maxScale/800;
    currentScale=maxScale;

    imageViewer->GetRenderer()->GetActiveCamera()->SetParallelScale(currentScale);


    //setup segblock image
    vtkSmartPointer<vtkLookupTable> lut= vtkLookupTable::New();
    lut->SetNumberOfTableValues(3);
    lut->SetRange(0.0,2.0);
    lut->SetTableValue(ImagePairManager::BACKGROUND,0.0,1.0,0.0,0.3); //set background
    lut->SetTableValue(ImagePairManager::BLOCKING,0.0,0.0,1.0,1.0);
    lut->SetTableValue(ImagePairManager::SEGMENTATION,1.0,0.0,0.0,1.0);
    lut->Build();

    vtkSmartPointer<vtkImageMapToColors> segblockMapper = vtkImageMapToColors::New();
    segblockMapper->SetLookupTable(lut);
    segblockMapper->PassAlphaToOutputOn(); //do I need this?
    segblockMapper->SetInput(imagePairManager->segblock);

    segblockActor = vtkImageActor::New();
    segblockActor->SetInput(segblockMapper->GetOutput());
    imageViewer->GetRenderer()->AddViewProp(segblockActor);

    //try to duplicate display bounds
    int displayExtent[6];
    imageViewer->GetImageActor()->GetDisplayExtent(displayExtent);
    segblockActor->SetDisplayExtent(displayExtent);
    qvtkWidget->update();



    //setup seedpoint drawing?

    //Enable connections between VTK events and our Qt Slots
    connections = vtkEventQtSlotConnect::New();

    //Setup mousewheel connection for zoom. We use high priority so we get called before the vtkinteractorstyle
    connections->Connect(qvtkWidget->GetInteractor(),
                vtkCommand::MouseWheelForwardEvent,
                this,
                SLOT(mouseWheelForward(vtkObject*,ulong,void*,void*,vtkCommand*)),
                NULL,
                1.0);


    connections->Connect(qvtkWidget->GetInteractor(),
                vtkCommand::MouseWheelBackwardEvent,
                this,
                SLOT(mouseWheelBackward(vtkObject*,ulong,void*,void*,vtkCommand*)),
                NULL,
                1.0
                );

    //setup connection for simple left click
    connections->Connect(qvtkWidget->GetInteractor(),
			vtkCommand::LeftButtonPressEvent,
			this,
			SLOT(mouseLeftClick(vtkObject*,ulong,void*,void*,vtkCommand*)),
			NULL,
			1.0);

    //setup connections for mouse dragging
    connections->Connect(qvtkWidget->GetInteractor(),
		vtkCommand::LeftButtonPressEvent,
		this,
		SLOT(dragHandler(vtkObject*,ulong,void*,void*,vtkCommand*)),
		NULL,
		2.0);


    connections->Connect(qvtkWidget->GetInteractor(),
		vtkCommand::MouseMoveEvent,
		this,
		SLOT(dragHandler(vtkObject*,ulong,void*,void*,vtkCommand*)),
		NULL,
		2.0);

    connections->Connect(qvtkWidget->GetInteractor(),
		vtkCommand::LeftButtonReleaseEvent,
		this,
		SLOT(dragHandler(vtkObject*,ulong,void*,void*,vtkCommand*)),
		NULL,
		2.0);

    //setup connections for mouseInWidget
    connections->Connect(qvtkWidget->GetInteractor(),
			vtkCommand::EnterEvent,
			this,
			SLOT(enterLeaveHandler(vtkObject*,ulong)),
			NULL,
			1.0
			);

    connections->Connect(qvtkWidget->GetInteractor(),
			vtkCommand::LeaveEvent,
			this,
			SLOT(enterLeaveHandler(vtkObject*,ulong)),
			NULL,
			1.0
			);

}

ViewManager::~ViewManager()
{

}

void ViewManager::setConstrast(double minIntensity, double maxIntensity)
{
    if(maxIntensity >= minIntensity)
    {
        imageViewer->SetColorLevel( (minIntensity + maxIntensity)/2.0 );
        imageViewer->SetColorWindow(maxIntensity - minIntensity);

        //update
        imageViewer->GetRenderWindow()->Render();
    }
}

int ViewManager::getCurrentSlice()
{
    return imageViewer->GetSlice();
}

vtkRenderer *ViewManager::getRenderer()
{
    if(imageViewer!=0)
        return imageViewer->GetRenderer();
    else
        return NULL;

}

void ViewManager::zoomIn()
{
    double tempScale = currentScale -scaleStep;

    if(tempScale >= minScale)
    {
        currentScale=tempScale;
        qDebug() << "Zoom scale set to " << currentScale;
    }
    else
    {
        qDebug() << "Zoom In limit reached";
    }

    forceZoom();

}

void ViewManager::zoomOut()
{
    double tempScale = currentScale +scaleStep;

    if(tempScale <= maxScale)
    {
        currentScale=tempScale;
        qDebug() << "Zoom scale set to " << currentScale;
    }
    else
    {
        qDebug() << "Zoom Out limit reached.";
    }

    forceZoom();
}

void ViewManager::mouseWheelForward(vtkObject *caller, unsigned long vtkEvent, void *clientData, void *callData, vtkCommand *command)
{
    zoomIn();

    //Prevent vtkInteractorStyle from intercepting the event
    command->AbortFlagOn();
}

void ViewManager::mouseWheelBackward(vtkObject *caller, unsigned long vtkEvent, void *clientData, void *callData, vtkCommand *command)
{
    zoomOut();

    //Prevent vtkInteractorStyle from intercepting the event
    command->AbortFlagOn();
}


void ViewManager::ChangeSlice(int slice)
{
	if(slice >= imagePairManager->getZMin() && slice <= imagePairManager->getZMax())
	{
        imageViewer->SetSlice(slice);

        //try to duplicate display extent so correct segblock slice is shown.
        int displayExtent[6];
        imageViewer->GetImageActor()->GetDisplayExtent(displayExtent);
        segblockActor->SetDisplayExtent(displayExtent);

        //segblockActor->SetPropertyKeys(imageViewer->GetImageActor()->GetPropertyKeys());//try duplicate props


        emit sliceChanged(slice);
	}

    //workaround bug where changing slice causes zoom to reset to something unexpected
    forceZoom();
}

void ViewManager::forceZoom()
{
    imageViewer->GetRenderer()->GetActiveCamera()->SetParallelScale(currentScale);
    imageViewer->GetRenderWindow()->Render();
}

void ViewManager::debugDump()
{
    /*  ImageViewer2 probe
    *
    *
    */

    double position[3]; //position in world co-ordinates
    imageViewer->GetImageActor()->GetPosition(position);
    qDebug() << "imageViewer's ImageActor position:" << position[0] << "," << position[1] << "," << position[2];

    double origin[3];
    imageViewer->GetImageActor()->GetOrigin(origin);
    qDebug() << "imageViewer's' Image Actor origin:" << origin[0] << "," << origin[1] << "," << origin[2];

    double scale[3];
    imageViewer->GetImageActor()->GetScale(scale);
    qDebug() << "imageViewer's' Image Actor scale:" << scale[0] << "," << scale[1] << "," << scale[2];

    double* xrange;
    double* yrange;
    double* zrange;
    xrange=imageViewer->GetImageActor()->GetXRange();
    yrange=imageViewer->GetImageActor()->GetYRange();
    zrange=imageViewer->GetImageActor()->GetZRange();
    qDebug() << "imageViewer's Image Actor X range [" << xrange[0] << "," << xrange[1] << "]";
    qDebug() << "imageViewer's Image Actor Y range [" << yrange[0] << "," << yrange[1] << "]";
    qDebug() << "imageViewer's Image Actor Z range [" << zrange[0] << "," << zrange[1] << "]";

    int displayExtent[6];
    imageViewer->GetImageActor()->GetDisplayExtent(displayExtent);
    qDebug() << "imageViewer's Image Actor X display extent" << displayExtent[0] << "," << displayExtent[1];
    qDebug() << "imageViewer's Image Actor Y display extent " << displayExtent[2] << "," << displayExtent[3];
    qDebug() << "imageViewer's Image Actor Z display extent " << displayExtent[4] << "," << displayExtent[5];

    double* centre; //centre of bounding box in world co-ordinates
    centre = imageViewer->GetImageActor()->GetCenter();
    qDebug() << "imageViewer's Image Actor's centre of bounding box:" << centre[0] << "," << centre[1] << "," << centre[2];

    /* Seg block information */
    //position in world co-ordinates
    segblockActor->GetPosition(position);
    qDebug() << "segblocks's ImageActor position:" << position[0] << "," << position[1] << "," << position[2];

    segblockActor->GetOrigin(origin);
    qDebug() << "segblockActor's' Image Actor origin:" << origin[0] << "," << origin[1] << "," << origin[2];

    segblockActor->GetScale(scale);
    qDebug() << "segblockActor's' Image Actor scale:" << scale[0] << "," << scale[1] << "," << scale[2];

    xrange=segblockActor->GetXRange();
    yrange=segblockActor->GetYRange();
    zrange=segblockActor->GetZRange();
    qDebug() << "segblock's Image Actor X range [" << xrange[0] << "," << xrange[1] << "]";
    qDebug() << "segblock's Image Actor Y range [" << yrange[0] << "," << yrange[1] << "]";
    qDebug() << "segblock's Image Actor Z range [" << zrange[0] << "," << zrange[1] << "]";


    segblockActor->GetDisplayExtent(displayExtent);
    qDebug() << "segblock's Image Actor X display extent" << displayExtent[0] << "," << displayExtent[1];
    qDebug() << "segblock's Image Actor Y display extent " << displayExtent[2] << "," << displayExtent[3];
    qDebug() << "segblock's Image Actor Z display extent " << displayExtent[4] << "," << displayExtent[5];

    centre = segblockActor->GetCenter();
    qDebug() << "segblock's Image Actor's centre of bounding box:" << centre[0] << "," << centre[1] << "," << centre[2];


    /* Camera information
    *
    */
    vtkCamera* camera = imageViewer->GetRenderer()->GetActiveCamera();
    camera->GetPosition(position);
    qDebug() << "Camera position:" << position[0] << "," << position[1] << "," << position[2];

    double viewUp[3];
    camera->GetViewUp(viewUp);
    qDebug() << "Camera view up:" << viewUp[0] << "," << viewUp[1] << "," << viewUp[2];

    double focalPoint[3];
    camera->GetFocalPoint(focalPoint);
    qDebug() << "Camera focal point:" <<  focalPoint[0] << "," << focalPoint[1] << "," << focalPoint[2];

    double projectionVector[3];
    camera->GetDirectionOfProjection(projectionVector);
    qDebug() << "Camera projection direction [from camera to focal point]:" << projectionVector[0] << "," << projectionVector[1] << "," << projectionVector[2];



}

void ViewManager::mouseLeftClick(vtkObject *caller, unsigned long vtkEvent, void *clientData, void *callData, vtkCommand *command)
{
	vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::SafeDownCast(caller);

	// Get the location of the click (in window coordinates)
	int* pos = iren->GetEventPosition();

	qDebug() << "ViewManager::mouseLeftClick() : Window position is:" << pos[0] << pos[1];

	vtkSmartPointer<vtkCellPicker> picker = vtkSmartPointer<vtkCellPicker>::New();

	// Pick from this location.
	picker->Pick(pos[0], pos[1], 0, imageViewer->GetRenderer());

	if(picker->GetCellId() != -1)
	{
		qDebug() << "ViewManager::mouseLeftClick() : IJK:" << picker->GetCellIJK()[0] << "," << picker->GetCellIJK()[1] << "," << picker->GetCellIJK()[2] << " intensity:" <<
		getLastMouseIntensity();
		;


		emit viewLeftClicked(picker->GetCellIJK()[0], picker->GetCellIJK()[1], picker->GetCellIJK()[2]);
	}
	else
		qDebug() << "ViewManager::mouseLeftClick() : Out of range!";

	//make sure vtkinteractorstyle doesn't catch event
	command->AbortFlagOn();
}

void ViewManager::dragHandler(vtkObject *caller, unsigned long vtkEvent, void *clientData, void *callData, vtkCommand *command)
{
	//Get event co-ordinates before processing the event type

	vtkRenderWindowInteractor* iren = vtkRenderWindowInteractor::SafeDownCast(caller);

	// Get the location of the click (in window coordinates)
	int* pos = iren->GetEventPosition();

	vtkSmartPointer<vtkCellPicker> picker = vtkSmartPointer<vtkCellPicker>::New();

	// Pick from this location.
	picker->Pick(pos[0], pos[1], 0, imageViewer->GetRenderer());

	//check we are in the image
	if(picker->GetCellId() == -1)
		return; //not in the image!

	mouseX= picker->GetCellIJK()[0];
	mouseY= picker->GetCellIJK()[1];
	mouseZ= picker->GetCellIJK()[2];
	//This is VERY dirty we should work out the cast at run time!
	mouseIntensity = *(static_cast<short*>(imagePairManager->original->GetScalarPointer(mouseX, mouseY, mouseZ)));

	//Inform other classes that the mouse has moved.
	emit mouseHasMoved();

	switch(vtkEvent)
	{
		case vtkCommand::LeftButtonPressEvent :
			dragOn = true;
			//qDebug() << "Drag start";
			emit dragEvent(picker->GetCellIJK()[0],picker->GetCellIJK()[1], picker->GetCellIJK()[2]);

		break;

		case vtkCommand::LeftButtonReleaseEvent :
			dragOn = false;
			//qDebug() << "Drag stop";
			emit dragEvent(picker->GetCellIJK()[0],picker->GetCellIJK()[1], picker->GetCellIJK()[2]);

		break;

		case vtkCommand::MouseMoveEvent :
			if(dragOn)
			{
				//qDebug() << "Dragging...";
				emit dragEvent(picker->GetCellIJK()[0],picker->GetCellIJK()[1], picker->GetCellIJK()[2]);
			}



		break;

		default:
			qWarning() << "ViewManager::dragHandler() unrecognised mouse event received.";

	}
}

void ViewManager::update()
{
    qDebug() << "ViewManager::update() requested";
	imageViewer->GetRenderWindow()->Render();
	qvtkWidget->update();

}

void ViewManager::enterLeaveHandler(vtkObject *caller, unsigned long vtkEvent)
{

	switch(vtkEvent)
	{
		case vtkCommand::EnterEvent :
			//qDebug() << "enterLeaveHandler() : Enter";
			mouseOverWidget=true;
			emit mouseIsOverWidget();
			break;
		case vtkCommand::LeaveEvent :
			//qDebug() << "enterLeaveHandler() : Leave";
			mouseOverWidget=false;
			emit mouseLeavesWidget();
			break;
		default:
			qWarning() << "Unexpected event received in ViewManager::enterLeaveHandler()";

	}
}



