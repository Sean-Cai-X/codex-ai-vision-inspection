#pragma once
#include <complex>
#include <map>
#include <vector>
#include <stdexcept>
#include <cstdint>
namespace cxgeom {
struct PixelCellEdge {int x=0,y=0,side=0;};
struct PixelCellContour {std::vector<std::complex<double>> points;std::vector<PixelCellEdge> edges;};
// Boundary of the union of selected unit pixel cells, centered at integer (x,y).
// Not an estimate of the continuous physical edge. Reject holes, disconnected loops and corner contacts.
inline PixelCellContour TraceSinglePixelCellBoundary(const std::vector<uint8_t>& mask,int width,int height){
 if(width<3||height<3||int64_t(width)*height>16777216||mask.size()!=size_t(width)*height)throw std::invalid_argument("CELL_MASK_SIZE");
 using P=std::pair<int,int>;struct Edge {P end;PixelCellEdge source;};
 std::map<P,Edge> edges;std::map<P,int> incoming;
 auto hit=[&](int x,int y){return x>=0&&y>=0&&x<width&&y<height&&mask[size_t(y)*width+x]!=0;};
 auto add=[&](P a,P b,int x,int y,int side){if(!edges.emplace(a,Edge{b,{x,y,side}}).second||++incoming[b]!=1)throw std::invalid_argument("CELL_BOUNDARY_JUNCTION");if(edges.size()>4096)throw std::invalid_argument("CELL_BOUNDARY_BUDGET");};
 for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(hit(x,y)){
  if(x==0||y==0||x==width-1||y==height-1)throw std::invalid_argument("CELL_BOUNDARY_IMAGE_BORDER");
  int l=2*x-1,r=2*x+1,t=2*y-1,b=2*y+1;
  if(!hit(x,y-1))add({l,t},{r,t},x,y,0);
  if(!hit(x+1,y))add({r,t},{r,b},x,y,1);
  if(!hit(x,y+1))add({r,b},{l,b},x,y,2);
  if(!hit(x-1,y))add({l,b},{l,t},x,y,3);
 }
 if(edges.empty())throw std::invalid_argument("CELL_BOUNDARY_EMPTY");
 PixelCellContour out;P start=edges.begin()->first,p=start;
 do{auto it=edges.find(p);if(it==edges.end())throw std::invalid_argument("CELL_BOUNDARY_BROKEN");
  out.points.emplace_back(p.first*.5,p.second*.5);out.edges.push_back(it->second.source);p=it->second.end;edges.erase(it);
 }while(p!=start);
 if(!edges.empty())throw std::invalid_argument("CELL_BOUNDARY_MULTIPLE_LOOPS");
 return out;
}
}
