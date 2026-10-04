#include "coco/state.hpp"
#include "coco/store.hpp"
#include "coco/detector.hpp"
#include <iostream>
#include <stdexcept>
void require(bool ok) { if(!ok) throw std::runtime_error("State/store check failed"); }
int main() {
  coco::PresenceState state("person"); auto t=coco::Clock::now();
  auto update=[&](bool seen,bool online,int seconds) { return state.update(seen,.9f,online,t+std::chrono::seconds(seconds),"test","2026-01-01T00:00:00Z"); };
  require(!update(true,true,0)); require(update(true,true,1).has_value());
  require(!update(false,true,2)); require(!update(true,true,3));
  require(!update(false,true,4)); require(update(false,true,9).has_value());
  require(state.state()=="out_of_view"); require(update(false,false,10).has_value());
  require(!update(true,true,11)); require(update(true,true,12).has_value());
  require(!update(true,true,13));
  coco::EventStore store(":memory:"); store.append({"test","person","visible","time",.9f});
  require(store.timeline().find("visible")!=std::string::npos);
  require(coco::json_string("a\"\n")=="\"a\\\"\\u000a\"");
  // Overlapping person boxes must suppress within class, but not suppress a cat.
  std::vector<float> tensor(84*3,0);
  for(int i=0;i<3;++i) { tensor[i]=320; tensor[3+i]=320; tensor[6+i]=100; tensor[9+i]=100; }
  tensor[4*3]=.9f; tensor[4*3+1]=.8f; tensor[(4+15)*3+2]=.7f;
  auto prep=coco::prepare(cv::Mat(480,848,CV_8UC3,cv::Scalar(0,0,0)),640);
  auto boxes=coco::decode_yolo(tensor.data(),84,3,prep,{848,480},.25f);
  require(boxes.size()==2 && boxes[0].class_id==0 && boxes[1].class_id==15);
  require(boxes[0].box.x>=0 && boxes[0].box.y>=0);
  // A stronger chair class must not become a cat merely through filtering.
  tensor[(4+56)*3+2]=.95f;
  boxes=coco::decode_yolo(tensor.data(),84,3,prep,{848,480},.25f);
  require(boxes.size()==1 && boxes[0].class_id==0);
  std::cout<<"state debounce, offline/recovery, SQLite and JSON checks passed\n";
}
