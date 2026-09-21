#ifndef _Image_Header
#define _Image_Header


#include <opencv2/opencv.hpp> 
#include <opencv2/photo.hpp>

#include <gp_Pnt.hxx>




enum CompositionMode {
    SOURCE_OVER,
    DESTINATION_OVER,
    MULTIPLY,
    SCREEN,
    OVERLAY,
    DARKEN,
    LIGHTEN,
    COLOR_DODGE,
    COLOR_BURN,
    HARD_LIGHT,
    SOFT_LIGHT,
    DIFFERENCE_IMAGE,
    EXCLUSION,
    BITWISE_AND,
    BITWISE_OR,
    BITWISE_XOR
};

[[maybe_unused]] static uchar Red(cv::Vec3b pixel)  { return pixel[2]; }
[[maybe_unused]] static uchar Green(cv::Vec3b pixel)  { return pixel[1]; }
[[maybe_unused]] static uchar Blue(cv::Vec3b pixel)  { return pixel[0]; }
// Edge polarity used by transition-based edge extraction.
enum EdgePolarity {
    WHITE_TO_BLACK =0,
    BLACK_TO_WHITE =1,
    BOTH_POLARITIES=2
};




// Image wraps a cv::Mat and exposes ROI-heavy helpers used by the
// inspection and geometry extraction pipeline.
class Image {
private:
    cv::Mat matImage;
    std::string imagePath;
    int width;
    int height;
    int type;

public:
    Image() ;
    Image(int iw, int ih, int format);

    int m_ishow = 0;

    void SaveROI(const char* pfilename){ (void)pfilename; }
    int getshow();
    void setshow(int ishow);

    void CopyFrom(void* pimg)
    {
        Image* pimage = (Image*)pimg;
        copyFromMat(pimage->getmat());
    }
    void copyFromMat(const cv::Mat& src) {
        src.copyTo(matImage);
        updateImageProperties();
    }
    cv::Mat& getmat() { return matImage; }
    void updateImageProperties();
    void load(const char* pfilename);
    void savefile(const char* pfilename) { save(pfilename == nullptr ? "" : pfilename); }

