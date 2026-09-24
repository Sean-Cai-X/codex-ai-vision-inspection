#include "ParserClass.h"
#include "muParserDef.h"
#include "muParserTest.h"

#include "shape.h"
#include "imagemanager.h"
#include "shapebase.h"

#include "CircleRingGauge.h"
#include "CxCrashLogHandler.h"
#include "CxScriptDirectBindings.h"
#include "CxScriptTypeTraitsDefs.h"
#include "CxUnifiedLog.h"
#include "CxTextInspect.h"
#include "FastMatch.h"
#include "FastMatchDiagnostic.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "FindObject.h"
#include "FindRect.h"
#include "FindSegmentation.h"
#include "GridPatternClassTool.h"
#include "RegionPatternTool.h"
#include "TorchTask.h"

#include <algorithm>

#include "Run.h"

typedef std::vector<double> vectordouble;
typedef std::vector<int> vectorint;
string getlocationstringx0(const string &strfile) {
  (void)strfile;
  string locationstring;
  return locationstring;
}

class SmartDouble {
  vectordouble m_vectresult;
  vectordouble m_vectdouble;

public:
  SmartDouble() {}
  ~SmartDouble() {}
  void push(double dvalue, double dresult) {
    m_vectdouble.push_back(dvalue);
    m_vectresult.push_back(dresult);
  }
  double getvalue(int inum) { return m_vectdouble[inum]; }
  double getresult(int inum) { return m_vectresult[inum]; }

  void set(double inum, double dvalue, double dresult) {
    m_vectdouble[((int)inum)] = dvalue;
    m_vectresult[((int)inum)] = dresult;
  }
  void clear() {
    m_vectdouble.clear();
    m_vectresult.clear();
  }
  int size() { return static_cast<int>(m_vectdouble.size()); }
  int getvaluetimes(double dvalue) {
    int itimes = 0;
    for (int i = 0; i < static_cast<int>(m_vectdouble.size()); i++) {
      if (m_vectdouble[i] == dvalue)
        itimes = itimes + 1;
    }
    return itimes;
  }
  double average() {
    double daverage = 0;
    for (int i = 0; i < static_cast<int>(m_vectdouble.size()); i++) {
      daverage += m_vectdouble[i];
    }
    return daverage / m_vectdouble.size();
  }
  double maxvalue() {
    double dmax = -11111;
    for (int i = 0; i < static_cast<int>(m_vectdouble.size()); i++) {
      if (dmax < m_vectdouble[i])
        dmax = m_vectdouble[i];
    }
    return dmax;
  }
  int maxnum() {
    double dmax = -11111;
    int i;
    for (i = 0; i < static_cast<int>(m_vectdouble.size()); i++) {
      if (dmax < m_vectdouble[i])
        dmax = m_vectdouble[i];
    }
    return i;
  }
  double minvalue() {
    double dmin = 9999;
    for (int i = 0; i < m_vectdouble.size(); i++) {
      if (dmin < m_vectdouble[i])
        dmin = m_vectdouble[i];
    }
    return dmin;
  }
  void save(const char *pchar) {
    int isize = static_cast<int>(m_vectdouble.size());
    if (isize <= 0)
      return;
    FILE *rf = std::fopen(pchar, "w+");
    if (rf == nullptr)
      return;
    rewind(rf);
    for (int i = 0; i < isize - 1; i++) {
      double dv = m_vectdouble[i];
      double dr = m_vectresult[i];
      fprintf(rf, "%f$%f,", dv, dr);
    }

    fprintf(rf, "%f", m_vectdouble[isize - 1]);
    fclose(rf);
  }
  std::vector<std::string> split(const std::string &str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);

    while (getline(tokenStream, token, delimiter)) {
      tokens.push_back(token);
    }

    return tokens;
  }
  void load(const char *pchar) {

    clear();
    FILE *rf = std::fopen(pchar, "rb");
    if (rf == nullptr)
      return;
    fseek(rf, 0, SEEK_END);
    int filesize = ftell(rf);
    char *pcharget = new char[filesize + 10];
    memset(pcharget, 0, filesize + 10);
    rewind(rf);
    fread((char *)(pcharget), filesize, 1, rf);

    string astr = pcharget;
    vector<string> strnumlist = split(astr, ',');
    for (int i = 0; i < static_cast<int>(strnumlist.size()); i++) {
      vector<string> strdr = split(strnumlist[i], '$');
      double dvalue = std::stod(strdr.at(0));
      double dresult = std::stod(strdr.at(1));
      push(dvalue, dresult);
    }
    delete[] pcharget;
    fclose(rf);
  }
};

class ArgOrderProbe {
public:
  ArgOrderProbe() { reset(); }

  void reset() {
    for (double &value : m_int4)
      value = 0.0;
    for (double &value : m_double4)
      value = 0.0;
    for (double &value : m_any4)
      value = 0.0;
    for (double &value : m_probe_values)
      value = 0.0;
  }

  void setint4(int a, int b, int c, int d) {
    m_int4[0] = static_cast<double>(a);
    m_int4[1] = static_cast<double>(b);
    m_int4[2] = static_cast<double>(c);
    m_int4[3] = static_cast<double>(d);
  }

  void setdouble4(double a, double b, double c, double d) {
    m_double4[0] = a;
    m_double4[1] = b;
    m_double4[2] = c;
    m_double4[3] = d;
  }

  void setany4(vectordouble &values) {
    for (double &value : m_any4)
      value = 0.0;
    const size_t limit = std::min(values.size(), static_cast<size_t>(4));
    for (size_t i = 0; i < limit; ++i)
      m_any4[i] = values[i];
  }

  void setdouble1(double a) { m_probe_values[0] = a; }
  void setdouble2(double a, double b) { m_probe_values[1] = Encode2(a, b); }
  void setdouble3(double a, double b, double c) {
    m_probe_values[2] = Encode3(a, b, c);
  }
  void setdouble4full(double a, double b, double c, double d) {
    m_probe_values[3] = Encode4(a, b, c, d);
  }

  void setint1(int a) { m_probe_values[4] = a; }
  void setint2(int a, int b) { m_probe_values[5] = Encode2(a, b); }
  void setint3(int a, int b, int c) { m_probe_values[6] = Encode3(a, b, c); }
  void setint4full(int a, int b, int c, int d) {
    m_probe_values[7] = Encode4(a, b, c, d);
  }
  void setint5(int a, int b, int c, int d, int e) {
    m_probe_values[8] = Encode5(a, b, c, d, e);
  }
  void setint6(int a, int b, int c, int d, int e, int f) {
    m_probe_values[9] = Encode6(a, b, c, d, e, f);
  }
  void setint7(int a, int b, int c, int d, int e, int f, int g) {
    m_probe_values[10] = Encode7(a, b, c, d, e, f, g);
  }

  void setintdouble2(int a, double b) { m_probe_values[11] = Encode2(a, b); }
  void setdoubleint2(double a, int b) { m_probe_values[12] = Encode2(a, b); }
  void setcharp1(const char *value) { m_probe_values[13] = CharCode(value); }
  void setdoublecharp2(double value, const char *text) {
    m_probe_values[14] = Encode2(value, CharCode(text));
  }
  void setcharpany(std::vector<std::string> &values) {
    m_probe_values[15] = EncodeCharVector(values);
  }
  void setanyfull(vectordouble &values) {
    m_probe_values[16] = EncodeNumericVector(values);
  }
  void setvoidp(void *value) {
    m_probe_values[17] = value != nullptr ? 1.0 : 0.0;
  }

  double returnvoidp(void *value) { return value != nullptr ? 1.0 : 0.0; }
  int returnint1int(int a) { return static_cast<int>(a); }
  int returnint2int(int a, int b) { return static_cast<int>(Encode2(a, b)); }
  double returnint1double(int a) { return a; }
  double returnint2double(int a, int b) { return Encode2(a, b); }
  double returnint3double(int a, int b, int c) { return Encode3(a, b, c); }
  int returnintdouble2int(int a, double b) {
    return static_cast<int>(Encode2(a, b));
  }
  int returndoubleint2int(double a, int b) {
    return static_cast<int>(Encode2(a, b));
  }
  double returnintdouble2double(int a, double b) { return Encode2(a, b); }
  double returndoubleint2double(double a, int b) { return Encode2(a, b); }
  int returncharpanyint(std::vector<std::string> &values) {
    return static_cast<int>(EncodeCharVector(values));
  }
  double returncharpanydouble(std::vector<std::string> &values) {
    return EncodeCharVector(values);
  }
  int returnanyint(vectordouble &values) {
    return static_cast<int>(EncodeNumericVector(values));
  }
  double returnanydouble(vectordouble &values) {
    return EncodeNumericVector(values);
  }
  double returndoublecharp2(double value, const char *text) {
    return Encode2(value, CharCode(text));
  }

  double probevalue(int slot) {
    return slot >= 0 && slot < 18 ? m_probe_values[slot] : -1.0;
  }

  double int4a() { return m_int4[0]; }
  double int4b() { return m_int4[1]; }
  double int4c() { return m_int4[2]; }
  double int4d() { return m_int4[3]; }

  double double4a() { return m_double4[0]; }
  double double4b() { return m_double4[1]; }
  double double4c() { return m_double4[2]; }
  double double4d() { return m_double4[3]; }

  double any4a() { return m_any4[0]; }
  double any4b() { return m_any4[1]; }
  double any4c() { return m_any4[2]; }
  double any4d() { return m_any4[3]; }

private:
  static double Encode2(double a, double b) { return a * 100.0 + b; }
  static double Encode3(double a, double b, double c) {
    return a * 10000.0 + b * 100.0 + c;
  }
  static double Encode4(double a, double b, double c, double d) {
    return a * 1000000.0 + b * 10000.0 + c * 100.0 + d;
  }
  static double Encode5(double a, double b, double c, double d, double e) {
    return a * 100000000.0 + b * 1000000.0 + c * 10000.0 + d * 100.0 + e;
  }
  static double Encode6(double a, double b, double c, double d, double e,
                        double f) {
    return a * 10000000000.0 + b * 100000000.0 + c * 1000000.0 + d * 10000.0 +
           e * 100.0 + f;
  }
  static double Encode7(double a, double b, double c, double d, double e,
                        double f, double g) {
    return a * 1000000000000.0 + b * 10000000000.0 + c * 100000000.0 +
           d * 1000000.0 + e * 10000.0 + f * 100.0 + g;
  }
  static double CharCode(const char *value) {
    return value != nullptr && value[0] != '\0'
               ? static_cast<double>(value[0] - 'A' + 1)
               : 0.0;
  }
  static double EncodeCharVector(std::vector<std::string> &values) {
    double encoded = 0.0;
    for (const std::string &value : values)
      encoded = encoded * 100.0 + CharCode(value.c_str());
    return encoded;
  }
  static double EncodeNumericVector(vectordouble &values) {
    double encoded = 0.0;
    for (double value : values)
      encoded = encoded * 100.0 + value;
    return encoded;
  }

  double m_int4[4];
  double m_double4[4];
  double m_any4[4];
  double m_probe_values[18];
};
class SmartTable {
  map<int, double> m_tabmap0;
  vector<int> m_sort0;

public:
  SmartTable() {}
  ~SmartTable() {}

  void valuescale(double vala, double valb) {
    if (vala < valb)
      for (int iv = static_cast<int>(vala); iv < static_cast<int>(valb); iv++) {
        m_tabmap0[iv] = 0;
      }
  }
  void addvalue(double val) {
    m_tabmap0[static_cast<int>(val)] = m_tabmap0[static_cast<int>(val)] + 1;
  }
  void push(double did, double dvalue1) {
    int id = static_cast<int>(did);
    m_tabmap0[id] = dvalue1;
  }
  double getmap(int inum) { return m_tabmap0[inum]; }

  void sort() {}

  double getsort(int inum) { return m_sort0[inum]; }

  void clear() {
    m_tabmap0.clear();
    m_sort0.clear();
  }

  void print() {}
};

