#include "pch.h"

#include "Image.h"
#include "Shape.h"
#include "shapebase.h"

#include "Sysctl.h"

#include <opencv2/opencv.hpp>

#include <cmath>
#include <vector>

#include <algorithm>
#include <chrono>
#include <iostream>

using namespace cv;
using namespace std;
using namespace chrono;

namespace fs = std::filesystem;
Image::Image() : imagePath(""), width(0), height(0), type(CV_8UC3) {
  matImage = cv::Mat(1536, 2048, CV_8UC3);
}
int Image::getshow() { return m_ishow; }
void Image::setshow(int ishow) { m_ishow = ishow; }

Image::Image(int iw, int ih, int format) {
  width = iw;
  height = ih;
  type = format;

  matImage = cv::Mat(height, width, format);

  if (format == CV_32FC1 || format == CV_32FC3) {
    matImage.setTo(cv::Scalar(0));
  }
}

void Image::updateImageProperties() {
  if (!matImage.empty()) {
    height = matImage.rows;
    width = matImage.cols;
    type = matImage.type();
  }
}

void Image::load(const char *pfilename) {
  imagePath = pfilename;
  matImage = cv::imread(imagePath);
  resizeImage(2048, 1536);
  if (matImage.empty())
    return;
  updateImageProperties();
}
void Image::reload() {
  if (m_files.size() > m_iloadfilenum) {
    string filename = m_files[m_iloadfilenum];
    load(filename.c_str());
  } else
    m_iloadfilenum = 0;
}
void Image::loadfiles(const char *pfilename) {
  m_files = DirFileFind(string(pfilename), string(".jpg"));
  if (m_files.size() <= 0) {
    m_files = DirFileFind(string(pfilename), string(".bmp"));
  }
  if (m_files.size() <= 0) {
    m_files = DirFileFind(string(pfilename), string(".png"));
  }
  if (m_files.size() > m_iloadfilenum) {
    string filename = m_files[m_iloadfilenum];

    load(filename.c_str());
    m_iloadfilenum++;
  } else
    m_iloadfilenum = 0;
}
int Image::getWidth() const { return width; }

int Image::getHeight() const { return height; }

int Image::getType() const { return type; }

uchar Image::getRed(int x, int y) const {
  if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
    return matImage.at<cv::Vec3b>(y, x)[2];
  }
  return 0;
}

uchar Image::getGreen(int x, int y) const {
  if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
    return matImage.at<cv::Vec3b>(y, x)[1];
  }
  return 0;
}

uchar Image::getBlue(int x, int y) const {
  if (matImage.channels() >= 3 && x >= 0 && x < width && y >= 0 && y < height) {
    return matImage.at<cv::Vec3b>(y, x)[0];
  }
  return 0;
}

bool Image::save(const std::string &fileName) const {
  std::string targetPath = fileName.empty() ? imagePath : fileName;
  if (matImage.empty() || targetPath.empty())
    return false;
  return cv::imwrite(targetPath, matImage);
}

cv::Vec3b Image::pixel(int x, int y) const {
  return matImage.at<cv::Vec3b>(y, x);
}
void Image::setPixel(int x, int y, const cv::Vec3b &value) {
  matImage.at<cv::Vec3b>(y, x) = value;
}
cv::Vec4b Image::pixelvalue(int x, int y) const {
  return matImage.at<cv::Vec4b>(y, x);
}
void Image::setpixelvalue(int x, int y, const cv::Vec4b &value) {
  matImage.at<cv::Vec4b>(y, x) = value;
}

void Image::convertToGrayScale() {
  if (matImage.channels() == 3) {
    cvtColor(matImage, matImage, cv::COLOR_BGR2GRAY);
    updateImageProperties();
  }
}

void Image::resizeImage(double factor) {
  cv::resize(matImage, matImage, cv::Size(), factor, factor, cv::INTER_LINEAR);
  updateImageProperties();
}

void Image::rotateImage(double angle) {
  cv::Point2f center(static_cast<float>((width - 1) / 2.0),
                     static_cast<float>((height - 1) / 2.0));
  cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
  cv::Rect2f bbox = cv::RotatedRect(cv::Point2f(),
                                    cv::Size2f(static_cast<float>(width),
                                               static_cast<float>(height)),
                                    static_cast<float>(angle))
                        .boundingRect2f();
  rot.at<double>(0, 2) += bbox.width / 2.0 - width / 2.0;
  rot.at<double>(1, 2) += bbox.height / 2.0 - height / 2.0;
  cv::warpAffine(matImage, matImage, rot, bbox.size());
  cv::Rect roi(0, 0, width, height);
  matImage = matImage(roi);
  updateImageProperties();
}

void Image::cropImage(int startX, int startY, int cropWidth, int cropHeight) {
  cv::Rect roi(startX, startY, cropWidth, cropHeight);
  matImage = matImage(roi);
  updateImageProperties();
}

Image Image::copy() const {
  Image imgCopy;
  imgCopy.matImage = matImage.clone();
  imgCopy.updateImageProperties();
  return imgCopy;
}

void Image::resizeImage(int newWidth, int newHeight) {
  cv::resize(matImage, matImage, cv::Size(newWidth, newHeight));
  updateImageProperties();
}

void Image::blur(int kernelSize) {
  cv::blur(matImage, matImage, cv::Size(kernelSize, kernelSize));
}

void Image::threshold(double thresh, double maxval) {
  cv::threshold(matImage, matImage, thresh, maxval, cv::THRESH_BINARY);
}

void Image::invertColors() { cv::bitwise_not(matImage, matImage); }

void Image::flipImage(bool horizontal, bool vertical) {
  int code = 0;
  if (horizontal)
    code += 1;
  if (vertical)
    code += 0;
  cv::flip(matImage, matImage, code);
}

Image Image::getROI(int startX, int startY, int roiWidth, int roiHeight) const {
  Image roiImg;
  roiImg.matImage =
      matImage(cv::Rect(startX, startY, roiWidth, roiHeight)).clone();
  roiImg.updateImageProperties();
  return roiImg;
}

void Image::setROI(const Image &roi, int startX, int startY) {
  cv::Rect roiRect(startX, startY, roi.getWidth(), roi.getHeight());
  roi.matImage.copyTo(matImage(roiRect));
}

void Image::getshape(void *pshape) {
  Shape *pshape0 = (Shape *)pshape;
  if (pshape0 == nullptr)
    return;

  setroi(static_cast<int>(pshape0->rect().TopLeft().X()),
         static_cast<int>(pshape0->rect().TopLeft().Y()),
         static_cast<int>(pshape0->rect().Width()),
         static_cast<int>(pshape0->rect().Height()));
}
void Image::setroi(int startX, int startY, int iw, int ih) {
  m_ix0 = startX;
  m_iy0 = startY;
  m_iw = iw;
  m_ih = ih;
}

void Image::bitwiseOr(const Image &img) {
  cv::bitwise_or(matImage, img.matImage, matImage);
}

void Image::bitwiseAnd(const Image &img) {
  cv::bitwise_and(matImage, img.matImage, matImage);
}

void Image::bitwiseOrROI(const Image &img, int startX, int startY) {
  cv::Rect roiRect(startX, startY, img.getWidth(), img.getHeight());
  cv::bitwise_or(matImage(roiRect), img.matImage, matImage(roiRect));
}

void Image::bitwiseAndROI(const Image &img, int startX, int startY) {
  cv::Rect roiRect(startX, startY, img.getWidth(), img.getHeight());
  cv::bitwise_and(matImage(roiRect), img.matImage, matImage(roiRect));
}

void Image::enlargeROI(int startX, int startY, int roiWidth, int roiHeight,
                       double factor) {
  cv::Rect roiRect(startX, startY, roiWidth, roiHeight);
  cv::Mat enlargedRoi;
  cv::resize(matImage(roiRect), enlargedRoi, cv::Size(), factor, factor,
             cv::INTER_LINEAR);
  cv::Rect targetRect(startX, startY, enlargedRoi.cols, enlargedRoi.rows);
  enlargedRoi.copyTo(matImage(targetRect));
  updateImageProperties();
}

void Image::gaussianBlurROI(int startX, int startY, int roiWidth, int roiHeight,
                            int kernelSize, double sigmaX) {
  cv::Rect roiRect(startX, startY, roiWidth, roiHeight);
  cv::GaussianBlur(matImage(roiRect), matImage(roiRect),
                   cv::Size(kernelSize, kernelSize), sigmaX);
}

void Image::show(const std::string &winName) const {
  cv::imshow(winName, matImage);
  cv::waitKey(0);
}

Image Image::sobelEdgeDetection(int dx, int dy, int ksize) const {
  Image result;
  cv::Mat grad;
  cv::Sobel(matImage, grad, CV_16S, dx, dy, ksize);
  convertScaleAbs(grad, grad);
  result.matImage = grad;
  result.updateImageProperties();
  return result;
}

Image Image::laplacianEdgeDetection(int ksize) const {
  Image result;
  cv::Mat laplacian;
  cv::Laplacian(matImage, laplacian, CV_16S, ksize);
  convertScaleAbs(laplacian, laplacian);
  result.matImage = laplacian;
  result.updateImageProperties();
  return result;
}

Image Image::cannyEdgeDetection(double threshold1, double threshold2,
                                int apertureSize) const {
  Image result;
  cv::Mat edges;
  cv::cvtColor(matImage, edges, cv::COLOR_BGR2GRAY);
  cv::Canny(edges, edges, threshold1, threshold2, apertureSize);
  result.matImage = edges;
  result.updateImageProperties();
  return result;
}

Image Image::watershedSegmentation(const cv::Mat &markers) const {
  Image result;
  cv::Mat imgGray;
  if (matImage.channels() == 3)
    cvtColor(matImage, imgGray, cv::COLOR_BGR2GRAY);
  else
    imgGray = matImage;

  cv::watershed(matImage, markers);
  result.matImage = markers.clone();
  result.updateImageProperties();
  return result;
}

Image Image::fixedThresholding(double thresh, double maxval,
                               int thresholdType) const {
  Image result;
  cv::Mat gray;
  if (matImage.channels() == 3)
    cvtColor(matImage, gray, cv::COLOR_BGR2GRAY);
  else
    gray = matImage;

  cv::threshold(gray, result.matImage, thresh, maxval, thresholdType);
  result.updateImageProperties();
  return result;
}

Image Image::ratioThresholding(double ratio, double maxval,
                               int thresholdType) const {
  double thresh = cv::mean(matImage)[0] * ratio;
  return fixedThresholding(thresh, maxval, thresholdType);
}

Image Image::otsuThresholding(double maxval, int thresholdType) const {
  Image result;
  cv::Mat gray;
  if (matImage.channels() == 3)
    cvtColor(matImage, gray, cv::COLOR_BGR2GRAY);
  else
    gray = matImage;

  cv::threshold(gray, result.matImage, 0, maxval, thresholdType);
  result.updateImageProperties();
  return result;
}

Image Image::adaptiveThresholding(int maxValue, int adaptiveMethod,
                                  int thresholdType, int blockSize,
                                  double C) const {
  if (blockSize <= 0 || blockSize % 2 == 0) {
    blockSize = blockSize + 1;
  }

  Image result;
  cv::Mat gray;

  if (matImage.channels() == 3)
    cvtColor(matImage, gray, cv::COLOR_BGR2GRAY);
  else
    gray = matImage;

  try {
    cv::adaptiveThreshold(gray, result.matImage, maxValue, adaptiveMethod,
                          thresholdType, blockSize, C);
  } catch (const cv::Exception &e) {
    throw std::runtime_error(
        std::string("Error during adaptive thresholding: ") + e.what());
  }

  result.updateImageProperties();
  return result;
}

#define intx0_5(i) (i >> 1)
#define intx0_25(i) (i >> 2)
#define intx0_125(i) (i >> 3)
#define intx0_062(i) (i >> 4)
#define intx0_031(i) (i >> 5)
#define intx0_015(i) (i >> 6)
#define intx0_007(i) (i >> 7)
#define intx0_003(i) (i >> 8)
#define intx0(i) (i >> 9)
#define intx2(i) (i << 1)
#define intx4(i) (i << 2)
#define intx8(i) (i << 3)
#define intx16(i) (i << 4)
#define intx256(i) (i << 8)

#define intx0_5(i) (i >> 1)
#define intx0_25(i) (i >> 2)
#define intx0_125(i) (i >> 3)
#define intx0_062(i) (i >> 4)
#define intx0_031(i) (i >> 5)
#define intx0_015(i) (i >> 6)
#define intx0_007(i) (i >> 7)
#define intx0_003(i) (i >> 8)
#define intx0(i) (i >> 9)
#define intx2(i) (i << 1)
#define intx4(i) (i << 2)
#define intx8(i) (i << 3)
#define intx16(i) (i << 4)
#define intx256(i) (i << 8)

