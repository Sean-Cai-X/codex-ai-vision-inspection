#ifndef CXTEXTINSPECT_H
#define CXTEXTINSPECT_H

#include "FastMatch.h"

#include <algorithm>
#include <filesystem>
#include <regex>

#include <sstream>
#include <string>
#include <utility>
#include <vector>


class FindObject;

class CxOcrStringList;

class CxOcrRegex {
public:
    explicit CxOcrRegex(const char* pattern) : expression_(pattern) {}
    const std::regex& expression() const { return expression_; }
private:
    std::regex expression_;
};

struct CxTextInspectGlyphCandidateSnapshot {
    int source_object_index = -1;
    int line_index = -1;
    int reading_order = -1;
    cv::Rect2d bbox_px;
    cv::Point2d centroid_px;
    double orientation_deg = 0.0;
    double projected_area = 0.0;
    double aspect_ratio = 0.0;
    double solidity = 0.0;
    std::string label;
    double appearance_score = 0.0;
    double geometry_score = -1.0;
    double confidence = 0.0;
    std::string source_object_ref;
    std::string status = "segmented";
};

class CxOcrString : public std::string {
public:
    using std::string::operator=;
    using std::string::string;
    CxOcrString() = default;

    CxOcrString(char value) : std::string(1, value) {}
    CxOcrString(const std::string& value) : std::string(value) {}
    CxOcrString(std::string&& value) : std::string(std::move(value)) {}
    bool isEmpty() const { return empty(); }
    int length() const { return static_cast<int>(size()); }
    std::string toStdString() const { return *this; }
    CxOcrString left(int count) const { return substr(0, static_cast<size_t>(std::max(count, 0))); }
    CxOcrString right(int count) const { const size_t span = static_cast<size_t>(std::max(count, 0)); return substr(size() > span ? size() - span : 0); }
    CxOcrString mid(int position, int count = -1) const { const size_t begin = static_cast<size_t>(std::max(position, 0)); if (begin >= size()) return {}; return count < 0 ? CxOcrString(substr(begin)) : CxOcrString(substr(begin, static_cast<size_t>(count))); }
    int indexOf(const CxOcrString& value) const { const size_t pos = find(value); return pos == npos ? -1 : static_cast<int>(pos); }

    template <typename Value>
    CxOcrString arg(const Value& value) const {
        std::ostringstream stream;
        stream << value;
        CxOcrString result(*this);
        for (int placeholderIndex = 1; placeholderIndex <= 9; ++placeholderIndex) {
            const std::string placeholder = "%" + std::to_string(placeholderIndex);
            const size_t position = result.find(placeholder);
            if (position != std::string::npos) {
                result.std::string::replace(position, placeholder.size(), stream.str());
                break;
            }
        }
        return result;
    }
    CxOcrString& replace(const CxOcrRegex& pattern, const CxOcrString& replacement) { std::string::operator=(std::regex_replace(*this, pattern.expression(), replacement)); return *this; }
    CxOcrStringList split(const CxOcrString& separator) const;

    CxOcrStringList split(const CxOcrRegex& separator) const;

    CxOcrStringList split(char separator) const;
};

class CxOcrStringList {
public:
    CxOcrStringList() = default;
    CxOcrStringList(const CxOcrString& value) { values_.push_back(value); }
    bool isEmpty() const { return values_.empty(); }
    int size() const { return static_cast<int>(values_.size()); }
    void clear() { values_.clear(); }
    void append(const CxOcrString& value) { values_.push_back(value); }

    void push_back(const CxOcrString& value) { values_.push_back(value); }
    void removeAt(int index) { if (index >= 0 && index < size()) values_.erase(values_.begin() + index); }
    const CxOcrString& at(int index) const { return values_.at(static_cast<size_t>(index)); }
    CxOcrString& at(int index) { return values_.at(static_cast<size_t>(index)); }
    const CxOcrString& operator[](int index) const { return values_[static_cast<size_t>(index)]; }
    CxOcrString& operator[](int index) { return values_[static_cast<size_t>(index)]; }
private:
    std::vector<CxOcrString> values_;
};

inline CxOcrStringList CxOcrString::split(const CxOcrString& separator) const {
    CxOcrStringList result;
    if (separator.empty()) { result.append(*this); return result; }
    size_t begin = 0;
    while (begin <= size()) {
        const size_t end = find(separator, begin);
        result.append(end == npos ? substr(begin) : substr(begin, end - begin));
        if (end == npos) break;
        begin = end + separator.size();
    }
    return result;
}

namespace std {
using String = ::CxOcrString;
using StringList = ::CxOcrStringList;
}

inline CxOcrStringList CxOcrString::split(char separator) const {
    return split(CxOcrString(1, separator));
}

inline CxOcrStringList CxOcrString::split(const CxOcrRegex& separator) const {
    CxOcrStringList result;
    std::sregex_token_iterator it(begin(), end(), separator.expression(), -1);
    const std::sregex_token_iterator endIt;
    for (; it != endIt; ++it) {
        result.append(it->str());
    }
    return result;
}

