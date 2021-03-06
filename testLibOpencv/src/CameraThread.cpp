/*
 * Copyright (C) 2014 EPFL
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */

/**
 * @file CameraThread.cpp
 * @brief Listens to the camera in a separate thread
 * @author Ayberk Özgür
 * @version 1.0
 * @date 2014-09-23
 */

#include"CameraThread.h"

//*****************************************************************************
// CameraTask implementation
//*****************************************************************************

CameraTask::CameraTask(BetterVideoCapture* camera, QVideoFrame* videoFrame, unsigned char* cvImageBuf, int width, int height)
{
    this->running = true;
    this->camera = camera;
    this->videoFrame = videoFrame;
    this->cvImageBuf = cvImageBuf;
    this->width = width;
    this->height = height;
}

CameraTask::~CameraTask()
{
    //Leave camera and videoFrame alone, they will be destroyed elsewhere
}

void CameraTask::stop()
{
    running = false;
}

void CameraTask::convertUVsp2UVp(unsigned char* __restrict srcptr, unsigned char* __restrict dstptr, int stride)
{
    for(int i=0;i<stride;i++){
        dstptr[i]           = srcptr[i*2];
        dstptr[i + stride]  = srcptr[i*2 + 1];
    }
}

void CameraTask::RGB242Yuv420p( unsigned char* __restrict  destination, unsigned char* __restrict  rgb,
                                const int &width, const int &height ) {
    const size_t image_size = width * height;
    unsigned char *dst_y = destination;
    unsigned char *dst_uv = destination + image_size;



    // Y plane
    for( size_t i = 0; i < image_size; ++i ) {
        *dst_y++ = ( ( 66*rgb[3*i] + 129*rgb[3*i+1] + 25*rgb[3*i+2] ) >> 8 ) + 16;
    }
    // UV plane
    for( size_t y=0; y<height; y+=2 ) {
        for( size_t x=0; x<width; x+=2 ) {
          const size_t i = y*width + x;
          *dst_uv++ = ( ( -38*rgb[3*i] + -74*rgb[3*i+1] + 112*rgb[3*i+2] ) >> 8 ) + 128;
          *dst_uv++ = ( ( 112*rgb[3*i] + -94*rgb[3*i+1] + -18*rgb[3*i+2] ) >> 8 ) + 128;
      }
    }
}
void CameraTask::doWork()
{

    if(videoFrame)
        videoFrame->map(QAbstractVideoBuffer::ReadOnly);

    while(running && videoFrame != NULL && camera != NULL){
        if(!camera->grabFrame())
            continue;
        unsigned char* cameraFrame = camera->retrieveFrame();

        //Get camera image into screen frame buffer
        if(videoFrame){

#ifdef ANDROID //Assume YUV420sp camera image and YUV420p QVideoFrame

			//processing video image
			cv::Mat srcImg = cv::Mat(height*3/2,width,CV_8UC1,(uchar *)cameraFrame);
			cv::Mat srcImgRGB = cv::Mat(height,width,CV_8UC3);

			// cv::Mat myuv(height + height/2, width, CV_8UC1, (uchar *)_yuv);
			//cv::Mat mdst(height, width, CV_8UC4, (uchar *)_dst);


            cv::cvtColor(srcImg, srcImgRGB, CV_YUV420sp2BGR);
			cv::rectangle( srcImgRGB,cv::Rect(50,50,255,255),cv::Scalar(255,0,255),2,8,0);
          //  cv::cvtColor(srcImgRGB, srcImg, CV_RGB2YUV_I420);
            RGB242Yuv420p( cameraFrame, srcImgRGB.data,width, height );

           // cv::Mat srcImg(height,width,CV_8UC1);
           // cv::Mat threshold(height,width,CV_8UC1);
           // memcpy(srcImg.data,cameraFrame,height*width );

            //cv::rectangle( srcImg,cv::Rect(50,50,255,255),cv::Scalar(255,255,255),2,8,0);
           // cv::threshold(srcImg,threshold,150.0,255.0,THRESH_BINARY);
            // memcpy(cameraFrame,srcImg.data,height*width );
            ///////////////////
            //Copy over Y channel
          
            memcpy(videoFrame->bits(),cameraFrame,height*width);

            //Convert semiplanar UV to planar UV
            convertUVsp2UVp(cameraFrame + height*width, videoFrame->bits() + height*width, height/2*width/2);


#endif

        }

        //Export camera image
        if(cvImageBuf){

#ifdef ANDROID //Assume YUV420sp camera image
            memcpy(cvImageBuf,cameraFrame,height*width*3/2);
#endif

        }

        emit imageReady();
    }
}

//*****************************************************************************
// CameraThread implementation
//*****************************************************************************

CameraThread::CameraThread(BetterVideoCapture* camera, QVideoFrame* videoFrame, unsigned char* cvImageBuf, int width, int height)
{
    task = new CameraTask(camera,videoFrame,cvImageBuf,width,height);
    task->moveToThread(&workerThread);
    connect(&workerThread, SIGNAL(started()), task, SLOT(doWork()));
    connect(task, SIGNAL(imageReady()), this, SIGNAL(imageReady()));
}

CameraThread::~CameraThread()
{
    stop();
    delete task;
}

void CameraThread::start()
{
    workerThread.start();
}

void CameraThread::stop()
{
    if(task != NULL)
        task->stop();
    workerThread.quit();
    workerThread.wait();
}