void Image::roi_5blur_gap_mud_bw(int igap, int ifindBorW, int iusegaus,
                                 int irate) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;
  int tx0 = m_ix0 - 6 > 0 ? m_ix0 - 6 : 3;
  int tx1 =
      m_ix0 + m_iw + 6 < getWidth() ? m_ix0 + m_iw + 3 : getWidth() - 3 - 1;

  if (iusegaus > 0)
    for (int y(m_iy0); y < m_iy0 + m_ih; y++) {
      for (int x(tx0); x < tx1; x++) {
        pixelCur = pixel(x, y);
        pixel_1 = pixel(x - 1, y);
        pixel_2 = pixel(x - 2, y);
        pixel1 = pixel(x + 1, y);
        pixel2 = pixel(x + 2, y);
        int ir0 = Red(pixelCur);
        int ig0 = Green(pixelCur);
        int ib0 = Blue(pixelCur);
        int ir_1 = Red(pixel_1);
        int ig_1 = Green(pixel_1);
        int ib_1 = Blue(pixel_1);
        int ir_2 = Red(pixel_2);
        int ig_2 = Green(pixel_2);
        int ib_2 = Blue(pixel_2);
        int ir1 = Red(pixel1);
        int ig1 = Green(pixel1);
        int ib1 = Blue(pixel1);
        int ir2 = Red(pixel2);
        int ig2 = Green(pixel2);
        int ib2 = Blue(pixel2);

        int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
                 intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
        int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
                 intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
        int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
                 intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

        ir = ir > 255 ? 255 : ir;
        ig = ig > 255 ? 255 : ig;
        ib = ib > 255 ? 255 : ib;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;

        cv::Vec3b blurredPixel =
            Rgb(static_cast<uchar>(ib), static_cast<uchar>(ig),
                static_cast<uchar>(ir));

        if (y - 1 >= 0)
          setPixel(x, y - 1, blurredPixel);
      }
    }

  cv::Vec3b pixelx;

  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;

  if (0 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);
        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;

        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x, y + 1, pixelx);
      }
    }
  } else if (1 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);
        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;

        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x, y + 1, pixelx);
      }
    }
  }
}
void Image::roi_7blur_gap_mud_bw(int igap, int ifindBorW, int iusegaus,
                                 int irate) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;
  int tx0 = m_ix0 - 6 > 0 ? m_ix0 - 6 : 3;
  int tx1 =
      m_ix0 + m_iw + 6 < getWidth() ? m_ix0 + m_iw + 3 : getWidth() - 3 - 1;

  if (iusegaus > 0)
    for (int y(m_iy0); y < m_iy0 + m_ih; y++) {
      for (int x(tx0); x < tx1; x++) {
        pixelCur = pixel(x, y);
        pixel_1 = pixel(x - 1, y);
        pixel_2 = pixel(x - 2, y);
        pixel1 = pixel(x + 1, y);
        pixel2 = pixel(x + 2, y);
        int ir0 = Red(pixelCur);
        int ig0 = Green(pixelCur);
        int ib0 = Blue(pixelCur);
        int ir_1 = Red(pixel_1);
        int ig_1 = Green(pixel_1);
        int ib_1 = Blue(pixel_1);
        int ir_2 = Red(pixel_2);
        int ig_2 = Green(pixel_2);
        int ib_2 = Blue(pixel_2);
        int ir1 = Red(pixel1);
        int ig1 = Green(pixel1);
        int ib1 = Blue(pixel1);
        int ir2 = Red(pixel2);
        int ig2 = Green(pixel2);
        int ib2 = Blue(pixel2);

        int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
                 intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
        int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
                 intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
        int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
                 intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

        ir = ir > 255 ? 255 : ir;
        ig = ig > 255 ? 255 : ig;
        ib = ib > 255 ? 255 : ib;

        cv::Vec3b blurredPixel =
            Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                static_cast<uchar>(ib));

        if (y - 1 >= 0)
          setPixel(x, y - 1, blurredPixel);
      }
    }

  cv::Vec3b pixelx;

  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;

  if (0 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x, y + 1, pixelx);
      }
    }
  } else if (1 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x, y + 1, pixelx);
      }
    }
  }
}
void Image::roi_5blur_gap_mud_bw_h(int igap, int ifindBorW, int iusegaus,
                                   int irate) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getWidth() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel_3, pixel1, pixel2, pixel3;
  cv::Vec3b pixel0;

  if (iusegaus > 0)
    for (int x(m_ix0 + 1); x < m_ix0 + m_iw; x++) {
      for (int y(m_iy0 + 3); y < m_iy0 + m_ih - 3; y++) {
        pixelCur = pixel(x, y);
        pixel_1 = pixel(x, y - 1);
        pixel_2 = pixel(x, y - 2);
        pixel_3 = pixel(x, y - 3);
        pixel1 = pixel(x, y + 1);
        pixel2 = pixel(x, y + 2);
        pixel3 = pixel(x, y + 3);
        int ir0 = Red(pixelCur);
        int ig0 = Green(pixelCur);
        int ib0 = Blue(pixelCur);
        int ir_1 = Red(pixel_1);
        int ig_1 = Green(pixel_1);
        int ib_1 = Blue(pixel_1);
        int ir_2 = Red(pixel_2);
        int ig_2 = Green(pixel_2);
        int ib_2 = Blue(pixel_2);
        int ir_3 = Red(pixel_3);
        int ig_3 = Green(pixel_3);
        int ib_3 = Blue(pixel_3);
        int ir1 = Red(pixel1);
        int ig1 = Green(pixel1);
        int ib1 = Blue(pixel1);
        int ir2 = Red(pixel2);
        int ig2 = Green(pixel2);
        int ib2 = Blue(pixel2);
        int ir3 = Red(pixel3);
        int ig3 = Green(pixel3);
        int ib3 = Blue(pixel3);

        int ir = intx0_062(ir_3) + intx0_125(ir_2) + intx0_25(ir_1) +
                 intx0_25(ir0) + intx0_062(ir0) + intx0_25(ir1) +
                 intx0_125(ir2) + intx0_062(ir3);
        int ig = intx0_062(ig_3) + intx0_125(ig_2) + intx0_25(ig_1) +
                 intx0_25(ig0) + intx0_062(ig0) + intx0_25(ig1) +
                 intx0_125(ig2) + intx0_062(ig3);
        int ib = intx0_062(ib_3) + intx0_125(ib_2) + intx0_25(ib_1) +
                 intx0_25(ib0) + intx0_062(ib0) + intx0_25(ib1) +
                 intx0_125(ib2) + intx0_062(ib3);

        ir = ir > 255 ? 255 : ir;
        ig = ig > 255 ? 255 : ig;
        ib = ib > 255 ? 255 : ib;

        cv::Vec3b blurredPixel =
            cv::Vec3b(static_cast<uchar>(ir), static_cast<uchar>(ig),
                      static_cast<uchar>(ib));

        if (x - 1 >= 0)
          setPixel(x - 1, y, blurredPixel);
      }
    }

  cv::Vec3b pixelx;
  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;

  if (0 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0 + 2; x--) {
      for (int y(m_iy0 + 2); y < m_iy0 + m_ih - igap - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x + 1, y, pixelx);
      }
    }
  } else if (1 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0 + 2); y < m_iy0 + m_ih - igap - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x + 1, y, pixelx);
      }
    }
  }
}
void Image::roi_7blur_gap_mud_bw_h(int igap, int ifindBorW, int iusegaus,
                                   int irate) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;

  if (iusegaus > 0)
    for (int x(m_ix0 + 1); x < m_ix0 + m_iw; x++) {
      for (int y(m_iy0 + 2); y < m_iy0 + m_ih - 2; y++) {
        pixelCur = pixel(x, y);
        pixel_1 = pixel(x, y - 1);
        pixel_2 = pixel(x, y - 2);
        pixel1 = pixel(x, y + 1);
        pixel2 = pixel(x, y + 2);
        int ir0 = Red(pixelCur);
        int ig0 = Green(pixelCur);
        int ib0 = Blue(pixelCur);
        int ir_1 = Red(pixel_1);
        int ig_1 = Green(pixel_1);
        int ib_1 = Blue(pixel_1);
        int ir_2 = Red(pixel_2);
        int ig_2 = Green(pixel_2);
        int ib_2 = Blue(pixel_2);
        int ir1 = Red(pixel1);
        int ig1 = Green(pixel1);
        int ib1 = Blue(pixel1);
        int ir2 = Red(pixel2);
        int ig2 = Green(pixel2);
        int ib2 = Blue(pixel2);

        int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
                 intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
        int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
                 intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
        int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
                 intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

        ir = ir > 255 ? 255 : ir;
        ig = ig > 255 ? 255 : ig;
        ib = ib > 255 ? 255 : ib;

        cv::Vec3b blurredPixel =
            cv::Vec3b(static_cast<uchar>(ir), static_cast<uchar>(ig),
                      static_cast<uchar>(ib));

        if (x - 1 >= 0)
          setPixel(x - 1, y, blurredPixel);
      }
    }

  cv::Vec3b pixelx;

  if (0 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0); y < m_iy0 + m_ih - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x + 1, y, pixelx);
      }
    }
  } else if (1 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0); y < m_iy0 + m_ih - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);

        ir = ir > 0 ? ir : 0;
        ig = ig > 0 ? ig : 0;
        ib = ib > 0 ? ib : 0;
        ir = ir * irate > 255 ? 255 : ir;
        ig = ig * irate > 255 ? 255 : ig;
        ib = ib * irate > 255 ? 255 : ib;
        pixelx = Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
                     static_cast<uchar>(ib));
        setPixel(x + 1, y, pixelx);
      }
    }
  }
}

void Image::roi_5blur_gap_mud_thre_bw(int ithre, int ireserve, int igap,
                                      int ifindBorW) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;
  int tx0 = m_ix0 - 6 > 0 ? m_ix0 - 6 : 3;
  int tx1 =
      m_ix0 + m_iw + 6 < getWidth() ? m_ix0 + m_iw + 3 : getWidth() - 3 - 1;
  for (int y(m_iy0); y < m_iy0 + m_ih; y++) {
    for (int x(tx0); x < tx1; x++) {
      pixelCur = pixel(x, y);
      pixel_1 = pixel(x - 1, y);
      pixel_2 = pixel(x - 2, y);
      pixel1 = pixel(x + 1, y);
      pixel2 = pixel(x + 2, y);
      int ir0 = Red(pixelCur);
      int ig0 = Green(pixelCur);
      int ib0 = Blue(pixelCur);
      int ir_1 = Red(pixel_1);
      int ig_1 = Green(pixel_1);
      int ib_1 = Blue(pixel_1);
      int ir_2 = Red(pixel_2);
      int ig_2 = Green(pixel_2);
      int ib_2 = Blue(pixel_2);
      int ir1 = Red(pixel1);
      int ig1 = Green(pixel1);
      int ib1 = Blue(pixel1);
      int ir2 = Red(pixel2);
      int ig2 = Green(pixel2);
      int ib2 = Blue(pixel2);

      int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
               intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
      int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
               intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
      int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
               intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

      ir = ir > 255 ? 255 : ir;
      ig = ig > 255 ? 255 : ig;
      ib = ib > 255 ? 255 : ib;

      cv::Vec3b blurredPixel =
          Rgb(static_cast<uchar>(ib), static_cast<uchar>(ig),
              static_cast<uchar>(ir));

      if (y - 1 >= 0)
        setPixel(x, y - 1, blurredPixel);
    }
  }

  cv::Vec3b pixelx;

  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;
  if (0 == ireserve) {
    pixelx0 = Rgb(0, 0, 0);
    pixelx1 = Rgb(255, 255, 255);
  } else if (1 == ireserve) {
    pixelx0 = Rgb(255, 255, 255);
    pixelx1 = Rgb(0, 0, 0);
  }
  if (0 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x, y + 1, pixelx);
      }
    }
  } else if (1 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x, y + 1, pixelx);
      }
    }
  }
}
void Image::roi_7blur_gap_mud_thre_bw(int ithre, int ireserve, int igap,
                                      int ifindBorW) {
  std::cout << "[DIAG] roi_7blur_gap_mud_thre_bw enter: threshold=" << ithre
            << " reserve=" << ireserve << " gap=" << igap
            << " method=" << ifindBorW << std::endl;

  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  {
    const cv::Mat &mat = getmat();
    int roi_x0 = m_ix0, roi_y0 = m_iy0, roi_w = m_iw, roi_h = m_ih;
    double min_val = 255, max_val = 0, sum_val = 0;
    int count = 0;
    int sample_step = std::max(1, (roi_w * roi_h) / 5000);
    for (int y = roi_y0; y < roi_y0 + roi_h && y < mat.rows; y += sample_step) {
      for (int x = roi_x0; x < roi_x0 + roi_w && x < mat.cols;
           x += sample_step) {
        if (mat.type() == CV_8UC3) {
          cv::Vec3b p = mat.at<cv::Vec3b>(y, x);
          double gray = 0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0];
          min_val = std::min(min_val, gray);
          max_val = std::max(max_val, gray);
          sum_val += gray;
          count++;
        }
      }
    }
    double mean_val = count > 0 ? sum_val / count : 0;
    std::cout << "[DIAG] input ROI=(" << roi_x0 << "," << roi_y0 << "," << roi_w
              << "," << roi_h << ")"
              << " min=" << min_val << " max=" << max_val
              << " mean=" << mean_val << " samples=" << count << std::endl;
  }

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;
  int tx0 = m_ix0 - 6 > 0 ? m_ix0 - 6 : 3;
  int tx1 =
      m_ix0 + m_iw + 6 < getWidth() ? m_ix0 + m_iw + 3 : getWidth() - 3 - 1;
  for (int y(m_iy0); y < m_iy0 + m_ih; y++) {
    for (int x(tx0); x < tx1; x++) {
      pixelCur = pixel(x, y);
      pixel_1 = pixel(x - 1, y);
      pixel_2 = pixel(x - 2, y);
      pixel1 = pixel(x + 1, y);
      pixel2 = pixel(x + 2, y);
      int ir0 = Red(pixelCur);
      int ig0 = Green(pixelCur);
      int ib0 = Blue(pixelCur);
      int ir_1 = Red(pixel_1);
      int ig_1 = Green(pixel_1);
      int ib_1 = Blue(pixel_1);
      int ir_2 = Red(pixel_2);
      int ig_2 = Green(pixel_2);
      int ib_2 = Blue(pixel_2);
      int ir1 = Red(pixel1);
      int ig1 = Green(pixel1);
      int ib1 = Blue(pixel1);
      int ir2 = Red(pixel2);
      int ig2 = Green(pixel2);
      int ib2 = Blue(pixel2);

      int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
               intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
      int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
               intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
      int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
               intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

      ir = ir > 255 ? 255 : ir;
      ig = ig > 255 ? 255 : ig;
      ib = ib > 255 ? 255 : ib;

      cv::Vec3b blurredPixel =
          Rgb(static_cast<uchar>(ir), static_cast<uchar>(ig),
              static_cast<uchar>(ib));

      if (y - 1 >= 0)
        setPixel(x, y - 1, blurredPixel);
    }
  }

  {
    const cv::Mat &mat = getmat();
    double min_val = 255, max_val = 0;
    int count = 0;
    int sample_step = std::max(1, (m_iw * m_ih) / 5000);
    for (int y = m_iy0; y < m_iy0 + m_ih && y < mat.rows; y += sample_step) {
      for (int x = m_ix0; x < m_ix0 + m_iw && x < mat.cols; x += sample_step) {
        if (mat.type() == CV_8UC3) {
          cv::Vec3b p = mat.at<cv::Vec3b>(y, x);
          double gray = 0.299 * p[2] + 0.587 * p[1] + 0.114 * p[0];
          min_val = std::min(min_val, gray);
          max_val = std::max(max_val, gray);
          count++;
        }
      }
    }
    std::cout << "[DIAG] after blur: min=" << min_val << " max=" << max_val
              << " samples=" << count << std::endl;
  }

  cv::Vec3b pixelx;

  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;
  if (0 == ireserve) {
    pixelx0 = cv::Vec3b(0, 0, 0);
    pixelx1 = cv::Vec3b(255, 255, 255);
  } else if (1 == ireserve) {
    pixelx0 = cv::Vec3b(255, 255, 255);
    pixelx1 = cv::Vec3b(0, 0, 0);
  }
  if (0 == ifindBorW) {
    {
      int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
      int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                   : getWidth() - igap - 1;
      double diff_min = 255, diff_max = 0;
      int diff_count = 0;
      int max_diff_r = 0, max_diff_g = 0, max_diff_b = 0;
      int sample_step = std::max(1, (m_iw * m_ih) / 5000);
      for (int y = m_iy0 + m_ih; y > m_iy0; y -= sample_step) {
        for (int x = tmpx0; x < tmpx1; x += sample_step) {
          cv::Vec3b p1 = pixel(x, y);
          cv::Vec3b p2 = pixel(x + igap, y);
          int dr = Red(p2) - Red(p1);
          int dg = Green(p2) - Green(p1);
          int db = Blue(p2) - Blue(p1);
          int max_diff = std::max({std::abs(dr), std::abs(dg), std::abs(db)});
          diff_min = std::min(diff_min, (double)max_diff);
          diff_max = std::max(diff_max, (double)max_diff);
          max_diff_r = std::max(max_diff_r, std::abs(dr));
          max_diff_g = std::max(max_diff_g, std::abs(dg));
          max_diff_b = std::max(max_diff_b, std::abs(db));
          diff_count++;
        }
      }
      std::cout << "[DIAG] gap diff (method=0): max_diff=" << diff_max
                << " max_r=" << max_diff_r << " max_g=" << max_diff_g
                << " max_b=" << max_diff_b << " threshold=" << ithre
                << " samples=" << diff_count << std::endl;
    }

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x, y + 1, pixelx);
      }
    }
  } else if (1 == ifindBorW) {

    int tmpx0 = m_ix0 > 0 ? m_ix0 : 0;
    int tmpx1 = m_ix0 + m_iw + igap < getWidth() ? m_ix0 + m_iw + igap
                                                 : getWidth() - igap - 1;

    for (int y(m_iy0 + m_ih); y > m_iy0; y--) {
      for (int x(tmpx0); x < tmpx1; x++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x + igap, y);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x, y + 1, pixelx);
      }
    }
  }

  {
    const cv::Mat &mat = getmat();
    int nonzero_count = 0;
    int total_sampled = 0;
    int edge_count = 0;
    int sample_step = std::max(1, (m_iw * m_ih) / 5000);
    for (int y = m_iy0; y < m_iy0 + m_ih && y < mat.rows; y += sample_step) {
      for (int x = m_ix0; x < m_ix0 + m_iw && x < mat.cols; x += sample_step) {
        if (mat.type() == CV_8UC3) {
          cv::Vec3b p = mat.at<cv::Vec3b>(y, x);
          if (p[0] > 0 || p[1] > 0 || p[2] > 0) {
            nonzero_count++;
            edge_count++;
          }
          total_sampled++;
        }
      }
    }
    std::cout << "[DIAG] after threshold: nonzero=" << nonzero_count
              << " edge_count=" << edge_count
              << " total_sampled=" << total_sampled << std::endl;
  }
  std::cout << "[DIAG] roi_7blur_gap_mud_thre_bw exit" << std::endl;
}
void Image::roi_5blur_gap_mud_thre_bw_h(int ithre, int ireserve, int igap,
                                        int ifindBorW) {
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getWidth() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel_3, pixel1, pixel2, pixel3;
  cv::Vec3b pixel0;
  for (int x(m_ix0 + 1); x < m_ix0 + m_iw; x++) {
    for (int y(m_iy0 + 3); y < m_iy0 + m_ih - 3; y++) {
      pixelCur = pixel(x, y);
      pixel_1 = pixel(x, y - 1);
      pixel_2 = pixel(x, y - 2);
      pixel_3 = pixel(x, y - 3);
      pixel1 = pixel(x, y + 1);
      pixel2 = pixel(x, y + 2);
      pixel3 = pixel(x, y + 3);
      int ir0 = Red(pixelCur);
      int ig0 = Green(pixelCur);
      int ib0 = Blue(pixelCur);
      int ir_1 = Red(pixel_1);
      int ig_1 = Green(pixel_1);
      int ib_1 = Blue(pixel_1);
      int ir_2 = Red(pixel_2);
      int ig_2 = Green(pixel_2);
      int ib_2 = Blue(pixel_2);
      int ir_3 = Red(pixel_3);
      int ig_3 = Green(pixel_3);
      int ib_3 = Blue(pixel_3);
      int ir1 = Red(pixel1);
      int ig1 = Green(pixel1);
      int ib1 = Blue(pixel1);
      int ir2 = Red(pixel2);
      int ig2 = Green(pixel2);
      int ib2 = Blue(pixel2);
      int ir3 = Red(pixel3);
      int ig3 = Green(pixel3);
      int ib3 = Blue(pixel3);

      int ir = intx0_062(ir_3) + intx0_125(ir_2) + intx0_25(ir_1) +
               intx0_25(ir0) + intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2) +
               intx0_062(ir3);
      int ig = intx0_062(ig_3) + intx0_125(ig_2) + intx0_25(ig_1) +
               intx0_25(ig0) + intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2) +
               intx0_062(ig3);
      int ib = intx0_062(ib_3) + intx0_125(ib_2) + intx0_25(ib_1) +
               intx0_25(ib0) + intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2) +
               intx0_062(ib3);

      ir = ir > 255 ? 255 : ir;
      ig = ig > 255 ? 255 : ig;
      ib = ib > 255 ? 255 : ib;

      cv::Vec3b blurredPixel =
          cv::Vec3b(static_cast<uchar>(ir), static_cast<uchar>(ig),
                    static_cast<uchar>(ib));

      if (x - 1 >= 0)
        setPixel(x - 1, y, blurredPixel);
    }
  }

  cv::Vec3b pixelx;
  cv::Vec3b pixelx0;
  cv::Vec3b pixelx1;
  if (0 == ireserve) {
    pixelx0 = cv::Vec3b(0, 0, 0);
    pixelx1 = cv::Vec3b(255, 255, 255);
  } else if (1 == ireserve) {
    pixelx0 = cv::Vec3b(255, 255, 255);
    pixelx1 = cv::Vec3b(0, 0, 0);
  }
  if (0 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0 + 2; x--) {
      for (int y(m_iy0 + 2); y < m_iy0 + m_ih - igap - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x + 1, y, pixelx);
      }
    }
  } else if (1 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0 + 2); y < m_iy0 + m_ih - igap - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = pixelx1;
        else
          pixelx = pixelx0;
        setPixel(x + 1, y, pixelx);
      }
    }
  }
}
void Image::roi_7blur_gap_mud_thre_bw_h(int ithre, int increase, int igap,
                                        int ifindBorW) {
  (void)increase;
  if (getWidth() < m_ix0 + m_iw) {
    m_ix0 = getWidth() - m_iw - 1;
  }
  if (getHeight() < m_iy0 + m_ih) {
    m_iy0 = getHeight() - m_ih - 1;
  }
  if (m_ix0 < 0)
    m_ix0 = 0;
  if (m_iy0 < 0)
    m_iy0 = 0;

  cv::Vec3b pixelCur, pixel_1, pixel_2, pixel1, pixel2;
  cv::Vec3b pixel0;
  for (int x(m_ix0 + 1); x < m_ix0 + m_iw; x++) {
    for (int y(m_iy0 + 2); y < m_iy0 + m_ih - 2; y++) {
      pixelCur = pixel(x, y);
      pixel_1 = pixel(x, y - 1);
      pixel_2 = pixel(x, y - 2);
      pixel1 = pixel(x, y + 1);
      pixel2 = pixel(x, y + 2);
      int ir0 = Red(pixelCur);
      int ig0 = Green(pixelCur);
      int ib0 = Blue(pixelCur);
      int ir_1 = Red(pixel_1);
      int ig_1 = Green(pixel_1);
      int ib_1 = Blue(pixel_1);
      int ir_2 = Red(pixel_2);
      int ig_2 = Green(pixel_2);
      int ib_2 = Blue(pixel_2);
      int ir1 = Red(pixel1);
      int ig1 = Green(pixel1);
      int ib1 = Blue(pixel1);
      int ir2 = Red(pixel2);
      int ig2 = Green(pixel2);
      int ib2 = Blue(pixel2);

      int ir = intx0_125(ir_2) + intx0_25(ir_1) + intx0_25(ir0) +
               intx0_062(ir0) + intx0_25(ir1) + intx0_125(ir2);
      int ig = intx0_125(ig_2) + intx0_25(ig_1) + intx0_25(ig0) +
               intx0_062(ig0) + intx0_25(ig1) + intx0_125(ig2);
      int ib = intx0_125(ib_2) + intx0_25(ib_1) + intx0_25(ib0) +
               intx0_062(ib0) + intx0_25(ib1) + intx0_125(ib2);

      ir = ir > 255 ? 255 : ir;
      ig = ig > 255 ? 255 : ig;
      ib = ib > 255 ? 255 : ib;

      cv::Vec3b blurredPixel =
          cv::Vec3b(static_cast<uchar>(ir), static_cast<uchar>(ig),
                    static_cast<uchar>(ib));

      if (x - 1 >= 0)
        setPixel(x - 1, y, blurredPixel);
    }
  }

  cv::Vec3b pixelx;
  if (0 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0); y < m_iy0 + m_ih - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel2) - Red(gapPixel1);
        int ig = Green(gapPixel2) - Green(gapPixel1);
        int ib = Blue(gapPixel2) - Blue(gapPixel1);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = cv::Vec3b(255, 255, 255);
        else
          pixelx = cv::Vec3b(0, 0, 0);
        setPixel(x + 1, y, pixelx);
      }
    }
  } else if (1 == ifindBorW) {
    for (int x(m_ix0 + m_iw - 2); x > m_ix0; x--) {
      for (int y(m_iy0); y < m_iy0 + m_ih - igap; y++) {
        cv::Vec3b gapPixel1 = pixel(x, y);
        cv::Vec3b gapPixel2 = pixel(x, y + igap);
        int ir = Red(gapPixel1) - Red(gapPixel2);
        int ig = Green(gapPixel1) - Green(gapPixel2);
        int ib = Blue(gapPixel1) - Blue(gapPixel2);
        if (ir > ithre || ig > ithre || ib > ithre)
          pixelx = cv::Vec3b(255, 255, 255);
        else
          pixelx = cv::Vec3b(0, 0, 0);
        setPixel(x + 1, y, pixelx);
      }
    }
  }
}

