#pragma once
#include <algorithm>
#include <string>

namespace home {
// Approximate charge using piecewise interpolation of Seeed's E-series points:
// https://wiki.seeedstudio.com/reterminal_e10xx_with_esphome_advanced/#battery-monitoring
// This voltage estimate is not a fuel gauge; charging/load can affect it.
inline int batteryPercent(int millivolts){
    if(millivolts<=0)return -1; // Missing measurement is unknown, not an empty cell.
    struct Point {int mv,percent;};
    constexpr Point curve[]={{3270,0},{3300,5},{3410,10},{3490,20},{3580,30},{3680,40},
        {3750,50},{3800,60},{3850,70},{3910,80},{3960,90},{4150,100}};
    if(millivolts<=curve[0].mv)return 0;
    for(unsigned i=1;i<sizeof(curve)/sizeof(curve[0]);i++){
        const auto& low=curve[i-1];const auto& high=curve[i];
        if(millivolts<=high.mv){
            int span=high.mv-low.mv;
            return low.percent+((millivolts-low.mv)*(high.percent-low.percent)+span/2)/span;
        }
    }
    return 100;
}
inline std::string batteryLabel(int percent){return percent<0?"?":"~"+std::to_string(percent)+"%";}
inline int batteryFill(int percent){return percent<=0?0:std::max(1,(std::min(percent,100)*9+50)/100);}
}
