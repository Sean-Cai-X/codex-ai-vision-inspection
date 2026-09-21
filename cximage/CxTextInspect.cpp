

#include "CxTextInspect.h"
#include "FindObject.h"
#include "imagemanager.h"
#include "ImageAnnotationLayer.h"
#include "PolylineShape.h"
#include "RectShape.h"

CxTextInspect::CxTextInspect()
    : FastMatch(), m_igridw(12), m_igridh(12), m_idebugrectsnum(-1),
      m_idebugfontnum(-1), m_image_thre(110), m_findobj_distance(6),
      m_findobj_searchtype(444), m_findobj_brow(1), m_findobj_minarea(0),
      m_findobj_maxarea(10000), m_findobj_minw(0), m_findobj_maxw(9999),
      m_findobj_minh(0), m_findobj_maxh(9999), m_findobj_bgedge(2),
      m_dmatchthre(0.5), m_icompareobject(3), m_ix_or(0), m_iy_or(0),
      m_ix_and(0), m_iy_and(0), m_idraw_x(-1), m_idraw_y(-1), m_idraw_map(0),
      m_igridwh(12), m_exnum(999), m_findobj_ioffsetx0(0),
      m_findobj_ioffsetx1(0), m_findobj_ioffsety0(0), m_findobj_ioffsety1(0),
      m_imagetype(3), m_resultnodesearchsum(3) {
  setname("OCR");
  fastmatch::setfindnum(1);
  setmatchthre(3);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);

  clearmodel();
  loadfontmodel();
  levelmodel();
  setlevelstring();

  Shape::setrect(30, 30, 200, 200);
  int icurmodule = ImageManager::GetCurMode();
  g_pbackimage = ImageManager::GetBackImage(icurmodule);
  g_pbackobjectimage = ImageManager::GetBackObjectImage(icurmodule);
  g_pbackfindobject = ImageManager::Getbackfindobject(icurmodule);

  m_pimagegrid = new Grid;
  m_pimagegrid->setshow(8);
  m_pimagegrid->setgrid(5, 5, 64, 64, 5, 5);
}

CxTextInspect::~CxTextInspect() {
  delete m_pimagegrid;
  mapclear();
}

void CxTextInspect::setrect(int ix, int iy, int iw, int ih) {
  Shape::setrect(ix, iy, iw, ih);
}

void CxTextInspect::setlayoutdirection(int direction) {
  m_layout_direction = std::clamp(direction, 0, 2);
  AssignGlyphReadingOrder();
}

void CxTextInspect::setlineoverlappercent(int percent) {
  m_line_overlap_percent = std::clamp(percent, 1, 100);
  AssignGlyphReadingOrder();
}

int CxTextInspect::getglyphcandidatecount() {
  return static_cast<int>(m_glyph_candidates.size());
}

double CxTextInspect::getglyphcandidatex(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].bbox_px.x
             : 0.0;
}

double CxTextInspect::getglyphcandidatey(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].bbox_px.y
             : 0.0;
}

double CxTextInspect::getglyphcandidatew(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].bbox_px.width
             : 0.0;
}

double CxTextInspect::getglyphcandidateh(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].bbox_px.height
             : 0.0;
}

int CxTextInspect::getglyphcandidateline(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].line_index
             : -1;
}

int CxTextInspect::getglyphcandidatereadingorder(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].reading_order
             : -1;
}

double CxTextInspect::getglyphcandidateconfidence(int index) {
  return index >= 0 && index < getglyphcandidatecount()
             ? m_glyph_candidates[index].confidence
             : 0.0;
}

void CxTextInspect::RebuildGlyphCandidatesFromFindObject() {
  setglyphcandidatesfromobject(g_pbackfindobject);
}

void CxTextInspect::setglyphcandidatesfromobject(void *pfindobject) {
  m_glyph_candidates.clear();
  m_recognized_text.clear();
  m_decode_failure.clear();

  FindObject *source = static_cast<FindObject *>(pfindobject);
  if (source == nullptr) {
    getmatchrects().clear();
    m_decode_failure = "findobject_source_unavailable";
    return;
  }

  const std::vector<FindObjectMeasurementSnapshot> &measurements =
      source->getmeasurements();
  for (int index = 0; index < static_cast<int>(measurements.size()); ++index) {
    const FindObjectMeasurementSnapshot &measurement = measurements[index];
    if (measurement.status != "measured" || measurement.bbox_px.width <= 0 ||
        measurement.bbox_px.height <= 0)
      continue;

    CxTextInspectGlyphCandidateSnapshot candidate;
    candidate.source_object_index = index;
    candidate.bbox_px = measurement.bbox_px;
    candidate.centroid_px = measurement.centroid_px;
    candidate.orientation_deg = measurement.orientation_deg;
    candidate.projected_area = measurement.projected_area;
    candidate.aspect_ratio = measurement.bbox_px.height > 0
                                 ? measurement.bbox_px.width /
                                       measurement.bbox_px.height
                                 : 0.0;
    candidate.solidity = measurement.solidity;
    candidate.source_object_ref = measurement.object_ref;
    candidate.status = "segmented";
    m_glyph_candidates.push_back(std::move(candidate));
  }

  AssignGlyphReadingOrder();
  getmatchrects().clear();
  if (!m_glyph_candidates.empty()) {
    setmatchrectnum(static_cast<int>(m_glyph_candidates.size()));
    for (int index = 0; index < getglyphcandidatecount(); ++index) {
      const cv::Rect2d &box = m_glyph_candidates[index].bbox_px;
      setmultimatchrect(index, static_cast<int>(box.x),
                        static_cast<int>(box.y),
                        std::max(1, static_cast<int>(box.width + 0.5)),
                        std::max(1, static_cast<int>(box.height + 0.5)));
    }
  } else {
    m_decode_failure = "no_glyph_candidates";
  }
}

void CxTextInspect::setglyphcandidatesfromfastmatch(void *pfastmatch) {
  m_glyph_candidates.clear();
  m_recognized_text.clear();
  m_decode_failure.clear();

  FastMatch *source = static_cast<FastMatch *>(pfastmatch);
  if (source == nullptr) {
    getmatchrects().clear();
    m_decode_failure = "fastmatch_source_unavailable";
    return;
  }

  const std::vector<FastMatchPoseCandidateSnapshot> &poses =
      source->getposecandidates();
  if (!poses.empty()) {
    for (const FastMatchPoseCandidateSnapshot &pose : poses) {
      if (pose.bbox_px.width <= 0 || pose.bbox_px.height <= 0)
        continue;
      CxTextInspectGlyphCandidateSnapshot candidate;
      candidate.source_object_index = pose.candidate_index;
      candidate.bbox_px = pose.bbox_px;
      candidate.centroid_px = pose.center_px;
      candidate.orientation_deg = pose.angle_deg;
      candidate.projected_area =
          pose.bbox_px.width * pose.bbox_px.height;
      candidate.aspect_ratio = pose.bbox_px.height > 0
                                   ? pose.bbox_px.width / pose.bbox_px.height
                                   : 0.0;
      candidate.appearance_score = pose.appearance_score;
      candidate.geometry_score = pose.geometry_score;
      candidate.confidence = pose.combined_score;
      candidate.source_object_ref =
          !pose.observed_geometry_ref.empty()
              ? pose.observed_geometry_ref
              : "fastmatch_pose:" + std::to_string(pose.candidate_index);
      candidate.status = pose.status;
      m_glyph_candidates.push_back(std::move(candidate));
    }
    AssignGlyphReadingOrder();
  } else {
    RectsShape source_rects;
    const int result_count = source->getresultcandidatecount();
    for (int index = 0; index < result_count; ++index)
      source_rects.addrect(source->getresolvedresultrect(index));
    RebuildGlyphCandidatesFromRects(source_rects);
  }

  getmatchrects().clear();
  if (!m_glyph_candidates.empty()) {
    setmatchrectnum(static_cast<int>(m_glyph_candidates.size()));
    for (int index = 0; index < getglyphcandidatecount(); ++index) {
      const cv::Rect2d &box = m_glyph_candidates[index].bbox_px;
      setmultimatchrect(index, static_cast<int>(box.x),
                        static_cast<int>(box.y),
                        std::max(1, static_cast<int>(box.width + 0.5)),
                        std::max(1, static_cast<int>(box.height + 0.5)));
    }
  } else {
    m_decode_failure = "no_glyph_candidates";
  }
}

void CxTextInspect::RebuildGlyphCandidatesFromRects(const RectsShape &rects) {
  const std::vector<CxTextInspectGlyphCandidateSnapshot> previous_candidates =
      m_glyph_candidates;
  m_glyph_candidates.clear();
  m_recognized_text.clear();
  m_decode_failure.clear();
  for (int index = 0; index < rects.size(); ++index) {
    const gp_Rectangle &rect = rects.getrect(index);
    CxTextInspectGlyphCandidateSnapshot candidate;
    candidate.source_object_index = index;
    candidate.bbox_px =
        cv::Rect2d(rect.TopLeft().X(), rect.TopLeft().Y(), rect.Width(),
                   rect.Height());
    candidate.centroid_px =
        cv::Point2d(candidate.bbox_px.x + candidate.bbox_px.width * 0.5,
                    candidate.bbox_px.y + candidate.bbox_px.height * 0.5);
    candidate.projected_area =
        candidate.bbox_px.width * candidate.bbox_px.height;
    candidate.aspect_ratio = candidate.bbox_px.height > 0
                                 ? candidate.bbox_px.width /
                                       candidate.bbox_px.height
                                 : 0.0;
    candidate.source_object_ref =
        "fastmatch_rect:" + std::to_string(index);
    candidate.status = "segmented";

    if (index < static_cast<int>(previous_candidates.size())) {
      const CxTextInspectGlyphCandidateSnapshot &previous =
          previous_candidates[index];
      candidate.source_object_index = previous.source_object_index;
      candidate.orientation_deg = previous.orientation_deg;
      candidate.projected_area =
          previous.projected_area > 0.0 ? previous.projected_area
                                        : candidate.projected_area;
      candidate.aspect_ratio =
          previous.aspect_ratio > 0.0 ? previous.aspect_ratio
                                      : candidate.aspect_ratio;
      candidate.solidity = previous.solidity;
      candidate.appearance_score = previous.appearance_score;
      candidate.geometry_score = previous.geometry_score;
      candidate.confidence = previous.confidence;
      if (!previous.source_object_ref.empty())
        candidate.source_object_ref = previous.source_object_ref;
      if (!previous.status.empty())
        candidate.status = previous.status;
    }
    m_glyph_candidates.push_back(std::move(candidate));
  }
  AssignGlyphReadingOrder();
  if (m_glyph_candidates.empty())
    m_decode_failure = "no_glyph_candidates";
}

void CxTextInspect::AssignGlyphReadingOrder() {
  if (m_glyph_candidates.empty())
    return;

  bool vertical = m_layout_direction == 2;
  if (m_layout_direction == 0) {
    cv::Rect2d extent = m_glyph_candidates.front().bbox_px;
    double mean_width = 0.0;
    double mean_height = 0.0;
    for (const CxTextInspectGlyphCandidateSnapshot &candidate :
         m_glyph_candidates) {
      extent |= candidate.bbox_px;
      mean_width += candidate.bbox_px.width;
      mean_height += candidate.bbox_px.height;
    }
    mean_width /= m_glyph_candidates.size();
    mean_height /= m_glyph_candidates.size();
    vertical = extent.height > extent.width * 1.5 &&
               mean_height >= mean_width;
  }

  std::vector<int> pending(m_glyph_candidates.size());
  for (int i = 0; i < static_cast<int>(pending.size()); ++i)
    pending[i] = i;
  std::stable_sort(pending.begin(), pending.end(), [&](int lhs, int rhs) {
    const auto &a = m_glyph_candidates[lhs];
    const auto &b = m_glyph_candidates[rhs];
    if (vertical) {
      if (a.centroid_px.x != b.centroid_px.x)
        return a.centroid_px.x < b.centroid_px.x;
      return a.centroid_px.y < b.centroid_px.y;
    }
    if (a.centroid_px.y != b.centroid_px.y)
      return a.centroid_px.y < b.centroid_px.y;
    return a.centroid_px.x < b.centroid_px.x;
  });

  std::vector<std::vector<int>> lines;
  const double required_overlap =
      static_cast<double>(m_line_overlap_percent) / 100.0;
  for (int index : pending) {
    const cv::Rect2d &box = m_glyph_candidates[index].bbox_px;
    int selected_line = -1;
    double selected_overlap = -1.0;
    for (int line_index = 0; line_index < static_cast<int>(lines.size());
         ++line_index) {
      const cv::Rect2d &first_box =
          m_glyph_candidates[lines[line_index].front()].bbox_px;
      const double overlap =
          vertical
              ? std::max(0.0,
                         std::min(box.x + box.width,
                                  first_box.x + first_box.width) -
                             std::max(box.x, first_box.x))
              : std::max(0.0,
                         std::min(box.y + box.height,
                                  first_box.y + first_box.height) -
                             std::max(box.y, first_box.y));
      const double reference =
          vertical ? std::min(box.width, first_box.width)
                   : std::min(box.height, first_box.height);
      const double ratio = reference > 0.0 ? overlap / reference : 0.0;
      if (ratio >= required_overlap && ratio > selected_overlap) {
        selected_line = line_index;
        selected_overlap = ratio;
      }
    }
    if (selected_line < 0)
      lines.push_back({index});
    else
      lines[selected_line].push_back(index);
  }

  std::stable_sort(lines.begin(), lines.end(), [&](const std::vector<int> &a,
                                                    const std::vector<int> &b) {
    return vertical
               ? m_glyph_candidates[a.front()].centroid_px.x <
                     m_glyph_candidates[b.front()].centroid_px.x
               : m_glyph_candidates[a.front()].centroid_px.y <
                     m_glyph_candidates[b.front()].centroid_px.y;
  });

  int reading_order = 0;
  for (int line_index = 0; line_index < static_cast<int>(lines.size());
       ++line_index) {
    std::stable_sort(lines[line_index].begin(), lines[line_index].end(),
                     [&](int lhs, int rhs) {
                       return vertical
                                  ? m_glyph_candidates[lhs].centroid_px.y <
                                        m_glyph_candidates[rhs].centroid_px.y
                                  : m_glyph_candidates[lhs].centroid_px.x <
                                        m_glyph_candidates[rhs].centroid_px.x;
                     });
    for (int index : lines[line_index]) {
      m_glyph_candidates[index].line_index = line_index;
      m_glyph_candidates[index].reading_order = reading_order++;
    }
  }
}

void CxTextInspect::FinalizeGlyphDecodingFromLegacyResults() {
  m_recognized_text.clear();
  std::vector<const CxTextInspectGlyphCandidateSnapshot *> ordered;
  ordered.reserve(m_glyph_candidates.size());
  for (CxTextInspectGlyphCandidateSnapshot &candidate : m_glyph_candidates) {
    if (candidate.source_object_index >= 0 &&
        candidate.source_object_index < m_resultstrlist.size()) {
      candidate.label =
          m_resultstrlist[candidate.source_object_index].toStdString();
      candidate.status = "recognized";
    } else {
      candidate.status = "unrecognized";
    }
    ordered.push_back(&candidate);
  }
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto *lhs,
                                                       const auto *rhs) {
    return lhs->reading_order < rhs->reading_order;
  });
  for (const CxTextInspectGlyphCandidateSnapshot *candidate : ordered)
    m_recognized_text += candidate->label;
  if (m_recognized_text.empty())
    m_recognized_text = m_resultstring.toStdString();
  m_qocrstring = m_recognized_text;
  m_resultstring = m_recognized_text;
  m_decode_failure = m_recognized_text.empty()
                         ? "no_recognized_glyphs"
                         : std::string();
}