void Image::colorFillConnectedComponents(double minArea, int minWidth,
                                         int minHeight,
                                         const cv::Scalar &fillColor) {
  std::vector<std::vector<cv::Point>> components =
      findConnectedComponents(minArea);
  for (const auto &comp : components) {
    cv::Rect boundingBox = cv::boundingRect(comp);
    if (boundingBox.width >= minWidth && boundingBox.height >= minHeight) {
      cv::fillConvexPoly(matImage, comp, fillColor);
    }
  }
}

std::vector<cv::Point> Image::getCentroids(double minArea, int minWidth,
                                           int minHeight) const {
  cv::Mat labels, stats, centroids;
  int nLabels =
      cv::connectedComponentsWithStats(matImage, labels, stats, centroids);

  std::vector<cv::Point> validCentroids;
  for (int i = 1; i < nLabels; ++i) {
    if (stats.at<int>(i, cv::CC_STAT_AREA) >= minArea &&
        stats.at<int>(i, cv::CC_STAT_WIDTH) >= minWidth &&
        stats.at<int>(i, cv::CC_STAT_HEIGHT) >= minHeight) {
      validCentroids.push_back(
          cv::Point(static_cast<int>(centroids.at<double>(i, 0)),
                    static_cast<int>(centroids.at<double>(i, 1))));
    }
  }
  return validCentroids;
}

std::vector<cv::Point> Image::getHorizontalMidpoints(double minArea,
                                                     int minWidth,
                                                     int minHeight) const {
  std::vector<cv::Rect> boundingBoxes =
      getBoundingBoxes(findConnectedComponents(minArea));
  std::vector<cv::Point> midpoints;
  for (const auto &rect : boundingBoxes) {
    if (rect.width >= minWidth && rect.height >= minHeight) {
      midpoints.emplace_back(rect.x + rect.width / 2, rect.y + rect.height / 2);
    }
  }
  return midpoints;
}

std::vector<cv::Point> Image::getVerticalMidpoints(double minArea, int minWidth,
                                                   int minHeight) const {
  std::vector<cv::Rect> boundingBoxes =
      getBoundingBoxes(findConnectedComponents(minArea));
  std::vector<cv::Point> midpoints;
  for (const auto &rect : boundingBoxes) {
    if (rect.width >= minWidth && rect.height >= minHeight) {
      midpoints.emplace_back(rect.x + rect.width / 2, rect.y + rect.height / 2);
    }
  }
  return midpoints;
}

std::vector<std::vector<cv::Point>>
Image::findConnectedComponents(double minArea, int minWidth,
                               int minHeight) const {
  cv::Mat labels, stats, centroids;
  int nLabels =
      cv::connectedComponentsWithStats(matImage, labels, stats, centroids);

  std::vector<std::vector<cv::Point>> components;
  for (int i = 1; i < nLabels; ++i) {
    if (stats.at<int>(i, cv::CC_STAT_AREA) >= minArea &&
        stats.at<int>(i, cv::CC_STAT_WIDTH) >= minWidth &&
        stats.at<int>(i, cv::CC_STAT_HEIGHT) >= minHeight) {
      std::vector<cv::Point> component;
      for (int y = 0; y < matImage.rows; ++y) {
        for (int x = 0; x < matImage.cols; ++x) {
          if (labels.at<int>(y, x) == i) {
            component.push_back(cv::Point(x, y));
          }
        }
      }
      components.push_back(component);
    }
  }
  return components;
}

std::vector<cv::Rect> Image::getBoundingBoxes(
    const std::vector<std::vector<cv::Point>> &components) const {
  std::vector<cv::Rect> boundingBoxes;
  for (const auto &component : components) {
    boundingBoxes.push_back(cv::boundingRect(component));
  }
  return boundingBoxes;
}

std::vector<int> Image::calculateAreas(
    const std::vector<std::vector<cv::Point>> &components) const {
  std::vector<int> areas;
  for (const auto &component : components) {
    areas.push_back(static_cast<int>(component.size()));
  }
  return areas;
}

void Image::TestROI(int ix0, int iy0, int ix1, int iy1, int ix2, int iy2) {
  int idis = static_cast<int>(
                 sqrt((ix0 - ix1) * (ix0 - ix1) + (iy0 - iy1) * (iy0 - iy1))) +
             static_cast<int>(
                 sqrt((ix0 - ix2) * (ix0 - ix2) + (iy0 - iy2) * (iy0 - iy2)));

  for (int i = ix1 - 200; i < ix1 + 200; ++i) {
    for (int j = iy1 - 200; j < iy1 + 200; ++j) {
      int idis0 =
          static_cast<int>(
              sqrt((i - ix1) * (i - ix1) + (j - iy1) * (j - iy1))) +
          static_cast<int>(sqrt((i - ix2) * (i - ix2) + (j - iy2) * (j - iy2)));
      if (idis0 > idis)
        setPixel(i, j, Rgb(255, 0, 0));
      if (idis0 < idis)
        setPixel(i, j, Rgb(0, 255, 0));
    }
  }

  for (int i = ix2 - 200; i < ix2 + 200; ++i) {
    for (int j = iy2 - 200; j < iy2 + 200; ++j) {
      int idis0 =
          static_cast<int>(
              sqrt((i - ix1) * (i - ix1) + (j - iy1) * (j - iy1))) +
          static_cast<int>(sqrt((i - ix2) * (i - ix2) + (j - iy2) * (j - iy2)));
      if (idis0 > idis)
        setPixel(i, j, Rgb(255, 0, 0));
      if (idis0 < idis)
        setPixel(i, j, Rgb(0, 255, 0));
    }
  }
}

void Image::colorizeROI(int ib, int ig, int ir) {
  cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Scalar fillColor(ib, ig, ir);
    roi.setTo(fillColor);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}
void Image::colorizeROI(const cv::Scalar &fillColor) {
  cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    roi.setTo(fillColor);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}

void Image::colorizePoints(const std::vector<gp_Pnt> &points,
                           const cv::Scalar &fillColor) {
  for (const auto &point : points) {
    int x = static_cast<int>(point.X());
    int y = static_cast<int>(point.Y());

    if (x >= 0 && x < width && y >= 0 && y < height) {
      cv::circle(matImage, cv::Point(x, y), 3, fillColor, -1);
    } else {
      std::cerr << "Point (" << x << ", " << y
                << ") is outside the image boundaries." << std::endl;
    }
  }
}

void Image::generateHeatmap(const std::vector<gp_Pnt> &points, double radius) {
  matImage = cv::Mat::zeros(matImage.cols, matImage.rows, CV_32F);

  for (const auto &point : points) {
    int x = static_cast<int>(point.X());
    int y = static_cast<int>(point.Y());

    int radiusInt = static_cast<int>(radius);
    for (int i = -radiusInt; i <= radiusInt; ++i) {
      for (int j = -radiusInt; j <= radiusInt; ++j) {
        if (x + i >= 0 && x + i < matImage.cols && y + j >= 0 &&
            y + j < matImage.rows) {
          float distance = static_cast<float>(sqrt(i * i + j * j));
          if (distance <= radius) {
            matImage.at<float>(y + j, x + i) += static_cast<float>(
                exp(-distance * distance / (2 * radius * radius)));
          }
        }
      }
    }
  }
}

void Image::pyramidThresholding(double dlevels, double dblockSize,
                                double offset) {
  int levels = static_cast<int>(dlevels);
  int blockSize = static_cast<int>(dblockSize);
  if (blockSize % 2 == 0) {
    blockSize = blockSize + 1;
  }
  std::vector<cv::Mat> pyramid;
  cv::Mat currentImg = matImage;

  if (currentImg.channels() == 3)
    cvtColor(currentImg, currentImg, cv::COLOR_BGR2GRAY);

  for (int i = 0; i < levels; ++i) {
    pyramid.push_back(currentImg);
    if (i < levels - 1) {
      cv::pyrDown(currentImg, currentImg);
    }
  }

  std::vector<cv::Mat> thresholdedPyramid;
  for (const auto &img : pyramid) {
    cv::Mat threshImg;
    cv::adaptiveThreshold(img, threshImg, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                          cv::THRESH_BINARY, blockSize, offset);
    thresholdedPyramid.push_back(threshImg);
  }

  cv::Mat finalBinary = cv::Mat::zeros(matImage.size(), CV_8UC1);
  for (int i = 0; i < levels; ++i) {
    cv::Mat resized;
    cv::resize(thresholdedPyramid[i], resized, matImage.size());

    double weight = pow(0.5, levels - i - 1);
    resized.convertTo(resized, CV_32F, weight);
    finalBinary.convertTo(finalBinary, CV_32F);
    finalBinary += resized;

    cv::threshold(finalBinary, finalBinary, 255, 255, cv::THRESH_TRUNC);
  }

  finalBinary.convertTo(finalBinary, CV_8U);

  copyResizedToROI(finalBinary);
}
Image Image::pyramidDynamicThresholding(double dlevels, double dblockSize,
                                        double offset) {
  int levels = static_cast<int>(dlevels);
  int blockSize = static_cast<int>(dblockSize);
  if (blockSize % 2 == 0) {
    blockSize = blockSize + 1;
  }
  std::vector<cv::Mat> pyramid;
  cv::Mat currentImg = matImage;

  if (currentImg.channels() == 3)
    cvtColor(currentImg, currentImg, cv::COLOR_BGR2GRAY);

  for (int i = 0; i < levels; ++i) {
    pyramid.push_back(currentImg);
    if (i < levels - 1) {
      cv::pyrDown(currentImg, currentImg);
    }
  }

  std::vector<cv::Mat> thresholdedPyramid;
  for (const auto &img : pyramid) {
    cv::Mat threshImg;
    cv::adaptiveThreshold(img, threshImg, 255, cv::ADAPTIVE_THRESH_MEAN_C,
                          cv::THRESH_BINARY, blockSize, offset);
    thresholdedPyramid.push_back(threshImg);
  }

  cv::Mat finalBinary = cv::Mat::zeros(matImage.size(), CV_8UC1);
  for (int i = 0; i < levels; ++i) {
    cv::Mat resized;
    cv::resize(thresholdedPyramid[i], resized, matImage.size());

    double weight = pow(0.5, levels - i - 1);
    resized.convertTo(resized, CV_32F, weight);
    finalBinary.convertTo(finalBinary, CV_32F);
    finalBinary += resized;

    cv::threshold(finalBinary, finalBinary, 255, 255, cv::THRESH_TRUNC);
  }

  finalBinary.convertTo(finalBinary, CV_8U);

  Image result;
  result.matImage = finalBinary;
  return result;
}

void Image::classifyConnectedComponentsByPyramid(int levels, double minArea,
                                                 double aspectRatioThreshold) {
  std::vector<cv::Mat> pyramid;
  cv::Mat currentImg = matImage;

  for (int i = 0; i < levels; ++i) {
    if (currentImg.channels() == 3)
      cvtColor(currentImg, currentImg, cv::COLOR_BGR2GRAY);
    pyramid.push_back(currentImg);
    if (i < levels - 1) {
      cv::pyrDown(currentImg, currentImg);
      cv::GaussianBlur(currentImg, currentImg, cv::Size(5, 5), 0);
    }
  }

  for (const auto &img : pyramid) {
    std::vector<std::vector<cv::Point>> components =
        findConnectedComponents(minArea, img);
    for (const auto &comp : components) {
      cv::Rect boundingBox = cv::boundingRect(comp);
      double aspectRatio =
          static_cast<double>(boundingBox.width) / boundingBox.height;

      if (aspectRatio > aspectRatioThreshold) {
        cv::drawContours(matImage, std::vector<std::vector<cv::Point>>{comp},
                         -1, cv::Scalar(0, 255, 0), 2);
      } else if (aspectRatio < 1.0 / aspectRatioThreshold) {
        cv::drawContours(matImage, std::vector<std::vector<cv::Point>>{comp},
                         -1, cv::Scalar(255, 0, 0), 2);
      } else {
        cv::drawContours(matImage, std::vector<std::vector<cv::Point>>{comp},
                         -1, cv::Scalar(0, 0, 255), 2);
      }
    }
  }
}

std::vector<std::vector<cv::Point>>
Image::findConnectedComponents(double minArea, const cv::Mat &img) const {
  cv::Mat labels, stats, centroids;
  int nLabels = cv::connectedComponentsWithStats(img, labels, stats, centroids);

  std::vector<std::vector<cv::Point>> components;
  for (int i = 1; i < nLabels; ++i) {
    if (stats.at<int>(i, cv::CC_STAT_AREA) >= minArea) {
      std::vector<cv::Point> component;
      for (int y = 0; y < img.rows; ++y) {
        for (int x = 0; x < img.cols; ++x) {
          if (labels.at<int>(y, x) == i) {
            component.push_back(cv::Point(x, y));
          }
        }
      }
      components.push_back(component);
    }
  }
  return components;
}

void Image::erodeROI(int erosionSize) {
  cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
  erodeROI(roiRect, erosionSize);
}
void Image::erodeROI(const cv::Rect &roiRect, int erosionSize) {
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(erosionSize, erosionSize));
    cv::erode(roi, roi, kernel);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}

