#pragma once

#include <string>
#include <vector>

#include "../DevAnalysisGuiState.h"

struct DevAnalysisDemoElement
{
  int entity_id = 0;
  std::string name;
  std::string entity_type;
  std::string kind_label;
  std::string source_stage;
  std::string status;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  bool visible = true;
  bool interactive = true;
};

struct DevAnalysisDemoAnnotation
{
  int annotation_id = 0;
  int target_entity_id = 0;
  std::string annotation_type;
  std::string text;
  float anchor_x = 0.0f;
  float anchor_y = 0.0f;
  float label_dx = 0.0f;
  float label_dy = 0.0f;
  std::string layer_name;
  bool visible = true;
};

struct DevAnalysisGeomAttachmentScene
{
  std::vector<DevAnalysisDemoElement> elements;
  std::vector<DevAnalysisDemoAnnotation> annotations;
  std::string attachment_rule;
  std::string interaction_rule;
};

DevAnalysisGeomAttachmentScene BuildDevAnalysisGeomAttachmentScene(DevAnalysisChainId chain_id);