namespace mu {
CxParserRuntime::CxParserRuntime() : m_iVal(0) { g_testcal = 0; }
void CxParserRuntime::ParserInitialClassFunction(int iusing) {
  switch (iusing) {
  case 0: {
    double *apdouble = 0;
    m_parser.DefineOrgClass("double", apdouble);

    RunClass *prun = 0;
    m_parser.DefineClass("TestRun", prun);
    m_parser.DefineClassFun("TestRun", prun, "run", &RunClass::Run);
    m_parser.DefineClassFun("TestRun", prun, "testrun", &RunClass::testrun);

    ImageManager *pmodule = 0;
    m_parser.DefineClass("Module", pmodule);
    m_parser.DefineClassFun("Module", pmodule, "Show", &ImageManager::setshow);
    m_parser.DefineClassFun("Module", pmodule, "setobjectshow",
                            &ImageManager::setobjectshow);

    SmartDouble *psmartd = 0;
    m_parser.DefineClass("SmartDouble", psmartd);
    m_parser.DefineClassFun("SmartDouble", psmartd, "set", &SmartDouble::set);
    m_parser.DefineClassFun("SmartDouble", psmartd, "save", &SmartDouble::save);
    m_parser.DefineClassFun("SmartDouble", psmartd, "load", &SmartDouble::load);
    m_parser.DefineClassFun("SmartDouble", psmartd, "getvalue",
                            &SmartDouble::getvalue);

    ArgOrderProbe *pargorderprobe = 0;
    m_parser.DefineClass("ArgOrderProbe", pargorderprobe);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "reset",
                            &ArgOrderProbe::reset);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint4",
                            &ArgOrderProbe::setint4);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdouble4",
                            &ArgOrderProbe::setdouble4);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setany4",
                            &ArgOrderProbe::setany4);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "int4a",
                            &ArgOrderProbe::int4a);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "int4b",
                            &ArgOrderProbe::int4b);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "int4c",
                            &ArgOrderProbe::int4c);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "int4d",
                            &ArgOrderProbe::int4d);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "double4a",
                            &ArgOrderProbe::double4a);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "double4b",
                            &ArgOrderProbe::double4b);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "double4c",
                            &ArgOrderProbe::double4c);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "double4d",
                            &ArgOrderProbe::double4d);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "any4a",
                            &ArgOrderProbe::any4a);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "any4b",
                            &ArgOrderProbe::any4b);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "any4c",
                            &ArgOrderProbe::any4c);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "any4d",
                            &ArgOrderProbe::any4d);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdouble1",
                            &ArgOrderProbe::setdouble1);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdouble2",
                            &ArgOrderProbe::setdouble2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdouble3",
                            &ArgOrderProbe::setdouble3);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdouble4full",
                            &ArgOrderProbe::setdouble4full);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint1",
                            &ArgOrderProbe::setint1);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint2",
                            &ArgOrderProbe::setint2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint3",
                            &ArgOrderProbe::setint3);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint4full",
                            &ArgOrderProbe::setint4full);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint5",
                            &ArgOrderProbe::setint5);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint6",
                            &ArgOrderProbe::setint6);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setint7",
                            &ArgOrderProbe::setint7);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setintdouble2",
                            &ArgOrderProbe::setintdouble2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdoubleint2",
                            &ArgOrderProbe::setdoubleint2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setcharp1",
                            &ArgOrderProbe::setcharp1);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setdoublecharp2",
                            &ArgOrderProbe::setdoublecharp2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setcharpany",
                            &ArgOrderProbe::setcharpany);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setanyfull",
                            &ArgOrderProbe::setanyfull);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "setvoidp",
                            &ArgOrderProbe::setvoidp);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnvoidp",
                            &ArgOrderProbe::returnvoidp);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnint1int",
                            &ArgOrderProbe::returnint1int);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnint2int",
                            &ArgOrderProbe::returnint2int);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnint1double",
                            &ArgOrderProbe::returnint1double);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnint2double",
                            &ArgOrderProbe::returnint2double);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnint3double",
                            &ArgOrderProbe::returnint3double);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returnintdouble2int",
                            &ArgOrderProbe::returnintdouble2int);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returndoubleint2int",
                            &ArgOrderProbe::returndoubleint2int);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returnintdouble2double",
                            &ArgOrderProbe::returnintdouble2double);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returndoubleint2double",
                            &ArgOrderProbe::returndoubleint2double);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returncharpanyint",
                            &ArgOrderProbe::returncharpanyint);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returncharpanydouble",
                            &ArgOrderProbe::returncharpanydouble);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnanyint",
                            &ArgOrderProbe::returnanyint);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "returnanydouble",
                            &ArgOrderProbe::returnanydouble);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe,
                            "returndoublecharp2",
                            &ArgOrderProbe::returndoublecharp2);
    m_parser.DefineClassFun("ArgOrderProbe", pargorderprobe, "probevalue",
                            &ArgOrderProbe::probevalue);

    SmartTable *psmart = 0;
    m_parser.DefineClass("SmartTable", psmart);
    m_parser.DefineClassFun("SmartTable", psmart, "push", &SmartTable::push);
    m_parser.DefineClassFun("SmartTable", psmart, "getmap",
                            &SmartTable::getmap);
    m_parser.DefineClassFun("SmartTable", psmart, "sort", &SmartTable::sort);
    m_parser.DefineClassFun("SmartTable", psmart, "getsort",
                            &SmartTable::getsort);
    m_parser.DefineClassFun("SmartTable", psmart, "clear", &SmartTable::clear);
    m_parser.DefineClassFun("SmartTable", psmart, "print", &SmartTable::print);
    m_parser.DefineClassFun("SmartTable", psmart, "valuescale",
                            &SmartTable::valuescale);
    m_parser.DefineClassFun("SmartTable", psmart, "addvalue",
                            &SmartTable::addvalue);

    Image *pimage = 0;
    m_parser.DefineClass("Image", pimage);
    m_parser.DefineClassFun("Image", pimage, "blur", &Image::blur);
    m_parser.DefineClassFun("Image", pimage, "load", &Image::load);
    m_parser.DefineClassFun("Image", pimage, "save", &Image::savefile);
    m_parser.DefineClassFun("Image", pimage, "getshow", &Image::getshow);
    m_parser.DefineClassFun("Image", pimage, "Show", &Image::setshow);
    m_parser.DefineClassFun("Image", pimage, "setroi", &Image::setroi);
    m_parser.DefineClassFun("Image", pimage, "Or", &Image::bitwiseOr);
    m_parser.DefineClassFun("Image", pimage, "And", &Image::bitwiseAnd);
    m_parser.DefineClassFun("Image", pimage, "roi_7blur_gap_mud_thre_bw",
                            &Image::roi_7blur_gap_mud_thre_bw);
    m_parser.DefineClassFun("Image", pimage, "roi_7blur_gap_mud_thre_bw_h",
                            &Image::roi_7blur_gap_mud_thre_bw_h);
    m_parser.DefineClassFun("Image", pimage, "pyramidThresholding",
                            &Image::pyramidThresholding);
    m_parser.DefineClassFun("Image", pimage, "colorizeROI",
                            &Image::colorizeROI);
    m_parser.DefineClassFun("Image", pimage, "OrROI", &Image::bitwiseOrROI);
    m_parser.DefineClassFun("Image", pimage, "AndROI", &Image::bitwiseAndROI);
    m_parser.DefineClassFun("Image", pimage, "CopyFrom", &Image::CopyFrom);
    m_parser.DefineClassFun("Image", pimage, "copyFromMat", &Image::CopyFrom);
    m_parser.DefineClassFun("Image", pimage, "erodeROI", &Image::erodeROI);
    m_parser.DefineClassFun("Image", pimage, "erodeVerticalROI",
                            &Image::erodeVerticalROI);
    m_parser.DefineClassFun("Image", pimage, "erodeHorizontalROI",
                            &Image::erodeHorizontalROI);
    m_parser.DefineClassFun("Image", pimage, "roieasythre",
                            &Image::ROIEasyThre);
    m_parser.DefineClassFun("Image", pimage, "roidenoising",
                            &Image::ROIDenoising);
    m_parser.DefineClassFun("Image", pimage, "roidenoisingmulti",
                            &Image::ROIDenoisingMulti);
    m_parser.DefineClassFun("Image", pimage, "pyrdown", &Image::pyrDown);
    m_parser.DefineClassFun("Image", pimage, "roipyrdown", &Image::ROIpyrDown);
    m_parser.DefineClassFun("Image", pimage, "getshape", &Image::getshape);
    m_parser.DefineClassFun("Image", pimage, "loadfiles", &Image::loadfiles);
    m_parser.DefineClassFun("Image", pimage, "reload", &Image::reload);
    m_parser.DefineClassFun("Image", pimage, "rotate", &Image::rotateImage);
    m_parser.DefineClassFun("Image", pimage, "roisobel", &Image::ROISobel);
    m_parser.DefineClassFun("Image", pimage, "roischarr", &Image::ROIScharr);

    m_parser.DefineClassFun("Image", pimage, "roi_5bgmb",
                            &Image::roi_5blur_gap_mud_bw);
    m_parser.DefineClassFun("Image", pimage, "roi_7bgmb",
                            &Image::roi_7blur_gap_mud_bw);
    m_parser.DefineClassFun("Image", pimage, "roi_5bgmbh",
                            &Image::roi_5blur_gap_mud_bw_h);
    m_parser.DefineClassFun("Image", pimage, "roi_7bgmbh",
                            &Image::roi_7blur_gap_mud_bw_h);
    m_parser.DefineClassFun("Image", pimage, "roimean", &Image::roimean);
    m_parser.DefineClassFun("Image", pimage, "roimagnitude",
                            &Image::roimagnitude);

    m_parser.DefineClassFun("Image", pimage, "test", &Image::Test);

    Shape *pshape = nullptr;
    m_parser.DefineClass("Shape", pshape);
    m_parser.DefineClassFun("Shape", pshape, "settype", &Shape::settype);
    m_parser.DefineClassFun("Shape", pshape, "setname", &Shape::setname);
    m_parser.DefineClassFun("Shape", pshape, "setrect", &Shape::setrect);
    m_parser.DefineClassFun("Shape", pshape, "setcolor", &Shape::setcolor);
    m_parser.DefineClassFun("Shape", pshape, "setfont", &Shape::setfont);
    m_parser.DefineClassFun("Shape", pshape, "translate", &Shape::translate);
    m_parser.DefineClassFun("Shape", pshape, "Show", &Shape::setshow);
    m_parser.DefineClassFun("Shape", pshape, "getshape", &Shape::shapesetroi);
    m_parser.DefineClassFun("Shape", pshape, "cutedge", &Shape::cutedge);

    ShapeBase *pshapebase = nullptr;
    m_parser.DefineClass("ShapeBase", pshapebase);
    m_parser.DefineClassFun("ShapeBase", pshapebase, "setshape",
                            &ShapeBase::setShape);
    m_parser.DefineClassFun("ShapeBase", pshapebase, "Show",
                            &ShapeBase::setshow);

    PointsShape *apoints = nullptr;
    m_parser.DefineClass("PointsShape", apoints);
    m_parser.DefineClassFun("PointsShape", apoints, "Show",
                            &PointsShape::setshow);
    m_parser.DefineClassFun("PointsShape", apoints, "setcolor",
                            &PointsShape::setcolor);
    m_parser.DefineClassFun("PointsShape", apoints, "addpoint",
                            &PointsShape::addpoint);
    m_parser.DefineClassFun("PointsShape", apoints, "clear",
                            &PointsShape::clear);
    m_parser.DefineClassFun("PointsShape", apoints, "calibration",
                            &PointsShape::calibration);
    m_parser.DefineClassFun("PointsShape", apoints, "size",
                            &PointsShape::script_size);
    m_parser.DefineClassFun("PointsShape", apoints, "getx",
                            &PointsShape::script_getx);
    m_parser.DefineClassFun("PointsShape", apoints, "gety",
                            &PointsShape::script_gety);
    m_parser.DefineClassFun("PointsShape", apoints, "crosspoint",
                            &PointsShape::crosspoint);
    m_parser.DefineClassFun("PointsShape", apoints, "pointsABadd",
                            &PointsShape::pointsABadd);
    m_parser.DefineClassFun("PointsShape", apoints, "gridpoints",
                            &PointsShape::gridpoints);
    m_parser.DefineClassFun("PointsShape", apoints, "gridrightline",
                            &PointsShape::gridrightline);
    m_parser.DefineClassFun("PointsShape", apoints, "pointslineadd",
                            &PointsShape::pointslineadd);
    m_parser.DefineClassFun("PointsShape", apoints, "circlepoints",
                            &PointsShape::circlepoints);
    m_parser.DefineClassFun("PointsShape", apoints, "halfcircle",
                            &PointsShape::halfcircle);
    m_parser.DefineClassFun("PointsShape", apoints, "ellipsepoints",
                            &PointsShape::ellipsepointsx);
    m_parser.DefineClassFun("PointsShape", apoints, "ratetopoint",
                            &PointsShape::ratetopoint);
    m_parser.DefineClassFun("PointsShape", apoints, "sample",
                            &PointsShape::sample);
    m_parser.DefineClassFun("PointsShape", apoints, "part", &PointsShape::part);
    m_parser.DefineClassFun("PointsShape", apoints, "makeshape",
                            &PointsShape::MakeShape);
    m_parser.DefineClassFun("PointsShape", apoints, "load", &PointsShape::load);
    m_parser.DefineClassFun("PointsShape", apoints, "save", &PointsShape::save);

    m_parser.DefineClassFun("PointsShape", apoints, "makeshape",
                            &PointsShape::MakeShape);

    m_parser.DefineClassFun("PointsShape", apoints, "clusterA",
                            &PointsShape::ClusterPointCloud);

    m_parser.DefineClassFun("PointsShape", apoints, "aptfilter",
                            &PointsShape::AdaptiveDistfilter);
    m_parser.DefineClassFun("PointsShape", apoints, "obbanglecenter",
                            &PointsShape::OBBCenterAngleSort);
    m_parser.DefineClassFun("PointsShape", apoints, "sortpoints",
                            &PointsShape::SortPoints);

    m_parser.DefineClassFun("PointsShape", apoints, "cluster",
                            &PointsShape::ClusterPoints);
    m_parser.DefineClassFun("PointsShape", apoints, "filter",
                            &PointsShape::FilterPoints);

    m_parser.DefineClassFun("PointsShape", apoints, "findcross",
                            &PointsShape::FindCrossPoints);

    LineShape *plineshape = nullptr;
    m_parser.DefineClass("LineShape", plineshape);
    m_parser.DefineClassFun("LineShape", plineshape, "setline",
                            static_cast<void (LineShape::*)(
                                double, double, double, double)>(
                                &LineShape::setline));
    m_parser.DefineClassFun("LineShape", plineshape, "Show",
                            &LineShape::setshow);
    m_parser.DefineClassFun("LineShape", plineshape, "move", &LineShape::Move);
    m_parser.DefineClassFun("LineShape", plineshape, "rotate",
                            &LineShape::Rotate);
    m_parser.DefineClassFun("LineShape", plineshape, "zoom", &LineShape::Zoom);
    m_parser.DefineClassFun("LineShape", plineshape, "setpenw",
                            &LineShape::setpenw);
    m_parser.DefineClassFun("LineShape", plineshape, "lineaex",
                            &LineShape::lineaex);
    m_parser.DefineClassFun("LineShape", plineshape, "linebex",
                            &LineShape::linebex);
    m_parser.DefineClassFun("LineShape", plineshape, "linecv",
                            &LineShape::linecv);

    FindCircle *pfindcircle = nullptr;
    const std::string_view findcircle_type_name =
        CxScriptTypeName(CxScriptTypeTraits<FindCircle>::id);
    m_parser.DefineClass(findcircle_type_name.data(), pfindcircle);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setcircle2", &FindCircle::cxscript_setcircle2);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setcircle", &FindCircle::cxscript_setcircle);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setannulus", &FindCircle::cxscript_setannulus);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setscanarc", &FindCircle::cxscript_setscanarc);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle, "setgap",
                            &FindCircle::Setgap);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle, "Setgap",
                            &FindCircle::Setgap);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle, "Show",
                            &FindCircle::setshow);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle, "measure",
                            &FindCircle::measure);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "measureRobust", &FindCircle::measureRobust);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setlinegap", &FindCircle::setlinegap);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setminedgerunwidth",
                            &FindCircle::setminedgerunwidth);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryresponseenabled",
                            &FindCircle::setboundaryresponseenabled);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryresponsemode",
                            &FindCircle::setboundaryresponsemode);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarypolarity",
                            &FindCircle::setboundarypolarity);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryselection",
                            &FindCircle::setboundaryselection);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarynthcandidate",
                            &FindCircle::setboundarynthcandidate);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryreferencepositionpermille",
                            &FindCircle::setboundaryreferencepositionpermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarysubpixel",
                            &FindCircle::setboundarysubpixel);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarybaseline",
                            &FindCircle::setboundarybaseline);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarydenoise",
                            &FindCircle::setboundarydenoise);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarysmoothingradius",
                            &FindCircle::setboundarysmoothingradius);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarybaselinewindow",
                            &FindCircle::setboundarybaselinewindow);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarywaveletscale",
                            &FindCircle::setboundarywaveletscale);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarythresholdpermille",
                            &FindCircle::setboundarythresholdpermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarylevelpermille",
                            &FindCircle::setboundarylevelpermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryhysteresispermille",
                            &FindCircle::setboundaryhysteresispermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarygatestartpermille",
                            &FindCircle::setboundarygatestartpermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarygateendpermille",
                            &FindCircle::setboundarygateendpermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryminplateauwidth",
                            &FindCircle::setboundaryminplateauwidth);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundaryminamplitudepermille",
                            &FindCircle::setboundaryminamplitudepermille);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarypairminwidth",
                            &FindCircle::setboundarypairminwidth);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarypairmaxwidth",
                            &FindCircle::setboundarypairmaxwidth);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setboundarypairanchor",
                            &FindCircle::setboundarypairanchor);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setmethod", &FindCircle::setmethod);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle, "setthre",
                            &FindCircle::setthre);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getshape", &FindCircle::getshape);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setcirclegap", &FindCircle::setcirclegap);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "fitcircle", &FindCircle::fitcircle);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "fitcirclefiltered",
                            &FindCircle::fitcirclefiltered);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setpointconsistency",
                            &FindCircle::setpointconsistency);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getpointconsistencyenabled",
                            &FindCircle::getpointconsistencyenabled);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getpointconsistencyrange",
                            &FindCircle::getpointconsistencyrange);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getpointconsistencyinputcount",
                            &FindCircle::getpointconsistencyinputcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getpointconsistencyoutputcount",
                            &FindCircle::getpointconsistencyoutputcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getpointconsistencyremovedcount",
                            &FindCircle::getpointconsistencyremovedcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getfitfilterinputcount",
                            &FindCircle::getfitfilterinputcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getfitfilterkeptcount",
                            &FindCircle::getfitfilterkeptcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getfitfilterrejectedcount",
                            &FindCircle::getfitfilterrejectedcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getfitfiltersigma",
                            &FindCircle::getfitfiltersigma);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getfitfilterthreshold",
                            &FindCircle::getfitfilterthreshold);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getavgdist", &FindCircle::getavgdist);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getresultcentx", &FindCircle::getresultcentx);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getresultcenty", &FindCircle::getresultcenty);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getradius", &FindCircle::getradius);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getannulusinner", &FindCircle::getannulusinner);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getannulusouter", &FindCircle::getannulusouter);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getannuluswidth", &FindCircle::getannuluswidth);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "hasannulus", &FindCircle::hasannulus);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getscanarcstart", &FindCircle::getscanarcstart);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getscanarcend", &FindCircle::getscanarcend);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "hasscanarc", &FindCircle::hasscanarc);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getavgdist", &FindCircle::getavgdist);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "getvalidpointcount",
                            &FindCircle::getvalidpointcount);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "hasfitresult", &FindCircle::hasfitresult_script);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setfitmeasuregap", &FindCircle::setfitmeasuregap);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "fitmeasure", &FindCircle::FitResultMeasure);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "FitResultMeasure", &FindCircle::FitResultMeasure);

    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setmaxelapsedms", &FindCircle::setmaxelapsedms);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setmaxscanlines", &FindCircle::setmaxscanlines);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setmaxsamples", &FindCircle::setmaxsamples);
    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "get_result", &FindCircle::get_result_script);

    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setlinesamplerate",
                            static_cast<void (FindCircle::*)(double)>(
                                &FindCircle::setlinesamplerate));

    m_parser.DefineClassFun(
        findcircle_type_name.data(), pfindcircle, "setgamarate",
        static_cast<void (FindCircle::*)(int)>(&FindCircle::setgamarate));

    m_parser.DefineClassFun(
        findcircle_type_name.data(), pfindcircle, "setfindsetting",
        static_cast<void (FindCircle::*)(int)>(&FindCircle::setfindsetting));

    m_parser.DefineClassFun(findcircle_type_name.data(), pfindcircle,
                            "setfilter",
                            static_cast<void (FindCircle::*)(int, int, int)>(
                                &FindCircle::setfilter_script));

    m_parser.DefineClassFun(
        findcircle_type_name.data(), pfindcircle, "setselectedgenum",
        static_cast<void (FindCircle::*)(int)>(&FindCircle::setselectedgenum));

    CircleRingGauge *pcircle_ring_gauge = nullptr;
    m_parser.DefineClass("CircleRingGauge", pcircle_ring_gauge);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "settolerance", &CircleRingGauge::settolerance);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "setouter",
                            &CircleRingGauge::setouter);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "setinner",
                            &CircleRingGauge::setinner);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "build",
                            &CircleRingGauge::build);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "has_result",
                            &CircleRingGauge::has_result);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "outer_radius", &CircleRingGauge::outer_radius);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "inner_radius", &CircleRingGauge::inner_radius);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "thickness",
                            &CircleRingGauge::thickness);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "center_distance",
                            &CircleRingGauge::center_distance);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "concentric_ok", &CircleRingGauge::concentric_ok);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "inside_ok",
                            &CircleRingGauge::inside_ok);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "thickness_ok", &CircleRingGauge::thickness_ok);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge, "score",
                            &CircleRingGauge::score);
    m_parser.DefineClassFun("CircleRingGauge", pcircle_ring_gauge,
                            "status_code", &CircleRingGauge::status_code);

    FastMatchDiagnostic *fastmatch_diagnostic = nullptr;
    m_parser.DefineClass("FastMatchDiagnostic", fastmatch_diagnostic);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "setpolicy", &FastMatchDiagnostic::setpolicy);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "setsource", &FastMatchDiagnostic::setsource);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "setlevel", &FastMatchDiagnostic::setlevel);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "setprofile", &FastMatchDiagnostic::setprofile);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "set_l1_l3_coverage_ok",
                            &FastMatchDiagnostic::set_l1_l3_coverage_ok);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "set_parameter_policy_valid",
                            &FastMatchDiagnostic::set_parameter_policy_valid);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "set_product_default_changed",
                            &FastMatchDiagnostic::set_product_default_changed);
    m_parser.DefineClassFun(
        "FastMatchDiagnostic", fastmatch_diagnostic,
        "set_original_measure_available",
        &FastMatchDiagnostic::set_original_measure_available);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "set_local_evidence_confirmed",
                            &FastMatchDiagnostic::set_local_evidence_confirmed);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic,
                            "set_component_warning",
                            &FastMatchDiagnostic::set_component_warning);
    m_parser.DefineClassFun("FastMatchDiagnostic", fastmatch_diagnostic, "run",
                            &FastMatchDiagnostic::run);

    FindEllipse *pfindellipse = nullptr;
    m_parser.DefineClass("FindEllipse", pfindellipse);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setellipse2",
                            &FindEllipse::setellipse2);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setellipse",
                            &FindEllipse::setellipse);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setbboxx0",
                            &FindEllipse::setbboxx0);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setbboxy0",
                            &FindEllipse::setbboxy0);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setbboxx1",
                            &FindEllipse::setbboxx1);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setbboxy1",
                            &FindEllipse::setbboxy1);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "buildbbox",
                            &FindEllipse::buildbbox);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setinnerpercent",
                            &FindEllipse::setinnerpercent);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getinnerpercent",
                            &FindEllipse::getinnerpercent);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setgap",
                            &FindEllipse::Setgap);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "Show",
                            &FindEllipse::setshow);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setlinegap",
                            &FindEllipse::setlinegap);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setminedgerunwidth",
                            &FindEllipse::setminedgerunwidth);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryresponseenabled",
                            &FindEllipse::setboundaryresponseenabled);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryresponsemode",
                            &FindEllipse::setboundaryresponsemode);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarypolarity",
                            &FindEllipse::setboundarypolarity);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryselection",
                            &FindEllipse::setboundaryselection);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarynthcandidate",
                            &FindEllipse::setboundarynthcandidate);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryreferencepositionpermille",
                            &FindEllipse::setboundaryreferencepositionpermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarysubpixel",
                            &FindEllipse::setboundarysubpixel);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarybaseline",
                            &FindEllipse::setboundarybaseline);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarydenoise",
                            &FindEllipse::setboundarydenoise);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarysmoothingradius",
                            &FindEllipse::setboundarysmoothingradius);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarybaselinewindow",
                            &FindEllipse::setboundarybaselinewindow);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarywaveletscale",
                            &FindEllipse::setboundarywaveletscale);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarythresholdpermille",
                            &FindEllipse::setboundarythresholdpermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarylevelpermille",
                            &FindEllipse::setboundarylevelpermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryhysteresispermille",
                            &FindEllipse::setboundaryhysteresispermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarygatestartpermille",
                            &FindEllipse::setboundarygatestartpermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarygateendpermille",
                            &FindEllipse::setboundarygateendpermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryminplateauwidth",
                            &FindEllipse::setboundaryminplateauwidth);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundaryminamplitudepermille",
                            &FindEllipse::setboundaryminamplitudepermille);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarypairminwidth",
                            &FindEllipse::setboundarypairminwidth);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarypairmaxwidth",
                            &FindEllipse::setboundarypairmaxwidth);
    m_parser.DefineClassFun("FindEllipse", pfindellipse,
                            "setboundarypairanchor",
                            &FindEllipse::setboundarypairanchor);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setmethod",
                            &FindEllipse::setmethod);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setfindsetting",
                            &FindEllipse::setfindsetting);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setselectedgenum",
                            &FindEllipse::setselectedgenum);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setpointconsistency",
                            &FindEllipse::setpointconsistency);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setthre",
                            &FindEllipse::setthre);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "measure",
                            &FindEllipse::measure);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "measureRobust",
                            &FindEllipse::measureRobust);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "fitellipse",
                            &FindEllipse::fitellipse);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getresultcentx",
                            &FindEllipse::getresultcentx);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getresultcenty",
                            &FindEllipse::getresultcenty);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getresultradiusx",
                            &FindEllipse::getresultradiusx);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getresultradiusy",
                            &FindEllipse::getresultradiusy);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getresultangle",
                            &FindEllipse::getresultangle);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getavgdist",
                            &FindEllipse::getavgdist);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "getvalidpointcount",
                            &FindEllipse::getvalidpointcount);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "hasfitresult",
                            &FindEllipse::hasfitresult);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "get_result",
                            &FindEllipse::get_result);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "clear",
                            &FindEllipse::clear);
    m_parser.DefineClassFun("FindEllipse", pfindellipse, "setlinesample",
                            &FindEllipse::setlinesamplerate);

    FindRect *pfindrect = nullptr;
    m_parser.DefineClass("FindRect", pfindrect);
    m_parser.DefineClassFun("FindRect", pfindrect, "setrect",
                            &FindRect::setrect);
    m_parser.DefineClassFun("FindRect", pfindrect, "setthre",
                            &FindRect::setthre);
    m_parser.DefineClassFun("FindRect", pfindrect, "setcompgap",
                            &FindRect::setcomparegap);
    m_parser.DefineClassFun("FindRect", pfindrect, "setlinegap",
                            &FindRect::setlinegap);
    m_parser.DefineClassFun("FindRect", pfindrect, "setmethod",
                            &FindRect::setmethod);
    m_parser.DefineClassFun("FindRect", pfindrect, "setgauge",
                            &FindRect::setgauge);
    m_parser.DefineClassFun("FindRect", pfindrect, "setfindsetting",
                            &FindRect::setfindsetting);
    m_parser.DefineClassFun("FindRect", pfindrect, "setfilter",
                            &FindRect::setfilter);
    m_parser.DefineClassFun("FindRect", pfindrect, "setminmaxarea",
                            &FindRect::setminmaxarea);
    m_parser.DefineClassFun("FindRect", pfindrect, "setminmaxwh",
                            &FindRect::setminmaxwh);
    m_parser.DefineClassFun("FindRect", pfindrect, "setpolygonepsilon",
                            &FindRect::setpolygonepsilon);
    m_parser.DefineClassFun("FindRect", pfindrect, "setfillratio",
                            &FindRect::setfillratio);
    m_parser.DefineClassFun("FindRect", pfindrect, "measure",
                            &FindRect::measure);
    m_parser.DefineClassFun("FindRect", pfindrect, "clear", &FindRect::clear);

    FindLine *pfindline = nullptr;
    const std::string_view findline_type_name =
        CxScriptTypeName(CxScriptTypeTraits<FindLine>::id);
    m_parser.DefineClass(findline_type_name.data(), pfindline);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setrect",
                            &FindLine::setrect);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setline",
                            &FindLine::setline_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "fitline",
                            &FindLine::FitLine);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setfitmode",
                            &FindLine::setfitmode);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setfitpointweight",
                            static_cast<void (FindLine::*)(int, double)>(
                                &FindLine::setfitpointweight));
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "translate",
                            &FindLine::translate);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "Show",
                            &FindLine::setshow);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "clear",
                            &FindLine::clear);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setwhgap",
                            &FindLine::setwhgap_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "SetWHgap",
                            &FindLine::setwhgap_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setscandirection", &FindLine::setscandirection);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "measure",
                            &FindLine::measure);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "measureRobust", &FindLine::measureRobust);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setlinesample", &FindLine::setlinesamplerate);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setlinegap",
                            &FindLine::setlinegap);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setminedgerunwidth",
                            &FindLine::setminedgerunwidth);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryresponseenabled",
                            &FindLine::setboundaryresponseenabled);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryresponsemode",
                            &FindLine::setboundaryresponsemode);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarypolarity",
                            &FindLine::setboundarypolarity);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryselection",
                            &FindLine::setboundaryselection);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarynthcandidate",
                            &FindLine::setboundarynthcandidate);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryreferencepositionpermille",
                            &FindLine::setboundaryreferencepositionpermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarysubpixel",
                            &FindLine::setboundarysubpixel);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarybaseline",
                            &FindLine::setboundarybaseline);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarydenoise",
                            &FindLine::setboundarydenoise);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarysmoothingradius",
                            &FindLine::setboundarysmoothingradius);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarybaselinewindow",
                            &FindLine::setboundarybaselinewindow);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarywaveletscale",
                            &FindLine::setboundarywaveletscale);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarythresholdpermille",
                            &FindLine::setboundarythresholdpermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarylevelpermille",
                            &FindLine::setboundarylevelpermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryhysteresispermille",
                            &FindLine::setboundaryhysteresispermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarygatestartpermille",
                            &FindLine::setboundarygatestartpermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarygateendpermille",
                            &FindLine::setboundarygateendpermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryminplateauwidth",
                            &FindLine::setboundaryminplateauwidth);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundaryminamplitudepermille",
                            &FindLine::setboundaryminamplitudepermille);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarypairminwidth",
                            &FindLine::setboundarypairminwidth);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarypairmaxwidth",
                            &FindLine::setboundarypairmaxwidth);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setboundarypairanchor",
                            &FindLine::setboundarypairanchor);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setmethod",
                            &FindLine::setmethod);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setthre",
                            &FindLine::setthre);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setgama",
                            &FindLine::setgamarate);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setobjfilter", &FindLine::setobjfilter);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setfilter",
                            &FindLine::setfilter);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setobjectfilterstrategy",
                            &FindLine::setobjectfilterstrategy);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setfindobjectstrategy",
                            &FindLine::setobjectfilterstrategy);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setpointconsistency",
                            &FindLine::setpointconsistency);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "findpattern",
                            &FindLine::findpattern);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "setcompgap",
                            &FindLine::setcomparegap);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "shapesetroi",
                            &FindLine::shapesetroi);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setselectedgenum", &FindLine::setselectedgenum);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "patternfilter", &FindLine::patternfilter);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "getshape",
                            &FindLine::getshape);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "sfilter",
                            &FindLine::SmartFilter);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "inflectionpoint", &FindLine::InflectionPoint);
    m_parser.DefineClassFun(
        findline_type_name.data(), pfindline, "setmeasurefallback",
        static_cast<void (FindLine::*)(int)>(&FindLine::setmeasurefallback));

    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setmaxelapsedms", &FindLine::setmaxelapsedms);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setmaxscanlines", &FindLine::setmaxscanlines);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setmaxsamples", &FindLine::setmaxsamples);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "setfilterprofile", &FindLine::setfilterprofile);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "get_result",
                            &FindLine::get_result_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "getvalidpointcount",
                            &FindLine::getvalidpointcount_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline,
                            "hasfitresult", &FindLine::hasfitresult_script);
    m_parser.DefineClassFun(findline_type_name.data(), pfindline, "getavgdist",
                            &FindLine::getavgdist_script);

    FindObject *pfobj = nullptr;
    m_parser.DefineClass("FindObject", pfobj);
    m_parser.DefineClassFun("FindObject", pfobj, "setrect",
                            &FindObject::setrect_script);
    m_parser.DefineClassFun("FindObject", pfobj, "measure",
                            &FindObject::measure);
    m_parser.DefineClassFun("FindObject", pfobj, "measurefast",
                            &FindObject::measurefast);
    m_parser.DefineClassFun("FindObject", pfobj, "measurecc",
                            &FindObject::measurecc);
    m_parser.DefineClassFun("FindObject", pfobj, "measurexbfs",
                            &FindObject::measurexbfs);
    m_parser.DefineClassFun("FindObject", pfobj, "measurepeakcomponents",
                            &FindObject::measurexbfs);
    m_parser.DefineClassFun("FindObject", pfobj, "Show", &FindObject::setshow);
    m_parser.DefineClassFun("FindObject", pfobj, "measurex",
                            &FindObject::measurex);
    m_parser.DefineClassFun("FindObject", pfobj, "measurexfast",
                            &FindObject::measurexfast);
    m_parser.DefineClassFun("FindObject", pfobj, "measurexcc",
                            &FindObject::measurexcc);
    m_parser.DefineClassFun("FindObject", pfobj, "measurexpeakbfs",
                            &FindObject::measurexpeakbfs);
    m_parser.DefineClassFun("FindObject", pfobj, "sethsogap",
                            &FindObject::sethsogap_script);
    m_parser.DefineClassFun("FindObject", pfobj, "setminmax",
                            &FindObject::setminmaxarea_script);
    m_parser.DefineClassFun("FindObject", pfobj, "setminmaxwh",
                            &FindObject::setminmaxwh_script);
    m_parser.DefineClassFun("FindObject", pfobj, "setbrow",
                            &FindObject::setbrow);
    m_parser.DefineClassFun("FindObject", pfobj, "setdistance",
                            &FindObject::setdistance);
    m_parser.DefineClassFun("FindObject", pfobj, "setsearchtype",
                            &FindObject::setsearchtype);
    m_parser.DefineClassFun("FindObject", pfobj, "edgeimage",
                            &FindObject::edgeimage);
    m_parser.DefineClassFun("FindObject", pfobj, "setedgeoi",
                            &FindObject::setedgeoi);
    m_parser.DefineClassFun("FindObject", pfobj, "setfilteredge",
                            &FindObject::setfilteredge);
    m_parser.DefineClassFun("FindObject", pfobj, "setoffset",
                            &FindObject::setoffset);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultcentx",
                            &FindObject::getresultcentx);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultcenty",
                            &FindObject::getresultcenty);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultx",
                            &FindObject::getresultx);
    m_parser.DefineClassFun("FindObject", pfobj, "getresulty",
                            &FindObject::getresulty);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultw",
                            &FindObject::getresultw);
    m_parser.DefineClassFun("FindObject", pfobj, "getresulth",
                            &FindObject::getresulth);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultsize",
                            &FindObject::getresultsize);
    m_parser.DefineClassFun("FindObject", pfobj, "getresultobjsnum",
                            &FindObject::getresultobjsnum);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugcomponentcount",
                            &FindObject::getdebugcomponentcount);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugacceptedcount",
                            &FindObject::getdebugacceptedcount);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugrejectedcount",
                            &FindObject::getdebugrejectedcount);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugmaxcomponentarea",
                            &FindObject::getdebugmaxcomponentarea);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugmaxcomponentw",
                            &FindObject::getdebugmaxcomponentw);
    m_parser.DefineClassFun("FindObject", pfobj, "getdebugmaxcomponenth",
                            &FindObject::getdebugmaxcomponenth);

    m_parser.DefineClassFun("FindObject", pfobj, "setgeometrycalibration",
                            &FindObject::setgeometrycalibration);
    m_parser.DefineClassFun("FindObject", pfobj, "setgeometryconnectivity",
                            &FindObject::setgeometryconnectivity);
    m_parser.DefineClassFun("FindObject", pfobj, "setmeasurementselection",
                            &FindObject::setmeasurementselection);
    m_parser.DefineClassFun("FindObject", pfobj, "setshowboundary",
                            &FindObject::setshowboundary);
    m_parser.DefineClassFun("FindObject", pfobj, "setshowmomentellipse",
                            &FindObject::setshowmomentellipse);
    m_parser.DefineClassFun("FindObject", pfobj, "setshowferet",
                            &FindObject::setshowferet);
    m_parser.DefineClassFun("FindObject", pfobj, "setshowgeometrycircles",
                            &FindObject::setshowgeometrycircles);
    m_parser.DefineClassFun("FindObject", pfobj, "setconclusionshape",
                            &FindObject::setconclusionshape);
    m_parser.DefineClassFun("FindObject", pfobj, "setbackgroundmethod",
                            &FindObject::setbackgroundmethod);
    m_parser.DefineClassFun("FindObject", pfobj, "setbackgroundborderwidth",
                            &FindObject::setbackgroundborderwidth);
    m_parser.DefineClassFun("FindObject", pfobj, "setbackgroundmorphologyradius",
                            &FindObject::setbackgroundmorphologyradius);
    m_parser.DefineClassFun("FindObject", pfobj, "setsubpixelenabled",
                            &FindObject::setsubpixelenabled);
    m_parser.DefineClassFun("FindObject", pfobj, "setsubpixelminimumgradient",
                            &FindObject::setsubpixelminimumgradient);
    m_parser.DefineClassFun("FindObject", pfobj, "setsubpixelisothreshold",
                            &FindObject::setsubpixelisothreshold);
    m_parser.DefineClassFun("FindObject", pfobj, "setactivegeometrybasis",
                            &FindObject::setactivegeometrybasis);
    m_parser.DefineClassFun("FindObject", pfobj, "getmeasurementcount",
                            &FindObject::getmeasurementcount);
    m_parser.DefineClassFun("FindObject", pfobj, "getarea",
                            &FindObject::getarea);
    m_parser.DefineClassFun("FindObject", pfobj, "getperimeter",
                            &FindObject::getperimeter);
    m_parser.DefineClassFun("FindObject", pfobj, "getequivalentside",
                            &FindObject::getequivalentside);
    m_parser.DefineClassFun("FindObject", pfobj, "getcircularity",
                            &FindObject::getcircularity);
    m_parser.DefineClassFun("FindObject", pfobj, "getsolidity",
                            &FindObject::getsolidity);
    m_parser.DefineClassFun("FindObject", pfobj, "getmajoraxis",
                            &FindObject::getmajoraxis);
    m_parser.DefineClassFun("FindObject", pfobj, "getminoraxis",
                            &FindObject::getminoraxis);
    m_parser.DefineClassFun("FindObject", pfobj, "getorientation",
                            &FindObject::getorientation);
    m_parser.DefineClassFun("FindObject", pfobj, "getferetmax",
                            &FindObject::getferetmax);
    m_parser.DefineClassFun("FindObject", pfobj, "getferetmin",
                            &FindObject::getferetmin);
    m_parser.DefineClassFun("FindObject", pfobj, "getinscribedradius",
                            &FindObject::getinscribedradius);
    m_parser.DefineClassFun("FindObject", pfobj, "getenclosingradius",
                            &FindObject::getenclosingradius);
    m_parser.DefineClassFun("FindObject", pfobj, "objectgrid",
                            &FindObject::objectgrid);
    m_parser.DefineClassFun("FindObject", pfobj, "setobjectgrid",
                            &FindObject::setobjectgrid);
    m_parser.DefineClassFun("FindObject", pfobj, "objectsort",
                            &FindObject::objectsort);

    FindSegmentation *pfindsegmentation = nullptr;
    m_parser.DefineClass("FindSegmentation", pfindsegmentation);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setbackend",
                            &FindSegmentation::setbackend);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setmodel",
                            &FindSegmentation::setmodel);

    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "settask",
                            &FindSegmentation::settask);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setmodelid",
                            &FindSegmentation::setmodelid);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setmodelpackage",
                            &FindSegmentation::setmodelpackage);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setmanifest",
                            &FindSegmentation::setmanifest);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setoutputroot", &FindSegmentation::setoutputroot);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setpostprocessprofile",
                            &FindSegmentation::setpostprocessprofile);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setparameterprofile",
                            &FindSegmentation::setparameterprofile);

    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setdevice",
                            &FindSegmentation::setdevice);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setthreshold", &FindSegmentation::setthreshold);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setpromptrect", &FindSegmentation::setpromptrect);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setpromptrectxyxy",
                            &FindSegmentation::setpromptrectxyxy);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setpoint",
                            &FindSegmentation::setpoint);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setpositivepoint",
                            &FindSegmentation::setpositivepoint);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setpositivepointxy",
                            &FindSegmentation::setpositivepointxy);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setnegativepoint",
                            &FindSegmentation::setnegativepoint);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setnegativepointxy",
                            &FindSegmentation::setnegativepointxy);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "setmode",
                            &FindSegmentation::setmode);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "setgeometrytype",
                            &FindSegmentation::setgeometrytype);

    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation, "segment",
                            &FindSegmentation::segment);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "extractboundary",
                            &FindSegmentation::extractboundary);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "buildoverlay", &FindSegmentation::buildoverlay);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "status_code", &FindSegmentation::status_code);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_contour_count",
                            &FindSegmentation::get_contour_count);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_primary_area",
                            &FindSegmentation::get_primary_area);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_count",
                            &FindSegmentation::get_geometry_count);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_residual",
                            &FindSegmentation::get_geometry_residual);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_support",
                            &FindSegmentation::get_geometry_support);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_center_x",
                            &FindSegmentation::get_geometry_center_x);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_center_y",
                            &FindSegmentation::get_geometry_center_y);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_radius",
                            &FindSegmentation::get_geometry_radius);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_axis_x",
                            &FindSegmentation::get_geometry_axis_x);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_axis_y",
                            &FindSegmentation::get_geometry_axis_y);
    m_parser.DefineClassFun("FindSegmentation", pfindsegmentation,
                            "get_geometry_angle_deg",
                            &FindSegmentation::get_geometry_angle_deg);


    TorchTask *ptorch_task = nullptr;
    m_parser.DefineClass("TorchTask", ptorch_task);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "settask",
                            &TorchTask::settask);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setcase",
                            &TorchTask::setcase);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setmodel",
                            &TorchTask::setmodel);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setmanifest",
                            &TorchTask::setmanifest);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setdatasetroot",
                            &TorchTask::setdatasetroot);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setdevice",
                            &TorchTask::setdevice);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setinputpath",
                            &TorchTask::setinputpath);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "settemplatepath",
                            &TorchTask::settemplatepath);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setoutputdir",
                            &TorchTask::setoutputdir);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "setrequestcontext",
                            &TorchTask::setrequestcontext);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "settimeout",
                            &TorchTask::settimeout);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "run", &TorchTask::run);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getok",
                            &TorchTask::getok);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "geterrorcode",
                            &TorchTask::geterrorcode);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getresultcount",
                            &TorchTask::getresultcount);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getmaskavailable",
                            &TorchTask::getmaskavailable);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getinferms",
                            &TorchTask::getinferms);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "gettrainms",
                            &TorchTask::gettrainms);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "gettotalms",
                            &TorchTask::gettotalms);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getstatus",
                            &TorchTask::getstatus);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getreason",
                            &TorchTask::getreason);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getactualdevice",
                            &TorchTask::getactualdevice);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getresultref",
                            &TorchTask::getresultref);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getevidenceref",
                            &TorchTask::getevidenceref);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getprimaryvisualref",
                            &TorchTask::getprimaryvisualref);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getmaskref",
                            &TorchTask::getmaskref);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getoverlayref",
                            &TorchTask::getoverlayref);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "gettrainersummary",
                            &TorchTask::gettrainersummary);
    m_parser.DefineClassFun("TorchTask", ptorch_task, "getmainlinesummary",
                            &TorchTask::getmainlinesummary);

    m_parser.DefineClassFun("FindObject", pfobj, "edge", &FindObject::Edge);
    m_parser.DefineClassFun("FindObject", pfobj, "setrelresultnum",
                            &FindObject::setrelationrectfromresultnum);
    m_parser.DefineClassFun("FindObject", pfobj, "setrelmatch",
                            &FindObject::setrelationrectfrom_matchresult);
    m_parser.DefineClassFun("FindObject", pfobj, "setrelxy",
                            &FindObject::setrelationxy);
    m_parser.DefineClassFun("FindObject", pfobj, "setrelzoom",
                            &FindObject::setrelationzoom);
    m_parser.DefineClassFun("FindObject", pfobj, "setreltorect",
                            &FindObject::setrelationtorect);
    m_parser.DefineClassFun("FindObject", pfobj, "setcolor",
                            &FindObject::setcolorstyle);
    m_parser.DefineClassFun("FindObject", pfobj, "setroithre",
                            &FindObject::SetImageROIthre);
    m_parser.DefineClassFun("FindObject", pfobj, "setroiincrease",
                            &FindObject::SetImageROIincrease);
    m_parser.DefineClassFun("FindObject", pfobj, "setroicomparegap",
                            &FindObject::SetImageROIcomparegap);
    m_parser.DefineClassFun("FindObject", pfobj, "setroifindborw",
                            &FindObject::SetImageROIfindBorW);
    m_parser.DefineClassFun("FindObject", pfobj, "setroiedge5o7",
                            &FindObject::SetImageROIedge_5o7);
    m_parser.DefineClassFun("FindObject", pfobj, "roithre",
                            &FindObject::ImageROIthre);
    m_parser.DefineClassFun("FindObject", pfobj, "roiedge",
                            &FindObject::ImageROIedge);
    m_parser.DefineClassFun("FindObject", pfobj, "roiedgeh",
                            &FindObject::ImageROIedgeH);
    m_parser.DefineClassFun("FindObject", pfobj, "shapesetroi",
                            &FindObject::shapesetroi);
    m_parser.DefineClassFun("FindObject", pfobj, "getshape",
                            &FindObject::getshape);

    FastMatch *pfastmatch = nullptr;
    const std::string_view fastmatch_type_name =
        CxScriptTypeName(CxScriptTypeTraits<FastMatch>::id);
    m_parser.DefineClass(fastmatch_type_name.data(), pfastmatch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setrect",
                            &FastMatch::setrectxywh_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrectxywh", &FastMatch::setrectxywh_script);

    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setgeometrysourceindex",
                            &FastMatch::setgeometrysourceindex);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setgeometryweightpercent",
                            &FastMatch::setgeometryweightpercent);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmaxposecandidates",
                            &FastMatch::setmaxposecandidates);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settemplategeometryfromobject",
                            &FastMatch::settemplategeometryfromobject);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "cleargeometrycandidates",
                            &FastMatch::cleargeometrycandidates);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "addgeometrycandidatesfromobject",
                            &FastMatch::addgeometrycandidatesfromobject);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettemplategeometryavailable",
                            &FastMatch::gettemplategeometryavailable);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettemplategeometrysourceindex",
                            &FastMatch::gettemplategeometrysourceindex);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettemplateboundarypointcount",
                            &FastMatch::gettemplateboundarypointcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettemplatearea",
                            &FastMatch::gettemplatearea);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettemplateorientation",
                            &FastMatch::gettemplateorientation);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidatecount",
                            &FastMatch::getposecandidatecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidatex",
                            &FastMatch::getposecandidatex);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidatey",
                            &FastMatch::getposecandidatey);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidateangle",
                            &FastMatch::getposecandidateangle);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidateappearancescore",
                            &FastMatch::getposecandidateappearancescore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidategeometryscore",
                            &FastMatch::getposecandidategeometryscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getposecandidatecombinedscore",
                            &FastMatch::getposecandidatecombinedscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "Show",
                            &FastMatch::setshow);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "learn",
                            &FastMatch::learn);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setcompgap", &FastMatch::setcomparegap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setwhgap",
                            &FastMatch::SetWHgap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlinesample", &FastMatch::setlinesamplerate);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlinegap", &FastMatch::setlinegap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setmethod",
                            &FastMatch::setmethod);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setthre",
                            &FastMatch::setthre);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setobjfilter", &FastMatch::setobjfilter);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setscanrotation", &FastMatch::setscanrotation);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnwgap", &FastMatch::setlearnwgap_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnhgap", &FastMatch::setlearnhgap_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnmethod",
                            &FastMatch::setlearnmethod_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnthre", &FastMatch::setlearnthre_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnlinegap",
                            &FastMatch::setlearnlinegap_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnobjfilter",
                            &FastMatch::setlearnobjfilter_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearncompgap",
                            &FastMatch::setlearncompgap_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnedgecount",
                            &FastMatch::setlearnedgecount_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setlearnselectededge",
                            &FastMatch::setlearnselectededge_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setfilter",
                            &FastMatch::setfilter);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "savemodel",
                            &FastMatch::savemodelfile);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "loadmodel",
                            &FastMatch::loadmodelfile);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "modelzero",
                            &FastMatch::ZeroPOS);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "modelrotate", &FastMatch::modelrotate);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "modelzoom",
                            &FastMatch::modelzoom);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "modelzeroposition", &FastMatch::modelzeroposition);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "match",
                            &FastMatch::match);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformsearchenabled",
                            &FastMatch::settransformsearchenabled);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltraceenabled", &FastMatch::setnormaltraceenabled);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltraceparams", &FastMatch::setnormaltraceparams);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltracecosts", &FastMatch::setnormaltracecosts);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltraceknn", &FastMatch::setnormaltraceknn);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltraceann", &FastMatch::setnormaltraceann);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltracegeometry", &FastMatch::setnormaltracegeometry);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltracedomain", &FastMatch::setnormaltracedomain);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setnormaltracejunction",
                            &FastMatch::setnormaltracejunction);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setformfitenabled", &FastMatch::setformfitenabled);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setformfitdenseparams",
                            &FastMatch::setformfitdenseparams);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setformfitannparams",
                            &FastMatch::setformfitannparams);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setformfitbudget", &FastMatch::setformfitbudget);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformcenter", &FastMatch::settransformcenter);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformextent", &FastMatch::settransformextent);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformangle", &FastMatch::settransformangle);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformscalepermille",
                            &FastMatch::settransformscalepermille);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformshearpermille",
                            &FastMatch::settransformshearpermille);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformprojectivepermille",
                            &FastMatch::settransformprojectivepermille);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformfromsegmentation",
                            &FastMatch::settransformfromsegmentation);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformfromtorch",
                            &FastMatch::settransformfromtorch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setcalibrationtestenabled",
                            &FastMatch::setcalibrationtestenabled);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setcalibrationtestxytransformscaled",
                            &FastMatch::setcalibrationtestxytransformscaled);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setcalibrationtestreprojectionrmse",
                            &FastMatch::setcalibrationtestreprojectionrmse);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformscalerangepercent",
                            &FastMatch::settransformscalerangepercent);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformanglerange",
                            &FastMatch::settransformanglerange);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformcoarsesteps",
                            &FastMatch::settransformcoarsesteps);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformfinerangepercent",
                            &FastMatch::settransformfinerangepercent);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformfineanglerange",
                            &FastMatch::settransformfineanglerange);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformmaxcandidates",
                            &FastMatch::settransformmaxcandidates);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformmaxsamples",
                            &FastMatch::settransformmaxsamples);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformmaxelapsedms",
                            &FastMatch::settransformmaxelapsedms);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformshearrangepermille",
                            &FastMatch::settransformshearrangepermille);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "settransformprojectiverangepermille",
                            &FastMatch::settransformprojectiverangepermille);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "transformmatch", &FastMatch::transformmatch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchexecuted",
                            &FastMatch::gettransformsearchexecuted);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchconverged",
                            &FastMatch::gettransformsearchconverged);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchbudgetexceeded",
                            &FastMatch::gettransformsearchbudgetexceeded);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchcandidatecount",
                            &FastMatch::gettransformsearchcandidatecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchsamplecount",
                            &FastMatch::gettransformsearchsamplecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchelapsedms",
                            &FastMatch::gettransformsearchelapsedms);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchscore",
                            &FastMatch::gettransformsearchscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchscalex",
                            &FastMatch::gettransformsearchscalex);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchscaley",
                            &FastMatch::gettransformsearchscaley);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchangle",
                            &FastMatch::gettransformsearchangle);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchshear",
                            &FastMatch::gettransformsearchshear);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchprojectiveu",
                            &FastMatch::gettransformsearchprojectiveu);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchprojectivev",
                            &FastMatch::gettransformsearchprojectivev);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchcalibrationapplied",
                            &FastMatch::gettransformsearchcalibrationapplied);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchcalibrationreprojectionrmse",
                            &FastMatch::gettransformsearchcalibrationreprojectionrmse);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchphysicalcx",
                            &FastMatch::gettransformsearchphysicalcx);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchphysicalcy",
                            &FastMatch::gettransformsearchphysicalcy);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchgradientscore",
                            &FastMatch::gettransformsearchgradientscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchresidual",
                            &FastMatch::gettransformsearchresidual);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "gettransformsearchrigidbaselinescore",
                            &FastMatch::gettransformsearchrigidbaselinescore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrotatemaxcandidates", &FastMatch::setrotatemaxcandidates);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrotatemaxelapsedms", &FastMatch::setrotatemaxelapsedms);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrotatemaxprobes", &FastMatch::setrotatemaxprobes);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrotatemaxsamples", &FastMatch::setrotatemaxsamples);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotatebudgetexceeded", &FastMatch::getrotatebudgetexceeded);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotatecandidatecount", &FastMatch::getrotatecandidatecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotateprobecount", &FastMatch::getrotateprobecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotatesamplecount", &FastMatch::getrotatesamplecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotateelapsedms", &FastMatch::getrotateelapsedms);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "matchmore",
                            &FastMatch::matchmore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmatchrect",
                            &FastMatch::setmatchrectxywh_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmatchrectxywh",
                            &FastMatch::setmatchrectxywh_script);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setexpectedrect",
                            &FastMatch::setexpectedrect);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setminscore", &FastMatch::setminscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "matchstepgap", &FastMatch::matchstepgap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "patternrootgrid", &FastMatch::patternrootgrid);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "patternzoom", &FastMatch::patternzoom);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "modeltranform", &FastMatch::patterntranform);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "rotatematch", &FastMatch::rotatematch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "rotatematchAB", &FastMatch::rotatematchAB);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "rotatematch_upgrade",
                            &FastMatch::rotatematchAB_upgrade);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "rotatematchAB05_upgrade",
                            &FastMatch::rotatematchAB05_upgrade);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "rotatematchAB025_upgrade",
                            &FastMatch::rotatematchAB025_upgrade);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setupgradenum", &FastMatch::setupgradenum);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "patterngap", &FastMatch::patternABgap2gap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "patternsample", &FastMatch::patternABsample);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "pattern2org", &FastMatch::pattern2org);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "reorgpattern", &FastMatch::org2pattern);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "patternsize", &FastMatch::ABpatternsize);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "samplemodel", &FastMatch::samplemodelAB);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "loadrotatemodel", &FastMatch::loadrotatemodelfile);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "loadrotate05model",
                            &FastMatch::loadrotate05modelfile);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "loadrotate025model",
                            &FastMatch::loadrotate025modelfile);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrotateangle", &FastMatch::setrotateangle);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setanglescale", &FastMatch::setrotateanglescale);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "multimatch", &FastMatch::multimatch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmultimatchrect", &FastMatch::setmultimatchrect);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmatchrectnum", &FastMatch::setmatchrectnum);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "imagelearn", &FastMatch::imagelearn);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "imagematch", &FastMatch::imagematch);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "imagemodelcompareshow",
                            &FastMatch::imagemodelcompareshow);

    GridPatternClassTool *pgrid_pattern = nullptr;
    const std::string_view grid_pattern_type_name =
        CxScriptTypeName(CxScriptTypeTraits<GridPatternClassTool>::id);
    m_parser.DefineClass(grid_pattern_type_name.data(), pgrid_pattern);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setrect", &GridPatternClassTool::setrect);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setnormalized",
                            &GridPatternClassTool::setnormalized);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setgrid", &GridPatternClassTool::setgrid);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setlevels", &GridPatternClassTool::setlevels);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setorientationbins",
                            &GridPatternClassTool::setorientationbins);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setforegroundthreshold",
                            &GridPatternClassTool::setforegroundthreshold);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setforegrounddark",
                            &GridPatternClassTool::setforegrounddark);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setequalizecontrast",
                            &GridPatternClassTool::setequalizecontrast);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setactiveforegroundpercent",
                            &GridPatternClassTool::setactiveforegroundpercent);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setactiveedgepercent",
                            &GridPatternClassTool::setactiveedgepercent);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setmaxoverlays",
                            &GridPatternClassTool::setmaxoverlays);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "setfusionmode",
                            &GridPatternClassTool::setfusionmode);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "analyze", &GridPatternClassTool::analyze);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getstatuscode",
                            &GridPatternClassTool::getstatuscode);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getactivecellcount",
                            &GridPatternClassTool::getactivecellcount);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getdescriptordim",
                            &GridPatternClassTool::getdescriptordim);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getlevelcount",
                            &GridPatternClassTool::getlevelcount);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getoverlaycount",
                            &GridPatternClassTool::getoverlaycount);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getoverlaytruncated",
                            &GridPatternClassTool::getoverlaytruncated);
    m_parser.DefineClassFun(grid_pattern_type_name.data(), pgrid_pattern,
                            "getelapsedms",
                            &GridPatternClassTool::getelapsedms);

    RegionPatternTool *pregion_pattern = nullptr;
    const std::string_view region_pattern_type_name =
        CxScriptTypeName(CxScriptTypeTraits<RegionPatternTool>::id);
    m_parser.DefineClass(region_pattern_type_name.data(), pregion_pattern);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setrect", &RegionPatternTool::setrect);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setnormalized", &RegionPatternTool::setnormalized);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setpooling", &RegionPatternTool::setpooling);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setbinary", &RegionPatternTool::setbinary);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setthreshold", &RegionPatternTool::setthreshold);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setforegrounddark",
                            &RegionPatternTool::setforegrounddark);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "setmaxoverlays",
                            &RegionPatternTool::setmaxoverlays);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "analyze", &RegionPatternTool::analyze);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getstatuscode", &RegionPatternTool::getstatuscode);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getdescriptordim",
                            &RegionPatternTool::getdescriptordim);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getforegroundpermille",
                            &RegionPatternTool::getforegroundpermille);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getmeanpermille",
                            &RegionPatternTool::getmeanpermille);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getstdpermille",
                            &RegionPatternTool::getstdpermille);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getpoolingrows",
                            &RegionPatternTool::getpoolingrows);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getpoolingcols",
                            &RegionPatternTool::getpoolingcols);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getoverlaycount",
                            &RegionPatternTool::getoverlaycount);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getoverlaytruncated",
                            &RegionPatternTool::getoverlaytruncated);
    m_parser.DefineClassFun(region_pattern_type_name.data(), pregion_pattern,
                            "getelapsedms", &RegionPatternTool::getelapsedms);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "savematchroi", &FastMatch::savematchroi);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "loadmapmodel", &FastMatch::loadfastimagemodel);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "savemapmodel", &FastMatch::savefastimagemodel);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "loadcalibration", &FastMatch::loadcalibration);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "savecalibration", &FastMatch::savecalibration);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getimagemodelreslut",
                            &FastMatch::getimagemodelreslut);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setclustergap", &FastMatch::setclustergap);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "saveimagemodel", &FastMatch::savematchimagemodel);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setmatchthre", &FastMatch::setmatchthre);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setfindnum", &FastMatch::setfindnum);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultnum", &FastMatch::getresultnum);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultcentx", &FastMatch::getresultcentx);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultcenty", &FastMatch::getresultcenty);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultcandidatecount",
                            &FastMatch::getresultcandidatecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultbestscore",
                            &FastMatch::getresultbestscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getmodelpointcount",
                            &FastMatch::getmodelpointcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getlearnacount", &FastMatch::getlearnacount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getlearnbcount", &FastMatch::getlearnbcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getlearna2count", &FastMatch::getlearna2count);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getlearnb2count", &FastMatch::getlearnb2count);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getlearnstatuscode",
                            &FastMatch::getlearnstatuscode);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getnormaltracecandidatecount",
                            &FastMatch::getnormaltracecandidatecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getnormaltracededuplicatedcount",
                            &FastMatch::getnormaltracededuplicatedcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getnormaltracepointcount",
                            &FastMatch::getnormaltracepointcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getnormaltracepaircount",
                            &FastMatch::getnormaltracepaircount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitstatuscode",
                            &FastMatch::getformfitstatuscode);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitreferencecount",
                            &FastMatch::getformfitreferencecount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitobservedcount",
                            &FastMatch::getformfitobservedcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitmutualcount",
                            &FastMatch::getformfitmutualcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitanchorcount",
                            &FastMatch::getformfitanchorcount);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitscore", &FastMatch::getformfitscore);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getformfitresidual",
                            &FastMatch::getformfitresidual);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getmaxresult", &FastMatch::getmaxresult);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setspecshow", &FastMatch::setspecshow);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrelresultnum",
                            &FastMatch::setrelationrectfromresultnum);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrelmatch",
                            &FastMatch::setrelationrectfrom_matchresult);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setrelxy",
                            &FastMatch::setrelationxy);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setrelzoom", &FastMatch::setrelationzoom);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "setreltorect", &FastMatch::setrelationtorect);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "setcolor",
                            &FastMatch::setcolorstyle);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "shapesetroi", &FastMatch::shapesetroi);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotateresultx", &FastMatch::getrotateresultx);
    CxTextInspect *pcxtextinspect = nullptr;
    const std::string_view cxtextinspect_type_name =
        CxScriptTypeName(CxScriptTypeTraits<CxTextInspect>::id);
    m_parser.DefineClass(cxtextinspect_type_name.data(), pcxtextinspect);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "setrect",
                            &CxTextInspect::setrectxywh_script);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "Show",
                            &CxTextInspect::setshow);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "stringsplit",
                            &CxTextInspect::stringsplit);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "fontsplit",
                            &CxTextInspect::fontsplit);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "exfontsplit", &CxTextInspect::exfontsplit);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "areasocr",
                            &CxTextInspect::areasocr);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setsplitimage", &CxTextInspect::setsplitimage);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setsplitobject", &CxTextInspect::setsplitobject);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setsplitobjectbg", &CxTextInspect::setsplitobjectbg);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setsplitobjectoffset",
                            &CxTextInspect::setsplitobjectoffset);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setlayoutdirection",
                            &CxTextInspect::setlayoutdirection);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setlineoverlappercent",
                            &CxTextInspect::setlineoverlappercent);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setglyphcandidatesfromobject",
                            &CxTextInspect::setglyphcandidatesfromobject);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setglyphcandidatesfromfastmatch",
                            &CxTextInspect::setglyphcandidatesfromfastmatch);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setocrthre", &CxTextInspect::setocrthre);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect, "setb2w",
                            &CxTextInspect::setb2w);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "setminscore", &CxTextInspect::setminscore);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidatecount",
                            &CxTextInspect::getglyphcandidatecount);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidatex",
                            &CxTextInspect::getglyphcandidatex);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidatey",
                            &CxTextInspect::getglyphcandidatey);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidatew",
                            &CxTextInspect::getglyphcandidatew);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidateh",
                            &CxTextInspect::getglyphcandidateh);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidateline",
                            &CxTextInspect::getglyphcandidateline);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidatereadingorder",
                            &CxTextInspect::getglyphcandidatereadingorder);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "getglyphcandidateconfidence",
                            &CxTextInspect::getglyphcandidateconfidence);
    m_parser.DefineClassFun(cxtextinspect_type_name.data(), pcxtextinspect,
                            "shapesetroi", &CxTextInspect::shapesetroi);

    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getrotateresulty", &FastMatch::getrotateresulty);

    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getresultcentpoints",
                            &FastMatch::getresultcentpoints);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch, "getshape",
                            &FastMatch::getshape);
    m_parser.DefineClassFun(fastmatch_type_name.data(), pfastmatch,
                            "getmaxresult", &FastMatch::getmaxresult);

    RegisterPendingDirectCxScriptBindings(m_parser);

    SmartDouble *avect = nullptr;
    m_parser.DefineClass("vector", avect);
    m_parser.DefineClassFun("vector", avect, "push", &SmartDouble::push);
    m_parser.DefineClassFun("vector", avect, "get", &SmartDouble::getvalue);
    m_parser.DefineClassFun("vector", avect, "get", &SmartDouble::getresult);
    m_parser.DefineClassFun("vector", avect, "set", &SmartDouble::set);
    m_parser.DefineClassFun("vector", avect, "clear", &SmartDouble::clear);
    m_parser.DefineClassFun("vector", avect, "size", &SmartDouble::size);
    m_parser.DefineClassFun("vector", avect, "average", &SmartDouble::average);
    m_parser.DefineClassFun("vector", avect, "maxvalue",
                            &SmartDouble::maxvalue);
    m_parser.DefineClassFun("vector", avect, "minvalue",
                            &SmartDouble::minvalue);
    m_parser.DefineClassFun("vector", avect, "maxnum", &SmartDouble::maxnum);
    m_parser.DefineClassFun("vector", avect, "save", &SmartDouble::save);
    m_parser.DefineClassFun("vector", avect, "load", &SmartDouble::load);
  }
    m_parser.UsingClass(true);
    m_pdoubleclass = (classbase *)GetClass(string("double"));
    break;
  }
  m_parser.UsingClass(true);
}
void CxParserRuntime::ParserElementShow(int ishow) {
  (void)ishow;
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();
  for (; item != classmap.begin();) {
    --item;
    if ("CFindPointEx" == item->first) {
#ifdef USE_CROIMeasurePointEx
      CROIMeasurePointEx *apimage = 0;
      for (int i = 0; i < pclass->size(); i++) {
        apimage = (CROIMeasurePointEx *)pclass->getvarpoint(i);
        if (ishow > 0)
          apimage->SetShow(35);
        else
          apimage->SetShow(0);
      }
#endif
    }
  }
}
void *CxParserRuntime::GetClass(const string &strclass) {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  classbasemap_type::const_iterator item = classmap.find(strclass);
  if (item != classmap.end()) {
    return (item->second);
  } else
    return 0;
}
void *CxParserRuntime::GetClassObj(const string &strclass,
                                   const string &strobj) {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  classbase *pclass;

  classbasemap_type::const_iterator item = classmap.find(strclass);
  if (item != classmap.end()) {
    pclass = (item->second);
    return pclass->getvarpoint(strobj);
  } else
    return 0;
}
void *CxParserRuntime::GetClassObj(const string &strclass, const int &iobjnum) {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  classbase *pclass;
  if (!classmap.size())
    return 0;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    if (strclass == item->first) {
      pclass = (item->second);

      if (iobjnum >= pclass->size())
        return 0;
      {
        *m_stream << pclass->getvar(iobjnum);
        *m_stream << "\r\n";
        return pclass->getvarpoint(iobjnum);
      }
    }
  }
  return NULL;
}