void Image::enhanceImage(double alpha, int beta) {
  matImage.convertTo(matImage, -1, alpha, beta);
}

void Image::enhanceROI(const cv::Rect &roiRect, double alpha, int beta) {
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    roi.convertTo(roi, -1, alpha, beta);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}

void Image::erodeVertical(int erosionHeight) {
  cv::Mat kernel =
      cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, erosionHeight));
  cv::erode(matImage, matImage, kernel);
}

void Image::erodeHorizontal(int erosionWidth) {
  cv::Mat kernel =
      cv::getStructuringElement(cv::MORPH_RECT, cv::Size(erosionWidth, 1));
  cv::erode(matImage, matImage, kernel);
}

void Image::erodeVerticalROI(int erosionHeight) {
  cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1, erosionHeight));
    cv::erode(roi, roi, kernel);
  }
}

void Image::erodeHorizontalROI(int erosionWidth) {
  cv::Rect roiRect(m_ix0, m_iy0, m_iw, m_ih);
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(erosionWidth, 1));
    cv::erode(roi, roi, kernel);
  }
}

void Image::erodePoints(const std::vector<gp_Pnt> &points, int erosionSize) {
  for (const auto &point : points) {
    int x = static_cast<int>(point.X());
    int y = static_cast<int>(point.Y());

    if (x >= 0 && x < width && y >= 0 && y < height) {
      cv::Rect rect(x - erosionSize / 2, y - erosionSize / 2, erosionSize,
                    erosionSize);
      rect &= cv::Rect(0, 0, width, height);
      cv::Mat kernel = cv::getStructuringElement(
          cv::MORPH_RECT, cv::Size(erosionSize, erosionSize));
      cv::erode(matImage(rect), matImage(rect), kernel);
    }
  }
}

void Image::enhancePoints(const std::vector<gp_Pnt> &points, double alpha,
                          int beta) {
  for (const auto &point : points) {
    int x = static_cast<int>(point.X());
    int y = static_cast<int>(point.Y());

    if (x >= 0 && x < width && y >= 0 && y < height) {
      cv::Rect rect(x - 1, y - 1, 3, 3);
      rect &= cv::Rect(0, 0, width, height);
      cv::Mat roi(matImage, rect);
      roi.convertTo(roi, -1, alpha, beta);
    }
  }
}

void Image::openROI(const cv::Rect &roiRect, int kernelSize) {
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::morphologyEx(roi, roi, cv::MORPH_OPEN, kernel);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}

void Image::closeROI(const cv::Rect &roiRect, int kernelSize) {
  if (roiRect.x >= 0 && roiRect.y >= 0 && roiRect.x + roiRect.width <= width &&
      roiRect.y + roiRect.height <= height) {
    cv::Mat roi(matImage, roiRect);
    cv::Mat kernel = cv::getStructuringElement(
        cv::MORPH_RECT, cv::Size(kernelSize, kernelSize));
    cv::morphologyEx(roi, roi, cv::MORPH_CLOSE, kernel);
  } else {
    std::cerr << "Invalid ROI rectangle!" << std::endl;
  }
}

int Image::find(std::vector<int> &parent, int x) {
  if (parent[x] != x) {
    parent[x] = find(parent, parent[x]);
  }
  return parent[x];
}

void Image::unite(std::vector<int> &parent, std::vector<int> &rank, int x,
                  int y) {
  int rootX = find(parent, x);
  int rootY = find(parent, y);
  if (rootX != rootY) {
    if (rank[rootX] > rank[rootY]) {
      parent[rootY] = rootX;
    } else if (rank[rootX] < rank[rootY]) {
      parent[rootX] = rootY;
    } else {
      parent[rootY] = rootX;
      rank[rootX]++;
    }
  }
}

double Image::colorDistance(const cv::Vec3b &color1, const cv::Vec3b &color2) {
  int bDiff = color1[0] - color2[0];
  int gDiff = color1[1] - color2[1];
  int rDiff = color1[2] - color2[2];
  return std::sqrt(bDiff * bDiff + gDiff * gDiff + rDiff * rDiff);
}

void Image::analyzeConnectedComponentsColor(double colorThreshold,
                                            double minArea,
                                            double aspectRatioMin,
                                            double aspectRatioMax) {
  cv::Mat labels = cv::Mat::zeros(matImage.size(), CV_32S);
  int currentLabel = 1;
  std::vector<int> parent;
  std::vector<int> rank;

  for (int y = 0; y < matImage.rows; ++y) {
    for (int x = 0; x < matImage.cols; ++x) {
      parent.push_back(currentLabel);
      rank.push_back(0);
      labels.at<int>(y, x) = currentLabel++;
    }
  }

  for (int y = 1; y < matImage.rows - 1; ++y) {
    for (int x = 1; x < matImage.cols - 1; ++x) {
      cv::Vec3b currentColor = matImage.at<cv::Vec3b>(y, x);

      cv::Vec3b upColor = matImage.at<cv::Vec3b>(y - 1, x);
      cv::Vec3b leftColor = matImage.at<cv::Vec3b>(y, x - 1);

      if (colorDistance(currentColor, upColor) <= colorThreshold) {
        unite(parent, rank, labels.at<int>(y, x), labels.at<int>(y - 1, x));
      }
      if (colorDistance(currentColor, leftColor) <= colorThreshold) {
        unite(parent, rank, labels.at<int>(y, x), labels.at<int>(y, x - 1));
      }
    }
  }

  std::vector<std::vector<cv::Point>> components(currentLabel);
  for (int y = 0; y < matImage.rows; ++y) {
    for (int x = 0; x < matImage.cols; ++x) {
      int originalLabel = labels.at<int>(y, x);
      int rootLabel = find(parent, originalLabel);
      components[rootLabel].push_back(cv::Point(x, y));
    }
  }

  cv::Mat coloredLabels = cv::Mat::zeros(matImage.size(), CV_8UC3);
  for (const auto &comp : components) {
    if (comp.size() < minArea)
      continue;

    cv::Rect boundingBox = cv::boundingRect(comp);
    double aspectRatio =
        static_cast<double>(boundingBox.width) / boundingBox.height;

    if (aspectRatio < aspectRatioMin || aspectRatio > aspectRatioMax)
      continue;

    cv::Vec3b color(static_cast<uchar>(rand() % 256),
                    static_cast<uchar>(rand() % 256),
                    static_cast<uchar>(rand() % 256));
    for (const auto &point : comp) {
      coloredLabels.at<cv::Vec3b>(point.y, point.x) = color;
    }

    cv::Point center(boundingBox.x + boundingBox.width / 2,
                     boundingBox.y + boundingBox.height / 2);
    cv::circle(coloredLabels, center, 3, cv::Scalar(255, 255, 255), -1);

    cv::Point horizontalMid(boundingBox.x + boundingBox.width / 2,
                            boundingBox.y);
    cv::Point verticalMid(boundingBox.x,
                          boundingBox.y + boundingBox.height / 2);
    cv::circle(coloredLabels, horizontalMid, 3, cv::Scalar(0, 255, 0), -1);
    cv::circle(coloredLabels, verticalMid, 3, cv::Scalar(0, 0, 255), -1);
  }

  matImage = coloredLabels;
}

void Image::analyzeConnectedComponentsPyramid(double minArea,
                                              double aspectRatioMin,
                                              double aspectRatioMax) {
  std::vector<cv::Mat> pyramids;
  int levels = 3;
  cv::Mat currentImage = matImage.clone();

  for (int i = 0; i < levels; ++i) {
    pyramids.push_back(currentImage);
    if (i < levels - 1) {
      cv::pyrDown(currentImage, currentImage);
    }
  }

  cv::Mat coloredLabels = cv::Mat::zeros(matImage.size(), CV_8UC3);

  for (int level = levels - 1; level >= 0; --level) {
    cv::Mat img = pyramids[level];
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::Mat gray, binary;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::findContours(binary, contours, hierarchy, cv::RETR_EXTERNAL,
                     cv::CHAIN_APPROX_SIMPLE);

    for (const auto &contour : contours) {
      double area = cv::contourArea(contour);
      if (area < minArea)
        continue;

      cv::Rect boundingBox = cv::boundingRect(contour);
      double aspectRatio =
          static_cast<double>(boundingBox.width) / boundingBox.height;

      if (aspectRatio < aspectRatioMin || aspectRatio > aspectRatioMax)
        continue;

      float scale = static_cast<float>(std::pow(2.0f, levels - 1 - level));
      cv::Rect originalBoundingBox(
          static_cast<int>(boundingBox.x * scale),
          static_cast<int>(boundingBox.y * scale),
          static_cast<int>(boundingBox.width * scale),
          static_cast<int>(boundingBox.height * scale));

      cv::Vec3b color(static_cast<uchar>(rand() % 256),
                      static_cast<uchar>(rand() % 256),
                      static_cast<uchar>(rand() % 256));
      cv::rectangle(coloredLabels, originalBoundingBox,
                    cv::Scalar(color[0], color[1], color[2]), -1);

      cv::Point center(originalBoundingBox.x + originalBoundingBox.width / 2,
                       originalBoundingBox.y + originalBoundingBox.height / 2);
      cv::circle(coloredLabels, center, 3, cv::Scalar(255, 255, 255), -1);

      cv::Point horizontalMid(originalBoundingBox.x +
                                  originalBoundingBox.width / 2,
                              originalBoundingBox.y);
      cv::Point verticalMid(originalBoundingBox.x,
                            originalBoundingBox.y +
                                originalBoundingBox.height / 2);
      cv::circle(coloredLabels, horizontalMid, 3, cv::Scalar(0, 255, 0), -1);
      cv::circle(coloredLabels, verticalMid, 3, cv::Scalar(0, 0, 255), -1);
    }
  }

  matImage = coloredLabels;
}

void Image::applyCompositionMode(cv::Mat &srcImage, cv::Mat &dstImage,
                                 int srcX0, int srcY0, int dstX0, int dstY0,
                                 int roiWidth, int roiHeight,
                                 CompositionMode mode) {
  if (srcX0 + roiWidth > srcImage.cols || srcY0 + roiHeight > srcImage.rows ||
      dstX0 + roiWidth > dstImage.cols || dstY0 + roiHeight > dstImage.rows) {
    std::cerr << "Error: ROI exceeds image boundaries!" << std::endl;
    return;
  }

  cv::Rect srcROI(srcX0, srcY0, roiWidth, roiHeight);
  cv::Rect dstROI(dstX0, dstY0, roiWidth, roiHeight);

  cv::Mat srcROIMat = srcImage(srcROI).clone();
  cv::Mat dstROIMat = dstImage(dstROI);

  switch (mode) {
  case SOURCE_OVER:
    srcROIMat.copyTo(dstROIMat);
    break;

  case DESTINATION_OVER:
    dstROIMat.copyTo(srcROIMat);
    srcROIMat.copyTo(dstROIMat);
    break;

  case MULTIPLY:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          dstPixel[c] = static_cast<uchar>((srcPixel[c] * dstPixel[c]) / 255.0);
        }
      }
    }
    break;

  case SCREEN:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          dstPixel[c] = static_cast<uchar>(
              255 - ((255 - srcPixel[c]) * (255 - dstPixel[c]) / 255.0));
        }
      }
    }
    break;

  case OVERLAY:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          if (dstPixel[c] < 128)
            dstPixel[c] =
                static_cast<uchar>(2 * dstPixel[c] * srcPixel[c] / 255.0);
          else
            dstPixel[c] = static_cast<uchar>(
                255 - 2 * (255 - dstPixel[c]) * (255 - srcPixel[c]) / 255.0);
        }
      }
    }
    break;

  case DARKEN:
    cv::min(srcROIMat, dstROIMat, dstROIMat);
    break;

  case LIGHTEN:
    cv::max(srcROIMat, dstROIMat, dstROIMat);
    break;

  case COLOR_DODGE:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          dstPixel[c] = static_cast<uchar>(
              (srcPixel[c] == 0)
                  ? 0
                  : std::min(255, dstPixel[c] * 255 / srcPixel[c]));
        }
      }
    }
    break;

  case COLOR_BURN:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          dstPixel[c] = static_cast<uchar>(
              (srcPixel[c] == 255)
                  ? 255
                  : std::max(0, (255 - (255 - dstPixel[c]) * 255 /
                                           (255 - srcPixel[c]))));
        }
      }
    }
    break;

  case HARD_LIGHT:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          if (srcPixel[c] < 128)
            dstPixel[c] =
                static_cast<uchar>(2 * dstPixel[c] * srcPixel[c] / 255.0);
          else
            dstPixel[c] = static_cast<uchar>(
                255 - 2 * (255 - dstPixel[c]) * (255 - srcPixel[c]) / 255.0);
        }
      }
    }
    break;

  case SOFT_LIGHT:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          float s = srcPixel[c] / 255.0f;
          float d = dstPixel[c] / 255.0f;
          dstPixel[c] = static_cast<uchar>(
              (d <= 0.5) ? (d * (s + 1) * 255)
                         : (d + (2 * s - 1) * (d * (1 - d)) * 255));
        }
      }
    }
    break;

  case DIFFERENCE_IMAGE:
    cv::absdiff(srcROIMat, dstROIMat, dstROIMat);
    break;

  case EXCLUSION:
    for (int y = 0; y < srcROIMat.rows; ++y) {
      for (int x = 0; x < srcROIMat.cols; ++x) {
        cv::Vec3b &srcPixel = srcROIMat.at<cv::Vec3b>(y, x);
        cv::Vec3b &dstPixel = dstROIMat.at<cv::Vec3b>(y, x);
        for (int c = 0; c < 3; ++c) {
          dstPixel[c] =
              static_cast<uchar>(srcPixel[c] + dstPixel[c] -
                                 2 * srcPixel[c] * dstPixel[c] / 255.0);
        }
      }
    }
    break;

  case BITWISE_AND:
    cv::bitwise_and(srcROIMat, dstROIMat, dstROIMat);
    break;

  case BITWISE_OR:
    cv::bitwise_or(srcROIMat, dstROIMat, dstROIMat);
    break;

  case BITWISE_XOR:
    cv::bitwise_xor(srcROIMat, dstROIMat, dstROIMat);
    break;

  default:
    std::cerr << "Unsupported composition mode!" << std::endl;
    break;
  }
}

void Image::ROItoROI_(cv::Mat &srcImage, cv::Mat &dstImage, int srcX0,
                      int srcY0, int srcWidth, int srcHeight, int dstX0,
                      int dstY0, int dstWidth, int dstHeight) {
  if (srcX0 < 0)
    srcX0 = 0;
  if (srcY0 < 0)
    srcY0 = 0;

  if (srcX0 + srcWidth > srcImage.cols || srcY0 + srcHeight > srcImage.rows ||
      dstX0 + dstWidth > dstImage.cols || dstY0 + dstHeight > dstImage.rows) {
    std::cerr << "Error: ROI exceeds image boundaries!" << std::endl;
    return;
  }

  cv::Rect srcROI(srcX0, srcY0, srcWidth, srcHeight);
  cv::Mat srcROIMat = srcImage(srcROI);

  cv::Rect dstROI(dstX0, dstY0, dstWidth, dstHeight);
  cv::Mat dstROIMat = dstImage(dstROI);

  srcROIMat.copyTo(dstROIMat);
}

void Image::ROItoROI(Image &img) {
  int srcX0 = m_ix0;
  int srcY0 = m_iy0;
  int srcWidth = m_iw;
  int srcHeight = m_ih;
  int dstX0 = srcX0;
  int dstY0 = srcY0;
  int dstWidth = srcWidth;
  int dstHeight = srcHeight;
  ROItoROI_(getmat(), img.getmat(), srcX0, srcY0, srcWidth, srcHeight, dstX0,
            dstY0, dstWidth, dstHeight);
}

void Image::ROIEasyThre(int ithre) {
  Image aimageroi = getROI();
  Image otsuBinaryImage =
      aimageroi.otsuThresholding(ithre, cv::THRESH_BINARY | cv::THRESH_OTSU);
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(otsuBinaryImage.getmat());
}

void Image::Denoising(int searchWindowSize, int blockWindowSize, float h,
                      float hColor) {
  cv::Mat dst;
  cv::fastNlMeansDenoisingColored(matImage, dst, h, hColor, blockWindowSize,
                                  searchWindowSize);
  matImage = dst;
}

void Image::DenoisingMulti(int searchWindowSize, int blockWindowSize, float h,
                           float hColor) {
  cv::Mat dst;
  std::vector<cv::Mat> frames = {matImage};
  cv::fastNlMeansDenoisingColoredMulti(frames, dst, 0, 1, h, hColor,
                                       blockWindowSize, searchWindowSize);
  matImage = dst;
}

void Image::pyrDown() { cv::pyrDown(matImage, matImage); }

void Image::ROIScharr(int ix, int iy) {

  Image aimageroi = getROI();
  cv::Mat grad;
  cv::Scharr(aimageroi.getmat(), grad, CV_16S, ix, iy);
  convertScaleAbs(grad, grad);
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(grad);
}
void Image::ROIpyrDown(int ilevel) {
  Image aimageroi = getROI();
  cv::Mat dst = aimageroi.getmat();
  for (int i = 0; i < ilevel; i++) {
    cv::pyrDown(dst, dst);
  }
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(dst);
}
void Image::ROIDenoising(double searchWindowSize, double blockWindowSize,
                         double h) {
  Image aimageroi = getROI();
  cv::Mat dst;
  cv::fastNlMeansDenoisingColored(
      aimageroi.getmat(), dst, static_cast<float>(h), static_cast<float>(h),
      static_cast<int>(blockWindowSize), static_cast<int>(searchWindowSize));
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(dst);
}