void CxTextInspect::PublishDisplayShapes(ICxShapeSink &sink,
                                   const std::string &owner_ref) {
  FastMatch::PublishDisplayShapes(sink, owner_ref);
  for (const CxTextInspectGlyphCandidateSnapshot &candidate : m_glyph_candidates) {
    const std::string suffix = std::to_string(candidate.reading_order);
    auto box = std::make_unique<RectShape>();
    box->setRect(candidate.bbox_px.x, candidate.bbox_px.y,
                 candidate.bbox_px.x + candidate.bbox_px.width,
                 candidate.bbox_px.y + candidate.bbox_px.height);
    sink.UpsertShape(owner_ref + ".glyph." + suffix, "CxTextInspect", owner_ref,
                     "glyph_candidate", candidate.status, false, true,
                     std::move(box));

    const double angle = candidate.orientation_deg * CV_PI / 180.0;
    const double half_length =
        0.5 * std::max(candidate.bbox_px.width, candidate.bbox_px.height);
    auto axis = std::make_unique<PolylineShape>();
    axis->addPoint(candidate.centroid_px.x - half_length * std::cos(angle),
                   candidate.centroid_px.y - half_length * std::sin(angle));
    axis->addPoint(candidate.centroid_px.x + half_length * std::cos(angle),
                   candidate.centroid_px.y + half_length * std::sin(angle));
    axis->close(false);
    sink.UpsertShape(owner_ref + ".glyph_axis." + suffix, "CxTextInspect",
                     owner_ref, "glyph_candidate", "main_axis", false, true,
                     std::move(axis));
  }
}