void *CxParserRuntime::GetDoubleValue(const string &strname) {
  if (m_pdoubleclass != 0)
    return m_pdoubleclass->getvarpoint(strname);
  return 0;
}
int CxParserRuntime::GetClassObjSum(const string &strclass) {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  classbase *pclass;
  if (!classmap.size())
    return 0;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    if (strclass == item->first) {
      pclass = (item->second);

      return pclass->size();
    }
  }
  return 0;
}

std::string CxParserRuntime::GetClassObjName(const std::string &class_name,
                                             int object_index) const {
  return m_parser.GetClassObjName(class_name, object_index);
}

void CxParserRuntime::SetExpr(const string &str) { m_parser.SetExpr(str); }
value_type CxParserRuntime::Eval() { return m_parser.Eval(); }
double CxParserRuntime::GetResult() { return m_parser.Eval(); }
void CxParserRuntime::DefineVar(const string &str, double *dvalue) {
  m_parser.DefineVar(str, dvalue);
}
void CxParserRuntime::DefineStringConstant(const string &name,
                                           const string &value) {
  m_parser.DefineStrConst(name, value);
}
void CxParserRuntime::SetVarFactory() {
  m_parser.SetVarFactory((mu::facfun_type)&CxParserRuntime::AddVariable, this);
}