void Image::ROIDenoisingMulti(double searchWindowSize, double blockWindowSize,
                              double h) {
  Image aimageroi = getROI();
  cv::Mat dst;
  std::vector<cv::Mat> frames = {aimageroi.getmat()};
  cv::fastNlMeansDenoisingColoredMulti(
      frames, dst, 0, 1, static_cast<float>(h), static_cast<float>(h),
      static_cast<int>(blockWindowSize), static_cast<int>(searchWindowSize));
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(dst);
}

double Image::roimean() {
  Image roiImg = getROI();
  double dMean = cv::mean(roiImg.matImage)[0];
  return dMean;
}
void Image::ROISobel(int ix, int iy, int ksize) {
  Image aimageroi = getROI();
  cv::Mat grad;
  cv::Sobel(aimageroi.getmat(), grad, CV_64F, ix, iy, ksize);
  convertScaleAbs(grad, grad);
  colorizeROI(cv::Scalar(0, 0, 0));
  copyResizedToROI(grad);
}
double Image::roimagnitude() {

  Image Img1;
  Img1.copyFromMat(getmat());
  Image Img2;
  Img2.copyFromMat(getmat());
  Image Img3;
  Img3.copyFromMat(getmat());
  Image Img4;
  Img4.copyFromMat(getmat());
  Img1.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img2.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img3.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img4.setroi(m_ix0, m_iy0, m_iw, m_ih);

  Img1.roi_5blur_gap_mud_bw(1, 0, 1, 10);
  Img2.roi_5blur_gap_mud_bw_h(1, 0, 1, 10);

  Img3.roi_5blur_gap_mud_bw(1, 1, 1, 10);
  Img4.roi_5blur_gap_mud_bw_h(1, 1, 1, 10);

  Img1.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img2.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img3.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img4.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);

  cv::Mat gray1;
  cv::cvtColor(Img1.getROI().getmat(), gray1, cv::COLOR_BGR2GRAY);
  cv::Mat gray2;
  cv::cvtColor(Img2.getROI().getmat(), gray2, cv::COLOR_BGR2GRAY);
  cv::Mat gray3;
  cv::cvtColor(Img3.getROI().getmat(), gray3, cv::COLOR_BGR2GRAY);
  cv::Mat gray4;
  cv::cvtColor(Img4.getROI().getmat(), gray4, cv::COLOR_BGR2GRAY);

  cv::Mat gray_float1, gray_float2, gray_float3, gray_float4;
  gray1.convertTo(gray_float1, CV_64F);
  gray2.convertTo(gray_float2, CV_64F);
  gray2.convertTo(gray_float3, CV_64F);
  gray2.convertTo(gray_float4, CV_64F);

  cv::Mat sobelMagnitude1;
  cv::magnitude(gray_float1, gray_float2, sobelMagnitude1);
  cv::Mat sobelMagnitude2;
  cv::magnitude(gray_float3, gray_float4, sobelMagnitude2);

  double dMean0 = cv::mean(sobelMagnitude1)[0] + cv::mean(sobelMagnitude2)[0];

  Img1.copyFromMat(getmat());
  Img2.copyFromMat(getmat());
  Img3.copyFromMat(getmat());
  Img4.copyFromMat(getmat());
  Img1.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img2.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img3.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img4.setroi(m_ix0, m_iy0, m_iw, m_ih);

  Img1.roi_5blur_gap_mud_bw(3, 0, 1, 10);
  Img2.roi_5blur_gap_mud_bw_h(3, 0, 1, 10);

  Img3.roi_5blur_gap_mud_bw(3, 1, 1, 10);
  Img4.roi_5blur_gap_mud_bw_h(3, 1, 1, 10);

  Img1.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img2.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img3.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img4.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);

  cv::cvtColor(Img1.getROI().getmat(), gray1, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img2.getROI().getmat(), gray2, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img3.getROI().getmat(), gray3, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img4.getROI().getmat(), gray4, cv::COLOR_BGR2GRAY);

  gray1.convertTo(gray_float1, CV_64F);
  gray2.convertTo(gray_float2, CV_64F);
  gray2.convertTo(gray_float3, CV_64F);
  gray2.convertTo(gray_float4, CV_64F);

  cv::magnitude(gray_float1, gray_float2, sobelMagnitude1);
  cv::magnitude(gray_float3, gray_float4, sobelMagnitude2);

  double dMean1 = cv::mean(sobelMagnitude1)[0] + cv::mean(sobelMagnitude2)[0];

  Img1.copyFromMat(getmat());
  Img2.copyFromMat(getmat());
  Img3.copyFromMat(getmat());
  Img4.copyFromMat(getmat());
  Img1.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img2.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img3.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img4.setroi(m_ix0, m_iy0, m_iw, m_ih);

  Img1.roi_5blur_gap_mud_bw(5, 0, 1, 10);
  Img2.roi_5blur_gap_mud_bw_h(5, 0, 1, 10);

  Img3.roi_5blur_gap_mud_bw(5, 1, 1, 10);
  Img4.roi_5blur_gap_mud_bw_h(5, 1, 1, 10);

  Img1.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img2.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img3.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img4.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);

  cv::cvtColor(Img1.getROI().getmat(), gray1, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img2.getROI().getmat(), gray2, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img3.getROI().getmat(), gray3, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img4.getROI().getmat(), gray4, cv::COLOR_BGR2GRAY);

  gray1.convertTo(gray_float1, CV_64F);
  gray2.convertTo(gray_float2, CV_64F);
  gray2.convertTo(gray_float3, CV_64F);
  gray2.convertTo(gray_float4, CV_64F);

  cv::magnitude(gray_float1, gray_float2, sobelMagnitude1);
  cv::magnitude(gray_float3, gray_float4, sobelMagnitude2);

  double dMean2 = cv::mean(sobelMagnitude1)[0] + cv::mean(sobelMagnitude2)[0];

  Img1.copyFromMat(getmat());
  Img2.copyFromMat(getmat());
  Img3.copyFromMat(getmat());
  Img4.copyFromMat(getmat());
  Img1.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img2.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img3.setroi(m_ix0, m_iy0, m_iw, m_ih);
  Img4.setroi(m_ix0, m_iy0, m_iw, m_ih);

  Img1.roi_5blur_gap_mud_bw(7, 0, 1, 10);
  Img2.roi_5blur_gap_mud_bw_h(7, 0, 1, 10);

  Img3.roi_5blur_gap_mud_bw(7, 1, 1, 10);
  Img4.roi_5blur_gap_mud_bw_h(7, 1, 1, 10);

  Img1.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img2.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img3.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);
  Img4.setroi(m_ix0 + 10, m_iy0 + 10, m_iw - 20, m_ih - 20);

  cv::cvtColor(Img1.getROI().getmat(), gray1, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img2.getROI().getmat(), gray2, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img3.getROI().getmat(), gray3, cv::COLOR_BGR2GRAY);
  cv::cvtColor(Img4.getROI().getmat(), gray4, cv::COLOR_BGR2GRAY);

  gray1.convertTo(gray_float1, CV_64F);
  gray2.convertTo(gray_float2, CV_64F);
  gray2.convertTo(gray_float3, CV_64F);
  gray2.convertTo(gray_float4, CV_64F);

  cv::magnitude(gray_float1, gray_float2, sobelMagnitude1);
  cv::magnitude(gray_float3, gray_float4, sobelMagnitude2);

  double dMean3 = cv::mean(sobelMagnitude1)[0] + cv::mean(sobelMagnitude2)[0];

  return dMean0 + dMean1 + dMean2 + dMean3;
}
void RemoveDuplicatePoints(vector<cv::Point2f> &ptIn, bool isClosed) {
  int num = static_cast<int>(ptIn.size());
  if (num < 5)
    return;

  vector<bool> ptJudge(num, true);
  cv::Point2f ptDiff;
  int numAll = 0;
  if (isClosed)
    numAll = num - 2;
  else
    numAll = num - 1;

  for (int i = 1; i < numAll; i++) {
    cv::Point2f ptI = ptIn[i];
    for (int j = i + 2; j < num; j++) {
      ptDiff = ptIn[j] - ptI;

      if (abs(ptDiff.x) < 2 && abs(ptDiff.y) < 2)
        ptJudge[j] = false;
    }
  }

  vector<cv::Point2f> ptSwap;
  ptSwap.reserve(num);
  for (int i = 0; i < num; i++) {
    if (ptJudge[i])
      ptSwap.emplace_back(ptIn[i]);
  }
  ptSwap.swap(ptIn);
}
int squareInt(int a) { return a * a; }
int calPointPointDist(const cv::Point &P1, const cv::Point &P2) {
  cv::Point ptDiff = P1 - P2;
  int dist = static_cast<int>(sqrt(squareInt(ptDiff.x) + squareInt(ptDiff.y)));
  return dist;
}

double calPointPointDist(const cv::Point2f &P1, const cv::Point2f &P2) {
  cv::Point2f ptDiff = P1 - P2;
  double dist = sqrt(pow(ptDiff.x, 2) + pow(ptDiff.y, 2));
  return dist;
}
int calPointPointDist2(const cv::Point &P1, const cv::Point &P2) {
  cv::Point ptDiff = P1 - P2;
  int dist = squareInt(ptDiff.x) + squareInt(ptDiff.y);
  return dist;
}
double calPointPointDist2(const cv::Point2f &P1, const cv::Point2f &P2) {
  cv::Point2f ptDiff = P1 - P2;
  double dist = pow(ptDiff.x, 2) + pow(ptDiff.y, 2);
  return dist;
}
std::tuple<int, int, int>
GetSample_(const int &index_size,
           const std::vector<std::tuple<int, int, int>> &sampled_indexes,
           std::mt19937_64 &generator) {
  assert(index_size > 2);
  std::uniform_int_distribution<int> dist(0, index_size - 1);

  bool has_sampled = false;
  while (true) {
    std::vector<int> index = {
        dist(generator),
        dist(generator),
        dist(generator),
    };
    if (index[0] == index[1] || index[0] == index[2] || index[1] == index[2]) {
      continue;
    }
    std::sort(index.begin(), index.end());

    has_sampled = false;
    for (auto &[i, j, k] : sampled_indexes) {
      if (index[0] == i && j == index[1] && k == index[2]) {
        has_sampled = true;
        break;
      }
    }

    if (has_sampled) {
      continue;
    } else {
      return {index[0], index[1], index[2]};
    }
  }
}

void GetCircle(const cv::Point2f p1, const cv::Point2f p2, const cv::Point2f p3,
               cv::Point2f &center, double &radius2) {
  double r12 = p1.x * p1.x + p1.y * p1.y;
  double r22 = p2.x * p2.x + p2.y * p2.y;
  double r32 = p3.x * p3.x + p3.y * p3.y;
  double A =
      p1.x * (p2.y - p3.y) - p1.y * (p2.x - p3.x) + p2.x * p3.y - p3.x * p2.y;
  double B = r12 * (p2.y - p3.y) + r22 * (p3.y - p1.y) + r32 * (p1.y - p2.y);
  double C = r12 * (p3.x - p2.x) + r22 * (p1.x - p3.x) + r32 * (p2.x - p1.x);

  if (A < 0.0001 && A > -0.0001) {
    A = 0.0001;
  }

  center.x = static_cast<float>(round(B / (2 * A)));

  center.y = static_cast<float>(round(C / (2 * A)));

  radius2 = sqrt((center.x - p2.x) * (center.x - p2.x) +
                 (center.y - p2.y) * (center.y - p2.y));
}

void RansacCircleFit2(std::vector<cv::Point2f> &pts,
                      const float inlier_threshold,
                      std::vector<int> &inlier_indexes, cv::Point2f &ptCenter,
                      double &fRadius, double rmin = 0,
                      double rmax = numeric_limits<double>::max()) {
  (void)inlier_indexes;
  if (pts.size() <= 3) {
    return;
  }
  if (inlier_threshold < 1e-6) {
    return;
  }

  int iterate_nums = 50000;
  float sample_points_min_distance = 5.0f;
  std::vector<std::tuple<int, int, int>> sampled_indexes;
  sampled_indexes.reserve(iterate_nums);
  std::vector<int> is_inlier(pts.size(), 0);
  std::vector<int> is_inlier_tmp(pts.size(), 0);
  int max_inlier_num = 0;
  int sample_count = 0;

  std::mt19937_64 generator(0x4358564953494F4EULL);

  double radius = 0.0f;
  cv::Point2f center;
  int nSampleTotal = 0;
  int nSampleValid = 0;
  while (sample_count < iterate_nums) {
    auto [p1, p2, p3] =
        GetSample_(static_cast<int>(pts.size()), sampled_indexes, generator);

    nSampleTotal++;

    if (std::abs(pts[p1].x - pts[p2].x) < sample_points_min_distance &&
        std::abs(pts[p1].y - pts[p2].y) < sample_points_min_distance &&
        std::abs(pts[p2].x - pts[p3].x) < sample_points_min_distance &&
        std::abs(pts[p2].y - pts[p3].y) < sample_points_min_distance) {
      continue;
    } else {
      sampled_indexes.push_back({p1, p2, p3});
    }

    nSampleValid++;
    GetCircle(pts[p1], pts[p2], pts[p3], center, radius);

    if (radius < rmin || radius > rmax)
      continue;

    int inlier_num = 0;
    std::vector<cv::Point2f> inliers;

    for (int i = 0; i < pts.size(); i++) {
      auto &p = pts[i];
      is_inlier_tmp[i] = 0;
      double p_2_center =
          std::sqrt(std::pow(p.x - center.x, 2) + std::pow(p.y - center.y, 2));
      if (std::abs(p_2_center - radius) < inlier_threshold) {
        is_inlier_tmp[i] = 1;
        inlier_num++;
        inliers.push_back(pts[i]);
      }
    }
    if (inlier_num > max_inlier_num) {
      max_inlier_num = inlier_num;
      is_inlier = is_inlier_tmp;
    }
    if (inlier_num == 0) {
      iterate_nums = 500;
    } else {
      double epsilon = 1.0 - double(inlier_num) / (double)pts.size();
      double p = 0.995;
      double s = 3.0;
      iterate_nums =
          int(std::log(1.0 - p) / std::log(1.0 - std::pow((1.0 - epsilon), s)));
    }
    sample_count++;
  }

  std::vector<cv::Point2f> inliers;
  inliers.reserve(max_inlier_num);
  for (int i = 0; i < is_inlier.size(); i++) {
    if (1 == is_inlier[i]) {
      inliers.push_back(pts[i]);
    }
  }

  ptCenter = center;
  fRadius = radius;

  pts.clear();
  pts.assign(inliers.begin(), inliers.end());
}

template <typename PointT5>
bool angleVecVecT(const PointT5 &v1, const PointT5 &v2, double &angle) {
  double lengthV1 = sqrt(pow(v1.x, 2) + pow(v1.y, 2));
  double lengthV2 = sqrt(pow(v2.x, 2) + pow(v2.y, 2));

  if (lengthV1 < 1e-5 || lengthV2 < 1e-5)
    return false;

  double cosTheta = (v1.x * v2.x + v1.y * v2.y) / (lengthV1 * lengthV2);
  cosTheta = std::max(cosTheta, -1.0);
  cosTheta = std::min(cosTheta, 1.0);
  angle = acos(cosTheta);
  return true;
}
bool angleVecVec(const cv::Point2f &v1, const cv::Point2f &v2, double &angle) {
  return angleVecVecT(v1, v2, angle);
}

