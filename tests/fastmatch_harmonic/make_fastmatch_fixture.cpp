#include <fstream>
#include <iostream>
#include <string>
// Deterministic raster fixture, generated outside the checkout. No Python/OpenCV.
int main(int argc,char** argv) {
    if(argc!=2)return 64;
    std::ofstream f(argv[1],std::ios::binary);
    f<<"P5\n320 240\n255\n";
    for(int y=0;y<240;++y)for(int x=0;x<320;++x) {
        unsigned char value=(x>=90 && x<230 && y>=80 && y<160)?30:220;
        f.put(static_cast<char>(value));
    }
    f.close();if(!f)return 1;
    std::cout<<"fixture_written\n";
}