void CxParserRuntime::FunTest(void *func) { m_Func = func; }

double *CxParserRuntime::AddVariable(const char *a_szName, void *pClass) {
  CxParserRuntime *me = reinterpret_cast<CxParserRuntime *>(pClass);

  *(me->m_stream) << "Generating new variable \"" << a_szName
                  << "\" (slots left: " << 99 - me->m_iVal << ")"
                  << "\r\n";

  me->m_afValBuf[me->m_iVal] = 0;
  if (me->m_iVal >= 99)
    throw mu::ParserError("Variable buffer overflow.");

  return &(me->m_afValBuf[me->m_iVal++]);
}

ParserByteCode::storage_type CxParserRuntime::GetByteCode() {
  return m_parser.GetStorageBase();
}
double CxParserRuntime::RunByteCode(ParserByteCode::storage_type Base) {
  m_parser.m_vByteCodeCollection.SetStorageBase(Base);
  double dresult = m_parser.RunCollectionCmdCode();
  m_parser.ClearCollection();
  return dresult;
}
double CxParserRuntime::RunOptCode() { return m_parser.RunCollectionOpt(); }
void CxParserRuntime::RunFastCode() { return m_parser.RunCode(); }
void CxParserRuntime::CopyRunOpt(int inum) { m_parser.CopyRUNOpt(inum); }
void CxParserRuntime::RunOptNum(int inum) {
  try {
    m_parser.RunOpt(inum);
  } catch (mu::Parser::exception_type &) {

    *m_stream << "\n RunOptNum RunTime Error exception_type ";
  }
}