int fitLargestCircle(vector<vector<cv::Point2f>> &ptContours,
                     cv::Point2f &Center, double &radius, double rLimit) {
  int index = -1;
  if (ptContours.size() == 0)
    return index;

  std::vector<int> inliers_indexes;
  double angleLimit = PI / 5;
  const int numLimit = 80;
  for (int i = 0; i < ptContours.size(); i++) {
    cv::Point2f circleCenter;
    RansacCircleFit2(ptContours[i], 10, inliers_indexes, circleCenter, radius);
    if (ptContours[i].size() < numLimit)
      continue;

    cv::Point2f ptDiff = ptContours[i].back();
    double angle = 0;
    cv::Point2f v1, v2;
    v1.x = ptContours[i].back().x - circleCenter.x;
    v1.y = ptContours[i].back().y - circleCenter.y;

    v2.x = ptContours[i][0].x - circleCenter.x;
    v2.y = ptContours[i][0].y - circleCenter.y;
    bool isCal = angleVecVec(v1, v2, angle);
    if (!isCal || angle > angleLimit)
      continue;

    for (int j = 0; j < ptContours[i].size() - 1; j++) {
      v1.x = ptContours[i][i].x - circleCenter.x;
      v1.y = ptContours[i][i].y - circleCenter.y;

      v2.x = ptContours[i][i + 1].x - circleCenter.x;
      v2.y = ptContours[i][i + 1].y - circleCenter.y;
      isCal = angleVecVec(v1, v2, angle);
      if (!isCal || angle > angleLimit) {
        isCal = false;
        break;
      }
    }

    if (radius >= rLimit || !isCal)
      continue;
    else {
      Center.x = circleCenter.x;
      Center.y = circleCenter.y;

      index = i;
      return index;
    }
  }
  return index;
}
cv::Mat gaussianFilter(cv::Mat &inputImage, int kernelSize, double sigma) {
  cv::Mat filteredImage;
  cv::GaussianBlur(inputImage, filteredImage, cv::Size(kernelSize, kernelSize),
                   sigma);
  return filteredImage;
}
int CalBinaryThresh(const cv::Mat &matMask, const cv::Mat &inputImage) {
  int nWidth = matMask.cols;
  int nHeight = matMask.rows;

  int threshold = -1;
  double G = 0;
  for (int T = 0; T < 256; T++) {
    double n1 = 0, n2 = 0;
    double m1 = 0, m2 = 0;
    double p1 = 0, p2 = 0;
    for (int i = 0; i < nHeight; i++) {
      const uchar *ptr = inputImage.ptr<uchar>(i);
      const uchar *ptrMask = matMask.ptr<uchar>(i);
      for (int j = 0; j < nWidth; j++) {
        if (!ptrMask[j] || ptr[j] < 1 || ptr[j] > 250)
          continue;
        int pixel = ptr[j];
        if (pixel > T) {
          n2 += 1;
          m2 += pixel;
        } else {
          n1 += 1;
          m1 += pixel;
        }
      }
    }
    if (n1 == 0)
      m1 = 0;
    else if (n2 == 0)
      m2 = 0;
    else {
      m1 /= n1;
      m2 /= n2;
    }
    p1 = n1 / (n1 + n2);
    p2 = n2 / (n1 + n2);
    double GTemp = p1 * p2 * pow(m1 - m2, 2);
    if (GTemp > G) {
      threshold = T;
      G = GTemp;
    }
  }

  return threshold;
}
void GetMatThresh(const cv::Mat &matMask, const cv::Mat &inputImage,
                  cv::Mat &matBinary, int threshold) {
  int nWidth = matMask.cols;
  int nHeight = matMask.rows;
  matBinary = cv::Mat::zeros(nHeight, nWidth, CV_8UC1);

  for (int i = 0; i < nHeight; i++) {
    uchar *ptr = matBinary.ptr<uchar>(i);
    const uchar *ptrIn = inputImage.ptr<uchar>(i);
    const uchar *ptrMask = matMask.ptr<uchar>(i);
    for (int j = 0; j < nWidth; j++) {
      if (!ptrMask[j])
        continue;

      int pixel = ptrIn[j];
      if (pixel > threshold)
        ptr[j] = 255;
      else
        ptr[j] = 0;
    }
  }
}

int RegionBinarization(const cv::Mat &matMask, const cv::Mat &inputImage,
                       cv::Mat &matBinary) {
  if (inputImage.empty() || matMask.empty() || inputImage.channels() != 1 ||
      matMask.channels() != 1)
    return -1;

  int threshold = CalBinaryThresh(matMask, inputImage);
  if (threshold == -1)
    return -1;

  GetMatThresh(matMask, inputImage, matBinary, threshold);
  return 0;
}
cv::Mat sobelEdgeDetection(cv::Mat inputImage, int kernelSize) {
  if (inputImage.channels() == 3)
    cv::cvtColor(inputImage, inputImage, cv::COLOR_BGR2GRAY);

  cv::Mat gradientX, gradientY;
  cv::Sobel(inputImage, gradientX, CV_16S, 1, 0, kernelSize, 1, 0,
            cv::BORDER_REPLICATE);
  cv::Sobel(inputImage, gradientY, CV_16S, 0, 1, kernelSize, 1, 0,
            cv::BORDER_REPLICATE);

  cv::Mat absGradientX, absGradientY;
  cv::convertScaleAbs(gradientX, absGradientX);
  cv::convertScaleAbs(gradientY, absGradientY);

  cv::Mat gradientImage;
  cv::addWeighted(absGradientX, 0.5, absGradientY, 0.5, 0, gradientImage);

  return gradientImage;
}

void cannyAdaptive(cv::Mat &edgeImage, cv::Mat &grayImage, cv::Mat matMask,
                   int kernelSZ = 3) {
  cv::Mat gradImg = sobelEdgeDetection(grayImage, kernelSZ);

  int hist[256] = {};
  for (int i = 0; i < gradImg.rows; i++) {
    uchar *ptr = gradImg.ptr<uchar>(i);
    const uchar *ptrMask = matMask.ptr<uchar>(i);
    for (int j = 0; j < gradImg.cols; j++) {
      if (ptrMask[j] == 255) {
        int val = ptr[j];
        hist[val]++;
      }
    }
  }

  cv::Scalar sumMat = cv::sum(matMask);
  int sum = static_cast<int>(sumMat[0] / (255 * 2));
  int valSum = 0;
  int pixel = 0;
  for (int i = 0; i < 256; i++) {
    valSum += hist[i];
    if (valSum > sum) {
      pixel = i;
      break;
    }
  }

  int gradMin = 20;
  int thresh = std::max(pixel, gradMin);
  cv::Canny(grayImage, edgeImage, thresh, thresh * 3);
}

cv::Mat cannyEdgeDetectionAdaptive(const cv::Mat &inputImage,
                                   const cv::Mat &matMask) {
  if (inputImage.empty())
    return cv::Mat();

  cv::Mat gray;
  if (inputImage.channels() == 1)
    inputImage.copyTo(gray);
  else if (inputImage.channels() > 1)
    cv::cvtColor(inputImage, gray, cv::COLOR_BGR2GRAY);
  else
    return cv::Mat();

  cv::Mat grayImage = gaussianFilter(gray, 3, 1.05);

  cv::Mat matThresh;
  int isSuccess = RegionBinarization(matMask, grayImage, matThresh);

  cv::Mat edgeImage;
  if (isSuccess == 0) {
    cv::Canny(matThresh, edgeImage, 100, 180);
  } else {
    cannyAdaptive(edgeImage, grayImage, matMask);
  }

  return edgeImage;
}

cv::Mat GetCircleArea(cv::Mat inputImage, vector<cv::Point> &ptArr,
                      int nThickness, cv::Mat &matMaskOut) {
  if (inputImage.empty()) {
    return cv::Mat();
  }
  int nWidth = inputImage.cols;
  int nHeight = inputImage.rows;
  cv::Mat matResult = cv::Mat::zeros(nHeight, nWidth, CV_8UC3);
  cv::Mat matMask = cv::Mat::zeros(nHeight, nWidth, CV_8UC1);

  cv::Point2f ptCenter;
  double dRadius;
  GetCircle(ptArr[0], ptArr[1], ptArr[2], ptCenter, dRadius);

  circle(matMask, ptCenter, (int)(dRadius + nThickness), cv::Scalar(255), -1);
  circle(matMask, ptCenter, std::max((int)(dRadius - nThickness), 0),
         cv::Scalar(0), -1);

  for (int j = 0; j < nHeight; j++) {
    for (int i = 0; i < nWidth; i++) {
      if (matMask.data[j * nWidth + i] == 255) {
        matResult.data[j * nWidth * 3 + 3 * i + 0] =
            inputImage.data[j * nWidth * 3 + 3 * i + 0];
        matResult.data[j * nWidth * 3 + 3 * i + 1] =
            inputImage.data[j * nWidth * 3 + 3 * i + 1];
        matResult.data[j * nWidth * 3 + 3 * i + 2] =
            inputImage.data[j * nWidth * 3 + 3 * i + 2];
      } else
        continue;
    }
  }

  matMask.copyTo(matMaskOut);

  return matResult;
}

void CircleFit(const std::vector<cv::Point2f> &points,
               cv::Point2f &circleCenter, double &radius) {
  if (points.empty()) {
    return;
  }

  double XiSum = 0;
  double Xi2Sum = 0;
  double Xi3Sum = 0;
  double YiSum = 0;
  double Yi2Sum = 0;
  double Yi3Sum = 0;
  double XiYiSum = 0;
  double Xi2YiSum = 0;
  double XiYi2Sum = 0;
  double WiSum = 0;

  for (size_t i = 0; i < points.size(); i++) {
    XiSum += points.at(i).x;
    Xi2Sum += points.at(i).x * points.at(i).x;
    Xi3Sum += points.at(i).x * points.at(i).x * points.at(i).x;
    YiSum += points.at(i).y;
    Yi2Sum += points.at(i).y * points.at(i).y;
    Yi3Sum += points.at(i).y * points.at(i).y * points.at(i).y;
    XiYiSum += points.at(i).x * points.at(i).y;
    Xi2YiSum += points.at(i).x * points.at(i).x * points.at(i).y;
    XiYi2Sum += points.at(i).x * points.at(i).y * points.at(i).y;
    WiSum += 1;
  }
  const int N = 3;
  cv::Mat A = cv::Mat::zeros(N, N, CV_64FC1);
  cv::Mat B = cv::Mat::zeros(N, 1, CV_64FC1);

  A.at<double>(0, 0) = Xi2Sum;
  A.at<double>(0, 1) = XiYiSum;
  A.at<double>(0, 2) = XiSum;

  A.at<double>(1, 0) = XiYiSum;
  A.at<double>(1, 1) = Yi2Sum;
  A.at<double>(1, 2) = YiSum;

  A.at<double>(2, 0) = XiSum;
  A.at<double>(2, 1) = YiSum;
  A.at<double>(2, 2) = WiSum;

  B.at<double>(0, 0) = -(Xi3Sum + XiYi2Sum);
  B.at<double>(1, 0) = -(Xi2YiSum + Yi3Sum);
  B.at<double>(2, 0) = -(Xi2Sum + Yi2Sum);

  cv::Mat X;
  cv::solve(A, B, X, cv::DECOMP_LU);
  double a = X.at<double>(0, 0);
  double b = X.at<double>(1, 0);
  double c = X.at<double>(2, 0);

  circleCenter.x = static_cast<float>(-0.5 * a);
  circleCenter.y = static_cast<float>(-0.5 * b);
  radius = 0.5 * std::sqrt(a * a + b * b - 4 * c);
}
tuple<cv::Point2f, float> GetCircleFit(vector<cv::Point2f> &vecPt) {
  cv::Point2f ptCenter;
  double fRadius = 0;

  std::vector<int> inliers_indexes;
  RansacCircleFit2(vecPt, 10, inliers_indexes, ptCenter, fRadius);

  cv::Point2f ptCenter2;
  CircleFit(vecPt, ptCenter2, fRadius);
  ptCenter.x = ptCenter2.x;
  ptCenter.y = ptCenter2.y;

  return {ptCenter, static_cast<float>(fRadius)};
}

tuple<cv::Point2f, float> Image::CircleFit_(vector<cv::Point2f> &vecPt) {
  cv::Point2f ptCenter;
  double fRadius = 0;

  std::vector<int> inliers_indexes;
  RansacCircleFit2(vecPt, 100, inliers_indexes, ptCenter, fRadius);

  cv::Point2f ptCenter2;
  CircleFit(vecPt, ptCenter2, fRadius);
  ptCenter.x = ptCenter2.x;
  ptCenter.y = ptCenter2.y;

  return {ptCenter, static_cast<float>(fRadius)};
}

#define EDGEDETECTMINNUM 10
int Image::GetLargestCircle(cv::Mat matInput, cv::Point2f &ptOut,
                            double &radiusOut) {
  if (matInput.empty())
    return -1;

  cv::Mat matGray = matInput.clone();
  if (matGray.channels() != 1)
    cvtColor(matGray, matGray, cv::COLOR_BGR2GRAY);

  int width = matGray.cols;
  int height = matGray.rows;

  cv::Mat matThresh;
  cv::Scalar meanVal = mean(matGray);
  double dMeanVal = meanVal[0];
  cv::threshold(matGray, matThresh, dMeanVal * 0.9, 255,
                cv::THRESH_BINARY | cv::THRESH_OTSU);

  cv::Mat kerThresh = getStructuringElement(cv::MORPH_RECT, cv::Size(20, 20));
  cv::dilate(matThresh, matThresh, kerThresh);
  cv::erode(matThresh, matThresh, kerThresh);

  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(matThresh, contours, hierarchy, cv::RETR_TREE,
                   cv::CHAIN_APPROX_SIMPLE);

  int circumference = 100;
  vector<cv::Point2f> ptContoursTemp;
  std::vector<std::vector<cv::Point2f>> ptContours;
  ptContours.reserve(contours.size());
  cv::Point pC, pV;
  for (auto &v : contours) {
    int sz = static_cast<int>(v.size());
    if (sz < circumference)
      continue;

    ptContoursTemp.resize(sz);
    for (int i = 0; i < sz; i++)
      ptContoursTemp[i] =
          cv::Point2f(static_cast<float>(v[i].x), static_cast<float>(v[i].y));

    RemoveDuplicatePoints(ptContoursTemp, true);
    pC.x = static_cast<int>(ptContoursTemp[0].x);
    pC.y = static_cast<int>(ptContoursTemp[0].y);
    pV.x = static_cast<int>(v.back().x);
    pV.y = static_cast<int>(v.back().y);
    int dist = calPointPointDist(pC, pV);
    if (dist > 50)
      continue;
    ptContours.push_back(ptContoursTemp);
  }
  sort(ptContours.begin(), ptContours.end(),
       [](vector<cv::Point2f> &a, vector<cv::Point2f> &b) {
         return a.size() > b.size();
       });

  double fRadius = 0;
  cv::Point2f circleCenter;
  double rLimit = std::min(height, width) / 2;
  int index = fitLargestCircle(ptContours, circleCenter, fRadius, rLimit);

  if (index == -1) {
    return -2;
  }

  vector<cv::Point> ptAnchor(3);
  for (int i = 0; i < 3; i++) {
    double angle = 120.0 / 360.0 * 2 * PI * i;
    ptAnchor[i].x = static_cast<int>(cos(angle) * fRadius + circleCenter.x);
    ptAnchor[i].y = static_cast<int>(sin(angle) * fRadius + circleCenter.y);
  }

  int nThickness = 10;
  cv::Mat matMask;
  cv::Mat matROI = GetCircleArea(matInput, ptAnchor, nThickness, matMask);
  cv::Mat matEdge = cannyEdgeDetectionAdaptive(matROI, matMask);
  if (matEdge.empty()) {
    return -2;
  }
  cv::Mat kernel = getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
  cv::erode(matMask, matMask, kernel);

  cv::Mat matEdgeN = cv::Mat::zeros(height, width, CV_8UC1);
  vector<cv::Point2f> vecResult;
  vecResult.reserve(width * height);
  for (int j = 0; j < height; j++)
    for (int i = 0; i < width; i++)
      if (matMask.data[j * width + i] == 255 &&
          matEdge.data[j * width + i] == 255) {
        cv::Point2f pt(static_cast<float>(i), static_cast<float>(j));
        vecResult.push_back(pt);
      }

  if (vecResult.size() < EDGEDETECTMINNUM) {
    return -2;
  }
  auto [center, radius] = GetCircleFit(vecResult);
  ptOut.x = center.x;
  ptOut.y = center.y;
  radiusOut = radius;

  return 0;
}

cv::Mat createLanserKernel(int kernel_size, double sigma) {
  cv::Mat kernel(kernel_size, kernel_size, CV_32FC1, cv::Scalar(0));
  int center = kernel_size / 2;
  double sigma2 = sigma * sigma;
  double sigma4 = sigma2 * sigma2;

  for (int i = 0; i < kernel_size; i++) {
    for (int j = 0; j < kernel_size; j++) {
      double x = j - center;
      double y = i - center;
      kernel.at<float>(i, j) =
          static_cast<float>((x * x / sigma4 - 1.0 / sigma2) *
                             exp(-(x * x + y * y) / (2 * sigma2)));
    }
  }

  double sum = cv::sum(kernel)[0];
  if (sum != 0)
    kernel /= sum;

  return kernel;
}

float bilinearInterp(const cv::Mat &mag, float x, float y) {
  int x0 = static_cast<int>(floor(x));
  int y0 = static_cast<int>(floor(y));
  int x1 = x0 + 1;
  int y1 = y0 + 1;

  if (x0 < 0 || x1 >= mag.cols || y0 < 0 || y1 >= mag.rows) {
    return 0.0f;
  }

  float dx = x - x0;
  float dy = y - y0;

  return (1 - dx) * (1 - dy) * mag.at<float>(y0, x0) +
         dx * (1 - dy) * mag.at<float>(y0, x1) +
         (1 - dx) * dy * mag.at<float>(y1, x0) +
         dx * dy * mag.at<float>(y1, x1);
}

bool findIntersection(const std::vector<double> &line,
                      const std::vector<cv::Point2f> &segment,
                      cv::Point2f &ptCross) {
  double a = line[0];
  double b = line[1];

  double dx = segment[1].x - segment[0].x;
  double dy = segment[1].y - segment[0].y;

  double x0 = line[2] - segment[0].x;
  double y0 = line[3] - segment[0].y;

  double det = a * dy - b * dx;

  if (std::abs(det) < 1e-6) {
    return false;
  }

  double s = (a * y0 - b * x0) / det;

  if (s >= 0.0 && s <= 1.0) {
    ptCross.x = static_cast<float>(segment[0].x + s * dx);
    ptCross.y = static_cast<float>(segment[0].y + s * dy);
    return true;
  }

  return false;
}

std::vector<cv::Point2f>
Line2Lseg(const std::vector<double> &line,
          const std::vector<std::vector<cv::Point2f>> &ptFrame) {
  try {
    std::vector<cv::Point2f> ptSE;
    ptSE.reserve(2);
    cv::Point2f ptTemp;
    for (auto &p : ptFrame) {
      bool isFind = findIntersection(line, p, ptTemp);
      if (isFind) {
        ptSE.push_back(ptTemp);
        if (ptSE.size() == 2)
          break;
      }
    }

    return ptSE;
  } catch (const cv::Exception &) {
    throw;
  }
}

