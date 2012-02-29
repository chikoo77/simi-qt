#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H 1

#include <QMainWindow>
#include <QString>
#include <QDir>
#include <QFileInfo>
#include "ui_mainwindow.h"



#include "vtkImageViewer2.h"
#include "vtkSmartPointer.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkStructuredPoints.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkStructuredPointsReader.h"
#include "customInteractorStyle.h"

/* EXTRA HEADERS FOR EXPERIMENT */

#include "vtkPolyData.h"
#include "vtkSmartPointer.h"
#include "vtkImageMapper.h"
#include "vtkActor2D.h"

/* END EXTRA HEADERS */

#include "segmentation.h"


//Forward Declare UiTester
class UiTester;

class MainWindow : public QMainWindow
{
	Q_OBJECT

	public:
		MainWindow();
		~MainWindow();
	
	private slots:
		void on_actionOpen_Image_triggered();
		void on_actionSlice_up_triggered();
		void on_actionSlice_down_triggered();
		void on_actionAbout_triggered();
		bool loadImage();

        void on_minIntensitySlider_valueChanged(int value);
	void on_maxIntensitySlider_valueChanged(int value);
	void on_sliceSlider_valueChanged(int value);

        void on_runAlgorithm_clicked();

signals:
		void sliceChanged(int sliceNumber);

	private:
		QFileInfo imageInfo;
		QDir workPath; //Directory used file open dialogs
		Ui::MainWindow* ui; //handle to user interface
		vtkSmartPointer<vtkStructuredPointsReader> reader;
		vtkSmartPointer<vtkImageViewer2> imageView;
		vtkSmartPointer<CustomInteractorStyle> customStyle;


		//setup methods
		void contrastControlSetup();
		void sliceControlSetup();


		void changeContrast();

		//Friend class for UI testing.
		friend class UiTester;
};

#endif