void CxParserRuntime::SetRunOpt(const string &strname) {
  m_parser.SetOptStack(strname);
}

void CxParserRuntime::RunOptString(const char *a_szName) {
#if defined(_WIN32)
  __try {
    m_parser.RunOptString(a_szName);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    *m_stream << a_szName;
    *m_stream << std::endl << "RunOptString RunTime Error" << std::endl;
  }
#else
  try {
    m_parser.RunOptString(a_szName);
  } catch (...) {
    *m_stream << a_szName;
    *m_stream << std::endl << "RunOptString RunTime Error" << std::endl;
  }
#endif
}
void CxParserRuntime::ClearOptMap() { m_parser.ClearOptStack(); }
void CxParserRuntime::RunOptNum_TimeLimit(int inum) { (void)inum; }
void CxParserRuntime::SetOptCollect(bool Open_Close) {
  m_parser.SetOptCollect(Open_Close);
}
void CxParserRuntime::SetByteCollection(bool btruefalse) {
  m_parser.SetColllection(btruefalse);
}
void CxParserRuntime::ClearByteCollection() { m_parser.ClearCollection(); }
bool CxParserRuntime::CommandLine(const string &astr) {
  std::string sLine = astr;
  if (sLine == ">>quit" || sLine == ">>quit\r\n") {
    exit(0);
  } else if (sLine == ">>help" || sLine == ">>help\r\n") {
    ShowHelp();
    return true;
  } else if (sLine == ">>list var" || sLine == ">>list var\r\n") {
    ListVar(&m_parser);
    return true;
  } else if (sLine == ">>list const" || sLine == ">>list const\r\n") {
    ListConst(&m_parser);
    return true;
  } else if (sLine == ">>list exprvar" || sLine == ">>list exprvar\r\n") {
    ListExprVar(&m_parser);
    return true;
  } else if (sLine == ">>list const" || sLine == ">>list const\r\n") {
    ListConst(&m_parser);
    return true;
  } else if (sLine == ">>list func" || sLine == ">>list func\r\n") {
    ListFunction(&m_parser);
    return true;
  } else if (sLine == ">>list class" || sLine == ">>list class\r\n") {
    ListClass(m_parser);
    return true;
  } else if (sLine == ">>list only name") {
    ListClassONLYName();
    ListClassONLYFUNCTION();
    return true;
  } else if (sLine == ">>run" || sLine == ">>run\r\n") {
    m_parser.RunCode();
    *m_stream << "create code :"
              << "\r\n";
    *m_stream << m_parser.m_StrCollection;
    return true;
  } else if (sLine == ">>run" || sLine == ">>run\r\n") {
    m_parser.RunCollectionOpt();
    return true;
  } else if (sLine == ">>open collec" || sLine == ">>open collec\r\n") {
    m_parser.SetColllection(true);
    return true;
  } else if (sLine == ">>close collec" || sLine == ">>close collec\r\n") {
    m_parser.SetColllection(false);
    return true;
  } else if (sLine == ">>clear collec" || sLine == ">>clear collec\r\n") {
    m_parser.ClearCollection();
    return true;
  } else if (sLine == ">>clear" || sLine == ">>clear\r\n") {
    m_stream->clear();
    return true;
  } else if (sLine == ">>clear var" || sLine == ">>clear var\r\n") {
    m_parser.ClearVar();
    return true;
  } else if (sLine == ">>clear all" || sLine == ">>clear all\r\n") {
    m_parser.ClearClassObj();
    m_parser.ClearVar();
    return true;
  } else if (sLine == ">>test" || sLine == ">>test\r\n") {
    SelfTest();
    return true;
  } else if (sLine == ">>Set VarFact" || sLine == ">>Set VarFact\r\n") {
    SetVarFactory();
    return true;
  } else if (sLine == ">>open classdef" || sLine == ">>open classdef\r\n") {
    m_parser.UsingClass(true);
    return true;
  } else if (sLine == ">>close classdef" || sLine == ">>close classdef\r\n") {
    m_parser.UsingClass(false);
    return true;
  } else {
  }
  return false;
}
void CxParserRuntime::ShowHelp() {
  *m_stream << "!-----------------help------------------- "
            << "\r\n";
  *m_stream << "Commands:\r\n";
  *m_stream << "  list var     - list parser variables\r\n";
  *m_stream << "  list exprvar - list expression variables\r\n";
  *m_stream << "  list const   - list all numeric parser constants\r\n";
  *m_stream << "  exit         - exits the parser\r\n";

  *m_stream << "  list func    - list parser express function"
            << "\r\n";
  *m_stream << "  list class   - list parser class define and class var"
            << "\r\n";

  *m_stream << "  open collec  - open a vector to collection expression"
            << "\r\n";
  *m_stream << "  close collec - close vector collection"
            << "\r\n";
  *m_stream << "  clear collec - clear expression vector"
            << "\r\n";
  *m_stream << "  run          - run collection expression"
            << "\r\n";

  *m_stream << "  clear       -clear output screen\r\n"
            << "\r\n";
  *m_stream << "  clear var   -clear variables"
            << "\r\n";

  *m_stream << "  open classdef  - make parser to recognize class define"
            << "\r\n";
  *m_stream << "  close classdef - close recognize class define"
            << "\r\n";

  *m_stream << "  test        -self test "
            << "\r\n";

  *m_stream << "Constants:\r\n";
  *m_stream << "  \"_e\"   2.718281828459045235360287\r\n";
  *m_stream << "  \"_pi\"  3.141592653589793238462643\r\n";
  *m_stream << "---------------------------------------\r\n";
  *m_stream << "Enter a formula or a command:\r\n";
}