std::vector<std::vector<cv::Point2f>>
getPtFrame(std::vector<cv::Point2f> &ptAnchor, int nThickness) {
  using namespace std;
  cv::Point2f VecLine = ptAnchor[1] - ptAnchor[0];
  double length = sqrt(pow(VecLine.x, 2) + pow(VecLine.y, 2));
  VecLine /= length;

  double cos90 = cos(PI / 2);
  double sin90 = sin(PI / 2);

  cv::Point2f Vec90;
  Vec90.x = static_cast<float>(cos90 * VecLine.x - sin90 * VecLine.y);
  Vec90.y = static_cast<float>(sin90 * VecLine.x - cos90 * VecLine.y);
  Vec90 *= nThickness;

  auto &pt1 = ptAnchor[0];
  auto &pt2 = ptAnchor[1];

  cv::Point2f ptRect[4];
  ptRect[0].x = pt1.x - Vec90.x;
  ptRect[0].y = pt1.y - Vec90.y;
  ptRect[1].x = pt1.x + Vec90.x;
  ptRect[1].y = pt1.y + Vec90.y;
  ptRect[2].x = pt2.x + Vec90.x;
  ptRect[2].y = pt2.y + Vec90.y;
  ptRect[3].x = pt2.x - Vec90.x;
  ptRect[3].y = pt2.y - Vec90.y;
  std::vector<std::vector<cv::Point2f>> ptFrame(4, std::vector<cv::Point2f>(2));
  ptFrame[0][0] = ptRect[0];
  ptFrame[0][1] = ptRect[1];

  ptFrame[1][0] = ptRect[2];
  ptFrame[1][1] = ptRect[3];

  ptFrame[2][0] = ptRect[1];
  ptFrame[2][1] = ptRect[2];

  ptFrame[3][0] = ptRect[3];
  ptFrame[3][1] = ptRect[0];

  return ptFrame;
}

std::vector<cv::Point2f> Image::FitLine_(std::vector<cv::Point2f> &ptIn,
                                         std::vector<cv::Point2f> &ptAnchor,
                                         int nThickness) {
  try {
    cv::Vec4d lineParams;
    cv::fitLine(ptIn, lineParams, cv::DIST_L2, 0, 0.01, 0.01);

    std::vector<double> line(4);
    line[0] = lineParams[0];
    line[1] = lineParams[1];
    line[2] = lineParams[2];
    line[3] = lineParams[3];

    std::vector<std::vector<cv::Point2f>> ptFrame =
        getPtFrame(ptAnchor, nThickness);
    return Line2Lseg(line, ptFrame);
  } catch (const cv::Exception &) {
    throw;
  }
}

bool isRectRingInBound(const vector<cv::Point> &ptIn, const Point2f &anchorPt,
                       int nThickness, int width, int height) {
  (void)nThickness;
  for (auto &p : ptIn) {
    if (p.x < 0 || p.y < 0 || p.x >= width || p.y >= height)
      return false;
  }

  if (anchorPt.x < 0 || anchorPt.y < 0 || anchorPt.x >= width ||
      anchorPt.y >= height)
    return false;

  Point2f ptMid = (ptIn[1] + ptIn[0]) / 2;
  Point2f ptVec = anchorPt - ptMid;
  for (auto &p : ptIn) {
    if (p.x + ptVec.x < 0 || p.x - ptVec.x < 0 || p.x + ptVec.x >= width ||
        p.x - ptVec.x >= width)
      return false;
    if (p.y + ptVec.y < 0 || p.y - ptVec.y < 0 || p.y + ptVec.y >= height ||
        p.y - ptVec.y >= height)
      return false;
  }

  return true;
}

struct SetConfig {
  SetConfig() {
    method = 0;
    FixedNum = 100;
    FixedDist = 12.0;
  }

  SetConfig operator=(SetConfig scfgIn) {
    SetConfig sfgOut;
    sfgOut.method = scfgIn.method;
    sfgOut.FixedNum = scfgIn.FixedNum;
    sfgOut.FixedDist = scfgIn.FixedDist;
    return sfgOut;
  }

  int method = 0;
  int FixedNum = 100;
  double FixedDist = 12.0;
};

vector<Point2f> ProcessLineSegment(const Mat &imgInput, double scale,
                                   const vector<cv::Point> &ptIn,
                                   vector<cv::Point2f> &ptOut,
                                   const Point2f &ptAnchor,
                                   int &bIsBlackToWhite, SetConfig &scfg) {
  (void)imgInput;
  (void)scale;
  (void)ptIn;
  (void)ptOut;
  (void)ptAnchor;
  (void)bIsBlackToWhite;
  (void)scfg;
  vector<Point2f> vecResult;

  return vecResult;
}

void Image::FitLightImage() {
  Mat matpySrc, resimg, matxor;
  getmat().copyTo(matpySrc);
  cv::pyrDown(matpySrc, matpySrc);
  cv::pyrDown(matpySrc, matpySrc);
  cv::pyrDown(matpySrc, matpySrc);
  cv::Mat gray, matcolor;
  if (matpySrc.channels() == 3)
    cvtColor(matpySrc, gray, cv::COLOR_BGR2GRAY);
  else
    gray = matpySrc;

  cv::threshold(gray, resimg, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);
  if (matpySrc.channels() == 3)
    cvtColor(resimg, matcolor, cv::COLOR_GRAY2BGR);

  matImage = matcolor;

  cv::Mat mask;
  mask = resimg;

  for (int iy = 0; iy < matxor.rows; ++iy) {
    for (int ix = 0; ix < matxor.cols; ++ix) {
      if (matpySrc.at<cv::Vec3b>(iy, ix) == cv::Vec3b(255, 255, 255))
        mask.at<cv::Scalar>(iy, ix) = cv::Scalar(0);
    }
  }

  Mat matSrc;
  matpySrc.copyTo(matSrc);
  cv::Scalar meanVal = cv::mean(matSrc, mask);
  (void)meanVal;

  return;
}

void Image::Test(void *pshape) {

  PointsShape *tpoints = (PointsShape *)pshape;
  ;
  if (tpoints == nullptr)
    return;

  std::vector<cv::Point2f> corners;
  featureDet2(this->getmat(), corners);
  tpoints->clear();
  for (const auto &apoint : corners) {
    gp_Pnt axyzpoint(apoint.x, apoint.y, 0);
    tpoints->addpoint(axyzpoint);
  }
  tpoints->setshow(1);
}

inline float getAntiAliasGray(const Mat &gray, const Point2f &pt) {
  int x = static_cast<int>(pt.x);
  int y = static_cast<int>(pt.y);
  float u = pt.x - x;
  float v = pt.y - y;

  if (x < 1 || x >= gray.cols - 2 || y < 1 || y >= gray.rows - 2) {
    if (x < 0 || x >= gray.cols - 1 || y < 0 || y >= gray.rows - 1)
      return 0.0f;

    float g00 = gray.at<uchar>(y, x);
    float g10 = gray.at<uchar>(y, x + 1);
    float g01 = gray.at<uchar>(y + 1, x);
    float g11 = gray.at<uchar>(y + 1, x + 1);
    float g0 = g00 * (1 - u) + g10 * u;
    float g1 = g01 * (1 - u) + g11 * u;
    return g0 * (1 - v) + g1 * v;
  }

  float g[4][4];
  for (int dy = -1; dy <= 2; dy++)
    for (int dx = -1; dx <= 2; dx++)
      g[dy + 1][dx + 1] = gray.at<uchar>(y + dy, x + dx);

  auto cubic = [](float t) {
    t = fabs(t);
    return (t <= 1)   ? 2 * t * t * t - 3 * t * t + 1
           : (t <= 2) ? -t * t * t + 3 * t * t - 2 * t
                      : 0.0f;
  };

  float result = 0.0f;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++)
      result += g[j][i] * cubic(i - 1 - u) * cubic(j - 1 - v);

  return clamp(result, 0.0f, 255.0f);
}
inline float getRobustAntiAliasGray(const Mat &gray, const Point2f &pt) {
  int x = static_cast<int>(pt.x);
  int y = static_cast<int>(pt.y);
  float u = pt.x - x;
  float v = pt.y - y;

  if (x < 1 || x >= gray.cols - 2 || y < 1 || y >= gray.rows - 2) {
    if (x < 0 || x >= gray.cols - 1 || y < 0 || y >= gray.rows - 1)
      return 0.0f;

    float g00 = gray.at<uchar>(y, x);
    float g10 = gray.at<uchar>(y, x + 1);
    float g01 = gray.at<uchar>(y + 1, x);
    float g11 = gray.at<uchar>(y + 1, x + 1);
    float g0 = g00 * (1 - u) + g10 * u;
    float g1 = g01 * (1 - u) + g11 * u;
    return g0 * (1 - v) + g1 * v;
  }

  float g[4][4];
  float sum = 0, sum2 = 0;
  int count = 0;
  for (int dy = -1; dy <= 2; dy++) {
    for (int dx = -1; dx <= 2; dx++) {
      g[dy + 1][dx + 1] = gray.at<uchar>(y + dy, x + dx);
      sum += g[dy + 1][dx + 1];
      sum2 += g[dy + 1][dx + 1] * g[dy + 1][dx + 1];
      count++;
    }
  }
  float mean = sum / count;
  float variance = (sum2 / count) - (mean * mean);

  const float textureThreshold = 300.0f;
  if (variance < textureThreshold) {
    auto cubic = [](float t) {
      t = fabs(t);
      return (t <= 1)   ? 2 * t * t * t - 3 * t * t + 1
             : (t <= 2) ? -t * t * t + 3 * t * t - 2 * t
                        : 0.0f;
    };

    float result = 0.0f;
    for (int j = 0; j < 4; j++)
      for (int i = 0; i < 4; i++)
        result += g[j][i] * cubic(i - 1 - u) * cubic(j - 1 - v);

    return clamp(result, 0.0f, 255.0f);
  } else {
    float smoothG[4][4];
    for (int j = 0; j < 4; j++) {
      for (int i = 0; i < 4; i++) {
        float s = 0, w = 0;
        for (int dj = -1; dj <= 1; dj++) {
          for (int di = -1; di <= 1; di++) {
            int nj = min(3, max(0, j + dj));
            int ni = min(3, max(0, i + di));
            float weight = (dj == 0 && di == 0) ? 1.0f : 0.25f;
            s += g[nj][ni] * weight;
            w += weight;
          }
        }
        smoothG[j][i] = s / w;
      }
    }

    auto cubic = [](float t) {
      t = fabs(t);
      return (t <= 1)   ? 2 * t * t * t - 3 * t * t + 1
             : (t <= 2) ? -t * t * t + 3 * t * t - 2 * t
                        : 0.0f;
    };

    float result = 0.0f;
    for (int j = 0; j < 4; j++)
      for (int i = 0; i < 4; i++)
        result += smoothG[j][i] * cubic(i - 1 - u) * cubic(j - 1 - v);

    return clamp(result, 0.0f, 255.0f);
  }
}

vector<Point2f> localSubpixelRefine(const Mat &gray,
                                    const vector<Point> &points,
                                    int boundaryIdx, int localRange = 3,
                                    int subPixelDensity = 5) {

  vector<Point2f> subpixelPoints;
  if (points.empty())
    return subpixelPoints;

  int start = max(0, boundaryIdx - localRange);
  int end = min((int)points.size() - 1, boundaryIdx + localRange);
  if (start >= end)
    return subpixelPoints;

  vector<Point2f> linearPoints;
  for (int i = start; i < end; i++) {
    const Point &p1 = points[i];
    const Point &p2 = points[i + 1];
    for (int s = 0; s <= subPixelDensity; s++) {
      float t = static_cast<float>(s) / subPixelDensity;
      linearPoints.emplace_back(p1.x + t * (p2.x - p1.x),
                                p1.y + t * (p2.y - p1.y));
    }
  }
  if (linearPoints.empty())
    return subpixelPoints;

  vector<float> grays;
  for (const auto &pt : linearPoints) {
    grays.push_back(getRobustAntiAliasGray(gray, pt));
  }

  float grayThreshold = 20.0f;
  int p3 = 0;
  int p4 = static_cast<int>(linearPoints.size()) - 1;
  bool foundP3 = false;

  for (int i = 1; i < grays.size(); i++) {
    float diff = abs(grays[i] - grays[i - 1]);
    if (!foundP3 && diff > grayThreshold) {
      p3 = i - 1;
      foundP3 = true;
    }
    if (foundP3 && diff <= grayThreshold * 0.3) {
      p4 = i;
      break;
    }
  }

  if (!foundP3) {
    int mid = static_cast<int>(linearPoints.size()) / 2;
    p3 = max(0, mid - subPixelDensity / 2);
    p4 = min((int)linearPoints.size() - 1, mid + subPixelDensity / 2);
  }

  int stepCount = p4 - p3 + 1;
  if (stepCount < 2) {
    subpixelPoints.push_back(linearPoints[p3]);
    return subpixelPoints;
  }

  float ratio = 0.5f;
  int ratioIdx = p3 + static_cast<int>(stepCount * ratio);
  ratioIdx = max(p3, min(p4, ratioIdx));
  subpixelPoints.push_back(linearPoints[ratioIdx]);

  return subpixelPoints;
}

vector<int> detectBoundaryPoints(const Mat &gray, const vector<Point> &points,
                                 float threshold = 30.0f) {
  vector<int> boundaries;
  if (points.size() < 3)
    return boundaries;

  for (int i = 1; i < points.size() - 1; i++) {
    float g_prev = gray.at<uchar>(points[i - 1].y, points[i - 1].x);
    float g_curr = gray.at<uchar>(points[i].y, points[i].x);
    float g_next = gray.at<uchar>(points[i + 1].y, points[i + 1].x);

    if (abs(g_curr - g_prev) > threshold && abs(g_next - g_curr) > threshold) {
      boundaries.push_back(i);
    }
  }
  return boundaries;
}

vector<Point2f> localSubpixelRefine(const Mat &gray,
                                    const vector<Point> &points,
                                    int boundaryIdx, double dThreshold,
                                    int localRange = 8,
                                    int subPixelDensity = 5) {

  vector<Point2f> subpixelPoints;
  if (points.empty())
    return subpixelPoints;

  int start = std::max(0, boundaryIdx - localRange);
  int end = std::min((int)points.size() - 1, boundaryIdx + localRange);
  if (start >= end)
    return subpixelPoints;

  std::vector<float> grays0;
  for (int i = start; i < end; i++) {
    cv::Point p1 = points[i];
    float g00 = gray.at<uchar>(p1.y, p1.x);
    grays0.push_back(g00);
  }

  std::vector<cv::Point2f> linearPoints;
  for (int i = start; i < end; i++) {
    const Point &p1 = points[i];
    const Point &p2 = points[i + 1];
    for (int s = 0; s <= subPixelDensity; s++) {
      float t = static_cast<float>(s) / subPixelDensity;
      Point2f interpPt(p1.x + t * (p2.x - p1.x), p1.y + t * (p2.y - p1.y));
      linearPoints.push_back(interpPt);
    }
  }
  if (linearPoints.empty())
    return subpixelPoints;

  vector<float> grays;
  for (const auto &pt : linearPoints) {
    grays.push_back(getRobustAntiAliasGray(gray, pt));
  }

  float grayThreshold = static_cast<float>(dThreshold);
  int p3 = 0;
  int p4 = static_cast<int>(linearPoints.size()) - 1;
  bool foundP3 = false;

  for (int i = 1; i < grays.size(); i++) {
    float diff = static_cast<float>(abs(grays[i] - grays[i - 1]));
    if (!foundP3 && diff > grayThreshold) {
      p3 = i - 1;
      foundP3 = true;
    }
    if (foundP3 && diff <= grayThreshold * 0.3) {
      p4 = i;
      break;
    }
  }

  if (!foundP3) {
    int mid = static_cast<int>(linearPoints.size()) / 2;
    p3 = max(0, mid - subPixelDensity / 2);
    p4 = min((int)linearPoints.size() - 1, mid + subPixelDensity / 2);
  }

  int stepCount = p4 - p3 + 1;
  if (stepCount < 2) {
    subpixelPoints.push_back(linearPoints[p3]);
    return subpixelPoints;
  }

  float ratio = 0.5f;
  int ratioIdx = p3 + static_cast<int>(stepCount * ratio);
  ratioIdx = max(p3, min(p4, ratioIdx));
  subpixelPoints.push_back(linearPoints[ratioIdx]);

  return subpixelPoints;
}