std::String CxTextInspect::char2string(std::String pchar) {
  if (std::String(pchar) == " ") {
    return std::String("space");
  } else if (std::String(pchar) == "!") {
    return std::String("exclam");
  } else if (std::String(pchar) == "\"") {
    return std::String("quotedbl");
  } else if (std::String(pchar) == "#") {
    return std::String("numbersign");
  } else if (std::String(pchar) == "$") {
    return std::String("dollar");
  } else if (std::String(pchar) == "%") {
    return std::String("percent");
  } else if (std::String(pchar) == "&") {
    return std::String("ampersand");
  } else if (std::String(pchar) == "'") {
    return std::String("apostrophe");
  } else if (std::String(pchar) == "'") {
    return std::String("quoteright");
  } else if (std::String(pchar) == "(") {
    return std::String("parenleft");
  } else if (std::String(pchar) == ")") {
    return std::String("parenright");
  } else if (std::String(pchar) == "*") {
    return std::String("asterisk");
  } else if (std::String(pchar) == "+") {
    return std::String("plus");
  } else if (std::String(pchar) == ",") {
    return std::String("comma");
  } else if (std::String(pchar) == "-") {
    return std::String("minus");
  } else if (std::String(pchar) == ".") {
    return std::String("period");
  } else if (std::String(pchar) == "/") {
    return std::String("slash");
  } else if (std::String(pchar) == "0") {
    return std::String("0");
  } else if (std::String(pchar) == "1") {
    return std::String("1");
  } else if (std::String(pchar) == "2") {
    return std::String("2");
  } else if (std::String(pchar) == "3") {
    return std::String("3");
  } else if (std::String(pchar) == "4") {
    return std::String("4");
  } else if (std::String(pchar) == "5") {
    return std::String("5");
  } else if (std::String(pchar) == "6") {
    return std::String("6");
  } else if (std::String(pchar) == "7") {
    return std::String("7");
  } else if (std::String(pchar) == "8") {
    return std::String("8");
  } else if (std::String(pchar) == "9") {
    return std::String("9");
  } else if (std::String(pchar) == ":") {
    return std::String("colon");
  } else if (std::String(pchar) == ";") {
    return std::String("semicolon");
  } else if (std::String(pchar) == "<") {
    return std::String("less");
  } else if (std::String(pchar) == "=") {
    return std::String("equal");
  } else if (std::String(pchar) == ">") {
    return std::String("greater");
  } else if (std::String(pchar) == "?") {
    return std::String("question");
  } else if (std::String(pchar) == "@") {
    return std::String("at");
  } else if (std::String(pchar) == "A") {
    return std::String("A");
  } else if (std::String(pchar) == "B") {
    return std::String("B");
  } else if (std::String(pchar) == "C") {
    return std::String("C");
  } else if (std::String(pchar) == "D") {
    return std::String("D");
  } else if (std::String(pchar) == "E") {
    return std::String("E");
  } else if (std::String(pchar) == "F") {
    return std::String("F");
  } else if (std::String(pchar) == "G") {
    return std::String("G");
  } else if (std::String(pchar) == "H") {
    return std::String("H");
  } else if (std::String(pchar) == "I") {
    return std::String("I");
  } else if (std::String(pchar) == "J") {
    return std::String("J");
  } else if (std::String(pchar) == "K") {
    return std::String("K");
  } else if (std::String(pchar) == "L") {
    return std::String("L");
  } else if (std::String(pchar) == "M") {
    return std::String("M");
  } else if (std::String(pchar) == "N") {
    return std::String("N");
  } else if (std::String(pchar) == "O") {
    return std::String("O");
  } else if (std::String(pchar) == "P") {
    return std::String("P");
  } else if (std::String(pchar) == "Q") {
    return std::String("Q");
  } else if (std::String(pchar) == "R") {
    return std::String("R");
  } else if (std::String(pchar) == "S") {
    return std::String("S");
  } else if (std::String(pchar) == "T") {
    return std::String("T");
  } else if (std::String(pchar) == "U") {
    return std::String("U");
  } else if (std::String(pchar) == "V") {
    return std::String("V");
  } else if (std::String(pchar) == "W") {
    return std::String("W");
  } else if (std::String(pchar) == "X") {
    return std::String("X");
  } else if (std::String(pchar) == "Y") {
    return std::String("Y");
  } else if (std::String(pchar) == "Z") {
    return std::String("Z");
  } else if (std::String(pchar) == "[") {
    return std::String("bracketleft");
  } else if (std::String(pchar) == "\\") {
    return std::String("backslash");
  } else if (std::String(pchar) == "]") {
    return std::String("bracketright");
  } else if (std::String(pchar) == "^") {
    return std::String("asciicircum");
  } else if (std::String(pchar) == "_") {
    return std::String("underscore");
  } else if (std::String(pchar) == "`") {
    return std::String("grave");
  } else if (std::String(pchar) == "`") {
    return std::String("quoteleft");
  } else if (std::String(pchar) == "a") {
    return std::String("a");
  } else if (std::String(pchar) == "b") {
    return std::String("b");
  } else if (std::String(pchar) == "c") {
    return std::String("c");
  } else if (std::String(pchar) == "d") {
    return std::String("d");
  } else if (std::String(pchar) == "e") {
    return std::String("e");
  } else if (std::String(pchar) == "f") {
    return std::String("f");
  } else if (std::String(pchar) == "g") {
    return std::String("g");
  } else if (std::String(pchar) == "h") {
    return std::String("h");
  } else if (std::String(pchar) == "i") {
    return std::String("i");
  } else if (std::String(pchar) == "j") {
    return std::String("j");
  } else if (std::String(pchar) == "k") {
    return std::String("k");
  } else if (std::String(pchar) == "l") {
    return std::String("l");
  } else if (std::String(pchar) == "m") {
    return std::String("m");
  } else if (std::String(pchar) == "n") {
    return std::String("n");
  } else if (std::String(pchar) == "o") {
    return std::String("o");
  } else if (std::String(pchar) == "p") {
    return std::String("p");
  } else if (std::String(pchar) == "q") {
    return std::String("q");
  } else if (std::String(pchar) == "r") {
    return std::String("r");
  } else if (std::String(pchar) == "s") {
    return std::String("s");
  } else if (std::String(pchar) == "t") {
    return std::String("t");
  } else if (std::String(pchar) == "u") {
    return std::String("u");
  } else if (std::String(pchar) == "v") {
    return std::String("v");
  } else if (std::String(pchar) == "w") {
    return std::String("w");
  } else if (std::String(pchar) == "x") {
    return std::String("x");
  } else if (std::String(pchar) == "y") {
    return std::String("y");
  } else if (std::String(pchar) == "z") {
    return std::String("z");
  } else if (std::String(pchar) == "{") {
    return std::String("braceleft");
  } else if (std::String(pchar) == "|") {
    return std::String("bar");
  } else if (std::String(pchar) == "}") {
    return std::String("braceright");
  } else if (std::String(pchar) == "~") {
    return std::String("asciitilde");
  } else
    return std::String(pchar);
}
std::String CxTextInspect::string2char(const std::String &strchar) {
  if (strchar == std::String("space")) {
    return std::String(" ");
  } else if (strchar == std::String("exclam")) {
    return std::String("!");
  } else if (strchar == std::String("quotedbl")) {
    return std::String("\"");
  } else if (strchar == std::String("numbersign")) {
    return std::String("#");
  } else if (strchar == std::String("dollar")) {
    return std::String("$");
  } else if (strchar == std::String("percent")) {
    return std::String("%");
  } else if (strchar == std::String("ampersand")) {
    return std::String("&");
  } else if (strchar == std::String("apostrophe")) {
    return std::String("'");
  } else if (strchar == std::String("quoteright")) {
    return std::String("'");
  } else if (strchar == std::String("parenleft")) {
    return std::String("(");
  } else if (strchar == std::String("parenright")) {
    return std::String(")");
  } else if (strchar == std::String("asterisk")) {
    return std::String("*");
  } else if (strchar == std::String("plus")) {
    return std::String("+");
  } else if (strchar == std::String("comma")) {
    return std::String(",");
  } else if (strchar == std::String("minus")) {
    return std::String("-");
  } else if (strchar == std::String("period")) {
    return std::String(".");
  } else if (strchar == std::String("slash")) {
    return std::String("/");
  } else if (strchar == std::String("0")) {
    return std::String("0");
  } else if (strchar == std::String("1")) {
    return std::String("1");
  } else if (strchar == std::String("2")) {
    return std::String("2");
  } else if (strchar == std::String("3")) {
    return std::String("3");
  } else if (strchar == std::String("4")) {
    return std::String("4");
  } else if (strchar == std::String("5")) {
    return std::String("5");
  } else if (strchar == std::String("6")) {
    return std::String("6");
  } else if (strchar == std::String("7")) {
    return std::String("7");
  } else if (strchar == std::String("8")) {
    return std::String("8");
  } else if (strchar == std::String("9")) {
    return std::String("9");
  } else if (strchar == std::String("colon")) {
    return std::String(":");
  } else if (strchar == std::String("semicolon")) {
    return std::String(";");
  } else if (strchar == std::String("less")) {
    return std::String("<");
  } else if (strchar == std::String("equal")) {
    return std::String("=");
  } else if (strchar == std::String("greater")) {
    return std::String(">");
  } else if (strchar == std::String("question")) {
    return std::String("?");
  } else if (strchar == std::String("at")) {
    return std::String("@");
  } else if (strchar == std::String("A")) {
    return std::String("A");
  } else if (strchar == std::String("B")) {
    return std::String("B");
  } else if (strchar == std::String("C")) {
    return std::String("C");
  } else if (strchar == std::String("D")) {
    return std::String("D");
  } else if (strchar == std::String("E")) {
    return std::String("E");
  } else if (strchar == std::String("F")) {
    return std::String("F");
  } else if (strchar == std::String("G")) {
    return std::String("G");
  } else if (strchar == std::String("H")) {
    return std::String("H");
  } else if (strchar == std::String("I")) {
    return std::String("I");
  } else if (strchar == std::String("J")) {
    return std::String("J");
  } else if (strchar == std::String("K")) {
    return std::String("K");
  } else if (strchar == std::String("L")) {
    return std::String("L");
  } else if (strchar == std::String("M")) {
    return std::String("M");
  } else if (strchar == std::String("N")) {
    return std::String("N");
  } else if (strchar == std::String("O")) {
    return std::String("O");
  } else if (strchar == std::String("P")) {
    return std::String("P");
  } else if (strchar == std::String("Q")) {
    return std::String("Q");
  } else if (strchar == std::String("R")) {
    return std::String("R");
  } else if (strchar == std::String("S")) {
    return std::String("S");
  } else if (strchar == std::String("T")) {
    return std::String("T");
  } else if (strchar == std::String("U")) {
    return std::String("U");
  } else if (strchar == std::String("V")) {
    return std::String("V");
  } else if (strchar == std::String("W")) {
    return std::String("W");
  } else if (strchar == std::String("X")) {
    return std::String("X");
  } else if (strchar == std::String("Y")) {
    return std::String("Y");
  } else if (strchar == std::String("Z")) {
    return std::String("Z");
  } else if (strchar == std::String("bracketleft")) {
    return std::String("[");
  } else if (strchar == std::String("backslash")) {
    return std::String("\\");
  } else if (strchar == std::String("bracketright")) {
    return std::String("]");
  } else if (strchar == std::String("asciicircum")) {
    return std::String("^");
  } else if (strchar == std::String("underscore")) {
    return std::String("_");
  } else if (strchar == std::String("grave")) {
    return std::String("`");
  } else if (strchar == std::String("quoteleft")) {
    return std::String("`");
  } else if (strchar == std::String("a")) {
    return std::String("a");
  } else if (strchar == std::String("b")) {
    return std::String("b");
  } else if (strchar == std::String("c")) {
    return std::String("c");
  } else if (strchar == std::String("d")) {
    return std::String("d");
  } else if (strchar == std::String("e")) {
    return std::String("e");
  } else if (strchar == std::String("f")) {
    return std::String("f");
  } else if (strchar == std::String("g")) {
    return std::String("g");
  } else if (strchar == std::String("h")) {
    return std::String("h");
  } else if (strchar == std::String("i")) {
    return std::String("i");
  } else if (strchar == std::String("j")) {
    return std::String("j");
  } else if (strchar == std::String("k")) {
    return std::String("k");
  } else if (strchar == std::String("l")) {
    return std::String("l");
  } else if (strchar == std::String("m")) {
    return std::String("m");
  } else if (strchar == std::String("n")) {
    return std::String("n");
  } else if (strchar == std::String("o")) {
    return std::String("o");
  } else if (strchar == std::String("p")) {
    return std::String("p");
  } else if (strchar == std::String("q")) {
    return std::String("q");
  } else if (strchar == std::String("r")) {
    return std::String("r");
  } else if (strchar == std::String("s")) {
    return std::String("s");
  } else if (strchar == std::String("t")) {
    return std::String("t");
  } else if (strchar == std::String("u")) {
    return std::String("u");
  } else if (strchar == std::String("v")) {
    return std::String("v");
  } else if (strchar == std::String("w")) {
    return std::String("w");
  } else if (strchar == std::String("x")) {
    return std::String("x");
  } else if (strchar == std::String("y")) {
    return std::String("y");
  } else if (strchar == std::String("z")) {
    return std::String("z");
  } else if (strchar == std::String("braceleft")) {
    return std::String("{");
  } else if (strchar == std::String("bar")) {
    return std::String("|");
  } else if (strchar == std::String("braceright")) {
    return std::String("}");
  } else if (strchar == std::String("asciitilde")) {
    return std::String("~");
  } else
    return std::String(strchar);
}
void CxTextInspect::mapclear() {
  int isize = m_pgrids_l72.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l72[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l72.clear();
  isize = m_pgrids_l36.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l36[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l36.clear();

  isize = m_pgrids_l12.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l12[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l12.clear();

  isize = m_pgrids_l6.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l6[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l6.clear();

  isize = m_pgrids_l3.size();
  for (int i = 0; i < isize; i++) {
    Grid *pgrid = m_pgrids_l3[i];
    if (pgrid)
      delete pgrid;
  }
  m_pgrids_l3.clear();
}
void CxTextInspect::mapgrid() {
  int ilevel = m_ilevle;
  mapclear();

  int ihnumx = 0;
  int ihnumy = 0;
  int il4size = imagefastmodelsize(4);
  for (int i = 0; i < il4size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(2050 + ihnumy * 72, 72 * ihnumx + 30, 72, 72);
    if (50 * ihnumx + 50 > 2000) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 72, 72, 2, 2);
    pgrid->SetModelWH(72, 72);

    SelectModel(4, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l72.push_back(pgrid);
  }

  ihnumx = 0;
  ihnumy = 0;
  int il3size = imagefastmodelsize(3);
  for (int i = 0; i < il3size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(1650 + ihnumy * 72, 72 * ihnumx + 30, 72, 72);
    if (50 * ihnumx + 50 > 3000) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 36, 36, 2, 2);
    pgrid->SetModelWH(36, 36);

    SelectModel(3, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l36.push_back(pgrid);
  }

  ihnumx = 0;
  ihnumy = 0;
  int il2size = imagefastmodelsize(2);
  for (int i = 0; i < il2size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(900 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);

    if (50 * ihnumx + 50 > 1500) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(2, 2, 12, 12, 2, 2);
    pgrid->SetModelWH(12, 12);

    SelectModel(2, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l12.push_back(pgrid);
  }
  ihnumy = 0;
  ihnumx = 0;
  int il1size = imagefastmodelsize(1);
  for (int i = 0; i < il1size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(120 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);
    if (30 * ihnumx + 30 > 700) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(4, 4, 6, 6, 4, 4);
    pgrid->SetModelWH(6, 6);

    SelectModel(1, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l6.push_back(pgrid);
  }
  int il0size = imagefastmodelsize(0);
  ihnumy = 0;
  ihnumx = 0;
  for (int i = 0; i < il0size; i++) {
    Grid *pgrid = new Grid;
    pgrid->setshow(8);
    pgrid->setroi(20 + ihnumy * 30, 30 * ihnumx + 30, 20, 20);
    if (30 * ihnumx + 30 > 700) {
      ihnumx = 0;
      ihnumy = ihnumy + 1;
    } else {
      ihnumx = ihnumx + 1;
    }
    pgrid->setgrid(8, 8, 3, 3, 8, 8);
    pgrid->SetModelWH(3, 3);

    SelectModel(0, i);
    pgrid->SetFastModel(*getcurimagemodel());
    m_pgrids_l3.push_back(pgrid);
  }

  m_ilevle = ilevel;
}
std::String CxTextInspect::filenametoOCRstring(std::String strbase) {
  std::String strkey;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::StringList strsplit = strbase.split('_');
  std::String split0, split1, split2, split3, split4;
  if (strsplit.size() > 0)
    split0 = strsplit[0];
  if (strsplit.size() > 1)
    split1 = strsplit[1];
  if (strsplit.size() > 2)
    split2 = strsplit[2];
  if (strsplit.size() > 3)
    split3 = strsplit[3];
  if (strsplit.size() > 4)
    split4 = strsplit[4];
  std::String strshow;
  if (split0 == "") {
    if (split1 == "") {
      strkey = std::String("_");
    } else if (split1 == "1") {
      strkey = std::String("\/");
    } else if (split1 == "2") {
      strkey = std::String("{");
    } else if (split1 == "3") {
      strkey = std::String("}");
    } else if (split1 == "4") {
      strkey = std::String("\\");
    } else if (split1 == "5") {
      strkey = std::String(">");
    } else if (split1 == "6") {
      strkey = std::String("<");
    } else if (split1 == "7") {
      strkey = std::String("@");
    } else if (split1 == "8") {
      strkey = std::String("!");
    } else if (split1 == "9") {
      strkey = std::String(":");
    } else if (split1 == "10") {
      strkey = std::String("\"");
    } else if (split1 == "11") {
      strkey = std::String("\'");
    } else if (split1 == "12") {
      strkey = std::String("~");
    } else if (split1 == "13") {
      strkey = std::String("#");
    } else if (split1 == "14") {
      strkey = std::String("$");
    } else if (split1 == "15") {
      strkey = std::String("%");
    } else if (split1 == "16") {
      strkey = std::String("^");
    } else if (split1 == "17") {
      strkey = std::String("&");
    } else if (split1 == "18") {
      strkey = std::String("*");
    } else if (split1 == "19") {
      strkey = std::String("(");
    } else if (split1 == "20") {
      strkey = std::String(")");
    } else if (split1 == "21") {
      strkey = std::String("-");
    } else if (split1 == "22") {
      strkey = std::String("+");
    } else if (split1 == "23") {
      strkey = std::String("=");
    } else if (split1 == "24") {
      strkey = std::String("?");
    } else if (split1 == "25") {
      strkey = std::String(".");
    } else {
      strkey = string2char(split1);
    }
    strshow = strkey + split2;
  } else {
    if (strsplit.size() <= 2) {
      strkey = string2char(split0);
      strshow = strkey + split1;
    } else if (strsplit.size() == 3) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strshow = strkey + strkey2 + split2;
    } else if (strsplit.size() == 4) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strkey3 = string2char(split2);
      strshow = strkey + strkey2 + strkey3 + split3;
    } else if (strsplit.size() >= 5) {
      strkey = string2char(split0);
      strkey2 = string2char(split1);
      strkey3 = string2char(split2);
      strkey4 = string2char(split3);
      strshow = strkey + strkey2 + strkey3 + strkey4 + split4;
    }
  }
  return strshow;
}
void CxTextInspect::loadfontmodel() {
  FastMatch::clearmodels_l12();
  FastMatch::clearmodels_l36();
  FastMatch::clearmodels_l72();
  FastMatch::imagemodesclear_l12();
  FastMatch::imagemodesclear_l36();
  FastMatch::imagemodesclear_l72();
  m_filenamelist_l12.clear();
  m_fontlist_l12.clear();
  m_filenamelist_l36.clear();
  m_fontlist_l36.clear();
  m_filenamelist_l72.clear();
  m_fontlist_l72.clear();
  const auto load = [this](const std::filesystem::path &directory, auto add,
                           CxOcrStringList &names, CxOcrStringList &labels) {
    std::error_code ec;
    std::vector<std::filesystem::path> files;
    if (std::filesystem::is_directory(directory, ec)) {
      for (const auto &entry :
           std::filesystem::directory_iterator(directory, ec)) {
        if (ec)
          break;
        std::error_code typeError;
        if (entry.is_regular_file(typeError) &&
            entry.path().extension() == ".imp")
          files.push_back(entry.path());
      }
    }
    std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) {
      return a.filename().string() < b.filename().string();
    });
    for (const auto &file : files) {
      const std::string path = file.string();
      add(path.c_str());
      const CxOcrString stem(file.stem().string());
      names.append(stem);
      labels.append(filenametoOCRstring(stem));
    }
  };
  load(
      "model/12x12",
      [this](const char *p) { FastMatch::addimagemodels_l12(p); },
      m_filenamelist_l12, m_fontlist_l12);
  load(
      "model/36x36",
      [this](const char *p) { FastMatch::addimagemodels_l36(p); },
      m_filenamelist_l36, m_fontlist_l36);
  load(
      "model/72x72",
      [this](const char *p) { FastMatch::addimagemodels_l72(p); },
      m_filenamelist_l72, m_fontlist_l72);
}
void CxTextInspect::setimagetype(int itype) { m_imagetype = itype; }
void CxTextInspect::setshow(int ishow) { fastmatch::setshow(ishow); }
void CxTextInspect::setshowpos(int ix, int iy) {
  m_idraw_x = ix;
  m_idraw_y = iy;
}
void CxTextInspect::drawshape() {
  // Qt painter rendering is deliberately deferred. The existing ImGui/Shape
  // path retains the base FastMatch geometry without introducing Qt runtime
  // dependencies into the algorithm module.
  FastMatch::drawshape();
}
void CxTextInspect::setshowmap(int ishow, int ilevel, int idebugfont) {
  m_ilevle = ilevel;
  m_idraw_map = ishow;
  m_idebugfontnum = idebugfont;
}
void CxTextInspect::setocrareasnum(int inum) { fastmatch::setmatchrectnum(inum); }
void CxTextInspect::setocrareas(int inum, int ix, int iy, int iw, int ih) {
  fastmatch::setmultimatchrect(inum, ix, iy, iw, ih);
}
void CxTextInspect::setocrthre(int ithre) { fastmatch::setmatchthre(ithre); }
void CxTextInspect::setb2w(int ib2w) { fastmatch::setb2w(ib2w); }

void CxTextInspect::setspecshow(int ishow) { fastmatch::setspecshow(ishow); }
void CxTextInspect::stringsplit(void *pimage) {
  ImageBase *pgetimage = static_cast<ImageBase *>(pimage);
  if (pgetimage == nullptr || g_pbackimage == nullptr ||
      g_pbackfindobject == nullptr) {
    m_decode_failure = "ocr_input_or_findobject_unavailable";
    return;
  }
  StringSplit(*pgetimage);
}
void CxTextInspect::fontsplit(void *pimage) {
  ImageBase *pgetimage = static_cast<ImageBase *>(pimage);
  if (pgetimage == nullptr || g_pbackimage == nullptr ||
      g_pbackfindobject == nullptr) {
    m_decode_failure = "ocr_input_or_findobject_unavailable";
    return;
  }
  FontSplit(*pgetimage);
}
void CxTextInspect::exfontsplit(void *pimage) {
  ImageBase *pgetimage = static_cast<ImageBase *>(pimage);
  if (pgetimage == nullptr || g_pbackimage == nullptr ||
      g_pbackfindobject == nullptr) {
    m_decode_failure = "ocr_input_or_findobject_unavailable";
    return;
  }
  ExFontSplit(*pgetimage);
}

void CxTextInspect::areasocr(void *pimage) {
  ImageBase *pgetimage = static_cast<ImageBase *>(pimage);
  if (pgetimage == nullptr) {
    m_decode_failure = "ocr_input_unavailable";
    return;
  }
  AreasOCR(*pgetimage);
}
void CxTextInspect::setdebug(int idebugrect, int idebugfont) {
  m_idebugrectsnum = idebugrect;
  m_idebugfontnum = idebugfont;
}
void CxTextInspect::setsplitimage(int ithre, int ixor, int iyor, int ixand,
                            int iyand) {
  m_image_thre = ithre;
  m_ix_or = ixor;
  m_iy_or = iyor;
  m_ix_and = ixand;
  m_iy_and = iyand;
}
void CxTextInspect::setsplitobjectbg(int ibgedge, int ibgmethod) {
  m_findobj_bgedge = ibgedge;
  m_findobj_bgmethod = ibgmethod;
}
void CxTextInspect::setsplitobject(int idistance, int isearchtype, int ibrow,
                             int iminarea, int ibgedge) {
  m_findobj_distance = idistance;
  m_findobj_searchtype = isearchtype;
  m_findobj_brow = ibrow;
  m_findobj_minarea = iminarea;
  m_findobj_bgedge = ibgedge;
}
void CxTextInspect::setsplitobjectoffset(int ix0, int ix1, int iy0, int iy1) {
  m_findobj_ioffsetx0 = ix0;
  m_findobj_ioffsetx1 = ix1;
  m_findobj_ioffsety0 = iy0;
  m_findobj_ioffsety1 = iy1;
}

void CxTextInspect::setsplitgrid(int iw, int ih, int igridnum) {
  g_pbackfindobject->setobjectgrid(iw, ih, igridnum);
}
void CxTextInspect::StringSplit(ImageBase &image) {
  gp_Rectangle arect = Shape::rect();

  const gp_Pnt topLeft = arect.TopLeft();
  const int roiX = static_cast<int>(topLeft.X());
  const int roiY = static_cast<int>(topLeft.Y());
  const int roiWidth = static_cast<int>(arect.Width());
  const int roiHeight = static_cast<int>(arect.Height());
  g_pbackimage->setroi(roiX, roiY, roiWidth, roiHeight);
  image.setroi(roiX, roiY, roiWidth, roiHeight);
  image.SetMode(3);
  image.ROItoROI(*g_pbackimage);

  // The legacy shift-and-combine preprocessing has no current Image API.
  // Zero offsets are the historic default; non-zero offset support is deferred
  // until its OpenCV-equivalent operation is reintroduced with tests.
  g_pbackimage->threshold(m_image_thre, 255.0);

  g_pbackfindobject->setrect(roiX, roiY, roiWidth, roiHeight);

  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);

  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);

  g_pbackfindobject->measure(g_pbackimage);
  RebuildGlyphCandidatesFromFindObject();

  int ix0 = g_pbackfindobject->getresultx(0);
  int iy0 = g_pbackfindobject->getresulty(0);

  int ih0 = g_pbackfindobject->getresulth(0);
  int iw0 = g_pbackfindobject->getresultw(0);

  ix0 = ix0 - 2 >= 0 ? ix0 - 2 : 0;
  iy0 = iy0 - 2 >= 0 ? iy0 - 2 : 0;

  setrect(ix0, iy0, iw0 + 4, ih0 + 4);
}

void CxTextInspect::FontSplit(ImageBase &image) {
  const gp_Rectangle rect = Shape::rect();
  const gp_Pnt topLeft = rect.TopLeft();
  const int x = static_cast<int>(topLeft.X());
  const int y = static_cast<int>(topLeft.Y());
  const int width = static_cast<int>(rect.Width());
  const int height = static_cast<int>(rect.Height());
  g_pbackimage->setroi(x, y, width, height);
  image.setroi(x, y, width, height);
  image.ROItoROI(*g_pbackimage);
  if (m_image_thre != 0)
    g_pbackimage->threshold(m_image_thre, 255.0);
  g_pbackfindobject->setrect(x, y, width, height);
  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);
  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);
  g_pbackfindobject->setoffset(m_findobj_ioffsetx0, m_findobj_ioffsetx1,
                               m_findobj_ioffsety0, m_findobj_ioffsety1);
  g_pbackfindobject->measure(g_pbackimage);
  g_pbackfindobject->setbackground(m_findobj_bgedge, m_findobj_bgmethod);
  g_pbackfindobject->resultsrectfilter();
  g_pbackfindobject->objectgrid(&image);
  RebuildGlyphCandidatesFromFindObject();
}

