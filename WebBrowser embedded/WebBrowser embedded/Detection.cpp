// ConsoleApplication1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "pxcsensemanager.h"
#include "util_render.h"
#include <pxcfaceconfiguration.h>
#include "pxcfacemodule.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <conio.h>
//#include "discpp.h"
#include <time.h>

#define MAX_FACES 1

using namespace std;


void drawRect(int x, int y) {
	HDC screenDC = ::GetDC(0);
	::Rectangle(screenDC, x-50, y-50, x+50, y+50);
	::ReleaseDC(0, screenDC);
}

int cameraApp()
{
	boolean DrawFocusRect = true;
	clock_t t1, t2;
	t1 = clock();

	boolean drawRectangle = false;

	int screen_Width = 1920;// GetSystemMetrics(SM_CXSCREEN) * 2; //1920
	int screen_Height = 1200;// GetSystemMetrics(SM_CYSCREEN) * 2; //1200

	int calibXMin = 0;
	int calibYMin = 0;
	int calibXMax = 0;
	int calibYMax = 0;

	// initialize the util render 
	UtilRender *renderColor = new UtilRender(L"COLOR STREAM");
	//FaceRender *renderer = new FaceRender(L"PROCEDURAL FACE TRACKING");

	// create the PXCSenseManager
	PXCSenseManager *psm = 0;
	psm = PXCSenseManager::CreateInstance();

	// select the color stream of size 640x480 and depth stream of size 320x240
	psm->EnableStream(PXCCapture::STREAM_TYPE_COLOR);// , 640, 480);

	// Enable face analysis in the multimodal pipeline:
	psm->EnableFace();

	//retrieve face module if ready
	PXCFaceModule* faceAnalyzer = psm->QueryFace();

	// initialize the PXCSenseManager pipeline
	psm->Init();

	// retrieves an instance of the PXCFaceData interface
	PXCFaceData* outputData = faceAnalyzer->CreateOutput();

	// retrieves an instance of the PXCFaceConfiguration interface
	PXCFaceConfiguration* config = faceAnalyzer->CreateActiveConfiguration();
	PXCFaceConfiguration::GazeConfiguration *gazec = config->QueryGaze();

	// configure the face module features
	// set the 3D face tracker. if caamera cannot support depth it will automatically roll back to 2D treacking.
	// if depth stream is not enabled it will be enabled if the camera supports it. 
	config->SetTrackingMode(PXCFaceConfiguration::TrackingModeType::FACE_MODE_COLOR_PLUS_DEPTH);	
	config->EnableAllAlerts();
	config->landmarks.isEnabled = true;
	config->landmarks.maxTrackedFaces = MAX_FACES;
	config->detection.isEnabled = true;
	config->detection.maxTrackedFaces = MAX_FACES;
	config->pose.isEnabled = true;
	config->pose.maxTrackedFaces = MAX_FACES;
	gazec->isEnabled = true; //turn on gaze
	config->QueryExpressions()->Enable();
	config->QueryExpressions()->EnableAllExpressions();
	config->QueryExpressions()->properties.maxTrackedFaces = MAX_FACES;
	config->QueryPulse()->Enable();
	config->ApplyChanges();


	PXCImage *colorIm;

	/* Set up output file*/
	FILE * fp = fopen("output.csv", "w");
	boolean start = true;
	char outputString[100];
	int freq = 250;// 250;
	int frameCounter = 0;
	int elapsedTime = 0;
	boolean calibComplete = false;
	boolean toggleColor = true;
	boolean calibTopLeftComplete = false;
	boolean calibBottomRightComplete = false;

	int calibTopLeftCounter = 0;
	int calibBottomRightCoutner = 0;

	/* Set up data for plotting*/
	int focusXArray[10000] = { 0 };
	//int timeArray = new int(10000);
	int frames = 0;

	float oldTempPointX = 0;
	float oldTempPointY = 0;

	while (true) {

		//slow down
		Sleep(freq);
		//update the output data to the latest availible
		outputData->Update();



		/* Detection Structs */
		PXCFaceData::DetectionData *detectionData;
		PXCRectI32 rectangle;
		int face_X, face_Y, face_Z;
		pxcF32 depth;
		int face_Depth;

		/* Pose Structs */
		PXCFaceData::PoseData *poseData;
		PXCFaceData::PoseEulerAngles angles;
		float face_Roll, face_Pitch, face_Yaw;
		PXCFaceData::HeadPosition headPosition;

		/* Pulse Structs */
		PXCFaceData::PulseData *pulseData;
		int rate;

		/* Gaze Structs*/
		PXCFaceData::GazeData* gazeData;
		PXCFaceData::GazePoint gazePoint;
		int screenPoint_X;
		int	screenPoint_Y;
		int confidence;


		/* Landmark Structs */
		PXCFaceData::LandmarksData* landmarkData;
		PXCFaceData::LandmarkPoint* landmarkPoints;
		pxcI32 numPoints;

		/* Expression Structs */
		PXCFaceData::ExpressionsData *expressionData;
		PXCFaceData::ExpressionsData::FaceExpressionResult expressionResult;
		int eyes_right, eyes_left, eyes_up, eyes_down, eye_closed_left, eye_closed_right;


		bool userIsFocused = false;

		PXCFaceData::Face *trackedFace = outputData->QueryFaceByIndex(0);

		boolean lookingLeft = false;
		boolean lookingRight = false;
		boolean lookingUp = false;
		boolean lookingDown = false;
		boolean lookingStraight = true;

		


		/* Set initial values*/
		face_X = 0;
		face_Y = 0;
		face_Z = 0;
		face_Depth = 0;
		face_Roll = 0.0;
		face_Pitch = 0.0;
		face_Yaw = 0.0;
		screenPoint_X = 0;
		screenPoint_Y = 0;
		confidence = 0;
		rate = 0;
		eyes_right = 0;
		eyes_left = 0;
		eyes_up = 0;
		eyes_down = 0;
		eye_closed_left = 0;
		eye_closed_right = 0;


		if (trackedFace != NULL)
		{
			//PXCFaceData::ExpressionsData::EXPRESSION_SMILE

			/* Query Detection Data */
			detectionData = trackedFace->QueryDetection();
			if (detectionData != NULL)
			{
				/* Get rectangle of the detected face and render */
				detectionData->QueryBoundingRect(&rectangle);
				face_X = rectangle.x;
				face_Y = rectangle.y;

				/* get the depth */
				detectionData->QueryFaceAverageDepth(&depth);
				face_Depth = depth;

			}

			/* Query Pose Data */
			poseData = trackedFace->QueryPose();
			if (poseData != NULL)
			{
				/* Get angle of the detected face and render */
				poseData->QueryPoseAngles(&angles);

				face_Roll = angles.roll;
				face_Pitch = angles.pitch;
				face_Yaw = angles.yaw;

				poseData->QueryHeadPosition(&headPosition);
				face_X = headPosition.headCenter.x;
				face_Y = headPosition.headCenter.y;
				face_Z = headPosition.headCenter.z;

			}

			/* Query Pulse Data */
			pulseData = trackedFace->QueryPulse();
			if (pulseData != NULL)
			{
				/* Get rate */
				rate = pulseData->QueryHeartRate();
			}


			/* Query Landmark Data */
			landmarkData = trackedFace->QueryLandmarks();
			if (landmarkData != NULL)
			{
				/* Get number of points from Landmark data*/
				numPoints = landmarkData->QueryNumPoints();

				/* Create an Array with the number of points */
				landmarkPoints = new PXCFaceData::LandmarkPoint[numPoints];

				/* Query Points from Landmark Data and render */
				landmarkData->QueryPoints(landmarkPoints);
			}

			/* Query Gaze Data */
			gazeData = trackedFace->QueryGaze();
			if (gazeData != NULL)
			{
				/* Get angle of the detected face and render */
				gazePoint = gazeData->QueryGazePoint();
				screenPoint_X = gazePoint.screenPoint.x;
				screenPoint_Y = gazePoint.screenPoint.y;
				confidence = gazePoint.confidence;
				//gazeData->QueryGazeHorizontalAngle();
				//gazeData->QueryGazeVerticalAngle();

			}

			/* Query Expression Data */
			expressionData = trackedFace->QueryExpressions();
			if (expressionData != NULL)
			{
				/* Get expression of the detected face and render */
				if (detectionData != NULL)
				{

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_TURN_LEFT, &expressionResult)) {
						eyes_left = expressionResult.intensity;
					}

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_TURN_RIGHT, &expressionResult)) {
						eyes_right = expressionResult.intensity;
					}

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_UP, &expressionResult)) {
						eyes_up = expressionResult.intensity;
					}

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_DOWN, &expressionResult)) {
						eyes_down = expressionResult.intensity;
					}

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_CLOSED_LEFT, &expressionResult)) {
						eye_closed_left = expressionResult.intensity;
					}

					if (expressionData->QueryExpression(PXCFaceData::ExpressionsData::EXPRESSION_EYES_CLOSED_RIGHT, &expressionResult)) {
						eye_closed_right = expressionResult.intensity;
					}
				}
			}
		}

		// Determine where pose is pointing		
		float tempPointX = -face_X + face_Z*tan(face_Yaw * 3.14 / 180.0);
		float tempPointY = -face_Y + face_Z*tan(face_Pitch * 3.14 / 180.0);
		int focusX = 0;// (screen_Width / 2) + (tempPointX / 180) * 1500;
		int focusY = 0;// (screen_Height / 2) - (tempPointY / 100) * 1000; */

		printf("%6.4lf, %6.4lf\n", tempPointX, tempPointY);

		//  Calibration Sequence
		int msecForCalib = 5000;
		HDC hDC_Desktop = GetDC(0);
		RECT rect = { 20, 20, 200, 200 };
		HBRUSH brush = CreateSolidBrush(RGB(0, 0, 255));
		boolean lookingOffScreen = false;

		//CALIBRATION

		//Get upper left pose data
		if (!calibTopLeftComplete && !calibBottomRightComplete) {

			rect = { 0, 0, 200, 200 };
			if (toggleColor) { brush = CreateSolidBrush(RGB(0, 0, 255)); toggleColor = false; }
			else { brush = CreateSolidBrush(RGB(0, 255, 0)); toggleColor = true; }

			//Look at the number of continuous cycles where the tempPointX and tempPointY are within 5% of each other
			//Also need to ensure that the tempPointX and tempPointY are in some realistic range - TBD
			if ((abs(tempPointX - oldTempPointX) / abs(tempPointX + oldTempPointX)) < .1) {
				calibTopLeftCounter++;
			}
			else calibTopLeftCounter = 0;

			//if the last 10 values are within 10% of each other, calibration is complete
			if (calibTopLeftCounter >= 10 && tempPointX < -50){// && tempPointY > 10) {
				calibXMin = tempPointX*1.2; //-195
				calibYMax = tempPointY*1.2; //91
				calibTopLeftComplete=true;
				brush = CreateSolidBrush(RGB(0, 0, 0));
			}

			oldTempPointX = tempPointX;
			oldTempPointY = tempPointY;
			
		}
		
		//Get lower right pose data
		if (calibTopLeftComplete && !calibBottomRightComplete) {
		//drawRect(3000, 2000);// -face_Y * 5);
			
			rect = { screen_Width - 200, screen_Height-200, screen_Width, screen_Height };
			if (toggleColor) { brush = CreateSolidBrush(RGB(0, 0, 255)); toggleColor = false; }
			else { brush = CreateSolidBrush(RGB(0, 255, 0)); toggleColor = true; }

			//Look at the number of continuous cycles where the tempPointX and tempPointY are within 5% of each other
			//Also need to ensure that the tempPointX and tempPointY are in some realistic range - TBD
			if ((abs(tempPointY - oldTempPointY) / abs(tempPointY + oldTempPointY)) < .1) {
				calibBottomRightCoutner++;
			}
			else calibBottomRightCoutner = 0;

			//if the last 10 values are within 10% of each other, calibration is complete
			if (calibBottomRightCoutner >= 10  && tempPointX > 50){// && tempPointY < -10) {
				calibXMax = tempPointX*1.2;//120
			
				if (tempPointY >= 0) { calibYMin = tempPointY*.6; }//-86}
				else calibYMin = tempPointY*1.2;

				calibBottomRightComplete = true;
				brush = CreateSolidBrush(RGB(0, 0, 0));
			}
			
			oldTempPointX = tempPointX;
			oldTempPointY = tempPointY;
			
		}
		
		//Run regular detection
		if (calibTopLeftComplete && calibBottomRightComplete){
			calibComplete = true;
			lookingOffScreen = false;
			if (tempPointX < calibXMin || tempPointX > calibXMax) { lookingOffScreen = true; }
			if (tempPointY < calibYMin || tempPointY > calibYMax) { lookingOffScreen = true; }

			focusX = ((abs(tempPointX - calibXMin)) / (abs(calibXMax - calibXMin)))*screen_Width;
			focusY = screen_Height - ((abs(tempPointY - calibYMin)) / (abs(calibYMax - calibYMin)))*screen_Height;

			if (lookingOffScreen) {
				focusX = 0; focusY = 0;
			}

			brush = CreateSolidBrush(RGB(255, 0, 0));
			rect = { focusX - 100, focusY - 100, focusX + 100, focusY + 100 };
		}

		if (eyes_up > 80 && eyes_down == 0) { lookingUp = true; }
		if (eyes_down > 80 && eyes_up == 0) { lookingDown = true; }//  lookingOffScreen = true;
		if (eyes_left > 80 && eyes_right == 0) { lookingLeft = true; }
		if (eyes_right > 80 && eyes_left == 0) { lookingRight = true; }
		//lookingStraight

		//check that the eyes are open
		if (eye_closed_left != 0 && eye_closed_right != 0) {
			lookingOffScreen = true;
		}

		userIsFocused = false;
		/* Calling GetDC with argument 0 retrieves the desktop's DC */
		if (trackedFace != NULL && calibComplete && !lookingOffScreen) {
			userIsFocused = true;

			//if ((focusX > .9*screen_Width) && eyes_right >= 80) {
			//	userIsFocused = false; //user is looking to the right when facing the right part of the screen
			//}
			//if ((focusX < .1*screen_Width) && eyes_left >= 80) {
			//	userIsFocused = false; //user is looking to the right when facing the right part of the screen
			//}
			//if ((focusY < .1*screen_Height) && eyes_up >= 80) {
			//	userIsFocused = false; //user is looking to the right when facing the right part of the screen
			//}
			//if ((focusY > .50*screen_Height) && eyes_down >= 80) {
			//	userIsFocused = false; //user is looking to the right when facing the right part of the screen
			//}
		}

		
		if ((userIsFocused  & drawRectangle) || !calibTopLeftComplete || !calibBottomRightComplete) {
			FillRect(hDC_Desktop, &rect, brush);
			HBRUSH brushBorder = CreateSolidBrush(RGB(0, 0, 255));
			brushBorder = CreateSolidBrush(RGB(255, 00, 0));
			FrameRect(hDC_Desktop, &rect, brushBorder);
			DeleteObject(brush);
			DeleteDC(hDC_Desktop);
		}
		
		// Calculate the Elapsed Time
		t2 = clock(); // we get the time now
		float difference = (((float)t2) - ((float)t1)); // gives the time elapsed since t1 in milliseconds
		float seconds = difference / 1000; // float value of seconds

		//Add header line to output file
		if (start) {
			fprintf(fp, "elapsedTime, face_X, face_Y, face_Z, face_Pitch, face_Yaw, focusX, focusY, eyes_left, eyes_right, eyes_up, eyes_down, userIsFocused\n");
			tempPointX = -face_X + face_Z*tan(face_Yaw * 3.14 / 180.0);
			float tempPointY = -face_Y + face_Z*tan(face_Pitch * 3.14 / 180.0);
			start = false;
		}
		snprintf(outputString, 100, "%6.4lf, %i, %i, %i, %6.4lf, %6.4lf, %i, %i, %i, %i, %i, %i, %d\n", seconds, face_X, face_Y, face_Z, face_Pitch, face_Yaw, focusX, focusY, eyes_left, eyes_right, eyes_up, eyes_down, userIsFocused);
		printf(outputString);
		fprintf(fp, outputString);
		elapsedTime = elapsedTime + freq;
		
		// This function blocks until all streams are ready (depth and color)
		psm->AcquireFrame(true);

		// retrieve all available image samples
		PXCCapture::Sample *sample = psm->QuerySample();

		// retrieve the image or frame by type from the sample
		colorIm = sample->color;

		//render the frame
		if (!renderColor->RenderFrame(colorIm));

		// release or unlock the current frame to fetch the next frame
		psm->ReleaseFrame();

	}
	
	return 0;
}

