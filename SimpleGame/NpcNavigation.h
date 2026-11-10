#pragma once
#include "GameWorld.h"
#include <queue>
#include <limits>

namespace NpcNavigation
{
    inline bool Clear(Game::Vec a, Game::Vec b)
    {
        if (!Game::Walkable(b)) return false;
        const double minX=std::min(a.x,b.x),maxX=std::max(a.x,b.x);
        const double minY=std::min(a.y,b.y),maxY=std::max(a.y,b.y);
        if(maxX-minX>2800 || maxY-minY>2800) return false;
        auto hitsBox=[&](Game::Vec center,double halfX,double halfY)
        {
            double low=0,high=1;
            auto clip=[&](double start,double delta,double from,double to)
            {
                if(std::abs(delta)<1e-12) return start>from && start<to;
                double t1=(from-start)/delta,t2=(to-start)/delta;
                if(t1>t2)std::swap(t1,t2);
                low=std::max(low,t1);high=std::min(high,t2);
                return low<high;
            };
            return clip(a.x,b.x-a.x,center.x-halfX,center.x+halfX) &&
                   clip(a.y,b.y-a.y,center.y-halfY,center.y+halfY) && low<high;
        };
        // Exact swept-point collision: sparse samples can miss the tip of a
        // building corner, producing a route that fails again at the same place.
        for(int v=0;v<Society::VillageCount;++v)
        {
            auto c=Game::VillageCenter(v);
            if(maxX<c.x-Game::HouseExtent || minX>c.x+Game::HouseExtent || maxY<c.y-Game::HouseExtent || minY>c.y+Game::HouseExtent) continue;
            int x1=std::max(0,(int)std::ceil((minX-c.x-Game::BuildingHalfWidth)/Game::HouseSpacing+3.5));
            int x2=std::min(7,(int)std::floor((maxX-c.x+Game::BuildingHalfWidth)/Game::HouseSpacing+3.5));
            int y1=std::max(0,(int)std::ceil((minY-c.y-Game::BuildingHalfDepth)/Game::HouseSpacing+3.5));
            int y2=std::min(7,(int)std::floor((maxY-c.y+Game::BuildingHalfDepth)/Game::HouseSpacing+3.5));
            for(int y=y1;y<=y2;++y)for(int x=x1;x<=x2;++x)
            {
                if((x==3||x==4)&&(y==3||y==4)) continue;
                if(hitsBox({c.x+(x-3.5)*Game::HouseSpacing,c.y+(y-3.5)*Game::HouseSpacing},Game::BuildingHalfWidth+.01,Game::BuildingHalfDepth+.01)) return false;
            }
            for(int building=0;building<3;++building)
                if(hitsBox(Game::VillageBuilding(v,building),building==2?25.01:Game::BuildingHalfWidth+.01,
                           building==2?23.01:Game::BuildingHalfDepth+.01)) return false;
        }
        const int x1=(int)std::floor((minX-20)/32),x2=(int)std::floor((maxX+20)/32);
        const int y1=(int)std::floor((minY-20)/32),y2=(int)std::floor((maxY+20)/32);
        const double dx=b.x-a.x,dy=b.y-a.y,length=dx*dx+dy*dy;
        for(int y=y1;y<=y2;++y)for(int x=x1;x<=x2;++x)
        {
            if(!Game::Obstacle(x,y))continue;
            Game::Vec c(x*32.0+16,y*32.0+16);
            double t=length>1e-12?std::max(0.0,std::min(1.0,((c.x-a.x)*dx+(c.y-a.y)*dy)/length)):0;
            double ex=a.x+dx*t-c.x,ey=a.y+dy*t-c.y;
            if(ex*ex+ey*ey<400.4001)return false;
        }
        return true;
    }

    constexpr double Cell = 16;
    inline bool Open(long long x, long long y)
    {
        struct Entry { long long x = 0, y = 0; bool valid = false, open = false; };
        static thread_local std::array<Entry, 16384> cache;
        auto& e = cache[Game::Hash((int)x, (int)y, 433) % cache.size()];
        if (!e.valid || e.x != x || e.y != y)
        {
            e = {x, y, true, Game::Walkable({x*Cell, y*Cell})};
        }
        return e.open;
    }