using ImageBase = Image;
using Findobject = FindObject;
using fastmatch = FastMatch;
using QRegExp = CxOcrRegex;


class CxOcrRect {
public:
    CxOcrRect(const gp_Rectangle& rectangle) : rectangle_(rectangle) {}
    int x() const { return static_cast<int>(rectangle_.TopLeft().X()); }
    int y() const { return static_cast<int>(rectangle_.TopLeft().Y()); }
    int width() const { return static_cast<int>(rectangle_.Width()); }
    int height() const { return static_cast<int>(rectangle_.Height()); }
    const gp_Rectangle& geometry() const { return rectangle_; }
private:
    gp_Rectangle rectangle_;
};

using Rect = CxOcrRect;

typedef struct levelnode
{
    int s_ilevel;
    int s_inode;
}levelnode;
typedef struct levelvalenode
{
    int s_ilevel;
    int s_inode;
    double s_dvalue;
}levelvaluenode;

class levelnodes {
public:
    void setsearchnum(int inum) { m_searchsum = std::max(0, inum); }

    void addnode(const levelvaluenode& node) {
        const auto insertion = std::find_if(
            s_nodes.begin(), s_nodes.end(),
            [&node](const levelvaluenode& current) {
                return node.s_dvalue > current.s_dvalue;
            });
        s_nodes.insert(insertion, node);
        if (m_searchsum >= 0 &&
            static_cast<int>(s_nodes.size()) > m_searchsum) {
            s_nodes.pop_back();
        }
    }

    std::vector<levelvaluenode>& getnodes() { return s_nodes; }

private:
    std::vector<levelvaluenode> s_nodes;
    int m_searchsum = 0;
};
class CxTextInspect: public FastMatch
{
public:
    CxTextInspect();
    ~CxTextInspect();
    void setshow(int ishow);
    virtual void setrect(int ix,int iy,int iw,int ih);
    void drawshape() override;
    void setocrareasnum(int inum);
    void setocrareas(int inum,int ix,int iy,int iw,int ih);
    void setocrthre(int ithre);
    void setb2w(int ib2w);
    void setspecshow(int ishow);
    void imagemodelshow();
    void imagematchshow();
    void imagecompareshow(int itype);
    void setshowpos(int ix,int iy);
    void modelmethod(int itype);

    void match72_matchimg();
    void match72_matchpat();

	void setgrid(int iw,int igrid);

    void levelmodel();
    void savelevelmodel();
    void SelectModel(int ilevle,int inum);

    int imagefastmapsize(int ilevel,int inum);
    int SelectMapModel(int ilevel,int inum,int i0);

    void selectmodel72(const char *pfilename);

    void SelectNameModel(int ilevel,const char *pfilename);

    void fontocr();
    void fontocr_level(int ilevel);
    void fontocr_levelex(int ilevel);
    void fontocr_level2();
    void checkocr_level3();

    void fontsplit(void *pimage);
    void exfontsplit(void *pimage);
    void stringsplit(void *pimage);
    void areasocr(void *pimage);
    void setdebug(int idebugrect,int idebugfont);
    void setsplitimage(int ithre,int ixor,int iyor,int ixand,int iyand);
    void setsplitobject(int idistance,int isearchtype,int ibrow,int iminarea,int ibgedge);
    void setsplitgrid(int iw,int ih,int igridnum);
    void setsplitobjectoffset(int ix0,int ix1,int iy0,int iy1);
    void setsplitobjectbg(int ibgedge,int ibgmethod);

    void autolearn(const char *pfilename);
    void autolearnex(const char *pfilename);
    void autolearnobj(const char *pfilename );
    void setlearngridwh(int igridwh);
    void autolearnmass(const char *pfilename );

    void learnmass_36(const char *pfilename );
    void checklearn(const char *pfilename );
    void checkmatch(const char *pfilename );

    void setimagetype(int itype);

	void string_exnum(int inum);
	void string_autolearnmass(const char *pstring);

    void setmatchvalid(double dthre);
    void setusingobject(int iusing);

    void match72();

    bool matchlevelnode01();
    bool matchlevelnode2();
    void fontocr_levelnode(int ilevel);

    int resultnodesize(int ilevel,int iareanum);
    void selectresultnode(int ilevel,int iareanum,int inum0);
    void resultnodereset(int iareasnum);
    void setresultnode(int inum,levelnode anode);

    bool matchlevelnodelist3x3(int ia);
    bool matchlevelnodelist6x6(int ia);
    bool matchlevelnodelist12x12(int ia);
    bool matchlevelnodelist36x36(int ia);
    bool matchlevelnodelist72x72(int ia);
    void fontocr_levelnodelist();

    void resultnodelistreset(int iareasnum);
    void setresultnodelist(int ilevel,int iareanum,levelnode anode);

    std::String char2string(std::String pchar);
    std::String string2char(const std::String &strchar);