void CxParserRuntime::SelfTest() {
  mu::Test::ParserTester pt;
  pt.Run();
}

void CxParserRuntime::ClearAll() {
  m_parser.ClearClassObj();
  m_parser.ClearVar();
}
void CxParserRuntime::ListVar(const mu::Parser *Pparser) {
  mu::varmap_type variables = Pparser->GetVar();
  if (!variables.size())
    return;
  *m_stream << "\nParser variables:\r\n";
  *m_stream << "-----------------\r\n";
  *m_stream << "Number: " << (int)variables.size() << "\r\n";
  varmap_type::const_iterator item = variables.begin();
  for (; item != variables.end(); ++item) {
    *m_stream << "Name: " << item->first << "   Address: [0x" << item->second
              << "]  ";
    m_parser.SetExpr(item->first);
    *m_stream << "Result: " << Pparser->Eval() << "\r\n";
  }
}

void CxParserRuntime::ListVar() { ListVar(&m_parser); }
void CxParserRuntime::ListConst(const mu::Parser *Pparser) {
  *m_stream << "\nParser constants:\r\n";
  *m_stream << "-----------------\r\n";
  mu::valmap_type cmap = Pparser->GetConst();
  if (!cmap.size()) {
    *m_stream << "Expression does not contain constants\r\n";
  } else {
    valmap_type::const_iterator item = cmap.begin();
    for (; item != cmap.end(); ++item)
      *m_stream << "  " << item->first << " =  " << item->second << "\r\n";
  }
}