    void loadfiles(const char* pfilename);
    void reload();
    std::vector<std::string> m_files;
    int m_iloadfilenum = 0;
    int getWidth() const;
    int getHeight() const;
    int getType() const;
    uchar getRed(int x, int y) const;
    uchar getGreen(int x, int y) const;
    uchar getBlue(int x, int y) const;
    void setRed(int x, int y, uchar red) {
        if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
            cv::Vec3b& pixel = matImage.at<cv::Vec3b>(y, x);
            pixel[2] = red;
        }
    }
    void setGreen(int x, int y, uchar green) {
        if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
            cv::Vec3b& pixel = matImage.at<cv::Vec3b>(y, x);
            pixel[1] = green;
        }
    }
    void setBlue(int x, int y, uchar blue) {
        if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
            cv::Vec3b& pixel = matImage.at<cv::Vec3b>(y, x);
            pixel[0] = blue;
        }
    }

    cv::Vec3b Rgb(uchar blue, uchar green, uchar red)
    {
        cv::Vec3b  pixel;
        pixel[0] = blue;
        pixel[1] = green;
        pixel[2] = red;
        return pixel;
    }
    bool save(const std::string& fileName = "") const;
    cv::Vec3b pixel(int x, int y) const;
    cv::Vec4b pixelvalue(int x, int y) const;
    void setPixel(int x, int y, const cv::Vec3b& value);

    void setpixelvalue(int x, int y, const cv::Vec4b& value);
    void convertToGrayScale();
    void resizeImage(double factor);
    void rotateImage(double angle);
    void cropImage(int startX, int startY, int width, int height);
    Image copy() const;
    // Resize the source image into the current ROI using nearest-neighbor
    // sampling so callers can reuse a preallocated destination image.
    void copyResizedToROI(const cv::Mat& src, int x, int y, int w, int h) {
        int dstX = std::max(0, x);
        int dstY = std::max(0, y);
        int dstW = std::min(w, width - dstX);
        int dstH = std::min(h, height - dstY);
        double scaleX = static_cast<double>(dstW) / src.cols;
        double scaleY = static_cast<double>(dstH) / src.rows;
        for (int row = 0; row < dstH; ++row) {
            int srcRow = static_cast<int>(row / scaleY);
            uchar* dstPtr = matImage.ptr<uchar>(dstY + row) + dstX * matImage.channels();
            const uchar* srcPtr = src.ptr<uchar>(srcRow);
            for (int col = 0; col < dstW; ++col) {
                int srcCol = static_cast<int>(col / scaleX);
                for (int c = 0; c < src.channels(); ++c) {
                    if (1 == src.channels() && 3 == matImage.channels())
                    {
                        dstPtr[col * matImage.channels() + c] = srcPtr[srcCol * src.channels() + c];
                        dstPtr[col * matImage.channels() + c + 1] = srcPtr[srcCol * src.channels() + c];
                        dstPtr[col * matImage.channels() + c + 2] = srcPtr[srcCol * src.channels() + c];

                    }
                    else
                    dstPtr[col * matImage.channels() + c] = srcPtr[srcCol * src.channels() + c];
                }
            }
        }
    }

    void copyResizedToROI(const cv::Mat& src) { 
        copyResizedToROI(src, m_ix0, m_iy0, m_iw, m_ih); 
    }
    void resizeImage(int newWidth, int newHeight);
    void blur(int kernelSize);
    void threshold(double thresh, double maxval);
    void invertColors();
    void flipImage(bool horizontal = false, bool vertical = false);
    Image getROI(int startX  , int startY , int width , int height ) const;
    Image getROI() { return getROI(m_ix0, m_iy0, m_iw, m_ih); }
    void setROI(const Image& roi, int startX, int startY);

    int m_ix0 = 100;
    int m_iy0 = 100;
    int m_iw = 500;
    int m_ih = 400;

    int ix0() { return m_ix0; }
    int iy0() { return m_iy0; }
    int iw() { return m_iw; }
    int ih() { return m_ih; }
    void setroi( int startX, int startY , int iw , int ih);
 
    void bitwiseOr(void* pimg)
    {
        Image* pimage = (Image*)pimg;
        bitwiseOr(*pimage);
    }
    void bitwiseOr(const Image& img);

    void bitwiseAnd(void* pimg)
    {
        Image* pimage = (Image*)pimg;
        bitwiseAnd(*pimage);
    }
    void bitwiseAnd(const Image& img);

    void bitwiseOrROI(void* pimg)
    {
        Image* pimage = (Image*)pimg;
        cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
        cv::bitwise_or(matImage(roiRect), pimage->matImage(roiRect), matImage(roiRect));
    }

    void bitwiseAndROI(void* pimg)
    {
        Image* pimage = (Image*)pimg;
        cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
        cv::bitwise_and(matImage(roiRect), pimage->matImage(roiRect), matImage(roiRect));
    }
    void bitwiseOrROI(const Image& img, int startX, int startY);
    void bitwiseAndROI(const Image& img, int startX, int startY);
    void enlargeROI(int startX, int startY, int width, int height, double factor);
    void gaussianBlurROI(int startX, int startY, int width, int height, int kernelSize, double sigmaX);
    void show(const std::string& winName = "Image") const;
    Image sobelEdgeDetection(int dx, int dy, int ksize = 3) const;
    Image laplacianEdgeDetection(int ksize = 3) const;
    Image cannyEdgeDetection(double threshold1, double threshold2, int apertureSize = 3) const;
    Image watershedSegmentation(const cv::Mat& markers) const;
    Image fixedThresholding(double thresh, double maxval, int type = cv::THRESH_BINARY) const;
    Image ratioThresholding(double ratio, double maxval, int type = cv::THRESH_BINARY) const;
    Image otsuThresholding(double maxval, int type = cv::THRESH_BINARY | cv::THRESH_OTSU) const;
    Image adaptiveThresholding(int maxValue, int adaptiveMethod = cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        int thresholdType = cv::THRESH_BINARY, int blockSize = 11, double C = 2) const;


    void roi_5blur_gap_mud_bw( int igap, int ifindBorW,int iusegaus, int irate);
    void roi_7blur_gap_mud_bw( int igap, int ifindBorW,int iusegaus, int irate);
    void roi_5blur_gap_mud_bw_h( int igap, int ifindBorW,int iusegaus, int irate);
    void roi_7blur_gap_mud_bw_h( int igap, int ifindBorW,int iusegaus, int irate);


    void roi_5blur_gap_mud_thre_bw(int ithre, int ireserve, int igap, int ifindBorW);
    void roi_7blur_gap_mud_thre_bw(int ithre, int ireserve, int igap, int ifindBorW);

    void roi_5blur_gap_mud_thre_bw_h(int ithre, int increase, int igap, int ifindBorW);
    void roi_7blur_gap_mud_thre_bw_h(int ithre, int increase, int igap, int ifindBorW);

    double roimean();
    double roimagnitude();
    void colorFillConnectedComponents(double minArea, int minWidth, int minHeight, const cv::Scalar& fillColor);
    std::vector<cv::Point> getCentroids(double minArea, int minWidth, int minHeight) const;
    std::vector<cv::Point> getHorizontalMidpoints(double minArea, int minWidth, int minHeight) const;
    std::vector<cv::Point> getVerticalMidpoints(double minArea, int minWidth, int minHeight) const;
    std::vector<std::vector<cv::Point>> findConnectedComponents(double minArea, int minWidth = 0, int minHeight = 0) const;
    std::vector<cv::Rect> getBoundingBoxes(const std::vector<std::vector<cv::Point>>& components) const;
    std::vector<int> calculateAreas(const std::vector<std::vector<cv::Point>>& components) const;

    void TestROI(int ix0, int iy0, int ix1, int iy1, int ix2, int iy2);

    void colorizeROI(int ib, int ig, int ir);
    void colorizeROI(const cv::Scalar& fillColor);
    void colorizePoints(const std::vector<gp_Pnt>& points, const cv::Scalar& fillColor);
     
    void generateHeatmap(const std::vector<gp_Pnt>& points,  double radius);
    void pyramidThresholding(double dlevels, double dblockSize, double offset = 0);
    Image pyramidDynamicThresholding(double dlevels, double dblockSize = 11, double offset = 0);
    void classifyConnectedComponentsByPyramid(int levels, double minArea = 100, double aspectRatioThreshold = 1.5);
    std::vector<std::vector<cv::Point>> findConnectedComponents(double minArea, const cv::Mat& img) const;

    void erodeROI(int erosionSize);
    void erodeROI(const cv::Rect& roiRect, int erosionSize);
    void enhanceImage(double alpha, int beta);
    void enhanceROI(const cv::Rect& roiRect, double alpha, int beta);
    void erodeVertical(int erosionHeight);
    void erodeHorizontal(int erosionWidth);
    void erodeVerticalROI(int erosionHeight);
    void erodeHorizontalROI(int erosionWidth);
    void erodePoints(const std::vector<gp_Pnt>& points, int erosionSize);
    void enhancePoints(const std::vector<gp_Pnt>& points, double alpha, int beta);
    void openROI(const cv::Rect& roiRect, int kernelSize);
    void closeROI(const cv::Rect& roiRect, int kernelSize);
    int find(std::vector<int>& parent, int x);
    void unite(std::vector<int>& parent, std::vector<int>& rank, int x, int y);
     double colorDistance(const cv::Vec3b& color1, const cv::Vec3b& color2);
     void analyzeConnectedComponentsColor(double colorThreshold = 50.0, double minArea = 100, double aspectRatioMin = 0.5, double aspectRatioMax = 2.0);
     void analyzeConnectedComponentsPyramid(double minArea = 100, double aspectRatioMin = 0.5, double aspectRatioMax = 2.0);


     void SetMode(int imode) { (void)imode; }

     void applyCompositionMode(cv::Mat& srcImage, cv::Mat& dstImage,
         int srcX0, int srcY0, int dstX0, int dstY0,
         int width, int height, CompositionMode mode);

     void ROItoROI_(cv::Mat& srcImage, cv::Mat& dstImage,
         int srcX0, int srcY0, int srcWidth, int srcHeight,
         int dstX0, int dstY0, int dstWidth, int dstHeight);

     void ROItoROI(Image& img);

     void ROIColorTable() {}
     void ROIColorTableBlur(int iGauss_Smoth, int ithre) { (void)iGauss_Smoth; (void)ithre; }
     void ROIColorTableEasyThre(int iandor, int ioffset) { (void)iandor; (void)ioffset; }
     void ROIEasyThre(int ithre = 255);
     void Denoising(int searchWindowSize = 21, int blockWindowSize = 7, float h = 3, float hColor = 3);
     void DenoisingMulti(int searchWindowSize = 21, int blockWindowSize = 7, float h = 10.0f, float hColor = 3);


    void pyrDown();
    void ROISobel(int ix, int iy, int ksize);
    void ROIScharr(int ix, int iy);

    void ROIpyrDown(int ilevel);
    void ROIDenoising(double searchWindowSize = 21, double blockWindowSize = 7, double h = 3.0f);
 
    void ROIDenoisingMulti(double searchWindowSize = 21, double blockWindowSize = 7, double h = 10.0f);

    void getshape(void* pshape);

    void GetSubPixel(void* pshape);


    static std::tuple<cv::Point2f, float> CircleFit_(std::vector<cv::Point2f>& vecPt);
    static int GetLargestCircle(cv::Mat matInput, cv::Point2f& ptOut, double& radiusOut);
    static std::vector<cv::Point2f> FitLine_(std::vector<cv::Point2f>& ptIn, std::vector<cv::Point2f>& ptAnchor, int nThickness);
    // Refine edge samples around detected boundary pixels.
    static std::vector<cv::Point2f> SubpixelProcess(
        const cv::Mat& gray,
        const std::vector<cv::Point>& pixelPoints,
        std::vector<int>& boundaryIndices,
        double dThreshold,
        int localRange = 3,
        int subPixelDensity = 5);


    void FitLightImage();
    // Bicubic color sampling for anti-aliased image queries.
    cv::Vec3f getAntiAliasColor(const cv::Mat& color, const cv::Point2f& pt);
    cv::Vec3f getRobustAntiAliasColor(const cv::Mat& color, const cv::Point2f& pt);
     
    cv::Mat m_tmplImg;

    void genTmplImage(const cv::Mat& img);

    void featureDet2(const cv::Mat& src_image, std::vector<cv::Point2f>& corners);
    
    void Test(void* pshape);
};

#endif // _Image_Header
