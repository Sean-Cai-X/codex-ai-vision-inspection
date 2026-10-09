#include "../../cximage/CxUiFont.h"
#include <iostream>
int main(int argc, char** argv) {
  ImGui::CreateContext();
  auto& io = ImGui::GetIO();
  const auto status = cxuifont::Load(io, argc > 1 ? argv[1] : "/missing/cxvision-cjk-font.ttf");
  unsigned char* pixels = nullptr; int width = 0, height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  bool ok = pixels && width > 0 && height > 0;
  ok = ok && io.Fonts->Fonts[0]->FindGlyphNoFallback(ImWchar('A'));
  ok = ok && status.find(argc > 1 ? "UI_FONT_CJK_READY" : "UI_FONT_CJK_MISSING") == 0;
  std::cout << status << " atlas=" << width << "x" << height << "\n";
  ImGui::DestroyContext();
  return ok ? 0 : 1;
}
