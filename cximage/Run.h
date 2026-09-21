#ifndef _Run_Header
#define _Run_Header


#include <opencv2/opencv.hpp> 
class ImageManager;
class  RunClass
{
public:
    RunClass() {};          // Class Constructor
    ~RunClass() {};         // Class destructor


    int m_isetcircle = 0;
    void Run();
    double fitcircle_(cv::Mat matInput, cv::Point2f& ptOut, double& radiusOut);
    double ProcessIdentifyCoordPattern(cv::Mat matInput,std::vector<cv::Point2f>& ptOut,
        cv::Rect& scanrect,cv::Rect& outrect);

    double ProcessIdentifyCoordMatch(cv::Mat matInput, std::vector<cv::Point2f>& ptOut,
        cv::Rect& scanrect, cv::Rect& outrect);

    double testrun();



#if 0
    static ImageManager* g_pmodule;
    static ImageManager* newmodule();
#endif

   
};

#endif