void CxTextInspect::ExFontSplit(ImageBase &image) {
  const gp_Rectangle rect = Shape::rect();
  const gp_Pnt topLeft = rect.TopLeft();
  const int x = static_cast<int>(topLeft.X());
  const int y = static_cast<int>(topLeft.Y());
  const int width = static_cast<int>(rect.Width());
  const int height = static_cast<int>(rect.Height());
  g_pbackimage->setroi(x, y, width, height);
  image.setroi(x, y, width, height);
  image.ROItoROI(*g_pbackimage);
  if (m_image_thre != 0)
    g_pbackimage->threshold(m_image_thre, 255.0);
  g_pbackfindobject->setrect(x, y, width, height);
  g_pbackfindobject->setdistance(m_findobj_distance);
  g_pbackfindobject->setsearchtype(m_findobj_searchtype);
  g_pbackfindobject->setbrow(m_findobj_brow);
  g_pbackfindobject->setminmaxarea(m_findobj_minarea, m_findobj_maxarea);
  g_pbackfindobject->setminmaxwh(m_findobj_minw, m_findobj_maxw, m_findobj_minh,
                                 m_findobj_maxh);
  g_pbackfindobject->setoffset(m_findobj_ioffsetx0, m_findobj_ioffsetx1,
                               m_findobj_ioffsety0, m_findobj_ioffsety1);
  g_pbackfindobject->measure(g_pbackimage);
  g_pbackfindobject->setbackground(m_findobj_bgedge, m_findobj_bgmethod);
  g_pbackfindobject->objectgrid(&image);
  RebuildGlyphCandidatesFromFindObject();
}
void CxTextInspect::AreasOCR(ImageBase &image) {
  RectsShape arects = fastmatch::getmatchrects();
  RebuildGlyphCandidatesFromRects(arects);

  m_resultstrlist.clear();
  m_resultstring.clear();

  int iareasnum = arects.size();
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = m_fontlist_l12.size();
      const gp_Rectangle &areaRect = arects.getrect(ia);
      int ix = static_cast<int>(areaRect.TopLeft().X());
      int iy = static_cast<int>(areaRect.TopLeft().Y());
      int iw = static_cast<int>(areaRect.Width());
      int ih = static_cast<int>(areaRect.Height());
      fastmatch::setmatchrect(ix, iy, iw, ih);
      double dmaxvalue = 0;
      int iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          fastmatch::match(&image);
          double dvalue = fastmatch::getmaxresult();
          double dimagevalue = 0;
          if (dvalue > 0.5) {
            fastmatch::imagematch(-1, 1);
            dimagevalue = fastmatch::getimagemodelreslut();
            if (dimagevalue > dmaxvalue) {
              dmaxvalue = dimagevalue;
              iresultfont = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }
      m_resultstrlist.push_back(m_fontlist_l12[iresultfont]);
      m_resultstring.append(m_fontlist_l12[iresultfont]);

      for (CxTextInspectGlyphCandidateSnapshot &candidate : m_glyph_candidates) {
        if (candidate.source_object_index != ia)
          continue;
        candidate.label = m_fontlist_l12[iresultfont].toStdString();
        candidate.appearance_score = std::clamp(dmaxvalue / 100.0, 0.0, 1.0);
        candidate.confidence = candidate.appearance_score;
        candidate.status = candidate.confidence >= m_dmatchthre
                               ? "recognized"
                               : "low_confidence";
        break;
      }
    }
  }
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
}
void CxTextInspect::imagemodelshow() { fastmatch::imagemodelshow(); }

void CxTextInspect::imagematchshow() { fastmatch::imagematchshow(); }
void CxTextInspect::imagecompareshow(int itype) {
  fastmatch::imagemodelcompareshow(itype);
}
void CxTextInspect::autolearn(const char *pfilename) {
  const gp_Rectangle arect = g_pbackfindobject->getgrid(m_idebugrectsnum);
  int ix = static_cast<int>(arect.TopLeft().X());
  int iy = static_cast<int>(arect.TopLeft().Y());
  int iw = static_cast<int>(arect.Width());
  int ih = static_cast<int>(arect.Height());

  const gp_Rectangle arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);
  fastmatch::imagelearn(-1, 1);
  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;
  if (0) {
    if (strkey == std::String("\/")) {
      strbase = std::String("_1");
    } else if (strkey == std::String("{")) {
      strbase = std::String("_2");
    } else if (strkey == std::String("}")) {
      strbase = std::String("_3");
    } else if (strkey == std::String("\\")) {
      strbase = std::String("_4");
    } else if (strkey == std::String(">")) {
      strbase = std::String("_5");
    } else if (strkey == std::String("<")) {
      strbase = std::String("_6");
    } else if (strkey == std::String("@")) {
      strbase = std::String("_7");
    } else if (strkey == std::String("!")) {
      strbase = std::String("_8");
    } else if (strkey == std::String(":")) {
      strbase = std::String("_9");
    } else if (strkey == std::String("\"")) {
      strbase = std::String("_10");
    } else if (strkey == std::String("\'")) {
      strbase = std::String("_11");
    } else if (strkey == std::String("~")) {
      strbase = std::String("_12");
    } else if (strkey == std::String("#")) {
      strbase = std::String("_13");
    } else if (strkey == std::String("$")) {
      strbase = std::String("_14");
    } else if (strkey == std::String("%")) {
      strbase = std::String("_15");
    } else if (strkey == std::String("^")) {
      strbase = std::String("_16");
    } else if (strkey == std::String("&")) {
      strbase = std::String("_17");
    } else if (strkey == std::String("*")) {
      strbase = std::String("_18");
    } else if (strkey == std::String("(")) {
      strbase = std::String("_19");
    } else if (strkey == std::String(")")) {
      strbase = std::String("_20");
    } else if (strkey == std::String("-")) {
      strbase = std::String("_21");
    } else if (strkey == std::String("+")) {
      strbase = std::String("_22");
    } else if (strkey == std::String("=")) {
      strbase = std::String("_23");
    } else if (strkey == std::String("?")) {
      strbase = std::String("_24");
    } else if (strkey == std::String(".")) {
      strbase = std::String("_25");
    } else {
      strbase = strkey;
    }
  }
  std::String qstrsavepat = std::String("./model/12x12/") + strbase +
                            std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/12x12/") + strbase +
                            std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = static_cast<int>(arectrecover.TopLeft().X());
  iy = static_cast<int>(arectrecover.TopLeft().Y());
  iw = static_cast<int>(arectrecover.Width());
  ih = static_cast<int>(arectrecover.Height());

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void CxTextInspect::autolearnex(const char *pfilename) {
  const gp_Rectangle arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  const int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  const int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  const int imaxlen = std::max(iobjw, iobjh);
  const int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = static_cast<int>(arect.TopLeft().X());
  int iy = static_cast<int>(arect.TopLeft().Y());
  int iw = static_cast<int>(arect.Width());
  int ih = static_cast<int>(arect.Height());

  const gp_Rectangle arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnex(-1, 1, igrid);

  Grid *pgrid = fastmatch::getgrid();
  std::String strgrid = pgrid->GetGridString();

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::String strkey5;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    } else if (qstr.size() == 4) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
    } else if (qstr.size() == 5) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
      strkey5 = qstr.mid(4, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  if (!strkey4.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey4);
  }
  if (!strkey5.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey5);
  }

  strbase = charstring;
  std::String qstrsavepat = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;
  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {
    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {

      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = static_cast<int>(arectrecover.TopLeft().X());
  iy = static_cast<int>(arectrecover.TopLeft().Y());
  iw = static_cast<int>(arectrecover.Width());
  ih = static_cast<int>(arectrecover.Height());

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void CxTextInspect::autolearnobj(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  g_pbackimage->setroi(ix, iy, iw, ih);
  m_pimagegrid->ROIImagetoModel(*g_pbackimage);
  m_pimagegrid->ZeroModel();

  m_pimagegrid->ReGrid(igrid_org, igrid_org);
  if (0) {
    m_pimagegrid->SetUnit(igrid_org, igrid_org);
    m_pimagegrid->UnitGrid();
  }

  Grid *pgrid = m_pimagegrid;
  std::String strgrid = pgrid->GetGridString();

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strkey4;
  std::String strkey5;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    } else if (qstr.size() == 4) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
    } else if (qstr.size() == 5) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
      strkey4 = qstr.mid(3, 1);
      strkey5 = qstr.mid(4, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  if (!strkey4.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey4);
  }
  if (!strkey5.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey5);
  }

  strbase = charstring;
  std::String qstrsavepat = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".pat");
  std::String qstrsaveimp = std::String("./model/") + strgrid +
                            std::String("/") + strbase + std::String("_") +
                            strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  bool bsaveok = false;
  int isavenum = 0;
  while (!bsaveok) {
    std::String strlast1 = std::String("%1").arg(isavenum);
    qstrsaveimp = std::String("./model/") + strgrid + std::String("/") +
                  strbase + std::String("_") + strlast1 + std::String(".imp");
    isavenum = isavenum + 1;
    if (!std::filesystem::exists(qstrsaveimp.toStdString())) {
      m_pimagegrid->savemapmodel(qstrsaveimp.toStdString().c_str());
      bsaveok = true;
    }
  }

  clearmodel();
  loadfontmodel();
  levelmodel();
  setlevelstring();
}

void CxTextInspect::setlearngridwh(int igridwh) { m_igridwh = igridwh; }

void CxTextInspect::string_exnum(int inum) { m_exnum = inum; }

void CxTextInspect::string_autolearnmass(const char *pstring) {
  std::String qstr(pstring);
  int istrnum = qstr.size();
  int igetobj = g_pbackfindobject->getresultobjsnum();
  if (istrnum == igetobj) {
    for (int i = 0; i < istrnum; i++) {
      m_idebugrectsnum = i;
      std::String qchar(pstring[i]);
      std::String qname = qchar + std::String("%1").arg(m_exnum);
      learnmass_36(qname.toStdString().c_str());
    }
    clearmodel();
    loadfontmodel();
    levelmodel();
    setlevelstring();

    mapgrid();
    setspecshow(-1);
  }
}
void CxTextInspect::autolearnmass(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();
  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnmass(-1, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}

void CxTextInspect::checklearn(const char *pfilename) {

  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearncheck(m_imagetype, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }

  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savemodelfile(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}
void CxTextInspect::checkmatch(const char *pfilename) {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);

  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);

  if (1) {

    std::String qsavedir;
    if (12 == igrid)
      qsavedir = std::String("./model/12x12/");
    else if (24 == igrid)
      qsavedir = std::String("./model/24x24/");
    else if (36 == igrid)
      qsavedir = std::String("./model/36x36/");
    else if (72 == igrid)
      qsavedir = std::String("./model/72x72/");
    else if (144 == igrid)
      qsavedir = std::String("./model/144x144/");

    std::String strfilename = pfilename;

    QRegExp rxnum("(\\d+)");
    std::StringList listother = strfilename.split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = strfilename.split(rxother);

    std::String strkey = strfilename;
    std::String strkey2;
    std::String strkey3;
    std::String strlast;
    if (listother.size() > 1) {
      std::String qstr = listother[0];
      if (qstr != "" && qstr.size() == 1)
        strkey = qstr;
      else if (qstr.size() == 2) {
        strkey = qstr.mid(0, 1);
        strkey2 = qstr.mid(1, 1);
      } else if (qstr.size() == 3) {
        strkey = qstr.mid(0, 1);
        strkey2 = qstr.mid(1, 1);
        strkey3 = qstr.mid(2, 1);
      }

      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
        strlast = listnum[0].mid(1);
      } else if (listnum[1] != "")
        strlast = listnum[1];
    }
    std::String strbase;
    std::String charstring = char2string(strkey);
    if (!strkey2.isEmpty()) {
      charstring = charstring + std::String("_") + char2string(strkey2);
    }
    if (!strkey3.isEmpty()) {
      charstring = charstring + std::String("_") + char2string(strkey3);
    }

    strbase = charstring;

    std::String qstrsavepat =
        qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
    std::String qstrsaveimp =
        qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
    std::String qstrbase = strbase + std::String("_") + strlast;

    fastmatch::loadmodelfile(qstrsavepat.toStdString().c_str());
    fastmatch::loadfastimagemodel(qstrsaveimp.toStdString().c_str());
  }

  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearncheck(m_imagetype, 1, igrid);

  return;
}
void CxTextInspect::match72() {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  if (1) {
    fastmatch::setmatchrect(ix, iy, iw, ih);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::match(g_pbackobjectimage);
    double dvalue = fastmatch::getmaxresult();

    double dimagevalue = 0;
    fastmatch::imagelearncheck(m_imagetype, 1, igrid);
    dimagevalue = fastmatch::getimagemodelreslut();
    m_dvalue = dvalue;
    m_dmaxvalue = dimagevalue;
  }

  std::String strt =
      std::String(" %1 ").arg(m_dvalue) + std::String(" %1").arg(m_dmaxvalue);
  Shape::setname(strt.toStdString().c_str());
}
void CxTextInspect::match72_matchpat() {
  if (m_idebugrectsnum < 0)
    return;
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();

  Rect arectrecover = Shape::rect();

  if (1) {
    fastmatch::setmatchrect(ix, iy, iw, ih);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::setfindnum(1);
    fastmatch::setmatchthre(3);
    fastmatch::match(g_pbackobjectimage);
    double dvalue = fastmatch::getmaxresult();
    m_dvalue = dvalue;
  }

  std::String strt = std::String(" %1 ").arg(m_dvalue);
  Shape::setname(strt.toStdString().c_str());
}
void CxTextInspect::match72_matchimg() {
  int imaxlen = g_pbackfindobject->getobjectgridw();
  int igrid = fastmatch::GetRectGridLevel(imaxlen);
  double dimagevalue = 0;
  fastmatch::imagelearncheck(m_imagetype, 1, igrid);
  dimagevalue = fastmatch::getimagemodelreslut();
  m_dmaxvalue = dimagevalue;

  std::String strt = std::String(" %1 %2").arg(m_dvalue).arg(m_dmaxvalue);
  Shape::setname(strt.toStdString().c_str());
}
void CxTextInspect::learnmass_36(const char *pfilename) {
  Rect arect = g_pbackfindobject->getgridex(m_idebugrectsnum);

  int iobjw = g_pbackfindobject->getresultw(m_idebugrectsnum);
  int iobjh = g_pbackfindobject->getresulth(m_idebugrectsnum);
  int imaxlen = iobjw > iobjh ? iobjw : iobjh;
  int igrid = 36;

  int ix = arect.x();
  int iy = arect.y();
  int iw = arect.width();
  int ih = arect.height();
  Rect arectrecover = Shape::rect();

  fastmatch::setmatchrect(ix, iy, iw, ih);
  FindLine::setrect(ix, iy, iw, ih);
  fastmatch::setthre(8);
  fastmatch::setcomparegap(2);
  fastmatch::setmethod(0);
  fastmatch::setlinegap(1);
  fastmatch::SetWHgap(1, 1);
  fastmatch::setlinesamplerate(0.002);
  fastmatch::setfilter(0, 0, 10000);
  fastmatch::learn(g_pbackobjectimage);
  fastmatch::setmatchrect(ix, iy, iw, ih);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::setfindnum(1);
  fastmatch::setmatchthre(3);
  fastmatch::match(g_pbackobjectimage);

  fastmatch::imagelearnmass(-1, 1, igrid);

  std::String qsavedir;
  if (12 == igrid)
    qsavedir = std::String("./model/12x12/");
  else if (24 == igrid)
    qsavedir = std::String("./model/24x24/");
  else if (36 == igrid)
    qsavedir = std::String("./model/36x36/");
  else if (72 == igrid)
    qsavedir = std::String("./model/72x72/");
  else if (144 == igrid)
    qsavedir = std::String("./model/144x144/");
  else
    qsavedir = std::String("./model/12x12/");

  std::String strfilename = pfilename;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strfilename.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strfilename.split(rxother);

  std::String strkey = strfilename;
  std::String strkey2;
  std::String strkey3;
  std::String strlast;
  if (listother.size() > 1) {
    std::String qstr = listother[0];
    if (qstr != "" && qstr.size() == 1)
      strkey = qstr;
    else if (qstr.size() == 2) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
    } else if (qstr.size() == 3) {
      strkey = qstr.mid(0, 1);
      strkey2 = qstr.mid(1, 1);
      strkey3 = qstr.mid(2, 1);
    }

    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
      strlast = listnum[0].mid(1);
    } else if (listnum[1] != "")
      strlast = listnum[1];
  }
  std::String strbase;
  std::String charstring = char2string(strkey);
  if (!strkey2.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey2);
  }
  if (!strkey3.isEmpty()) {
    charstring = charstring + std::String("_") + char2string(strkey3);
  }
  strbase = charstring;

  std::String qstrsavepat =
      qsavedir + strbase + std::String("_") + strlast + std::String(".pat");
  std::String qstrsaveimp =
      qsavedir + strbase + std::String("_") + strlast + std::String(".imp");
  std::String qstrbase = strbase + std::String("_") + strlast;

  fastmatch::savefastimagemodel(qstrsaveimp.toStdString().c_str());
  fastmatch::savefastimagepatmodel(qstrsavepat.toStdString().c_str());

  return;

  int ifontsize = m_fontlist_l12.size();
  for (int in = 0; in < ifontsize; in++) {

    std::String qstr = m_fontlist_l12[in];
    if (strfilename == qstr) {
      fastmatch::setcurmodels(in);
      fastmatch::setcurimagemodels(in);
      break;
    }
    if (in == ifontsize - 1) {
      fastmatch::addimagemodels_l12(qstrsaveimp.toStdString().c_str());

      m_filenamelist_l12.append(qstrbase);
      m_fontlist_l12.append(strfilename);
    }
  }

  ix = arectrecover.x();
  iy = arectrecover.y();
  iw = arectrecover.width();
  ih = arectrecover.height();

  Shape::setrect(ix, iy, iw, ih);

  levelmodel();
}