Point2f optimizeBoundarySubpixel(const Mat &gray,
                                 const vector<Point2f> &localSubpixels,
                                 const vector<Point> &originalPoints,
                                 int boundaryIdx, float lineAngle) {

  Point2f boundaryPt(originalPoints[boundaryIdx]);
  int closestIdx = 0;
  float minDist = static_cast<float>(norm(localSubpixels[0] - boundaryPt));

  for (int i = 1; i < localSubpixels.size(); i++) {
    float dist = static_cast<float>(norm(localSubpixels[i] - boundaryPt));
    if (dist < minDist) {
      minDist = dist;
      closestIdx = i;
    }
  }

  if (closestIdx <= 0 || closestIdx >= localSubpixels.size() - 1)
    return boundaryPt;

  const Point2f &currPt = localSubpixels[closestIdx];
  Point2f dirVec =
      localSubpixels[closestIdx + 1] - localSubpixels[closestIdx - 1];
  float dirLen = static_cast<float>(norm(dirVec)) + 1e-6f;
  dirVec /= dirLen;
  Point2f normalDir(-dirVec.y, dirVec.x);

  float gCurr = static_cast<float>(getRobustAntiAliasGray(gray, currPt));
  float gPos =
      static_cast<float>(getRobustAntiAliasGray(gray, currPt + normalDir));
  float gNeg =
      static_cast<float>(getRobustAntiAliasGray(gray, currPt - normalDir));

  float diffPos = abs(gCurr - gPos);
  float diffNeg = abs(gCurr - gNeg);
  float totalDiff = diffPos + diffNeg + 1e-6f;
  float subOffset = (diffNeg > diffPos) ? -0.5f * (diffPos / totalDiff)
                                        : 0.5f * (diffNeg / totalDiff);
  subOffset = max(-0.5f, min(0.5f, subOffset));

  Point2f subPixel = currPt + normalDir * subOffset;
  float a = sin(lineAngle);
  float b = -cos(lineAngle);
  float c = cos(lineAngle) * originalPoints[0].y -
            sin(lineAngle) * originalPoints[0].x;
  float dist = fabs(a * subPixel.x + b * subPixel.y + c);

  if (dist > 0.3f) {
    float t = (a * (originalPoints[0].x - subPixel.x) +
               b * (originalPoints[0].y - subPixel.y)) /
              (a * a + b * b);
    subPixel.x += a * t;
    subPixel.y += b * t;
  }

  return subPixel;
}

vector<Point2f> simplifiedSubpixelProcess(const Mat &gray,
                                          const vector<Point> &pixelPoints,
                                          int localRange = 3,
                                          int subPixelDensity = 5) {

  vector<int> boundaryIndices = detectBoundaryPoints(gray, pixelPoints);
  if (boundaryIndices.empty()) {
    vector<Point2f> result;
    for (const auto &pt : pixelPoints)
      result.emplace_back((float)pt.x, (float)pt.y);
    return result;
  }

  vector<Point2f> finalPoints;

  vector<bool> processed(pixelPoints.size(), false);

  for (int bIdx : boundaryIndices) {
    int start = max(0, bIdx - localRange);
    for (int i = static_cast<int>(finalPoints.size()); i < start; i++) {
      if (!processed[i]) {
        finalPoints.emplace_back((float)pixelPoints[i].x,
                                 (float)pixelPoints[i].y);
        processed[i] = true;
      }
    }

    vector<Point2f> localSubpixels = localSubpixelRefine(
        gray, pixelPoints, bIdx, 30, localRange, subPixelDensity);

    Point2f optimized = optimizeBoundarySubpixel(
        gray, localSubpixels, pixelPoints, bIdx,
        static_cast<float>(atan2(pixelPoints.back().y - pixelPoints[0].y,
                                 pixelPoints.back().x - pixelPoints[0].x)));

    for (const auto &sp : localSubpixels) {
      finalPoints.push_back(sp);
    }

    for (int i = start;
         i <= min((int)pixelPoints.size() - 1, bIdx + localRange); i++)
      processed[i] = true;
  }

  for (int i = 0; i < pixelPoints.size(); i++) {
    if (!processed[i])
      finalPoints.emplace_back((float)pixelPoints[i].x,
                               (float)pixelPoints[i].y);
  }

  return finalPoints;
}

std::vector<Point2f>
Image::SubpixelProcess(const Mat &gray, const std::vector<Point> &pixelPoints,
                       vector<int> &boundaryIndices, double dThreshold,
                       int localRange, int subPixelDensity) {

  vector<Point2f> result;
  if (boundaryIndices.empty()) {
    return result;
  }

  vector<bool> processed(pixelPoints.size(), false);

  for (int bIdx : boundaryIndices) {
    vector<Point2f> localSubpixels = localSubpixelRefine(
        gray, pixelPoints, bIdx, dThreshold, localRange, subPixelDensity);

    Point2f finalPoint = optimizeBoundarySubpixel(
        gray, localSubpixels, pixelPoints, bIdx,
        static_cast<float>(atan2(pixelPoints.back().y - pixelPoints[0].y,
                                 pixelPoints.back().x - pixelPoints[0].x)));
    result.push_back(finalPoint);
  }

  return result;
}

Vec3f Image::getAntiAliasColor(const Mat &color, const Point2f &pt) {
  CV_Assert(color.type() == CV_8UC3);

  int x = static_cast<int>(pt.x);
  int y = static_cast<int>(pt.y);
  float u = pt.x - x;
  float v = pt.y - y;

  if (x < 1 || x >= color.cols - 2 || y < 1 || y >= color.rows - 2) {
    if (x < 0 || x >= color.cols - 1 || y < 0 || y >= color.rows - 1) {
      return Vec3f(0, 0, 0);
    }

    Vec3b g00 = color.at<Vec3b>(y, x);
    Vec3b g10 = color.at<Vec3b>(y, x + 1);
    Vec3b g01 = color.at<Vec3b>(y + 1, x);
    Vec3b g11 = color.at<Vec3b>(y + 1, x + 1);

    Vec3f g0(g00[2] * (1 - u) + g10[2] * u, g00[1] * (1 - u) + g10[1] * u,
             g00[0] * (1 - u) + g10[0] * u);
    Vec3f g1(g01[2] * (1 - u) + g11[2] * u, g01[1] * (1 - u) + g11[1] * u,
             g01[0] * (1 - u) + g11[0] * u);

    return Vec3f(g0[0] * (1 - v) + g1[0] * v, g0[1] * (1 - v) + g1[1] * v,
                 g0[2] * (1 - v) + g1[2] * v);
  }

  auto cubic = [](float t) {
    t = fabs(t);
    if (t <= 1)
      return 2 * t * t * t - 3 * t * t + 1;
    if (t <= 2)
      return -t * t * t + 3 * t * t - 2 * t;
    return 0.0f;
  };

  Vec3f rgb[4][4];
  for (int dy = -1; dy <= 2; dy++) {
    for (int dx = -1; dx <= 2; dx++) {
      Vec3b bgr = color.at<Vec3b>(y + dy, x + dx);
      rgb[dy + 1][dx + 1] = Vec3f(bgr[2], bgr[1], bgr[0]);
    }
  }

  Vec3f result(0, 0, 0);
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 4; i++) {
      float weight = cubic(i - 1 - u) * cubic(j - 1 - v);
      result[0] += rgb[j][i][0] * weight;
      result[1] += rgb[j][i][1] * weight;
      result[2] += rgb[j][i][2] * weight;
    }
  }

  result[0] = clamp(result[0], 0.0f, 255.0f);
  result[1] = clamp(result[1], 0.0f, 255.0f);
  result[2] = clamp(result[2], 0.0f, 255.0f);

  return result;
}

Vec3f Image::getRobustAntiAliasColor(const Mat &color, const Point2f &pt) {
  CV_Assert(color.type() == CV_8UC3);

  int x = static_cast<int>(pt.x);
  int y = static_cast<int>(pt.y);
  float u = pt.x - x;
  float v = pt.y - y;

  if (x < 1 || x >= color.cols - 2 || y < 1 || y >= color.rows - 2) {
    return getAntiAliasColor(color, pt);
  }

  Vec3f rgb[4][4];
  Vec3f sum(0, 0, 0), sum2(0, 0, 0);
  int count = 0;

  for (int dy = -1; dy <= 2; dy++) {
    for (int dx = -1; dx <= 2; dx++) {
      Vec3b bgr = color.at<Vec3b>(y + dy, x + dx);
      rgb[dy + 1][dx + 1] = Vec3f(bgr[2], bgr[1], bgr[0]);
      sum += rgb[dy + 1][dx + 1];
      sum2[0] += rgb[dy + 1][dx + 1][0] * rgb[dy + 1][dx + 1][0];
      sum2[1] += rgb[dy + 1][dx + 1][1] * rgb[dy + 1][dx + 1][1];
      sum2[2] += rgb[dy + 1][dx + 1][2] * rgb[dy + 1][dx + 1][2];
      count++;
    }
  }

  Vec3f mean = sum / count;
  Vec3f variance(sum2[0] / count - mean[0] * mean[0],
                 sum2[1] / count - mean[1] * mean[1],
                 sum2[2] / count - mean[2] * mean[2]);
  float avgVariance = (variance[0] + variance[1] + variance[2]) / 3.0f;

  const float textureThreshold = 800.0f;
  if (avgVariance < textureThreshold) {
    return getAntiAliasColor(color, pt);
  } else {
    Vec3f smoothRgb[4][4];
    for (int j = 0; j < 4; j++) {
      for (int i = 0; i < 4; i++) {
        Vec3f s(0, 0, 0);
        float wSum = 0;
        for (int dj = -1; dj <= 1; dj++) {
          for (int di = -1; di <= 1; di++) {
            int nj = min(3, max(0, j + dj));
            int ni = min(3, max(0, i + di));
            float weight = (dj == 0 && di == 0) ? 1.0f : 0.25f;
            s += rgb[nj][ni] * weight;
            wSum += weight;
          }
        }
        smoothRgb[j][i] = s / wSum;
      }
    }

    auto cubic = [](float t) {
      t = fabs(t);
      return (t <= 1)   ? 2 * t * t * t - 3 * t * t + 1
             : (t <= 2) ? -t * t * t + 3 * t * t - 2 * t
                        : 0.0f;
    };

    Vec3f result(0, 0, 0);
    for (int j = 0; j < 4; j++) {
      for (int i = 0; i < 4; i++) {
        float weight = cubic(i - 1 - u) * cubic(j - 1 - v);
        result += smoothRgb[j][i] * weight;
      }
    }

    return Vec3f(clamp(result[0], 0.0f, 255.0f), clamp(result[1], 0.0f, 255.0f),
                 clamp(result[2], 0.0f, 255.0f));
  }
}

cv::Point2f calculateIntersection(float rho1, float theta1, float rho2,
                                  float theta2) {
  float a1 = cos(theta1);
  float b1 = sin(theta1);
  float a2 = cos(theta2);
  float b2 = sin(theta2);

  float det = a1 * b2 - a2 * b1;
  if (std::abs(det) < 1e-5) {
    return cv::Point2f(-1, -1);
  }

  float x = (b2 * rho1 - b1 * rho2) / det;
  float y = (a1 * rho2 - a2 * rho1) / det;
  return cv::Point2f(x, y);
}
cv::Point2f FindCenter(const cv::Mat matSrc) {
  cv::Point2f ptCenter(0, 0);
  if (matSrc.empty()) {
    return ptCenter;
  }
  cv::Mat gray;
  cv::cvtColor(matSrc, gray, cv::COLOR_BGR2GRAY);
  cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0.5);
  cv::Mat otsu_thresh;
  double otsu_thresh_val = cv::threshold(gray, otsu_thresh, 0, 255,
                                         cv::THRESH_BINARY + cv::THRESH_OTSU);
  double low_threshold = otsu_thresh_val * 0.5;
  double high_threshold = otsu_thresh_val * 1.5;
  cv::Mat edges;
  cv::Canny(gray, edges, low_threshold, high_threshold);
  double edge_strength = cv::countNonZero(edges);

  int min_vote_threshold = 50;
  int max_vote_threshold = 200;
  int vote_threshold = cv::saturate_cast<int>(
      min_vote_threshold + (edge_strength / gray.total()) *
                               (max_vote_threshold - min_vote_threshold));

  std::vector<cv::Vec2f> lines;
  cv::HoughLines(edges, lines, 1, CV_PI / 180, vote_threshold);

  cv::Vec2f line1, line2;
  double max_diff = 0;

  for (size_t i = 0; i < lines.size(); i++) {
    for (size_t j = i + 1; j < lines.size(); j++) {
      double theta1 = lines[i][1];
      double theta2 = lines[j][1];

      double angle = std::abs(theta1 - theta2);
      if (angle > CV_PI) {
        angle = 2 * CV_PI - angle;
      }

      if (angle < 0.05 || angle > CV_PI - 0.05) {
        continue;
      }

      if (std::abs(angle - CV_PI / 2) < std::abs(max_diff - CV_PI / 2)) {
        max_diff = angle;
        line1 = lines[i];
        line2 = lines[j];
      }
    }
  }
  ptCenter = calculateIntersection(line1[0], line1[1], line2[0], line2[1]);
  std::vector<cv::Point2f> corners = {ptCenter};
  try {
    cv::Size winSize = cv::Size(30, 30);
    cv::Size zeroZone = cv::Size(-1, -1);
    cv::TermCriteria criteria = cv::TermCriteria(
        cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 40, 0.001);
    cv::cornerSubPix(gray, corners, winSize, zeroZone, criteria);

  } catch (const std::exception &) {
  }
  return corners[0];
}

void Image::genTmplImage(const cv::Mat &img) {
  int img_center_x = img.cols / 2;
  int img_center_y = img.rows / 2;

  m_tmplImg = img(cv::Range(img_center_y - 100, img_center_y + 100),
                  cv::Range(img_center_x - 100, img_center_x + 100));
}

void Image::featureDet2(const cv::Mat &src_image,
                        std::vector<cv::Point2f> &corners) {
  corners = {};

  cv::Mat image;
  cv::cvtColor(src_image, image, cv::COLOR_BGR2GRAY);

  cv::Mat tmp_gray;
  cv::cvtColor(m_tmplImg, tmp_gray, cv::COLOR_BGR2GRAY);

  cv::Point2f tmp_corner = FindCenter(m_tmplImg);

  float tmpl_width = static_cast<float>(m_tmplImg.cols);
  float tmpl_height = static_cast<float>(m_tmplImg.rows);
  if (tmp_corner.x < tmpl_width / 2.0 - 100 ||
      tmp_corner.x > tmpl_width / 2.0 + 100 ||
      tmp_corner.y < tmpl_height / 2.0 - 100 ||
      tmp_corner.y > tmpl_height / 2.0 + 100) {
    return;
  }

  cv::Mat result;
  cv::matchTemplate(image, tmp_gray, result, cv::TM_SQDIFF_NORMED);

  double minVal, maxVal;
  cv::Point minLoc, maxLoc;
  cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

  cv::Point2f corner;
  corner.x = minLoc.x + tmp_corner.x;
  corner.y = minLoc.y + tmp_corner.y;

  corners.push_back(corner);

  try {
    cv::Size winSize = cv::Size(30, 30);
    cv::Size zeroZone = cv::Size(-1, -1);
    cv::TermCriteria criteria = cv::TermCriteria(
        cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 40, 0.001); // 终止条件
    cv::cornerSubPix(image, corners, winSize, zeroZone, criteria);
  } catch (const std::exception &) {
  }
}

static const cv::Mat tmplImg;
cv::Point2f FindCrossPoint(const cv::Mat &src_image) {
  cv::Mat localTmplImg;

  if (localTmplImg.empty())
    if (!src_image.empty()) {
      int img_center_x = src_image.cols / 2;
      int img_center_y = src_image.rows / 2;
      localTmplImg =
          src_image(cv::Range(img_center_y - 100, img_center_y + 100),
                    cv::Range(img_center_x - 100, img_center_x + 100));
      cv::imwrite("./theImg.bmp", localTmplImg);
    }

  std::vector<cv::Point2f> corners;

  corners = {};

  cv::Mat image;
  cv::cvtColor(src_image, image, cv::COLOR_BGR2GRAY);

  cv::Mat tmp_gray;
  cv::cvtColor(localTmplImg, tmp_gray, cv::COLOR_BGR2GRAY);

  cv::Point2f tmp_corner = FindCenter(localTmplImg);

  float tmpl_width = static_cast<float>(localTmplImg.cols);
  float tmpl_height = static_cast<float>(localTmplImg.rows);
  if (tmp_corner.x < tmpl_width / 2.0 - 100 ||
      tmp_corner.x > tmpl_width / 2.0 + 100 ||
      tmp_corner.y < tmpl_height / 2.0 - 100 ||
      tmp_corner.y > tmpl_height / 2.0 + 100) {
    return cv::Point2f(0, 0);
  }
  cv::Mat result;
  cv::matchTemplate(image, tmp_gray, result, cv::TM_SQDIFF_NORMED);

  double minVal, maxVal;
  cv::Point minLoc, maxLoc;
  cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

  cv::Point2f corner;
  corner.x = minLoc.x + tmp_corner.x;
  corner.y = minLoc.y + tmp_corner.y;

  corners.push_back(corner);

  try {
    cv::Size winSize = cv::Size(30, 30);
    cv::Size zeroZone = cv::Size(-1, -1);
    cv::TermCriteria criteria = cv::TermCriteria(
        cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 40, 0.001);
    cv::cornerSubPix(image, corners, winSize, zeroZone, criteria);
  } catch (const std::exception &) {
  }
  if (corners.size() > 0) {
    cv::Point2f feature_pt;

    std::vector<std::tuple<cv::Point2f, int>> corner_votes;
    for (const auto &pt : corners) {
      bool is_exist = false;
      for (size_t i = 0; i < corner_votes.size(); i++) {
        if (std::get<0>(corner_votes[i]).x == pt.x &&
            std::get<0>(corner_votes[i]).y == pt.y) {
          is_exist = true;
          std::get<1>(corner_votes[i]) = std::get<1>(corner_votes[i]) + 1;
        }
      }

      if (!is_exist) {
        corner_votes.push_back(std::make_tuple(pt, 1));
      }
    }

    int max_vote = 0;
    for (const auto &vote : corner_votes) {
      if (std::get<1>(vote) > max_vote) {
        feature_pt = std::get<0>(vote);
        max_vote = std::get<1>(vote);
      }
    }

    return feature_pt;
  }
  return cv::Point2f(0, 0);
}
