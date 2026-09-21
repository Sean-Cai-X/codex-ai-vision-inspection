
#ifndef _MainFrm_Header
#define _MainFrm_Header

#include "../ElementProcess/ElementFactory.h"
#include "../ImageProcess/IImageProcess.h"

class MainFrm {
  MainFrm();
  ~MainFrm();

  static list<CElementBase *> list_Elements;
  static CElementBase *focusEle;
  static list<CElementBase *> selectElements;
  static list<CElementBase *> fullElements;
  static vector<CElementBase *> mySolutionList;
  static bool DrawManualPoint();
  static bool DrawRectPoint(const cv::Mat &matInput, float fRatio);
  static bool DrawCirclePoint(Mat matInput, float fRatio);
  static bool DrawLine(Mat matInput, float fRatio);
  static bool DrawCircle(Mat matInput, float fRatio);
  static bool DrawArc(Mat matInput, float fRatio);
  static bool DrawRect(Mat matInput, float fRatio);
  static bool DrawEllipse(Mat matInput, float fRatio);
  static bool DrawORing(Mat matInput, float fRatio);
  static bool DrawContourCircle(Mat matInput, float fRatio);
  static bool DrawClosedContour(Mat matInput, float fRatio);
  static bool GetNearestPoint(Mat matInput, float fRatio);
  static bool GetFarthestPoint(Mat matInput, float fRatio);

  IImageProcess *m_pProc;

  static vector<Point2f> GetPointFromCircleArea(const Mat &imgInput,
                                                cv::Point ptCenter,
                                                float fRadius,
                                                cv::Point &ptOutput);

  static vector<cv::Point2f>
  GetPointFromRectArea(const Mat &imgInput, const vector<cv::Point2f> &ptIn,
                       float nThickness, cv::Point2f &ptOutput);

  static vector<Point2f> ProcessLineSegment(const Mat &imgInput, double scale,
                                            POINT pt1, POINT pt2,
                                            POINT &ptOutStart, POINT &ptOutEnd,
                                            bool &bIsBlackToWhite);

  static vector<Point2f> ProcessCircle(const Mat &imgInput, Point2f ptArr[],
                                       int nThickness, Point2f &ptCenter,
                                       double &dRadius,
                                       Rect2f &rcBoundingCircle,
                                       bool &bIsBlackToWhite);

  static vector<Point2f> ProcessArc(const Mat &imgInput, cv::Point ptArr[],
                                    int nThickness, cv::Point &ptCenter,
                                    double &dRadius, cv::Point &ptStart,
                                    cv::Point &ptEnd, cv::Point &ptArcCen,
                                    bool &bIsBlackToWhite);

  static vector<Point2f> ProcessRectRing(const Mat &imgInput,
                                         const vector<cv::Point> &ptIn,
                                         int nThickness, double &angle,
                                         Point2f &ptOutStart, Point2f &ptOutEnd,
                                         bool &IsFromBlackToWhite);

  static vector<Point2f> ProcRectPoints(vector<Point2f> vecPoints, int nNum,
                                        Point2f ptStart, Point2f ptEnd,
                                        void *pProcess);

  static vector<Point2f> ProcessEllipseRing(const Mat &imgInput,
                                            cv::Point ptArr[], int nThickness,
                                            RotatedRect &rotRect,
                                            bool &bIsBlackToWhite);

  static vector<Point2f> ProcessORing(const Mat &imgInput, cv::Point ptInput,
                                      double dRadius, cv::Point &ptCenter1,
                                      double &dRadius1, cv::Point &ptCenter2,
                                      double &dRadius2, Rect &rcBoundingCircle);

  static vector<Point2f> ProcessContourCircle(const Mat &imgInput,
                                              Point2f ptInput, double dRadius,
                                              Point2f &ptCenterOutput,
                                              double &dRadiusOutput,
                                              Rect2f &rcBoundingCircle);

  static vector<Point2f> ProcessClosedContour(const Mat &imgInput,
                                              Point2f ptInput, double dRadius);

  static vector<Point2f> GetNearestPointInROI(const Mat &imgInput,
                                              cv::Point ptInput[],
                                              int nThickness,
                                              Point2f &ptOutput);

  static vector<Point2f> GetFarthestPointInROI(const Mat &imgInput,
                                               cv::Point ptInput[],
                                               int nThickness,
                                               Point2f &ptOutput);

  static int ProcessOpenFollower0(vector<Point2f> &ptMcs,
                                  vector<Point2f> &ptClick, double distThresh);
  static int ProcessOpenFollowerN(vector<Point2f> &ptAll,
                                  vector<Point2f> &ptMcs,
                                  const vector<Point2f> &ptClick,
                                  double distThresh);

  static int AutoFocus();
  static UINT FocusThreadProc(LPVOID pParam);

  int AutoFucosForOpticalCali();

  static void SolutionOkCallBack(const std::string &uid);
  static void SeletShapesCallBack(vector<std::string> uids);
  static void ZoomViewCallBack(bool IsUp);
  static void ShowSolutionDialog(vector<CElementBase *> solutionList);
  static void AddElement(ElementType typeName, ShapeRecognition rec,
                         tuple<string, string, string> tuples,
                         bool isUpdate = false);
  static CElementBase *InitElement(ElementType typeName, ShapeRecognition rec,
                                   tuple<string, string, string> tuples);
  static bool SetupView(int i);
  static bool ZoomSubView(int value);
  static bool ZoomView(int i);
  static void EraseALL();
  static void CloseWindow(int nCode);
  static CElementBase *FindPrevCoordSys(CElementBase *curElement);
  static void AddToolType(CElementBase *element, ToolType toolType,
                          ElementType selectEleType, bool isMulPointInput);
  static void HandleTools(CElementBase *&lastElement,
                          list<CElementBase *> selectEles,
                          list<CElementBase *> elements,
                          list<CElementBase *> fullElments, bool isMulPoint);
  static CElementBase *GetCurElement(list<CElementBase *> elements,
                                     string curUid);
  static void TryAddToListElement(list<CElementBase *> &elements,
                                  CElementBase *element);
  static void StructuralElement(CElementBase *element);
  static void ImportElements(vector<CElementBase *> newElements);

  static void OCCViewEraseAll();

  static void doOpticalCali();
  static void doPlatFormCali();
  static void doVerticalCali();
  static void doLightCali();
  static void OpenFileNew();
  static void OpenFileOld();
  static void FileSave();
  static void FileSaveAs();
  static void FileImportIWP();
  static void OnEleSave();
};
#endif