void CxParserRuntime::ListExprVar(const mu::Parser *Pparser) {
  std::string sExpr = Pparser->GetExpr();
  if (sExpr.length() == 0) {
    *m_stream << "Expression string is empty\r\n";
    return;
  }
  *m_stream << "\nExpression variables:\r\n";
  *m_stream << "---------------------\r\n";
  *m_stream << "Expression: " << Pparser->GetExpr() << "\r\n";
  varmap_type variables = Pparser->GetUsedVar();
  if (!variables.size()) {
    *m_stream << "Expression does not contain variables\r\n";
  } else {
    *m_stream << "Number: " << (int)variables.size() << "\r\n";
    mu::varmap_type::const_iterator item = variables.begin();
    for (; item != variables.end(); ++item)
      *m_stream << "Name: " << item->first << "   Address: [0x" << item->second
                << "]\r\n";
  }
}
void CxParserRuntime::ListFunction(const mu::Parser *pParser) {
  using mu::funmap_type;
  funmap_type funmap = pParser->GetFunDef();
  funmap_type::const_iterator item;
  *m_stream << "\nFunctions available:\r\n";
  for (item = funmap.begin(); item != funmap.end(); ++item) {
    *m_stream << "  " << item->first;
    *m_stream << "(";
    int iArgc = item->second.GetArgc();
    if (iArgc >= 0) {
      for (int i = 0; i < iArgc; ++i) {
        char cVar[] = "val ";
        cVar[3] = '1' + (char)i;
        *m_stream << cVar;
        if (i != iArgc - 1)
          *m_stream << ",";
      }
    } else {
      *m_stream << "...";
    }
    *m_stream << ")\r\n";
  }
}