    inline bool Edge(long long x, long long y, int dx, int dy)
    {
        struct Entry { long long x = 0, y = 0; int direction = -1; bool valid = false, clear = false; };
        static thread_local std::array<Entry, 16384> cache;
        const int direction = (dy+1)*3+dx+1;
        auto& e = cache[Game::Hash((int)x, (int)y, 457+direction) % cache.size()];
        if (!e.valid || e.x != x || e.y != y || e.direction != direction)
            e = {x, y, direction, true, Clear({x*Cell,y*Cell},{(x+dx)*Cell,(y+dy)*Cell})};
        return e.clear;
    }

    inline bool Plan(Game::Vec start, Game::Vec goal, std::vector<std::array<double,2>>& path)
    {
        path.clear();
        if (!Game::Walkable(goal)) return false;
        const long long left = (long long)std::floor(std::min(start.x,goal.x)/Cell)-10;
        const long long top = (long long)std::floor(std::min(start.y,goal.y)/Cell)-10;
        const int width = (int)std::ceil(std::abs(start.x-goal.x)/Cell)+23;
        const int height = (int)std::ceil(std::abs(start.y-goal.y)/Cell)+23;
        if (width > 160 || height > 160) return false;
        const int sx = (int)std::llround(start.x/Cell-left), sy = (int)std::llround(start.y/Cell-top);
        int first = -1;
        double closest = 1e30;
        for (int y=sy-1;y<=sy+1;++y) for (int x=sx-1;x<=sx+1;++x)
        {
            Game::Vec p((left+x)*Cell,(top+y)*Cell);
            double d = Game::Distance(start,p);
            if (d < closest && Open(left+x,top+y) && Clear(start,p)) { first=y*width+x;closest=d; }
        }
        if (first < 0) return false;
        struct Node { double cost = 1e30; int parent = -1; bool closed = false; };
        struct Visit { double score, cost; int id; bool operator<(const Visit& b) const { return score > b.score || (score == b.score && id > b.id); } };
        std::vector<Node> nodes(width*height);
        std::priority_queue<Visit> pending;
        auto point = [&](int id) { return Game::Vec((left+id%width)*Cell,(top+id/width)*Cell); };
        nodes[first].cost = 0;
        pending.push({Game::Distance(point(first),goal),0,first});
        int found = -1, expanded = 0;
        while (!pending.empty() && expanded < 4096)
        {
            auto item=pending.top();pending.pop();
            auto& node=nodes[item.id];
            if (node.closed || item.cost > node.cost) continue;
            node.closed=true;++expanded;
            Game::Vec p=point(item.id);
            if (Game::Distance(p,goal) <= Cell*1.5 && Clear(p,goal)) { found=item.id;break; }
            int x=item.id%width,y=item.id/width;
            for (int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
            {
                int nx=x+dx,ny=y+dy;
                if ((!dx&&!dy) || nx<0 || ny<0 || nx>=width || ny>=height || !Open(left+nx,top+ny)) continue;
                if (!Edge(left+x,top+y,dx,dy)) continue;
                int id=ny*width+nx;
                double cost=node.cost+(dx&&dy ? Cell*1.4142135623730951 : Cell);
                if (cost >= nodes[id].cost) continue;
                nodes[id].cost=cost;nodes[id].parent=item.id;
                pending.push({cost+Game::Distance(point(id),goal),cost,id});
            }
        }
        if (found < 0) return false;
        std::vector<Game::Vec> reverse;
        for(int at=found;at>=0;at=nodes[at].parent) reverse.push_back(point(at));
        std::reverse(reverse.begin(),reverse.end());reverse.push_back(goal);
        Game::Vec from=start;
        for(size_t i=0;i<reverse.size();)
        {
            size_t last=i;
            // Smooth the grid route into a small number of stable, collision-checked legs.
            for(size_t j=i;j<reverse.size() && j<i+32;++j) if(Clear(from,reverse[j])) last=j;
            from=reverse[last];path.push_back({{from.x,from.y}});i=last+1;
            if(path.size()>96) {path.clear();return false;}
        }
        return !path.empty();
    }
}
