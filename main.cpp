#include <iostream>
#include <opencv2/opencv.hpp>
#include <time.h>
#include <thread>
#include <fstream>
#include <stdlib.h>
using namespace std;
using namespace cv;

#define MAX_USERS 5

bool run_stop;
   
string user[MAX_USERS];

/**
* Convert input text to speech.
*/
void say(string text)
{
   cout << "SAY: " << text << endl;
   string cmd = "espeak -a 200 -p 0 -v english-us \"" + text + "\" >/dev/null &";
   cout << cmd << endl;
   system(cmd.c_str());
   return;
}

/**
* Get similarity ratio between captured image and existing image
*/
float faceDetect(Mat compare, string imgFile)
{
   flip(compare, compare, 0);
   imwrite("/dev/shm/compare.jpg",compare);
   string cmd = "br -algorithm FaceRecognition -compare /dev/shm/compare.jpg " + imgFile + " >/dev/shm/output";
   system(cmd.c_str());
   ifstream fin("/dev/shm/output");
   if(fin.fail())
   {
      cout << "ERROR: Cannot open output file!\n";
   }
   float result;
   fin >> result;
   fin.close();
   return result;
}

/**
* Convert integet to string for use in servo controls
*/
string toString(int i)
{
   ostringstream oss;
   oss << i;
   return oss.str();
}

int main()
{
   cout << "Program started.\n";
   
   // Initialize the camera:
   system("sudo /home/pi/timeout.sh");
   VideoCapture capture(0);
   cout << "Video capture started.\n";
   capture.set(CV_CAP_PROP_FRAME_WIDTH,320);
   capture.set(CV_CAP_PROP_FRAME_HEIGHT,240);
   if(!capture.isOpened()){
	   cout << "Failed to connect to the camera." << endl;
   }
   
   run_stop = false;
   Mat frame1, frame2;
   double min, max;
   Mat diff;
   
   // Initialize users:
   user[0] = "Ryan Hildreth";

   // Initialize servo controls:
   bool movement_detected = false;
   bool direction = 0;
   int x_angle = 60;
   int y_angle = 220;
   string cmd;
   string cmd_secondary;
   
   cout << "Video device initialized.\n";
   
   system("sudo /home/pi/servod --pcm");

   sleep(2);
   cmd = "echo 0=" + toString(y_angle) + ">/dev/servoblaster";
   system(cmd.c_str());
   sleep(1);
   cmd_secondary = "echo 1=" + toString(x_angle) + ">/dev/servoblaster";
   system(cmd_secondary.c_str());
   
   cout << "Servo controller initialized.\n";

   start:
   capture.open(0);
   
   while(!run_stop)
   {
      
      // MOTION DETECTION:
      for(int i = 0 ; i < 6; i++)
      {
         // Get image 1:
         capture >> frame1;
         capture >> frame1;
         cvtColor(frame1, frame1, CV_RGB2GRAY);
         resize(frame1, frame1, Size(), 0.1, 0.1, INTER_LINEAR);

         if(frame1.empty())
         {
	         cout << "Failed to capture an image" << endl;
            return 0;
         }
         
         usleep(700000);
         
         // Get image 2:
         capture >> frame2;
         capture >> frame2;
         cvtColor(frame2, frame2, CV_RGB2GRAY);
         resize(frame2, frame2, Size(), 0.1, 0.1, INTER_LINEAR);

         if(frame2.empty())
         {
	         cout << "Failed to capture an image" << endl;
            return 0;
         }

         // Check for differences...
         absdiff(frame1, frame2, diff);
         
         minMaxIdx(diff, &min, &max);
         
         if(max > 64 && norm(diff, NORM_L2) > 200)
         {
            cout << "Movement detected.\n";
            cout << "Difference          = " << max << endl;
            cout << "Absolute Difference = " << norm(diff, NORM_L2) << endl;
            system("aplay /home/pi/sounds/buzz.wav -q &");
            say("Motion detected. Approach the camera and prepare for identification.");
            sleep(1);

            // FACE RECOGNITION:
            // Get image for comparison (skipping one frame):
            capture >> frame1;
            capture >> frame1;
            
            if(faceDetect(frame1, "/home/pi/faces/0-1.jpg") > 0.70)
            {
               say("User " + user[0] + " detected. Entering standby mode.");
               // Wait 5 minutes, then return to start
               capture.release();
               sleep(300);
               goto start;
            }
            else
            {
               usleep(500000);
               // Try one more time:
               capture >> frame1;
               capture >> frame1;
               if(faceDetect(frame1, "/home/pi/faces/0-1.jpg") > 0.70)
              {
                  say("User " + user[0] + " detected. Entering standby mode.");
                  // Wait 5 minutes, then return to start
                  capture.release();
                  sleep(300);
                  goto start;
              }
              else
              {
                  say("No authorized user found. Unauthorized persons will vacate the area.");
                  sleep(3);
              }
            }
         }

         usleep(700000);
      }
      
      // SERVO CONTROL:
      cout << "Calculating new angles...\n";
      x_angle += (45 - (90 * direction));
      if(x_angle > 250 || x_angle < 70)
      {
         direction = (!direction);
         x_angle += 2 * (45 - (90 * direction));
         //x_angle = 60;
         //if(y_angle == 200)
         //   y_angle = 180;
         //else if(y_angle == 180)
         //   y_angle = 200;
         //cmd = "echo 0=" + toString(y_angle) + ">/dev/servoblaster";
         //system(cmd.c_str());
         //sleep(1);
      }
      cout << "X: " << x_angle << " | " << "Y: " << y_angle << endl;
      cmd_secondary = "echo 1=" + toString(x_angle) + ">/dev/servoblaster";
      system(cmd_secondary.c_str());
      cout << cmd_secondary << endl;
      
      sleep(2);
      system("aplay /home/pi/sounds/beep.wav -q &");
   }
   cout << "Releaseing video device.\n";
   capture.release();
   cout << "Releaseing servo controller.\n";
   system("echo 0=0>/dev/servoblaster");
   system("echo 1=0>/dev/servoblaster");
   return 0;
}
