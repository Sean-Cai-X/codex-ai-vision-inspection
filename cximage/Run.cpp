#include "pch.h"

#include "Run.h"

#include "Image.h"
#include "shapebase.h"
#include "Shape.h"
#include "imagemanager.h"

#include "FindLine.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindObject.h"
#include "FastMatch.h"

 
void RunClass::Run()
{ 
#if 0
    ImageManager m_amodule;
#endif
    Image m_occtimage;
    FastMatch m_Match;
	
    m_occtimage.load("1.bmp");
    m_Match.setrect(550, 500, 1100, 1100);
    m_Match.setobjfilter(1);
    m_Match.SetWHgap(5, 5);
    m_Match.setthre(35);
    m_Match.setlinegap(3);
    m_Match.setcomparegap(20);

    m_Match.learn(&m_occtimage);
    if (0)
        m_Match.savemodelfile("D:\\testrun001.pat");

    m_Match.loadmodelfile("D:\\testrun001.pat");
    m_Match.setmatchrect(50, 50, 2200, 1900);
    m_Match.matchstepgap(10, 10);
    m_Match.setmatchthre(10);
    m_Match.setminscore(0.65);
    m_Match.setfindnum(1);
    m_Match.match(&m_occtimage);

    double dvalue1 = m_Match.getmaxresult();
    double dvaluex = m_Match.getresolvedresultcentx(-1);
    double dvaluey = m_Match.getresolvedresultcenty(-1);
   
}

#include <fstream>
#include <iostream>
#include "CxUnifiedLog.h"
using namespace std;

#if 0
ImageManager * RunClass::g_pmodule = nullptr;
ImageManager* RunClass::newmodule()
{
    if (g_pmodule == nullptr)
    {
        static ImageManager _themodule;
        return &_themodule;
    }
    else 
        return  g_pmodule;
}
#endif
double RunClass::fitcircle_(cv::Mat matInput, cv::Point2f& ptOut, double& radiusOut)
{
#if 0
    ImageManager m_amodule;
#endif
    Image m_occtimage; 
    FindCircle afindcircle0;
    FindCircle afindcircle1;
    m_occtimage.copyFromMat(matInput);
    CXLOG_INFO("Run", "image_copy", "ok", "copy ok");
    if (0 == m_isetcircle)
    { 
        afindcircle0.setcircle(850, 690, 0, 690);

        afindcircle0.setmethod(1);
        afindcircle0.Setgap(5); 
        afindcircle0.setmethod(0);
        afindcircle0.setthre(20);
        afindcircle0.setlinegap(3);


        afindcircle1.setcircle(850, 690, 0, 690);

        afindcircle1.setmethod(1);
        afindcircle1.Setgap(5);
        afindcircle1.setmethod(1);
        afindcircle1.setthre(20);
        afindcircle1.setlinegap(3);

        m_isetcircle = 1;
    }
    afindcircle0.measure(&m_occtimage);
    afindcircle0.fitcircle();
    afindcircle0.setfitmeasuregap(80);
    afindcircle0.FitResultMeasure(&m_occtimage);
  
    cv::Point2f point0;
    double dradius0 = afindcircle0.getradius();
    double davgdist0 = afindcircle0.getavgdist();

    point0.x = afindcircle0.getresultcentx();
    point0.y = afindcircle0.getresultcenty();

    afindcircle1.measure(&m_occtimage);
    afindcircle1.fitcircle();
    afindcircle1.setfitmeasuregap(80);
    afindcircle1.FitResultMeasure(&m_occtimage);

    cv::Point2f point1;
    double dradius1 = afindcircle0.getradius();
    double davgdist1 = afindcircle0.getavgdist();

    point1.x = afindcircle0.getresultcentx();
    point1.y = afindcircle0.getresultcenty();

    if (davgdist0 < davgdist1)
    {

        ptOut.x = point0.x;

        ptOut.y = point0.y;

        radiusOut = dradius0;

    }
    else
    {

        ptOut.x = point1.x;

        ptOut.y = point1.y;

        radiusOut = dradius1;

    }
     

 
    CXLOG_INFO("Run", "algorithm_run", "ok", "run ok");
  
	return 0;
}
double RunClass::testrun()
{
   cv::Mat matImage = cv::imread("1.bmp");
   cv::Point2f apoint;
   double radiusOut;
   return apoint.x;
}
 