void CxTextInspect::setlevelstring() {
  int istrsize = m_fontlist_l12.size();
  int istrsize1 = m_fontlist_l36.size();

  int ijsize = getlevel6_12map().size();
  int iksize = getlevel12_36map().size();
  int ilsize = getlevel36_72map().size();

  int iaddmax = ijsize - istrsize;
  int iaddmax1 = iksize - istrsize1;
  for (int il = 0; il < ilsize; il++) {
    int ilnum = getlevel36_72map()[il];
    if (ilnum >= istrsize1) {
      for (int iz = 0; iz < iaddmax1; iz++) {
        if (ilnum == istrsize1 + iz) {
          m_fontlist_l36.push_back(m_fontlist_l72[il]);
        }
      }
    }
  }
  for (int ik = 0; ik < iksize; ik++) {
    int iknum = getlevel12_36map()[ik];
    if (iknum >= istrsize) {
      for (int iz = 0; iz < iaddmax; iz++) {
        if (iknum == istrsize + iz) {
          m_fontlist_l12.push_back(m_fontlist_l36[ik]);
        }
      }
    }
  }
}
void CxTextInspect::levelmodel() {
  list_duplicatesmodel_l12();
  list_duplicatesmodel_l36();
  list_duplicatesmodel_l72();

  levelmodels_l72tol36();
  levelmodels_l36tol12();
  levelmodels_l12tol6();
  levelmodels_l6tol3();
}
void CxTextInspect::setgrid(int iw, int igrid) { fastmatch::setgrid(iw, igrid); }
void CxTextInspect::savelevelmodel() { savelevel0_l1(); }
void CxTextInspect::setmatchvalid(double dthre) { m_dmatchthre = dthre; }
void CxTextInspect::setusingobject(int iusing) { m_icompareobject = iusing; }
void CxTextInspect::fontocr() {
  m_ilevle = 2;
  m_resultstrlist.clear();
  m_resultstring.clear();
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imodelobjectw = 0;
  int imodelobjectb = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = m_fontlist_l12.size();
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          imodelobjectw = fastmatch::getmodeleasyobjectw_l12(i);
          imodelobjectb = fastmatch::getmodeleasyobjectb_l12(i);

          fastmatch::match(g_pbackobjectimage);

          dvalue = fastmatch::getmaxresult();

          dimagevalue = 0;
          if (dvalue >= m_dmatchthre) {
            fastmatch::imagematch(-1, 1);
            dimagevalue = fastmatch::getimagemodelreslut();

            imatchobjectb = fastmatch::geteasyobjectb();
            imatchobjectw = fastmatch::geteasyobjectw();
            int iobj = 1;
            if (m_icompareobject == 0) {
              iobj = 1;
            } else if (m_icompareobject == 1) {
              iobj = 0;
              if (imatchobjectb == imodelobjectb &&
                  imatchobjectw == imodelobjectw) {
                iobj = 1;
              }
            } else if (m_icompareobject == 2) {
              iobj = 0;
              if (imatchobjectb == imodelobjectb) {
                iobj = 1;
              }
            } else if (m_icompareobject == 3) {
              iobj = 0;

              if (imatchobjectw == imodelobjectw) {
                iobj = 1;
              }
            }

            if (dimagevalue > dmaxvalue && iobj > 0) {
              dmaxvalue = dimagevalue;
              iresultfont = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

        std::String strkey = m_fontlist_l12[iresultfont];

        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);

      } else {
        std::String strkey = m_fontlist_l12[iresultfont];
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(imodelobjectb)
                           .arg(imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    fastmatch::modelstocurrent_l12(iresultfont);
    fastmatch::imagemodelstocurrent_l12(iresultfont);

    fastmatch::match(g_pbackobjectimage);

    fastmatch::imagematch(-1, 1);
    imodelobjectw = fastmatch::getmodeleasyobjectw_l12(iresultfont);
    imodelobjectb = fastmatch::getmodeleasyobjectb_l12(iresultfont);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(imodelobjectb)
                      .arg(imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
}

void CxTextInspect::SelectModel(int ilevle, int inum) {
  m_ilevle = ilevle;
  switch (ilevle) {
  case 0:
    fastmatch::modelstocurrent_l3(inum);
    fastmatch::imagemodelstocurrent_l3(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum);

    break;
  case 1:
    fastmatch::modelstocurrent_l6(inum);
    fastmatch::imagemodelstocurrent_l6(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum);

    break;
  case 2:
    fastmatch::modelstocurrent_l12(inum);
    fastmatch::imagemodelstocurrent_l12(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum);
    break;
  case 3:
    fastmatch::modelstocurrent_l36(inum);
    fastmatch::imagemodelstocurrent_l36(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(inum);
    break;
  case 4:
    fastmatch::modelstocurrent_l72(inum);
    fastmatch::imagemodelstocurrent_l72(inum);
    m_imodelobjectw = fastmatch::getmodeleasyobjectw_l72(inum);
    m_imodelobjectb = fastmatch::getmodeleasyobjectb_l72(inum);
    break;
  }
}

void CxTextInspect::selectmodel72(const char *pfilename) {
  SelectNameModel(4, pfilename);
}

void CxTextInspect::SelectNameModel(int ilevel, const char *pfilename) {
  m_ilevle = ilevel;

  int iselnum = 0;
  std::String strname(pfilename);

  switch (ilevel) {

  case 2: {
    int isize = m_filenamelist_l12.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_filenamelist_l12[i]) {
        iselnum = i;
        break;
      }
    }
  } break;

  case 3: {
    int isize = m_filenamelist_l36.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_filenamelist_l36[i]) {
        iselnum = i;
        break;
      }
    }
  } break;

  case 4: {
    int isize = m_fontlist_l72.size();
    for (int i = 0; i < isize; i++) {
      if (strname == m_fontlist_l72[i]) {
        iselnum = i;
        break;
      }
    }
  } break;
  }

  SelectModel(ilevel, iselnum);
}
void CxTextInspect::modelmethod(int itype) { fastmatch::modelmethod(itype); }

