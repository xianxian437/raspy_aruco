#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>
#include <opencv2/aruco.hpp>
#include <opencv2/opencv.hpp>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float32.h>
#include "std_msgs/MultiArrayLayout.h"
#include "std_msgs/MultiArrayDimension.h"
#include <vector>

using namespace cv;
using namespace std;

class ImageConverter
{
  ros::NodeHandle nh_;
  image_transport::ImageTransport it_;
  image_transport::Subscriber image_sub_;
  image_transport::Publisher image_pub;
  ros::NodeHandle n2;
  ros::Publisher pub;
  ros::Publisher yaw_pub;
  Mat cameraMatrix;
  Mat distCoeffs;

public:
  ImageConverter()
    : it_(nh_)
  {
    // Subscrive to input video feed and publish output video feed
    image_sub_ = it_.subscribe("iris/camera_red_iris/image_raw", 1,
      &ImageConverter::imageCb, this);
    pub = n2.advertise<std_msgs::Float32MultiArray>("array", 5);
   // yaw_pub = n2.advertise<std_msgs::Float32>("yaw_array", 1);

    image_pub = it_.advertise("/aruco_camera", 1);

    // Defining camera values
    cameraMatrix = (Mat1d(3,3) << 476.7030836014194, 0.0, 400.5, 0.0, 476.7030836014194, 400.5, 0.0, 0.0, 1.0);
    //distCoeffs;
    //aruco::MarkerDetector Detector;
    //std_msgs::Float32MultiArray array;

  }

  void imageCb(const sensor_msgs::ImageConstPtr& msg)
  {
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }

    // ARUCO IMPLEMENTATION //

    Mat image = cv_ptr->image;
    vector<int> markerIds;
    vector< vector<Point2f> > markerCorners;
    Ptr<aruco::Dictionary> dictionary = aruco::getPredefinedDictionary(aruco::DICT_6X6_250);
    aruco::detectMarkers(image, dictionary, markerCorners, markerIds);
    vector<Vec3d> rvecs, tvecs;
    aruco::estimatePoseSingleMarkers(markerCorners, 0.3, cameraMatrix, distCoeffs, rvecs, tvecs);

    aruco::drawDetectedMarkers(cv_ptr->image, markerCorners, markerIds);
    image_pub.publish(cv_ptr->toImageMsg());

    //yaw_pub.publish((rvecs.at(0))[2]);

    std_msgs::Float32MultiArray array;
    // Pose Calculation //
    if (tvecs.size() > 0) {
      Mat rot_mat;
      Mat t_mat = (Mat1f(3,1) << 0.0, 0.0, 0.0);
      Mat xx = (Mat1f(3,1) << 0.0, 0.0, 0.0);
      Mat xx_prime = (Mat1f(3,1) << 0.0,0.0,0.0);  // Center of aruco marker, in aruco frame.
      for (int i = 0; i < 3; i++) {
       t_mat.at<float>(i,0) = (tvecs.at(0))[i];
      }
      Rodrigues(rvecs.at(0), rot_mat);
      rot_mat.convertTo(rot_mat, CV_32F);  // Convert rotation matrix output to 32-bit float.
      xx = rot_mat.t()*xx_prime - rot_mat.t()*t_mat;

      for (int i = 0; i < 3; i++) {
        array.data.push_back(xx.at<float>(i,0));
      }
      pub.publish(array);
    }
    
  }
};

int main(int argc, char** argv)
{
  ros::init(argc, argv, "image_converter");

  ImageConverter ic;
  ros::Rate r(40);

  while (ros::ok()) {
    ros::spinOnce();
    r.sleep();
  }
  return 0;
}