double RunClass::ProcessIdentifyCoordPattern(cv::Mat matInput, std::vector<cv::Point2f>& ptOut,
    cv::Rect& scanrect, cv::Rect& outrect)
{
#if 0
    newmodule();
#endif
    Image m_occtimage;
    FastMatch m_Match;

    m_occtimage.copyFromMat(matInput);
    {
        int ix = scanrect.x;
        int iy = scanrect.y;
        int iw = scanrect.width;
        int ih = scanrect.height;

        m_Match.setrect(ix, iy, iw, ih);
        m_Match.setobjfilter(1);
        m_Match.SetWHgap(15, 15);
        m_Match.setthre(16);
        m_Match.setlinegap(3);
        m_Match.setcomparegap(20);
        m_Match.learn(&m_occtimage);
        m_Match.savemodelfile("D:\\testrun001.pat");
        m_Match.ABtoShape(ptOut);
        m_Match.ZeroPOS();
        m_Match.pattern2org();

    }
    double dpatternsize = m_Match.ABpatternsize();
    if (dpatternsize == 0)
    {
        m_Match.loadmodelfile("D:\\testrun001.pat");
        m_Match.pattern2org();
    }
    dpatternsize = m_Match.ABpatternsize();
    if (dpatternsize > 0)
        if (0)
        {

            int ix = scanrect.x;
            int iy = scanrect.y;
            int iw = scanrect.width;
            int ih = scanrect.height;

            m_Match.patternABsample(3);
            m_Match.setmatchrect(ix, iy, iw, ih);
            m_Match.matchstepgap(10, 10);
            m_Match.setmatchthre(10);
            m_Match.setminscore(0.65);
            m_Match.setfindnum(1);
            m_Match.match(&m_occtimage);

            m_Match.org2pattern();
            m_Match.patternABgap2gap(0.2);
            m_Match.setmatchthre(10);
            m_Match.setminscore(0.65);
            m_Match.setfindnum(1);
            m_Match.matchmore(&m_occtimage);
        }

    CXLOG_INFO("Run", "algorithm_run", "ok", "run ok");
    double dvalue1 = m_Match.getmaxresult();
    double dvaluex = m_Match.getresolvedresultcentx(-1);
    double dvaluey = m_Match.getresolvedresultcenty(-1);

    return 0;
}

double RunClass::ProcessIdentifyCoordMatch(cv::Mat matInput, std::vector<cv::Point2f>& ptOut,
    cv::Rect& scanrect, cv::Rect& outrect)
{
#if 0
    newmodule();
#endif
    Image m_occtimage;
    FastMatch m_Match;

    m_occtimage.copyFromMat(matInput);
  
    m_Match.loadrotatemodelfile("D:\\testrun001.pat");

    double dpatternsize = m_Match.ABpatternsize();
    if (dpatternsize > 0) 
    {
        int ix = scanrect.x;
        int iy = scanrect.y;
        int iw = scanrect.width;
        int ih = scanrect.height;

        m_Match.samplemodelAB(100);
        m_Match.setmatchrect(ix, iy, iw, ih);
        m_Match.matchstepgap(10, 10);
        m_Match.setmatchthre(10);
        m_Match.setminscore(0.65);
        m_Match.setfindnum(1);
        m_Match.setrotateanglescale(-15, 15);
        m_Match.rotatematchAB(&m_occtimage);
     }

    CXLOG_INFO("Run", "algorithm_run", "ok", "run ok");
    double dvalue1 = m_Match.getrotateresultscore();
    double dvaluex = m_Match.getrotateresultcentx(-1);
    double dvaluey = m_Match.getrotateresultcenty(-1);
     

    m_Match.getrotateresultrectpoints(ptOut);
 
    return 0;
} 