void CxParserRuntime::ListClass(mu::Parser &Pparser) {
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  *m_stream << "\nParser Class:\r\n";
  *m_stream << "-----------------\r\n";
  *m_stream << "Number: " << (int)classmap.size() << "\r\n";
  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    *m_stream << "Class Name: " << item->first << "   Address: [0x"
              << item->second << "]  "
              << "\r\n";
    classbase *pclass = (item->second);

    *m_stream << "      Object Number: " << (int)pclass->size() << "\r\n";
    *m_stream << "      member Function Number: " << (int)pclass->funcsize()
              << "\r\n";

    for (int i = 0; i < pclass->size(); i++) {
      *m_stream << "            Object Name: " << pclass->getvar(i)
                << "   Address: [0x" << pclass->getvarpoint(i) << "]  "
                << "\r\n";
    }
    for (int i = 0; i < pclass->funcsize(); i++) {
      *m_stream << "             member Function: " << pclass->getfuncname(i)
                << "   Type: [" << pclass->getfunctype(i) << "]  "
                << "\r\n";
    }
  }
  *m_stream << "\r\n";
}

void CxParserRuntime::ListClassONLYName() {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  if (!classmap.size())
    return;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    *m_stream << item->first << "\r\n";
  }
  *m_stream << "\r\n";
}

void CxParserRuntime::ListClassONLYFUNCTION() {
  mu::classbasemap_type classmap = m_parser.GetClassMap();
  if (!classmap.size())
    return;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    classbase *pclass = (item->second);
    for (int i = 0; i < pclass->funcsize(); i++) {
      *m_stream << pclass->getfuncname(i) << "\r\n";
    }
  }
  *m_stream << "\r\n";
}

void CxParserRuntime::ListClass() { ListClass(m_parser); }
void CxParserRuntime::FindClassObject(mu::Parser &Pparser,
                                      const char *a_szClass) {
  string astrclass(a_szClass);

  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    classbase *pclass = (item->second);
    if (item->first == astrclass) {
      for (int i = 0; i < pclass->size(); i++) {
        *m_stream << "            Object Name: " << pclass->getvar(i)
                  << "   Address: [0x" << pclass->getvarpoint(i) << "]  "
                  << "\r\n";
      }
      for (int i = 0; i < pclass->funcsize(); i++) {
        *m_stream << "             member Function: " << pclass->getfuncname(i)
                  << "   Type: [" << pclass->getfunctype(i) << "]  "
                  << "\r\n";
      }
    }
  }
  *m_stream << "\r\n";
}
bool CxParserRuntime::IsObject(mu::Parser &Pparser,
                               const char *a_szClassObject) {
  string astrclassobj(a_szClassObject);
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return false;
  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    classbase *pclass = (item->second);
    for (int i = 0; i < pclass->size(); i++) {
      if (astrclassobj == pclass->getvar(i))
        return true;
    }
  }
  return false;
}
bool CxParserRuntime::IsObjectVar(const char *a_sz) {
  return IsObject(m_parser, a_sz);
}
void CxParserRuntime::ListFormula(mu::Parser &Pparser) {
  mu::string_type astring = Pparser.GetFormula();
  *m_stream << " Formula: " << astring << "\r\n";
}
void CxParserRuntime::SetStream(std::ostream *a_stream) {
  assert(a_stream);
  m_stream = a_stream;
}
void CxParserRuntime::SetCreateCodeStream(std::ostream *a_stream) {
  assert(a_stream);
  m_createstream = a_stream;
}
bool CxParserRuntime::Compile(const char *a_szLine) {
  try {
    if (CommandLine(a_szLine)) {
      m_iget = 0;
      return 1;
    }
    {
      SetExpr(a_szLine);
      *m_stream << "Result:" << Eval() << "\r\n";

      *m_stream << "================Build: 1 OK , 0 Fail ============"
                << "\r\n";
    }
  } catch (mu::Parser::exception_type &e) {
    const string message = e.GetMsg();
    if (message.find("stack is empty") != string::npos) {
      *m_stream << "Result:<void>\r\n";
      *m_stream << "================Build: 1 OK , 0 Fail ============"
                << "\r\n";
      return 1;
    }

    *m_stream << "\nError: ";
    *m_stream << " Message:  " << message << "\r\n";
    *m_stream << " Token: " << e.GetToken();
    *m_stream << " Position: " << (int)e.GetPos() << "\r\n";
    *m_stream << " Errc: " << e.GetCode() << "\r\n";
    *m_stream << "================Build: 0 OK , 1 Fail ============"
              << "\r\n";
    return 0;
  }
  return 1;
}

bool CxParserRuntime::CompileCollectedScript(const std::string &script,
                                             std::string &reason) {
  try {
    std::vector<std::string> statements;
    std::string current;
    int brace_depth = 0;
    bool in_string = false;
    bool escaped = false;
    bool line_comment = false;
    bool block_comment = false;

    for (size_t i = 0; i < script.size(); ++i) {
      const char ch = script[i];
      const char next = i + 1 < script.size() ? script[i + 1] : '\0';

      if (line_comment) {
        if (ch == '\n')
          line_comment = false;
        continue;
      }
      if (block_comment) {
        if (ch == '*' && next == '/') {
          block_comment = false;
          ++i;
        }
        continue;
      }
      if (!in_string && ch == '/' && next == '/') {
        line_comment = true;
        ++i;
        continue;
      }
      if (!in_string && ch == '/' && next == '*') {
        block_comment = true;
        ++i;
        continue;
      }

      current.push_back(ch);
      if (in_string) {
        if (escaped)
          escaped = false;
        else if (ch == '\\')
          escaped = true;
        else if (ch == '"')
          in_string = false;
        continue;
      }
      if (ch == '"') {
        in_string = true;
        continue;
      }
      if (ch == '{')
        ++brace_depth;
      else if (ch == '}')
        --brace_depth;

      if ((ch == ';' && brace_depth == 0) || (ch == '}' && brace_depth == 0)) {
        statements.push_back(current);
        current.clear();
      }
    }
    if (current.find_first_not_of(" \t\r\n") != std::string::npos)
      statements.push_back(current);

    if (statements.empty()) {
      reason = "CompileCollectedScript produced no statements";
      return false;
    }

    m_collectedScriptStatements = std::move(statements);
    reason.clear();
    return true;
  } catch (mu::Parser::exception_type &e) {
    reason = "CompileCollectedScript failed: " + e.GetMsg();
    m_collectedScriptStatements.clear();
    return false;
  } catch (...) {
    reason = "CompileCollectedScript crashed";
    m_collectedScriptStatements.clear();
    return false;
  }
}

bool CxParserRuntime::RunCollectedScript(std::string &reason) {
  try {
    for (const auto &statement : m_collectedScriptStatements) {
      std::string statement_preview = statement;
      constexpr std::size_t kMaxStatementPreview = 240;
      if (statement_preview.size() > kMaxStatementPreview)
        statement_preview.resize(kMaxStatementPreview);
      SetCxCrashBreadcrumb("CxParserRuntime::RunCollectedScript:statement:" +
                           statement_preview);
      CXLOG_INFO("CxParserRuntime", "cxscript_statement_begin", "running",
                 statement_preview);
      if (!Compile(statement.c_str())) {
        reason = "RunCollectedScript failed near statement: " + statement;
        CXLOG_ERROR("CxParserRuntime", "cxscript_statement_end", "failed",
                    statement_preview);
        m_collectedScriptStatements.clear();
        return false;
      }
      CXLOG_INFO("CxParserRuntime", "cxscript_statement_end", "finished",
                 statement_preview);
    }
    m_collectedScriptStatements.clear();
    reason.clear();
    return true;
  } catch (mu::Parser::exception_type &e) {
    reason = "RunCollectedScript failed: " + e.GetMsg();
    m_collectedScriptStatements.clear();
    return false;
  } catch (...) {
    reason = "RunCollectedScript crashed";
    m_collectedScriptStatements.clear();
    return false;
  }
}

void CxParserRuntime::ResetRun() {
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    if ("GDIimage32" == item->first) {
      return;
    }
  }
}
void CxParserRuntime::DragImageParserElement(int ipointx, int ipointy) {
  (void)ipointx;
  (void)ipointy;
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();
}
void CxParserRuntime::HitTestImageParserElement(int ipointx, int ipointy) {
  (void)ipointx;
  (void)ipointy;
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    if ("CFindPoint" == item->first) {
#ifdef USE_CROIMeasurePoint
#endif
    }
  }
}
void CxParserRuntime::MouseDownParserElement(int PointX, int PointY) {
  (void)PointX;
  (void)PointY;
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    if ("FastMatch" == item->first) {
#ifdef USE_CFastMatch

#endif
    }
  }
}
void CxParserRuntime::MouseUpParserElement(int PointX, int PointY) {
  (void)PointX;
  (void)PointY;
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    if ("FastMatch" == item->first) {
#ifdef USE_CFastMatch

#endif
    }
  }
}

bool CxParserRuntime::MouseRDownParserElement() {

  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return false;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    if ("CPolygonShape" == item->first) {
    }
  }

  return false;
}

void CxParserRuntime::StopRun() { m_parser.RunStop(); }
void CxParserRuntime::SetRunOk() { m_parser.RunOk(); }
void CxParserRuntime::GetImageObjectAtt() {
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    classbase *pclass = (item->second);
    if ("double" == item->first) {
      double *adouble = 0;
      for (int i = 0; i < pclass->size(); i++) {
        adouble = (double *)pclass->getvarpoint(i);
        if (adouble) {
          *m_createstream << pclass->getvar(i) << "=" << *adouble << ";\r\n";
        }
      }
    }
  }
}
void CxParserRuntime::GetImageObjectAutoSave() {
  mu::Parser &Pparser = m_parser;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;
  classbasemap_type::const_iterator item = classmap.end();

  for (; item != classmap.begin();) {
    --item;
    classbase *pclass = (item->second);
    if ("double" == item->first) {
      double *adouble = 0;
      for (int i = 0; i < pclass->size(); i++) {
        adouble = (double *)pclass->getvarpoint(i);
        if (adouble) {
          *m_createstream << pclass->getvar(i) << "=" << *adouble << ";\r\n";
        }
      }
    } else if ("Shape" == item->first) {
      Shape *ashape = 0;
      for (int i = 0; i < pclass->size(); i++) {
        ashape = (Shape *)pclass->getvarpoint(i);
        if (ashape) {
          *m_createstream << pclass->getvar(i) << ".setrect("
                          << ashape->rect().TopLeft().X() << ","
                          << ashape->rect().TopLeft().Y() << ","
                          << ashape->rect().Width() << ","
                          << ashape->rect().Height() << ")"
                          << ";\r\n";
        }
      }
    } else if ("ShapeBase" == item->first) {
    } else if ("LineShape" == item->first) {
    } else if ("FindLine" == item->first) {
      FindLine *ashape = 0;
      for (int i = 0; i < pclass->size(); i++) {
        ashape = (FindLine *)pclass->getvarpoint(i);
        if (ashape) {
          *m_createstream << pclass->getvar(i) << ".setrect("
                          << ashape->rect().TopLeft().X() << ","
                          << ashape->rect().TopLeft().Y() << ","
                          << ashape->rect().Width() << ","
                          << ashape->rect().Height() << ")"
                          << ";\r\n";
        }
      }
    }
  }
}

void CxParserRuntime::CreateClassDef(const char *pclassname,
                                     const char *pclassdef) {
  m_parser.DefineCreateClass(pclassname, pclassdef);
}
void CxParserRuntime::CreateClassFunc(const char *pclassname,
                                      const char *pclassfucname,
                                      const char *pclassfucdef) {
  m_parser.DefineCreateClasFun(pclassname, pclassfucname, pclassfucdef);
}
void CxParserRuntime::ListCreateClassDef(mu::Parser &Pparser,
                                         const char *pclassname) {
  string_type astr(pclassname);
  string_type strclass;
  CreateClass *paclass;
  int iclassmemsum = 0;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    classbase *pclass = (item->second);
    strclass = pclass->getclass();
    if (astr == strclass)
      if (pclass->Iscreateclass()) {
        paclass = (CreateClass *)pclass;
        iclassmemsum = paclass->GetClassMemberNum();
        for (int i = 0; i < iclassmemsum; i++) {
          *m_stream << paclass->GetClassDefName(i) << " "
                    << paclass->GetClassMemberName(i) << ";\r\n";
        }
      }
  }
  *m_stream << "\r\n";
}
void CxParserRuntime::ListCreateClassFunDef(mu::Parser &Pparser,
                                            const char *pclassname,
                                            const char *pclassfuncname) {
  string_type astr(pclassname);
  string_type strclass;
  CreateClass *paclass;
  int iclassmemsum = 0;
  mu::classbasemap_type classmap = Pparser.GetClassMap();
  if (!classmap.size())
    return;

  classbasemap_type::const_iterator item = classmap.begin();
  for (; item != classmap.end(); ++item) {
    classbase *pclass = (item->second);
    strclass = pclass->getclass();
    if (astr == strclass)
      if (pclass->Iscreateclass()) {
        paclass = (CreateClass *)pclass;
        *m_stream << paclass->GetFuncDef(pclassfuncname) << "\r\n";
      }
  }
  *m_stream << "\r\n";
}
void CxParserRuntime::ListCreateClassDef(const char *pclassname) {
  ListCreateClassDef(m_parser, pclassname);
}
void CxParserRuntime::ListCreateClassFunDef(const char *pclassname,
                                            const char *pclassfuncname) {
  ListCreateClassFunDef(m_parser, pclassname, pclassfuncname);
}
} // namespace mu
