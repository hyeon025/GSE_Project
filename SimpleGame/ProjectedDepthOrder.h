#pragma once
#include <array>
#include <vector>
#include <queue>
#include <functional>
#include <cmath>
#include <algorithm>

// Cached model painter ordering: compare depth where triangles actually overlap,
// rather than comparing their centroids (which hides doors behind tall wall faces).
namespace ProjectedDepthOrder
{
    struct Point { double x, y, depth; };
    using Triangle = std::array<Point, 3>;
    inline double Cross(Point a, Point b, Point p)
    {
        return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
    }
    inline double Depth(const Triangle& t, Point p)
    {
        double denominator = Cross(t[0],t[1],t[2]);
        double b = Cross(t[0],p,t[2])/denominator;
        double c = Cross(t[0],t[1],p)/denominator;
        return t[0].depth+b*(t[1].depth-t[0].depth)+c*(t[2].depth-t[0].depth);
    }
    inline bool Overlap(const Triangle& a, const Triangle& b, Point& sample)
    {
        if (std::abs(Cross(a[0],a[1],a[2]))<1e-8 || std::abs(Cross(b[0],b[1],b[2]))<1e-8) return false;
        // Clipping two triangles produces at most six vertices. Keep scratch
        // storage on the stack; an NPC pose can perform thousands of comparisons.
        std::array<Point,12> polygon{},clipped{};
        std::copy(a.begin(),a.end(),polygon.begin());
        size_t count=3;
        const double orientation = Cross(b[0],b[1],b[2])>0 ? 1 : -1;
        for (int edge=0;edge<3 && count;++edge)
        {
            size_t outputCount=0;
            Point previous=polygon[count-1];
            double last=orientation*Cross(b[edge],b[(edge+1)%3],previous);
            for (size_t i=0;i<count;++i)
            {
                auto current=polygon[i];
                double distance=orientation*Cross(b[edge],b[(edge+1)%3],current);
                if ((last>=0)!=(distance>=0))
                {
                    double t=last/(last-distance);
                    clipped[outputCount++]={previous.x+(current.x-previous.x)*t,
                                            previous.y+(current.y-previous.y)*t,0};
                }
                if (distance>=0) clipped[outputCount++]=current;
                previous=current;last=distance;
            }
            polygon.swap(clipped);count=outputCount;
        }
        if (count<3) return false;
        double area=0;sample={0,0,0};
        for (size_t i=0;i<count;++i)
        {
            auto p=polygon[i],q=polygon[(i+1)%count];
            area+=p.x*q.y-p.y*q.x;sample.x+=p.x;sample.y+=p.y;
        }
        if (std::abs(area)<1e-6) return false;
        sample.x/=count;sample.y/=count;return true;
    }
    inline std::vector<size_t> Order(const std::vector<Triangle>& triangles)
    {
        struct Bounds {double left,right,top,bottom;};
        const size_t n=triangles.size();
        std::vector<Bounds> bounds;
        for (const auto& t:triangles)
        {
            Bounds b={t[0].x,t[0].x,t[0].y,t[0].y};
            for (const auto& p:t) {b.left=std::min(b.left,p.x);b.right=std::max(b.right,p.x);b.top=std::min(b.top,p.y);b.bottom=std::max(b.bottom,p.y);}
            bounds.push_back(b);
        }
        std::vector<std::vector<size_t>> edges(n);
        std::vector<size_t> incoming(n,0),result;
        std::vector<size_t> sweep(n);
        for(size_t i=0;i<n;++i) sweep[i]=i;
        std::stable_sort(sweep.begin(),sweep.end(),[&](size_t a,size_t b){return bounds[a].left<bounds[b].left;});
        for (size_t si=0;si<n;++si) for(size_t sj=si+1;sj<n;++sj)
        {
            const size_t i=sweep[si],j=sweep[sj];
            const auto& a=bounds[i];const auto& b=bounds[j];
            // Once the next left edge passes this right edge, all later
            // triangles are disjoint too. Vertical bounds prune the survivors.
            if(b.left>=a.right) break;
            if(a.right<=b.left || b.right<=a.left || a.bottom<=b.top || b.bottom<=a.top) continue;
            Point p;
            if(!Overlap(triangles[i],triangles[j],p)) continue;
            double delta=Depth(triangles[i],p)-Depth(triangles[j],p);
            if(std::abs(delta)<1e-4) continue;
            size_t far=delta<0?i:j,near=delta<0?j:i;
            edges[far].push_back(near);++incoming[near];
        }
        std::priority_queue<size_t,std::vector<size_t>,std::greater<size_t>> ready;
        std::vector<bool> done(n,false);
        for(size_t i=0;i<n;++i) if(!incoming[i]) ready.push(i);
        while(result.size()<n)
        {
            // Intersecting solids may form a cycle; keep every face in stable fallback order.
            if(ready.empty()) for(size_t i=0;i<n;++i) if(!done[i]) {ready.push(i);break;}
            size_t i=ready.top();ready.pop();if(done[i]) continue;
            done[i]=true;result.push_back(i);
            for(auto j:edges[i]) if(!done[j] && incoming[j] && !--incoming[j]) ready.push(j);
        }
        return result;
    }
}