void CxTextInspect::fontocr_level(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;
  m_ilevle = ilevel;
  if (0 == ilevel) {
    m_l3resultlist.clear();
  } else if (1 == ilevel) {
    m_l6resultlist.clear();
  } else if (2 == ilevel) {
    m_l12resultlist.clear();
  }
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmodelsize(ilevel);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      g_pbackobjectimage->setroi(ix, iy, iw, ih);

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw * 0.25, ih * 0.25);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);
        g_pbackobjectimage->copyResizedToROI(g_pbackimage->getROI(ix, iy, iw, ih).getmat());
        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw * 0.5, ih * 0.5);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);
        g_pbackobjectimage->copyResizedToROI(g_pbackimage->getROI(ix, iy, iw, ih).getmat());
        break;
      default:
      case 2:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);
        g_pbackobjectimage->copyResizedToROI(g_pbackimage->getROI(ix, iy, iw, ih).getmat());
        break;
      }

      dmaxvalue = 0;
      int imaxnum = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          SelectModel(ilevel, i);
          fastmatch::match(g_pbackimage);
          dvalue = fastmatch::getmaxresult();
          dimagevalue = 0;
          if (dvalue >= dmaxvalue) {
            imaxnum = i;
            dmaxvalue = dvalue;
          }
          if ((0 == ilevel && -1 != m_idebugfontnum) || 1 == ilevel ||
              2 == ilevel) {
            if (dvalue >= m_dmatchthre) {
              switch (ilevel) {
              case 0:
                fastmatch::imagematchex(3);
                break;
              case 1:
                fastmatch::imagematchex(6);
                break;
              case 2:
                fastmatch::imagematchex(12);
                break;
              }
              dimagevalue = fastmatch::getimagemodelreslut();

              imatchobjectb = fastmatch::geteasyobjectb();
              imatchobjectw = fastmatch::geteasyobjectw();
              int iobj = 1;
              if (m_icompareobject == 0) {
                iobj = 1;
              } else if (m_icompareobject == 1) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb &&
                    imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 2) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 3) {
                iobj = 0;

                if (imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              }

              if (dimagevalue > dmaxvalue && iobj > 0) {
                dmaxvalue = dimagevalue;
                iresultfont = i;
              }
              if (-1 == m_idebugfontnum && 100 == dimagevalue)
                break;
            }
          }
        }
      }

      if (-1 == m_idebugfontnum && 0 == ilevel) {
        SelectModel(ilevel, imaxnum);
        fastmatch::match(g_pbackimage);
        dvalue = fastmatch::getmaxresult();
        switch (ilevel) {
        case 0:
          fastmatch::imagematchex(3);
          break;
        case 1:
          fastmatch::imagematchex(6);
          break;
        case 2:
          fastmatch::imagematchex(12);
          break;
        case 3:
          fastmatch::imagematchex(36);
          break;
        }
        dimagevalue = fastmatch::getimagemodelreslut();

        imatchobjectb = fastmatch::geteasyobjectb();
        imatchobjectw = fastmatch::geteasyobjectw();
        int iobj = 1;
        if (m_icompareobject == 0) {
          iobj = 1;
        } else if (m_icompareobject == 1) {
          iobj = 0;
          if (imatchobjectb == m_imodelobjectb &&
              imatchobjectw == m_imodelobjectw) {
            iobj = 1;
          }
        } else if (m_icompareobject == 2) {
          iobj = 0;
          if (imatchobjectb == m_imodelobjectb) {
            iobj = 1;
          }
        } else if (m_icompareobject == 3) {
          iobj = 0;

          if (imatchobjectw == m_imodelobjectw) {
            iobj = 1;
          }
        }

        if (dimagevalue > dmaxvalue && iobj > 0) {
          dmaxvalue = dimagevalue;
          iresultfont = imaxnum;
        }
        m_l3resultlist.push_back(imaxnum);
      } else if (1 == ilevel) {
        m_l6resultlist.push_back(iresultfont);
      } else if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);

          m_l12resultlist.push_back(iresultfont);
        } else {
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
          m_l12resultlist.push_back(iresultfont);
        }
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    SelectModel(ilevel, iresultfont);

    fastmatch::match(g_pbackobjectimage);

    switch (ilevel) {
    case 0:
      fastmatch::imagematchex(3);
      break;
    case 1:
      fastmatch::imagematchex(6);
      break;
    case 2:
      fastmatch::imagematchex(12);
      break;
    case 3:
      fastmatch::imagematchex(36);
      break;
    }

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else {
    if (0 == ilevel) {
      std::String qshowstr;
      int isize = m_l3resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1_").arg(m_l3resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (1 == ilevel) {
      std::String qshowstr;
      int isize = m_l6resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1_").arg(m_l6resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (2 == ilevel)
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
  }
}
int CxTextInspect::imagefastmapsize(int ilevel, int inum) {
  int igetsize = 0;
  if (0 == ilevel) {
    return imagefastmodelsize(ilevel);
  } else if (1 == ilevel) {
    int il0num = m_l3resultlist[inum];
    int isize = getlevel3_6map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il0num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  } else if (2 == ilevel) {
    int il1num = m_l6resultlist[inum];
    int isize = getlevel6_12map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel6_12map()[i] == il1num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  } else if (3 == ilevel) {
    int il1num = m_l12resultlist[inum];
    int isize = getlevel12_36map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel12_36map()[i] == il1num) {
        igetsize = igetsize + 1;
      }
    }
    return igetsize;
  }
  return 0;
}
int CxTextInspect::SelectMapModel(int ilevel, int inum, int i0) {
  m_ilevle = ilevel;
  int igetsize = 0;
  if (0 == ilevel) {
    SelectModel(ilevel, i0);
    return i0;
  } else if (1 == ilevel) {
    int il0num = m_l3resultlist[inum];
    int isize = getlevel3_6map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il0num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l6(i);
          fastmatch::imagemodelstocurrent_l6(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  } else if (2 == ilevel) {
    int il1num = m_l6resultlist[inum];
    int isize = getlevel6_12map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel3_6map()[i] == il1num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l12(i);
          fastmatch::imagemodelstocurrent_l12(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  } else if (3 == ilevel) {
    int il1num = m_l12resultlist[inum];
    int isize = getlevel12_36map().size();
    for (int i = 0; i < isize; i++) {
      if (getlevel12_36map()[i] == il1num) {
        if (igetsize == i0) {
          fastmatch::modelstocurrent_l36(i);
          fastmatch::imagemodelstocurrent_l36(i);
          m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(i);
          m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(i);
          return i;
        }
        igetsize = igetsize + 1;
      }
    }
  }
  return 0;
}
int CxTextInspect::resultnodesize(int ilevel, int iareanum) {
  if (0 == ilevel) {
    return imagefastmodelsize(ilevel);
  } else {
    levelnode anode = m_reslutnodelist[iareanum];
    int iselectnum = 0;
    int isize = getlevel3_6map().size();
    int jsize = getlevel6_12map().size();
    int ksize = getlevel12_36map().size();
    if (0 == anode.s_ilevel) {
      if (anode.s_inode < isize && anode.s_inode >= 0) {
        if (0 == ilevel) {
          return 1;
        } else if (1 == ilevel) {
          for (int i = 0; i < isize; i++) {
            if (getlevel3_6map()[i] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        } else if (2 == ilevel) {
          for (int i = 0; i < isize; i++) {
            if (getlevel3_6map()[i] == anode.s_inode) {
              for (int j = 0; j < jsize; j++) {
                if (getlevel6_12map()[j] == i) {
                  iselectnum = iselectnum + 1;
                }
              }
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }
    } else if (1 == anode.s_ilevel) {
      if (anode.s_inode < isize && anode.s_inode >= 0) {
        if (0 == ilevel) {
          return 1;
        } else if (1 == ilevel) {
          return 1;
        } else if (2 == ilevel) {
          for (int j = 0; j < jsize; j++) {
            if (getlevel6_12map()[j] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }

    } else if (2 == anode.s_ilevel) {
      if (anode.s_inode < jsize && anode.s_inode >= 0) {
        if (ilevel < 3)
          return 1;
        else if (3 == ilevel) {
          for (int k = 0; k < ksize; k++) {
            if (getlevel12_36map()[k] == anode.s_inode) {
              iselectnum = iselectnum + 1;
            }
          }
          return iselectnum;
        }
      } else {
        return imagefastmodelsize(ilevel);
      }
    }
  }
}
void CxTextInspect::selectresultnode(int ilevel, int iareanum, int inum0) {
  if (m_reslutnodelist.size() <= iareanum)
    return;
  levelnode anode = m_reslutnodelist[iareanum];
  int iselectnum = 0;
  int isize = getlevel3_6map().size();
  int jsize = getlevel6_12map().size();
  int ksize = getlevel12_36map().size();
  if (0 == anode.s_ilevel) {
    if (anode.s_inode < isize && anode.s_inode >= 0) {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;
      } else if (1 == ilevel) {
        for (int i = 0; i < isize; i++) {
          if (getlevel3_6map()[i] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l6(i);
              fastmatch::imagemodelstocurrent_l6(i);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(i);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(i);
              m_selectnode.s_ilevel = 1;
              m_selectnode.s_inode = i;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      } else if (2 == ilevel) {
        for (int i = 0; i < isize; i++) {
          if (getlevel3_6map()[i] == anode.s_inode) {
            for (int j = 0; j < jsize; j++) {
              if (getlevel6_12map()[j] == i) {
                if (iselectnum == inum0) {
                  fastmatch::modelstocurrent_l12(j);
                  fastmatch::imagemodelstocurrent_l12(j);
                  m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(j);
                  m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(j);
                  m_selectnode.s_ilevel = 2;
                  m_selectnode.s_inode = j;
                  return;
                }
                iselectnum = iselectnum + 1;
              }
            }
          }
        }
      }
    } else {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;

      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;

      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  } else if (1 == anode.s_ilevel) {
    if (anode.s_inode < isize && anode.s_inode >= 0) {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l6(anode.s_inode);
        fastmatch::imagemodelstocurrent_l6(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(anode.s_inode);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = anode.s_inode;
        return;
      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(anode.s_inode);
        fastmatch::imagemodelstocurrent_l6(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(anode.s_inode);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = anode.s_inode;
        return;
      } else if (2 == ilevel) {
        for (int j = 0; j < jsize; j++) {
          if (getlevel6_12map()[j] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l12(j);
              fastmatch::imagemodelstocurrent_l12(j);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(j);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(j);
              m_selectnode.s_ilevel = 2;
              m_selectnode.s_inode = j;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      }
    } else {
      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;
      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;
      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  } else if (2 == anode.s_ilevel) {
    if (anode.s_inode < jsize && anode.s_inode >= 0) {
      if (ilevel < 3) {
        fastmatch::modelstocurrent_l12(anode.s_inode);
        fastmatch::imagemodelstocurrent_l12(anode.s_inode);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(anode.s_inode);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(anode.s_inode);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = anode.s_inode;
      } else if (3 == ilevel) {
        for (int k = 0; k < ksize; k++) {
          if (getlevel12_36map()[k] == anode.s_inode) {
            if (iselectnum == inum0) {
              fastmatch::modelstocurrent_l36(k);
              fastmatch::imagemodelstocurrent_l36(k);
              m_imodelobjectw = fastmatch::getmodeleasyobjectw_l36(k);
              m_imodelobjectb = fastmatch::getmodeleasyobjectb_l36(k);
              m_selectnode.s_ilevel = 3;
              m_selectnode.s_inode = k;
              return;
            }
            iselectnum = iselectnum + 1;
          }
        }
      }

      return;
    } else {

      if (0 == ilevel) {
        fastmatch::modelstocurrent_l3(inum0);
        fastmatch::imagemodelstocurrent_l3(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l3(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l3(inum0);
        m_selectnode.s_ilevel = 0;
        m_selectnode.s_inode = inum0;
        return;

      } else if (1 == ilevel) {
        fastmatch::modelstocurrent_l6(inum0);
        fastmatch::imagemodelstocurrent_l6(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l6(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l6(inum0);
        m_selectnode.s_ilevel = 1;
        m_selectnode.s_inode = inum0;
        return;

      } else if (2 == ilevel) {
        fastmatch::modelstocurrent_l12(inum0);
        fastmatch::imagemodelstocurrent_l12(inum0);
        m_imodelobjectw = fastmatch::getmodeleasyobjectw_l12(inum0);
        m_imodelobjectb = fastmatch::getmodeleasyobjectb_l12(inum0);
        m_selectnode.s_ilevel = 2;
        m_selectnode.s_inode = inum0;
        return;
      }
    }
  }
}
void CxTextInspect::resultnodereset(int iareasnum) {
  m_reslutnodelist.clear();
  for (int i = 0; i < iareasnum; i++) {
    levelnode anode;
    anode.s_ilevel = 0;
    anode.s_inode = -1;
    m_reslutnodelist.push_back(anode);
  }
}
void CxTextInspect::setresultnode(int inum, levelnode inode) {
  m_reslutnodelist[inum] = inode;
}

void CxTextInspect::fontocr_levelex(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;
  m_ilevle = ilevel;
  if (0 == ilevel) {
    resultnodereset(iareasnum);
    m_l3resultlist.clear();
  } else if (1 == ilevel) {
    if (m_l3resultlist.size() <= 0)
      return;
    if (m_l3resultlist.size() != iareasnum)
      return;
    m_l6resultlist.clear();
  } else if (2 == ilevel) {
    if (m_l6resultlist.size() <= 0)
      return;
    if (m_l6resultlist.size() != iareasnum)
      return;
  } else if (3 == ilevel) {
    if (m_l12resultlist.size() <= 0)
      return;
    if (m_l12resultlist.size() != iareasnum)
      return;
  }
  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int iselmaxnum = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmapsize(ilevel, ia);

      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      g_pbackobjectimage->setroi(ix, iy, iw, ih);

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw * 0.25, ih * 0.25);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);

        m_pimagegrid->ROIImagetoModel(*g_pbackobjectimage);

        if (0) {
          m_pimagegrid->SetUnit(12, 12);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(12, 12);
        m_pimagegrid->GridZoom(3, 3);
        m_pimagegrid->SetUnit(3, 3);

        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw * 0.5, ih * 0.5);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);

        m_pimagegrid->ROIImagetoModel(*g_pbackobjectimage);

        if (0) {
          m_pimagegrid->SetUnit(12, 12);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(12, 12);

        m_pimagegrid->GridZoom(6, 6);
        m_pimagegrid->SetUnit(6, 6);
        break;
      default:
      case 2:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        g_pbackobjectimage->ROIColorTable();
        g_pbackobjectimage->ROIColorTableBlur(0, -1);
        g_pbackobjectimage->ROIColorTableEasyThre(1, 0);

        break;
      }

      dmaxvalue = 0;
      iselmaxnum = 0;
      int imaxnum = 0;
      iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        int iselnum = SelectMapModel(ilevel, ia, i);
        if (-1 == m_idebugfontnum || iselnum == m_idebugfontnum) {
          MatchGrid(m_pimagegrid);
          dimagevalue = fastmatch::getimagemodelreslut();

          imatchobjectb = fastmatch::geteasyobjectb();
          imatchobjectw = fastmatch::geteasyobjectw();
          int iobj = 1;
          if (m_icompareobject == 0) {
            iobj = 1;
          } else if (m_icompareobject == 1) {
            iobj = 0;
            if (imatchobjectb == m_imodelobjectb &&
                imatchobjectw == m_imodelobjectw) {
              iobj = 1;
            }
          } else if (m_icompareobject == 2) {
            iobj = 0;
            if (imatchobjectb == m_imodelobjectb) {
              iobj = 1;
            }
          } else if (m_icompareobject == 3) {
            iobj = 0;

            if (imatchobjectw == m_imodelobjectw) {
              iobj = 1;
            }
          }

          if (dimagevalue > dmaxvalue && iobj > 0) {
            dmaxvalue = dimagevalue;
            iresultfont = iselnum;
            imaxnum = iselnum;
            iselmaxnum = i;
          }
          if (-1 == m_idebugfontnum && 100 == dimagevalue) {
            break;
          }
        }
        if (iselnum == m_idebugfontnum)
          break;
      }

      if (0 == ilevel) {
        m_l3resultlist.push_back(imaxnum);
      } else if (1 == ilevel) {
        m_l6resultlist.push_back(iresultfont);
      } else if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }

        m_l12resultlist.push_back(iresultfont);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    int iselnum = SelectMapModel(ilevel, m_idebugrectsnum, iselmaxnum);
    fastmatch::MatchGrid(m_pimagegrid);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(iselnum);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else {
    if (0 == ilevel) {
      std::String qshowstr;
      int isize = m_l3resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1|").arg(m_l3resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }

      Shape::setname(qshowstr.toStdString().c_str());
    } else if (1 == ilevel) {
      std::String qshowstr;
      int isize = m_l6resultlist.size();
      for (int i = 0; i < isize; i++) {
        std::String astrnum = std::String("%1|").arg(m_l6resultlist[i]);
        qshowstr = qshowstr + astrnum;
      }
      Shape::setname(qshowstr.toStdString().c_str());
    } else if (2 == ilevel)
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
  }
}

void CxTextInspect::fontocr_level2() {

  int iareasnum = g_pbackfindobject->getresultobjsnum();
  if (m_l6resultlist.size() <= 0)
    return;
  if (m_l6resultlist.size() != iareasnum)
    return;
  m_resultstrlist.clear();
  m_resultstring.clear();

  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iresultnum = 0;
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = imagefastmapsize(2, ia);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      iresultnum = 0;
      if (1 == isize) {
        int iselnum = SelectMapModel(2, ia, 0);
        dmaxvalue = 100;
        iresultfont = iselnum;
        iresultnum = 0;

      } else
        for (int i = 0; i < isize; i++) {
          int iselnum = SelectMapModel(2, ia, i);
          if (-1 == m_idebugfontnum || i == m_idebugfontnum) {

            fastmatch::match(g_pbackobjectimage);

            dvalue = fastmatch::getmaxresult();

            dimagevalue = 0;
            if (dvalue >= m_dmatchthre) {
              fastmatch::imagematch(-1, 1);
              dimagevalue = fastmatch::getimagemodelreslut();

              imatchobjectb = fastmatch::geteasyobjectb();
              imatchobjectw = fastmatch::geteasyobjectw();
              int iobj = 1;
              if (m_icompareobject == 0) {
                iobj = 1;
              } else if (m_icompareobject == 1) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb &&
                    imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 2) {
                iobj = 0;
                if (imatchobjectb == m_imodelobjectb) {
                  iobj = 1;
                }
              } else if (m_icompareobject == 3) {
                iobj = 0;

                if (imatchobjectw == m_imodelobjectw) {
                  iobj = 1;
                }
              }

              if (dimagevalue > dmaxvalue && iobj > 0) {
                dmaxvalue = dimagevalue;
                iresultfont = iselnum;
                iresultnum = i;
              }
              if (-1 == m_idebugfontnum && 100 == dimagevalue)
                break;
            }
          }
        }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

        std::String strkey = m_fontlist_l12[iresultfont];
        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);

      } else {
        std::String strkey = m_fontlist_l12[iresultfont];
        m_resultstrlist.push_back(strkey);
        m_resultstring.append(strkey);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(dvalue)
                           .arg(dimagevalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    int iselnum = SelectMapModel(2, m_idebugrectsnum, iresultnum);

    fastmatch::match(g_pbackobjectimage);

    fastmatch::imagematch(-1, 1);

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);
    strt = strt + std::String(" b(%3 %5)w(%4 %6)")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    Shape::setname(strt.toStdString().c_str());
  } else
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
}
void CxTextInspect::checkocr_level3() {
  int iareasnum = g_pbackfindobject->getresultobjsnum();

  m_l12resultlist.clear();

  int isize = m_reslutnodelist.size();
  for (int it = 0; it < isize; it++) {
    if (2 == m_reslutnodelist[it].s_ilevel) {
      m_l12resultlist.push_back(m_reslutnodelist[it].s_inode);
    } else {
      Shape::setname(
          std::String("node result level != 2").toStdString().c_str());
      return;
    }
  }

  m_resultstrlist.clear();
  m_resultstring.clear();

  double dvalue = 0;
  double dimagevalue = 0;
  double dmaxvalue = 0;
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  int iresultfont = 0;
  int iresultnum = 0;
  if (iareasnum <= 0)
    return;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = resultnodesize(3, ia);
      Rect arect = g_pbackfindobject->getgrid(ia);
      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();
      fastmatch::setmatchrect(ix, iy, iw, ih);
      dmaxvalue = 0;
      iresultfont = 0;
      iresultnum = 0;
      for (int i = 0; i < isize; i++) {
        selectresultnode(3, ia, i);
        if (-1 == m_idebugfontnum || i == m_idebugfontnum) {
          fastmatch::match(g_pbackobjectimage);

          dvalue = fastmatch::getmaxresult();

          dimagevalue = 0;
          if (dvalue >= m_dmatchthre) {
            fastmatch::imagematch_grid(-1, 1, 36);
            dimagevalue = fastmatch::getimagemodelreslut();

            if (dimagevalue > dmaxvalue) {
              dmaxvalue = dimagevalue;

              m_iresultfont = m_selectnode.s_inode;
              iresultfont = m_selectnode.s_inode;
              iresultnum = i;
            }
            if (-1 == m_idebugfontnum && 100 == dimagevalue)
              break;
          }
        }
      }

      if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {
        QRegExp rxnum("(\\d+)");
        std::StringList listother = m_fontlist_l36[m_iresultfont].split(rxnum);

        QRegExp rxother("(\\D+)");
        std::StringList listnum = m_fontlist_l36[m_iresultfont].split(rxother);

        std::String strkey = m_fontlist_l36[m_iresultfont];
        if (listother.size() > 0) {
          if (listother[0] != "")
            strkey = listother[0];
          if (listnum.size() < 2) {
            strkey = listnum[0].mid(0, 1);
          }
        }
        std::String astr =
            std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);
        m_resultstrlist.push_back(strkey + astr);
        m_resultstring.append(strkey + astr);

      } else {
        std::String astr =
            std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);

        std::String strkey = m_filenamelist_l36[iresultfont];
        m_resultstrlist.push_back(strkey + astr);
        m_resultstring.append(strkey + astr);
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {
    std::String astr = std::String("  %1   %2 ").arg(dvalue).arg(dimagevalue);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    selectresultnode(3, m_idebugrectsnum, iresultnum);

    fastmatch::match(g_pbackobjectimage);

    fastmatch::imagematch_grid(-1, 1, 36);

    std::String strt = std::String(" %1 ").arg(iresultfont) + m_resultstring +
                       std::String(" %1").arg(dmaxvalue);

    Shape::setname(strt.toStdString().c_str());
  } else
  FinalizeGlyphDecodingFromLegacyResults();
  Shape::setname(m_resultstring.toStdString().c_str());
}

bool CxTextInspect::matchlevelnode01() {
  MatchGrid(m_pimagegrid);
  double dimagevalue = fastmatch::getimagemodelreslut();

  int imatchobjectb = fastmatch::geteasyobjectb();
  int imatchobjectw = fastmatch::geteasyobjectw();
  int iobj = 1;
  if (m_icompareobject == 0) {
    iobj = 1;
  } else if (m_icompareobject == 1) {
    iobj = 0;
    if (imatchobjectb == m_imodelobjectb && imatchobjectw == m_imodelobjectw) {
      iobj = 1;
    }
  } else if (m_icompareobject == 2) {
    iobj = 0;
    if (imatchobjectb == m_imodelobjectb) {
      iobj = 1;
    }
  } else if (m_icompareobject == 3) {
    iobj = 0;

    if (imatchobjectw == m_imodelobjectw) {
      iobj = 1;
    }
  }

  if (dimagevalue > m_dmaxvalue && iobj > 0) {
    m_iselmaxnum = m_selectnode.s_inode;
    m_dmaxvalue = dimagevalue;
    m_iresultfont = m_selectnode.s_inode;
    m_imaxnum = m_selectnode.s_inode;
  }
  if (-1 == m_idebugfontnum && 100 == dimagevalue) {
    return false;
  }

  return true;
}
bool CxTextInspect::matchlevelnode2() {
  fastmatch::match(g_pbackimage);

  double dvalue = fastmatch::getmaxresult();
  double dimagevalue = 0;
  if (dvalue >= m_dmatchthre) {
    fastmatch::imagematch(-1, 1, 12, 0);
    dimagevalue = fastmatch::getimagemodelreslut();
    int imatchobjectb = fastmatch::geteasyobjectb();
    int imatchobjectw = fastmatch::geteasyobjectw();
    int iobj = 1;
    if (m_icompareobject == 0) {
      iobj = 1;
    } else if (m_icompareobject == 1) {
      iobj = 0;
      if (imatchobjectb == m_imodelobjectb &&
          imatchobjectw == m_imodelobjectw) {
        iobj = 1;
      }
    } else if (m_icompareobject == 2) {
      iobj = 0;
      if (imatchobjectb == m_imodelobjectb) {
        iobj = 1;
      }
    } else if (m_icompareobject == 3) {
      iobj = 0;

      if (imatchobjectw == m_imodelobjectw) {
        iobj = 1;
      }
    }
    if (dimagevalue > m_dmaxvalue) {
      if (iobj > 0) {
        m_dvalue = dvalue;
        m_dmaxvalue = dimagevalue;
        m_iresultfont = m_selectnode.s_inode;
        m_imaxnum = m_selectnode.s_inode;
      } else if (m_dmaxvalue <= 0) {
        m_dvalue = dvalue;
        m_dmaxvalue = dimagevalue;
        m_iresultfont = m_selectnode.s_inode;
        m_imaxnum = m_selectnode.s_inode;
      }
    }
    if (-1 == m_idebugfontnum && 100 == dimagevalue) {
      return false;
    }

    return true;
  } else
    return true;
}
void CxTextInspect::resultnodelistreset(int iareanum) {
  m_reslutnodeslistgrid3x3.clear();
  m_reslutnodeslistgrid6x6.clear();
  m_reslutnodeslistgrid12x12.clear();
  m_reslutnodeslistgrid36x36.clear();
  m_reslutnodeslistgrid72x72.clear();

  for (int ia = 0; ia < iareanum; ia++) {
    levelnodes anodes;
    anodes.setsearchnum(m_resultnodesearchsum);
    m_reslutnodeslistgrid3x3.push_back(anodes);
    m_reslutnodeslistgrid6x6.push_back(anodes);
    m_reslutnodeslistgrid12x12.push_back(anodes);
  }
}
bool CxTextInspect::matchlevelnodelist3x3(int ia) {
  int isize = imagefastmodelsize(0);
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    fastmatch::modelstocurrent_l3(i);
    fastmatch::imagemodelstocurrent_l3(i);
    MatchGrid(m_pimagegrid);
    dimagevalue = fastmatch::getimagemodelreslut();

    levelvalenode anode;
    anode.s_ilevel = 0;
    anode.s_inode = i;
    anode.s_dvalue = dimagevalue;

    m_reslutnodeslistgrid3x3[ia].addnode(anode);
  }

  return true;
}
bool CxTextInspect::matchlevelnodelist6x6(int ia) {
  int isize = m_reslutnodeslistgrid3x3[ia].getnodes().size();
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    int inodenum = m_reslutnodeslistgrid3x3[ia].getnodes()[i].s_inode;

    for (int j = 0; j < getlevel3_6map().size(); j++) {
      if (getlevel3_6map()[j] == inodenum) {
        fastmatch::modelstocurrent_l6(j);
        fastmatch::imagemodelstocurrent_l6(j);
        MatchGrid(m_pimagegrid);
        dimagevalue = fastmatch::getimagemodelreslut();
        levelvalenode anode;
        anode.s_ilevel = 1;
        anode.s_inode = j;
        anode.s_dvalue = dimagevalue;

        m_reslutnodeslistgrid6x6[ia].addnode(anode);
      }
    }
  }

  return true;
}
bool CxTextInspect::matchlevelnodelist12x12(int ia) {
  int isize = m_reslutnodeslistgrid6x6[ia].getnodes().size();
  double dimagevalue = 0;
  for (int i = 0; i < isize; i++) {
    int inodenum = m_reslutnodeslistgrid6x6[ia].getnodes()[i].s_inode;

    for (int j = 0; j < getlevel6_12map().size(); j++) {
      if (getlevel6_12map()[j] == inodenum) {
        fastmatch::modelstocurrent_l12(j);
        fastmatch::imagemodelstocurrent_l12(j);
        fastmatch::match(g_pbackimage);
        double dvalue = fastmatch::getmaxresult();
        double dimagevalue = 0;
        if (dvalue >= m_dmatchthre) {
          fastmatch::imagematch(-1, 1, 12, 0);
          dimagevalue = fastmatch::getimagemodelreslut();

          levelvalenode anode;
          anode.s_ilevel = 2;
          anode.s_inode = j;

          anode.s_dvalue = dimagevalue;
          m_reslutnodeslistgrid12x12[ia].addnode(anode);
        }
      }
    }
  }

  return true;
}
bool CxTextInspect::matchlevelnodelist36x36(int ia) {
  fastmatch::match(g_pbackimage);
  double dvalue = fastmatch::getmaxresult();
  double dimagevalue = 0;
  if (dvalue >= m_dmatchthre) {
    fastmatch::imagematch(-1, 1, 12, 0);
    dimagevalue = fastmatch::getimagemodelreslut();
    if (dimagevalue > m_dmaxvalue) {
      m_dvalue = dvalue;
      m_dmaxvalue = dimagevalue;
      m_iresultfont = m_selectnode.s_inode;
      m_imaxnum = m_selectnode.s_inode;
    }
    if (-1 == m_idebugfontnum && 100 == dimagevalue) {
      return false;
    }
    return true;
  } else
    return true;
}

void CxTextInspect::fontocr_levelnode(int ilevel) {
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 1)
    return;
  m_ilevle = ilevel;

  m_dvalue = 0;
  m_dmaxvalue = 0;
  if (0 == ilevel) {
    resultnodereset(iareasnum);
  }
  int imatchobjectw = 0;
  int imatchobjectb = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      int isize = resultnodesize(ilevel, ia);

      Rect arect = g_pbackfindobject->getgridex(ia);

      int iobjw = g_pbackfindobject->getresultw(ia);
      int iobjh = g_pbackfindobject->getresulth(ia);
      int imaxlen = iobjw > iobjh ? iobjw : iobjh;
      int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();

      if (ix < 0 || iy < 0 || iw <= 0 || ih <= 0)
        continue;

      switch (ilevel) {
      case 0:
        fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackobjectimage->setroi(ix, iy, iw, ih);
        g_pbackimage->setroi(ix, iy, iw, ih);
        g_pbackobjectimage->SetMode(3);
        g_pbackobjectimage->ROItoROI(*g_pbackimage);

        g_pbackimage->ROIColorTable();
        g_pbackimage->ROIColorTableBlur(0, -1);
        g_pbackimage->ROIColorTableEasyThre(1, 0);

        m_pimagegrid->ROIImagetoModel(*g_pbackimage);
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        if (0) {
          m_pimagegrid->SetUnit(igrid_org, igrid_org);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->GridZoom(3, 3);
        m_pimagegrid->SetUnit(3, 3);

        break;
      case 1:
        fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackimage->setroi(ix, iy, iw, ih);
        m_pimagegrid->ROIImagetoModel(*g_pbackimage);
        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        if (0) {
          m_pimagegrid->SetUnit(igrid_org, igrid_org);
          m_pimagegrid->UnitGrid();
        }
        m_pimagegrid->GridZoom(6, 6);
        m_pimagegrid->SetUnit(6, 6);
        break;
      default:
      case 2:
        if (igrid_org == 12)
          fastmatch::setmatchrect(ix, iy, iw, ih);
        else {
          g_pbackimage->setroi(ix, iy, iw, ih);
          m_pimagegrid->ROIImagetoModel(*g_pbackimage);

          m_pimagegrid->ZeroModel();
          m_pimagegrid->ReGrid(igrid_org, igrid_org);
          if (0) {
            m_pimagegrid->SetUnit(igrid_org, igrid_org);
            m_pimagegrid->UnitGrid();
          }

          m_pimagegrid->GridZoom(12, 12);
          m_pimagegrid->SetUnit(12, 12);
        }
        break;
      case 3:
        fastmatch::setmatchrect(ix, iy, iw, ih);

        break;
      }

      m_dmaxvalue = 0;
      m_iselmaxnum = 0;
      m_iresultfont = 0;
      for (int i = 0; i < isize; i++) {
        if (-1 == m_idebugfontnum)
          selectresultnode(ilevel, ia, i);
        else
          SelectModel(ilevel, m_idebugfontnum);
        if (1) {
          if (0 == ilevel || 1 == ilevel) {
            if (!matchlevelnode01()) {
              setresultnode(ia, m_selectnode);
              break;
            }
          } else if (2 == ilevel) {
            if (1 == isize && -1 == m_idebugfontnum) {
              m_dvalue = 1;
              m_dmaxvalue = 99;
              m_iresultfont = m_selectnode.s_inode;
              m_imaxnum = m_selectnode.s_inode;
              break;
            } else if (igrid_org != 12) {
              if (!matchlevelnode01()) {
                setresultnode(ia, m_selectnode);
                break;
              }
            } else if (!matchlevelnode2())
              break;
          }
        }

        if (m_selectnode.s_inode == m_idebugfontnum)
          break;
      }
      if (1 == ilevel || 0 == ilevel) {
        if (m_reslutnodelist[ia].s_inode == -1) {
          if (m_reslutnodelist[ia].s_ilevel == 0) {
            if (ilevel == 1) {
              m_reslutnodelist[ia].s_inode = -1;
              m_reslutnodelist[ia].s_ilevel = 1;
            }
          } else if (m_reslutnodelist[ia].s_ilevel == 1) {
            if (ilevel == 2) {
              m_reslutnodelist[ia].s_inode = -1;
              m_reslutnodelist[ia].s_ilevel = 2;
            }
          }
        }
      }
      if (2 == ilevel) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {

          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }
      }
    }
  }
  if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {

    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                           .arg(m_dvalue)
                           .arg(m_dmaxvalue)
                           .arg(imatchobjectb)
                           .arg(imatchobjectw)
                           .arg(m_imodelobjectb)
                           .arg(m_imodelobjectw);
    Shape::setname(astr.toStdString().c_str());
  } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
    if (m_idebugrectsnum >= iareasnum)
      return;
    selectresultnode(ilevel, m_idebugrectsnum, m_iselmaxnum);
    if (0 == ilevel || 1 == ilevel)
      fastmatch::MatchGrid(m_pimagegrid);
    else if (2 == ilevel) {
      fastmatch::match(g_pbackobjectimage);
      m_dvalue = fastmatch::getmaxresult();
      fastmatch::imagematch(-1, 1);
    }
    imatchobjectb = fastmatch::geteasyobjectb();
    imatchobjectw = fastmatch::geteasyobjectw();
    std::String strt = std::String(" %1 %2 ").arg(m_dvalue).arg(m_dmaxvalue) +
                       m_resultstring +
                       std::String(" %1").arg(m_selectnode.s_inode);
    strt = strt + std::String(" b(%3 %5)w(%4 %6) ")
                      .arg(imatchobjectb)
                      .arg(imatchobjectw)
                      .arg(m_imodelobjectb)
                      .arg(m_imodelobjectw);
    if (2 == ilevel)
      strt = strt + m_fontlist_l12[m_iresultfont];
    Shape::setname(strt.toStdString().c_str());
  } else {
    std::String qshowstr;
    int isize = m_reslutnodelist.size();
    for (int it = 0; it < isize; it++) {
      std::String astrnum = std::String("%3_%2(%1)")
                                .arg(m_reslutnodelist[it].s_ilevel)
                                .arg(m_reslutnodelist[it].s_inode)
                                .arg(it);
      qshowstr = qshowstr + astrnum;
    }
    Shape::setname(qshowstr.toStdString().c_str());
  }
}

void CxTextInspect::fontocr_levelnodelist() {
  int iareasnum = g_pbackfindobject->getresultobjsnum();
  m_resultstrlist.clear();
  m_resultstring.clear();
  if (iareasnum <= 0)
    return;

  m_dvalue = 0;
  m_dmaxvalue = 0;
  resultnodelistreset(iareasnum);

  int imatchobjectw = 0;
  int imatchobjectb = 0;

  for (int ia = 0; ia < iareasnum; ia++) {
    if (-1 == m_idebugrectsnum || ia == m_idebugrectsnum) {
      Rect arect = g_pbackfindobject->getgridex(ia);

      int iobjw = g_pbackfindobject->getresultw(ia);
      int iobjh = g_pbackfindobject->getresulth(ia);
      int imaxlen = iobjw > iobjh ? iobjw : iobjh;
      int igrid_org = fastmatch::GetRectGridLevel(imaxlen);

      int ix = arect.x();
      int iy = arect.y();
      int iw = arect.width();
      int ih = arect.height();

      if (ix < 0 || iy < 0 || iw <= 0 || ih <= 0)
        continue;

      fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackobjectimage->setroi(ix, iy, iw, ih);
        g_pbackimage->setroi(ix, iy, iw, ih);
      g_pbackobjectimage->SetMode(3);
        g_pbackobjectimage->ROItoROI(*g_pbackimage);

      g_pbackimage->ROIColorTable();
      g_pbackimage->ROIColorTableBlur(0, -1);
        g_pbackimage->ROIColorTableEasyThre(1, 0);

      m_pimagegrid->ROIImagetoModel(*g_pbackimage);
      m_pimagegrid->ZeroModel();
      m_pimagegrid->ReGrid(igrid_org, igrid_org);

      m_pimagegrid->GridZoom(3, 3);
      m_pimagegrid->SetUnit(3, 3);

      matchlevelnodelist3x3(ia);

      fastmatch::setmatchrect(ix, iy, iw, ih);
        g_pbackimage->setroi(ix, iy, iw, ih);
      m_pimagegrid->ROIImagetoModel(*g_pbackimage);
      m_pimagegrid->ZeroModel();
      m_pimagegrid->ReGrid(igrid_org, igrid_org);

      m_pimagegrid->GridZoom(6, 6);
      m_pimagegrid->SetUnit(6, 6);

      matchlevelnodelist6x6(ia);
      fastmatch::setmatchrect(ix, iy, iw, ih);
      if (0) {
          g_pbackimage->setroi(ix, iy, iw, ih);
        m_pimagegrid->ROIImagetoModel(*g_pbackimage);

        m_pimagegrid->ZeroModel();
        m_pimagegrid->ReGrid(igrid_org, igrid_org);

        m_pimagegrid->GridZoom(12, 12);
        m_pimagegrid->SetUnit(12, 12);
      }
      matchlevelnodelist12x12(ia);

      m_dmaxvalue = 0;
      m_iselmaxnum = 0;
      m_iresultfont = 0;

      if (0) {
        if (-1 == m_idebugrectsnum && -1 == m_idebugfontnum) {

          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);

          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
          }
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        } else {
          m_reslutnodelist[ia].s_inode = m_iresultfont;
          m_reslutnodelist[ia].s_ilevel = 2;
          int iresultfont = m_iresultfont;
          std::String strkey = m_fontlist_l12[iresultfont];
          m_resultstrlist.push_back(strkey);
          m_resultstring.append(strkey);
        }
      }
    }
  }
  if (0) {
    if (-1 != m_idebugrectsnum && -1 != m_idebugfontnum) {

      imatchobjectb = fastmatch::geteasyobjectb();
      imatchobjectw = fastmatch::geteasyobjectw();
      std::String astr = std::String("  %1   %2 b(%3 %5)w(%4 %6)")
                             .arg(m_dvalue)
                             .arg(m_dmaxvalue)
                             .arg(imatchobjectb)
                             .arg(imatchobjectw)
                             .arg(m_imodelobjectb)
                             .arg(m_imodelobjectw);
      Shape::setname(astr.toStdString().c_str());
    } else if (-1 != m_idebugrectsnum && -1 == m_idebugfontnum) {
      if (m_idebugrectsnum >= iareasnum)
        return;
      fastmatch::MatchGrid(m_pimagegrid);
      {
        fastmatch::match(g_pbackobjectimage);
        m_dvalue = fastmatch::getmaxresult();
        fastmatch::imagematch(-1, 1);
      }
      imatchobjectb = fastmatch::geteasyobjectb();
      imatchobjectw = fastmatch::geteasyobjectw();
      std::String strt = std::String(" %1 %2 ").arg(m_dvalue).arg(m_dmaxvalue) +
                         m_resultstring +
                         std::String(" %1").arg(m_selectnode.s_inode);
      strt = strt + std::String(" b(%3 %5)w(%4 %6) ")
                        .arg(imatchobjectb)
                        .arg(imatchobjectw)
                        .arg(m_imodelobjectb)
                        .arg(m_imodelobjectw);
      strt = strt + m_fontlist_l12[m_iresultfont];
      Shape::setname(strt.toStdString().c_str());
    } else {
      std::String qshowstr;
      int isize = m_reslutnodelist.size();
      for (int it = 0; it < isize; it++) {
        std::String astrnum = std::String("%3_%2(%1)")
                                  .arg(m_reslutnodelist[it].s_ilevel)
                                  .arg(m_reslutnodelist[it].s_inode)
                                  .arg(it);
        qshowstr = qshowstr + astrnum;
      }
      Shape::setname(qshowstr.toStdString().c_str());
    }
  }
}

void CxTextInspect::shownoderesult() {
  std::String qshowstr;
  int isize = m_reslutnodelist.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  for (int i = 0; i < isize; i++) {
    int iresultfont = m_reslutnodelist[i].s_inode;
    if (iresultfont < 0)
      continue;

    QRegExp rxnum("(\\d+)");

    std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
    std::String strkey = m_fontlist_l12[iresultfont];
    if (listother.size() > 0) {
      if (listother[0] != "")
        strkey = listother[0];
      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
      }
      std::StringList strpat = strkey.split("*");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpatA.isEmpty()) {
            strpatA = strpat[0];
            strkey = std::String("");
          } else if (!strpatA.isEmpty()) {
            strpatB = strpat[1];
            { strkey = strpatB; }
            strpatA = std::String("");
            strpatB = std::String("");
          }
        }
      }
      strpat = strkey.split("|");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpata.isEmpty()) {
            strpata = strpat[0];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
            strpatb = strpat[1];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                     (strpatc.isEmpty())) {
            strpatc = strpat[1];
            { strkey = strpatc; }
            strpata = std::String("");
            strpatb = std::String("");
            strpatc = std::String("");
          }
        }
      }
    }
    qshowstr = qshowstr + strkey;
  }

  Shape::setname(qshowstr.toStdString().c_str());
}
void CxTextInspect::shownoderesultex() {
  std::String qshowstr;
  int isize = m_reslutnodelist.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  std::StringList strlista;
  std::StringList strlistb;
  std::StringList strlistc;

  int il12size = getduplicateslist_l12().size();

  for (int i = 0; i < isize; i++) {
    int iresultfont = m_reslutnodelist[i].s_inode;
    if (iresultfont < 0)
      continue;
    int igetvalue = getduplicateslist_l12()[iresultfont];
    if (igetvalue != 0) {
      strlista.clear();
      for (int j = 0; j < il12size; j++) {
        if (igetvalue == getduplicateslist_l12()[j]) {
          std::String strget = ABC2string(m_fontlist_l12[j]);
          if (!strget.isEmpty())
            strlista.push_back(strget);
        }
      }
    }

    QRegExp rxnum("(\\d+)");
    std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

    QRegExp rxother("(\\D+)");
    std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
    std::String strkey = m_fontlist_l12[iresultfont];
    if (listother.size() > 0) {
      if (listother[0] != "")
        strkey = listother[0];
      if (listnum.size() < 2) {
        strkey = listnum[0].mid(0, 1);
      }
      std::StringList strpat = strkey.split("*");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpatA.isEmpty()) {
            strpatA = strpat[0];
            strkey = std::String("");
          } else if (!strpatA.isEmpty()) {
            strpatB = strpat[1];
            {
              if (strlista.size() != 0 && strlistb.size() == 0) {
                strkey = strpatA;
              } else if (strlista.size() == 0 && strlistb.size() != 0) {
                strkey = strpatB;
              } else if (strlista.size() != 0 && strlistb.size() != 0) {
                for (int ia = 0; ia < strlista.size(); ia++) {
                  for (int ib = 0; ib < strlistb.size(); ib++) {
                    if (strlista[ia] == strlistb[ib]) {
                      strkey = strlista[ia];
                      goto ENDIAIBLOOP;
                    }
                  }
                }
              ENDIAIBLOOP:
                strlista.clear();
                strlistb.clear();

              } else
                strkey = strpatB;
            }
            strpatA = std::String("");
            strpatB = std::String("");
            strlista.clear();
            strlistb.clear();
          }
        }
      }
      strpat = strkey.split("|");
      if (strpat.size() > 1) {
        if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
          if (strpata.isEmpty()) {
            strpata = strpat[0];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
            strpatb = strpat[1];
            strkey = std::String("");
          } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                     (strpatc.isEmpty())) {
            strpatc = strpat[1];
            { strkey = strpatc; }
            strpata = std::String("");
            strpatb = std::String("");
            strpatc = std::String("");
            strlista.clear();
            strlistb.clear();
            strlistc.clear();
          }
        }
      }
    }
    qshowstr = qshowstr + strkey;

    strlistb = strlista;
    strlista.clear();
  }

  m_qocrstring = qshowstr;
  Shape::setname(qshowstr.toStdString().c_str());
}
void CxTextInspect::shownodelistresult12x12() {

  std::String qshowstr;
  int isize = m_reslutnodeslistgrid12x12.size();
  std::String strpatA("");
  std::String strpatB("");

  std::String strpata("");
  std::String strpatb("");
  std::String strpatc("");

  std::StringList strlista;
  std::StringList strlistb;
  std::StringList strlistc;

  int il12size = getduplicateslist_l12().size();

  for (int i = 0; i < isize; i++) {
    if (m_idebugrectsnum == -1 || m_idebugrectsnum == i) {
      int inodesize = m_reslutnodeslistgrid12x12[i].getnodes().size();
      if (inodesize > 0) {
        int iresultfont = m_reslutnodeslistgrid12x12[i].getnodes()[0].s_inode;
        if (iresultfont < 0)
          continue;
        int ilistsize = getduplicateslist_l12().size();
        if (ilistsize > iresultfont) {

          int igetvalue = getduplicateslist_l12()[iresultfont];
          if (igetvalue != 0) {
            strlista.clear();
            for (int j = 0; j < il12size; j++) {
              if (igetvalue == getduplicateslist_l12()[j]) {
                std::String strget = ABC2string(m_fontlist_l12[j]);
                if (!strget.isEmpty())
                  strlista.push_back(strget);
              }
            }
          }

          QRegExp rxnum("(\\d+)");
          std::StringList listother = m_fontlist_l12[iresultfont].split(rxnum);

          QRegExp rxother("(\\D+)");
          std::StringList listnum = m_fontlist_l12[iresultfont].split(rxother);
          std::String strkey = m_fontlist_l12[iresultfont];
          if (listother.size() > 0) {
            if (listother[0] != "")
              strkey = listother[0];
            if (listnum.size() < 2) {
              strkey = listnum[0].mid(0, 1);
            }
            std::StringList strpat = strkey.split("*");
            if (strpat.size() > 1) {
              if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
                if (strpatA.isEmpty()) {
                  strpatA = strpat[0];
                  strkey = std::String("");
                } else if (!strpatA.isEmpty()) {
                  strpatB = strpat[1];
                  {
                    if (strlista.size() != 0 && strlistb.size() == 0) {
                      strkey = strpatA;
                    } else if (strlista.size() == 0 && strlistb.size() != 0) {
                      strkey = strpatB;
                    } else if (strlista.size() != 0 && strlistb.size() != 0) {
                      for (int ia = 0; ia < strlista.size(); ia++) {
                        for (int ib = 0; ib < strlistb.size(); ib++) {
                          if (strlista[ia] == strlistb[ib]) {
                            strkey = strlista[ia];
                            goto ENDIAIBLOOP;
                          }
                        }
                      }
                    ENDIAIBLOOP:
                      strlista.clear();
                      strlistb.clear();

                    } else
                      strkey = strpatB;
                  }
                  strpatA = std::String("");
                  strpatB = std::String("");
                  strlista.clear();
                  strlistb.clear();
                }
              }
            }
            strpat = strkey.split("|");
            if (strpat.size() > 1) {
              if ((!strpat[0].isEmpty()) || (!strpat[1].isEmpty())) {
                if (strpata.isEmpty()) {
                  strpata = strpat[0];
                  strkey = std::String("");
                } else if ((!strpata.isEmpty()) && strpatb.isEmpty()) {
                  strpatb = strpat[1];
                  strkey = std::String("");
                } else if ((!strpata.isEmpty()) && (!strpatb.isEmpty()) &&
                           (strpatc.isEmpty())) {
                  strpatc = strpat[1];
                  { strkey = strpatc; }
                  strpata = std::String("");
                  strpatb = std::String("");
                  strpatc = std::String("");
                  strlista.clear();
                  strlistb.clear();
                  strlistc.clear();
                }
              }
            }
          }
          qshowstr = qshowstr + strkey;

          strlistb = strlista;
          strlista.clear();
        } else {
          qshowstr = qshowstr + std::String("?");
        }

      } else {
        qshowstr = qshowstr + std::String("?");
      }
    }
  }

  m_qocrstring = qshowstr;
  Shape::setname(qshowstr.toStdString().c_str());
}

