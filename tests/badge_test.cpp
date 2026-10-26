#include "badge_format.h"
#include "ui_layout.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <vector>
#include <zlib.h>

int main(int argc,char** argv) {
    // HTTPS writes may refresh lastData after the UI sampled its frame time.
    assert(!badge::uploadExpired(1000,1001));
    assert(!badge::uploadExpired(1000,1050));
    assert(!badge::uploadExpired(1000,1000));
    assert(!badge::uploadExpired(121000,1000));
    assert(badge::uploadExpired(121001,1000));
    constexpr uint32_t end=std::numeric_limits<uint32_t>::max();
    assert(!badge::uploadExpired(end-5,4));
    assert(!badge::uploadExpired(5,end-5));
    assert(badge::uploadExpired(120001,end));
    assert(argc==2);
    std::ifstream stream(argv[1],std::ios::binary);
    std::vector<uint8_t> file{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
    badge::Info info;
    assert(badge::parse(file.data(),file.size(),file.size(),info));
    assert(info.count==1 && info.bytes==file.size() && info.milliseconds==1000);
    std::vector<uint8_t> pixels(badge::frameBytes);uLongf size=pixels.size();
    const auto& frame=info.frames[0];
    assert(uncompress(pixels.data(),&size,file.data()+frame.offset,frame.bytes)==Z_OK && size==badge::frameBytes);
    const auto pixel=[&](int x,int y){return badge::u16(pixels.data()+(y*466+x)*2);};
    assert(pixel(5,5)==0xf800 && pixel(460,5)==0x07e0 && pixel(5,460)==0x001f && pixel(460,460)==0xffff);
    for(size_t at:{size_t(0),size_t(4),size_t(8),size_t(10),size_t(12),size_t(16)}) {
        auto broken=file;broken[at]^=0x7f;
        assert(!badge::parse(broken.data(),broken.size(),broken.size(),info));
    }
    assert(!badge::parse(file.data(),15,file.size(),info));
    assert(!badge::parse(file.data(),file.size(),file.size()-1,info));
    auto broken=file;broken[20]=broken[21]=broken[22]=broken[23]=0;
    assert(!badge::parse(broken.data(),broken.size(),broken.size(),info));
    broken=file;broken[24]=broken[25]=broken[26]=broken[27]=255;
    assert(!badge::parse(broken.data(),broken.size(),broken.size(),info));
    const float gravity[4][2]={{-1,0},{0,-1},{1,0},{0,1}};
    const float angles[4]={0,90,180,-90};
    for(size_t i=0;i<4;++i) {
        badge::Orientation pose;
        pose.update(gravity[i][0],gravity[i][1],0,0,1);
        assert(std::abs(badge::wrap(pose.angle()-angles[i]))<0.001f);
    }
    badge::Orientation orientation;
    assert(orientation.drawAngle(false)==0);
    for(int step=1;step<=16;++step) {
        orientation.rotateManual();
        assert(orientation.drawAngle(false)==(step%8)*45);
    }
    orientation.rotateManual();
    orientation.update(0,-1,0,0,1);
    assert(orientation.drawAngle(true)==-90);
    assert(orientation.drawAngle(false)==45);
    orientation.reset();
    assert(orientation.drawAngle(false)==45);
    orientation.update(-1,0,0,0,1);assert(std::abs(orientation.angle())<0.001f);
    for(int i=0;i<40;++i) orientation.update(0,-1,0,0,0.025f);
    assert(orientation.angle()>89 && orientation.angle()<=90);
    const float saved=orientation.angle();
    orientation.update(std::numeric_limits<float>::quiet_NaN(),0,0,0,0.025f);
    assert(orientation.angle()==saved);
    orientation.update(0,0,1,std::numeric_limits<float>::max(),0.025f);assert(orientation.angle()==saved);
    orientation.update(0,0,1,0,0.1f);assert(orientation.angle()==saved);
    orientation.update(0,0,1,-90,0.1f);assert(std::abs(badge::wrap(orientation.angle()-saved)-9)<0.01f);
    orientation.reset();orientation.update(0.999848f,-0.0174524f,0,0,1);
    const float start=orientation.angle();orientation.update(0.999848f,0.0174524f,0,0,0.025f);
    assert(std::abs(badge::wrap(orientation.angle()-start))<2);
    for(int selected=0;selected<ui::homeCount;++selected) {
        assert((ui::hit(ui::Screen::Home,233,233,selected)==ui::Target{ui::Action::Open,selected}));
        assert((ui::hit(ui::Screen::Home,346,351,selected)==ui::Target{ui::Action::Open,(selected+ui::homeCount-1)%ui::homeCount}));
    }
    assert(ui::hit(ui::Screen::Badge,112,84,0,false,false).action==ui::Action::Open);
    assert(ui::hit(ui::Screen::Badge,112,84,0,false,true).action==ui::Action::Back);
    assert(ui::hit(ui::Screen::Badge,233,389,0,false,true).action==ui::Action::Select);
    std::cout<<"OK: iPhone RGB565/zlib interoperability, malformed Badge bounds, orientation, gravity fallback and home navigation\n";
}
