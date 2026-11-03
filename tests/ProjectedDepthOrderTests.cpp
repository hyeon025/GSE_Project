#include "../SimpleGame/ProjectedDepthOrder.h"
#include <cassert>
#include <set>

int main()
{
    using namespace ProjectedDepthOrder;
    Triangle wall={{{0,0,0},{10,0,0},{0,10,100}}};
    Triangle door={{{1,1,15},{2,1,15},{1,2,25}}};
    // Centroid depth incorrectly puts the small inset behind the tall wall.
    auto order=Order({door,wall});
    assert(order.size()==2 && order[0]==1 && order[1]==0);
    auto reverse=door;std::swap(reverse[0],reverse[1]);
    order=Order({reverse,wall});assert(order[0]==1 && order[1]==0);
    Triangle separate={{{20,20,0},{21,20,0},{20,21,0}}};
    order=Order({door,separate});assert(order[0]==0 && order[1]==1);
    Triangle edge={{{0,0,0},{0,0,1},{0,0,2}}};
    order=Order({door,wall,separate,edge});
    assert(std::set<size_t>(order.begin(),order.end())==std::set<size_t>({0,1,2,3}));
    Point p;assert(!Overlap(separate,wall,p));assert(Overlap(door,wall,p));
    assert(Depth(door,p)>Depth(wall,p));
    // Multiple layers in separate horizontal clusters exercise sweep pruning.
    std::vector<Triangle> layers;
    for (int depth=4;depth>=0;--depth)
        for (int cluster=0;cluster<8;++cluster)
        {
            double x=cluster*20.;
            layers.push_back({{{x,0,(double)depth},{x+10,0,(double)depth},{x,10,(double)depth}}});
        }
    order=Order(layers);
    std::vector<size_t> position(layers.size());
    for(size_t i=0;i<order.size();++i)position[order[i]]=i;
    for(size_t cluster=0;cluster<8;++cluster)
        for(size_t layer=0;layer<4;++layer)
            assert(position[(layer+1)*8+cluster]<position[layer*8+cluster]);
}