std::String CxTextInspect::ABC2string(std::String strget) {
  std::String strpatA;
  std::String strpatB;

  std::String strpata;
  std::String strpatb;
  std::String strpatc;

  QRegExp rxnum("(\\d+)");
  std::StringList listother = strget.split(rxnum);

  QRegExp rxother("(\\D+)");
  std::StringList listnum = strget.split(rxother);
  std::String strkey = strget;

  if (listother.size() > 0) {
    if (listother[0] != "")
      strkey = listother[0];
    if (listnum.size() < 2) {
      strkey = listnum[0].mid(0, 1);
    }
    std::StringList strpat = strkey.split("*");
    if (strpat.size() > 1) {
      if (!strpat[0].isEmpty()) {
        return strpat[0];
      } else if (!strpat[1].isEmpty()) {
        return strpat[1];
      }
    }
    strpat = strkey.split("|");
    if (strpat.size() > 1) {
      if (!strpat[0].isEmpty()) {
        return strpat[0];
      } else if (!strpat[1].isEmpty()) {
        return strpat[1];
      }
    }
  }
  return std::String("");
}
std::String CxTextInspect::getreslultstring() { return m_qocrstring; }
void CxTextInspect::stringresulthead(const char *pchar) {
  m_qocrstring = std::String(pchar) + m_qocrstring;
}
void CxTextInspect::stringresulttail(const char *pchar) {
  m_qocrstring = m_qocrstring + std::String(pchar);
}

void CxTextInspect::clipboardresult() {
  // Clipboard ownership is provided by the host GUI; the algorithm layer has no Qt dependency.
}

void CxTextInspect::setminscore(double dminscore) {
  fastmatch::setminscore(dminscore);
}
void CxTextInspect::shapesetroi(void *pshape) { Shape::shapesetroi(pshape); }
