#include <cassert>
#include <cmath>
#include "single_select_hero_rune_layout.h"
namespace { bool close_enough(float a,float b){return std::fabs(a-b)<0.0001f;} }
int main(){
 namespace layout=nevergone::single_select_hero_rune_layout;
 assert(layout::frame_index(1,false)==0); assert(layout::frame_index(1,true)==1); assert(layout::frame_index(5,true)==9);
 assert(layout::frame_index(0,false)==-1); assert(layout::frame_index(6,false)==-1);
 for(int tag=1;tag<=5;++tag){assert(layout::enabled_tag(tag)); assert(close_enough(layout::opacity(tag),1.0f));}
 assert(!layout::enabled_tag(0)); assert(!layout::enabled_tag(6));
 assert(close_enough(layout::center_y(1),495.0f)); assert(close_enough(layout::center_y(2),405.0f)); assert(close_enough(layout::center_y(3),315.0f)); assert(close_enough(layout::center_y(4),225.0f)); assert(close_enough(layout::center_y(5),135.0f));
 layout::FrameGeometry one; one.width=79; one.height=99; one.source_width=79; one.source_height=99;
 auto q=layout::quad_for_surface(one,1,1136,640); assert(q.valid); assert(close_enough(q.x0,528.5f*2.0f/1136.0f-1.0f)); assert(close_enough(q.x1,607.5f*2.0f/1136.0f-1.0f));
 layout::FrameGeometry two; two.width=87; two.height=83; two.top=3; two.source_width=87; two.source_height=89;
 q=layout::quad_for_surface(two,2,1136,640); assert(q.valid); assert(close_enough(q.x0,524.5f*2.0f/1136.0f-1.0f)); assert(close_enough(q.x1,611.5f*2.0f/1136.0f-1.0f));
 const auto doubled=layout::quad_for_surface(one,1,2272,1280); assert(doubled.valid); assert(close_enough(doubled.x0,528.5f*2.0f/1136.0f-1.0f));
 layout::FrameGeometry invalid=one; invalid.width=100; assert(!layout::quad_for_surface(invalid,1,1136,640).valid); assert(!layout::quad_for_surface(one,0,1136,640).valid);
 return 0;
}
