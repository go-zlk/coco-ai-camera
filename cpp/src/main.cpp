#include "coco/api.hpp"
#include "coco/detector.hpp"
#include "coco/source.hpp"
#include "coco/state.hpp"
#include "coco/store.hpp"
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
namespace {
volatile std::sig_atomic_t stopping=0;
void interrupt(int) { stopping=1; }
}
int main(int argc,char** argv) {
  try {
    std::map<std::string,std::string> args{{"--source-id","camera_1"},{"--backend","tensorrt"},{"--db","data/context.db"},{"--port","8090"},{"--imgsz","640"},{"--conf","0.25"},{"--interval-ms","1000"},{"--seconds","0"}};
    for(int i=1;i<argc;++i) {
      std::string key=argv[i];
      if(key=="--help") { std::cout<<"coco-context --source <RTSP|csi|USB index> --model <raw.engine|model.onnx> [--backend tensorrt|onnx] [--db data/context.db] [--source-id camera_1] [--port 8090] [--seconds 0] [--interval-ms 1000] [--imgsz 640] [--conf 0.25]\n"; return 0; }
      if((!args.count(key)&&key!="--source"&&key!="--model")||i+1>=argc) throw std::runtime_error("Unknown argument or missing value");
      args[key]=argv[++i];
    }
    if(!args.count("--source")||!args.count("--model")) throw std::runtime_error("--source and --model are required");
    int port=std::stoi(args["--port"]),size=std::stoi(args["--imgsz"]),interval=std::stoi(args["--interval-ms"]);
    double seconds=std::stod(args["--seconds"]); float conf=std::stof(args["--conf"]);
    if(port<1||port>65535||size<32||interval<1||seconds<0||conf<=0||conf>=1) throw std::runtime_error("Invalid configuration range");
    auto detector=coco::make_detector(args["--backend"],args["--model"],size,conf);
    coco::EventStore store(args["--db"]);
    coco::Context current; current.source_id=args["--source-id"];
    std::mutex context_mutex;
    coco::ContextApi api(port,[&](const std::string& path)->std::string {
      if(path=="/v1/events") return store.timeline();
      if(path=="/v1/context/current"||path=="/health") { std::lock_guard<std::mutex> lock(context_mutex); return coco::context_json(current); }
      return {};
    });
    coco::CameraSource source(args["--source"]);
    coco::PresenceState person("person"),cat("cat");
    std::signal(SIGINT,interrupt); std::signal(SIGTERM,interrupt);
    api.start(); source.start();
    auto started=coco::Clock::now(),last_frame=started,next=started;
    uint64_t inferred=0;
    auto emit=[&](std::optional<coco::Event> event) {
      if(event) { store.append(*event); std::cout<<"[event] "<<event->category<<' '<<event->state<<' '<<event->observed_at<<std::endl; }
    };
    std::cout<<"[service] local API port "<<port<<"; raw media storage disabled\n";
    while(!stopping && (seconds==0||std::chrono::duration<double>(coco::Clock::now()-started).count()<seconds)) {
      auto frame=source.take(std::chrono::milliseconds(100));
      auto now=coco::Clock::now();
      if(frame && std::chrono::duration<double>(now-frame->received).count()<2) {
        last_frame=frame->received;
        if(now<next) continue;
        auto begin=coco::Clock::now(); auto detections=detector->infer(*frame);
        now=coco::Clock::now(); next=now+std::chrono::milliseconds(interval); ++inferred;
        size_t people=0,cats=0; float pc=0,cc=0;
        for(const auto& d:detections) {
          if(d.class_id==0) { ++people; pc=std::max(pc,d.confidence); }
          if(d.class_id==15) { ++cats; cc=std::max(cc,d.confidence); }
        }
        emit(person.update(people>0,pc,true,frame->received,current.source_id,frame->observed_at));
        emit(cat.update(cats>0,cc,true,frame->received,current.source_id,frame->observed_at));
        std::lock_guard<std::mutex> lock(context_mutex);
        current.health="online"; current.person=person.state(); current.cat=cat.state();
        current.person_count=people; current.cat_count=cats; current.sequence=frame->sequence;
        current.observed_at=frame->observed_at; current.updated=frame->received;
        current.inference_ms=std::chrono::duration<double,std::milli>(now-begin).count(); current.dropped_frames=source.dropped();
      } else if(std::chrono::duration<double>(now-last_frame).count()>=5) {
        auto at=coco::utc_now();
        emit(person.update(false,0,false,now,current.source_id,at)); emit(cat.update(false,0,false,now,current.source_id,at));
        std::lock_guard<std::mutex> lock(context_mutex);
        current.health="offline"; current.person=person.state(); current.cat=cat.state(); current.person_count=0; current.cat_count=0;
      }
    }
    source.stop(); api.stop();
    std::cout<<"[summary] inference_checks="<<inferred<<" dropped_frames="<<source.dropped()<<'\n';
    if(inferred==0) return 2;
    return 0;
  } catch(const std::exception& e) { std::cerr<<"[fatal] "<<e.what()<<'\n'; return 1; }
}