    std::String filenametoOCRstring(std::String strbase);

    std::String getreslultstring();
    void clipboardresult();
    std::String ABC2string(std::String strget);
    void setlevelstring();
    void setshowmap(int ishow,int ilevel,int idebugfont);
    void mapclear();
    void mapgrid();
    void shownoderesult();
    void shownoderesultex();
    void shownodelistresult12x12();
    void setminscore(double dminscore);
    void stringresulthead(const char*pchar);
    void stringresulttail(const char*pchar);
    void shapesetroi(void *pshape);

    void setrectxywh_script(int height, int width, int y, int x) {
        setrect(x, y, width, height);
    }
    void setlayoutdirection(int direction);
    void setlineoverlappercent(int percent);
    void setglyphcandidatesfromobject(void* pfindobject);
    void setglyphcandidatesfromfastmatch(void* pfastmatch);
  int getglyphcandidatecount();
  double getglyphcandidatex(int index);
  double getglyphcandidatey(int index);
  double getglyphcandidatew(int index);
  double getglyphcandidateh(int index);
  int getglyphcandidateline(int index);
  int getglyphcandidatereadingorder(int index);
  double getglyphcandidateconfidence(int index);
    const std::vector<CxTextInspectGlyphCandidateSnapshot>& getglyphcandidates() const {
        return m_glyph_candidates;
    }
    const std::string& getrecognizedtext() const { return m_recognized_text; }
    const std::string& getdecodefailure() const { return m_decode_failure; }
    void PublishDisplayShapes(ICxShapeSink& sink,
                              const std::string& owner_ref) override;
private:
    int m_igridw;
    int m_igridh;
    void loadfontmodel();

    double m_dmatchthre;
    int m_icompareobject;

    std::StringList m_fontlist_l12;
    std::StringList m_filenamelist_l12;

    std::StringList m_fontlist_l36;
    std::StringList m_filenamelist_l36;

    std::StringList m_fontlist_l72;
    std::StringList m_filenamelist_l72;

    std::StringList m_resultstrlist;
    std::String m_resultstring;

    std::vector<int> m_l3resultlist;
    std::vector<int> m_l6resultlist;
	std::vector<int> m_l12resultlist;
	std::vector<int> m_l36resultlist;

    std::vector<levelnode> m_reslutnodelist;

    std::vector<levelnodes> m_reslutnodeslistgrid3x3;
    std::vector<levelnodes> m_reslutnodeslistgrid6x6;
    std::vector<levelnodes> m_reslutnodeslistgrid12x12;
    std::vector<levelnodes> m_reslutnodeslistgrid36x36;
    std::vector<levelnodes> m_reslutnodeslistgrid72x72;

    int m_resultnodesearchsum;

    int m_idebugrectsnum;
    int m_idebugfontnum;
    void AreasOCR(Image &image);
    void FontSplit(Image &image);
    void ExFontSplit(Image &image);
    void StringSplit(Image &image);

    Image* g_pbackimage;
    Image* g_pbackobjectimage;
    FindObject* g_pbackfindobject;
    int m_image_thre;
    int m_findobj_distance;
    int m_findobj_searchtype;
    int m_findobj_brow;
    int m_findobj_minarea;
    int m_findobj_maxarea;
    int m_findobj_minw;
    int m_findobj_maxw;
    int m_findobj_minh;
    int m_findobj_maxh;
    int m_findobj_bgedge;
    int m_findobj_bgmethod;

    int m_findobj_ioffsetx0;
    int m_findobj_ioffsetx1;
    int m_findobj_ioffsety0;
    int m_findobj_ioffsety1;

    int m_ix_or;
    int m_iy_or;
    int m_ix_and;
    int m_iy_and;

    int m_idraw_x;
    int m_idraw_y;

    int m_imodelobjectw;
    int m_imodelobjectb;

    int m_idraw_map;
    int m_ilevle;

    int m_imagetype;

    std::vector<Grid*> m_pgrids_l72;
    std::vector<Grid*> m_pgrids_l36;
    std::vector<Grid*> m_pgrids_l12;
    std::vector<Grid*> m_pgrids_l6;
    std::vector<Grid*> m_pgrids_l3;
	Grid *m_pimagegrid;

    double m_dvalue;
    double m_dmaxvalue;
    double m_iresultfont;
    double m_imaxnum;
    double m_iselmaxnum;
    levelnode m_selectnode;

    int m_igridwh;
    int m_exnum;

    std::String m_qocrstring;

    int m_layout_direction = 0;
    int m_line_overlap_percent = 50;
    std::vector<CxTextInspectGlyphCandidateSnapshot> m_glyph_candidates;
    std::string m_recognized_text;
    std::string m_decode_failure;

    void RebuildGlyphCandidatesFromFindObject();
    void RebuildGlyphCandidatesFromRects(const RectsShape& rects);
    void AssignGlyphReadingOrder();
    void FinalizeGlyphDecodingFromLegacyResults();
};








#endif
