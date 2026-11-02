#include "../SimpleGame/CharacterStyle.h"
#include <cassert>
#include <set>
#include <tuple>
#include <limits>

int main()
{
    using namespace Art;
    std::set<std::tuple<unsigned,unsigned,unsigned,unsigned>> seen;
    for(unsigned id=1;id<=Styles::CombinationCount;++id)
    {
        auto s=Styles::Npc(id),again=Styles::Npc(id);
        assert(s.enabled && s.skin<5 && s.hair<8 && s.cloth>=1 && s.cloth<=12 && s.expression<6);
        assert(s.skin==again.skin && s.hair==again.hair && s.cloth==again.cloth && s.expression==again.expression);
        assert(seen.insert(std::make_tuple(s.skin,s.hair,s.cloth,s.expression)).second);
        for(auto color:Styles::Palette(s))for(auto c:color)assert(c>=0 && c<=1 && std::isfinite(c));
    }
    auto player=Styles::Player();assert(player.enabled && player.cloth==0 && player.expression==2);
    auto large=Styles::Npc(std::numeric_limits<unsigned>::max());assert(large.cloth>0 && large.expression<6);
    large.skin=large.hair=large.cloth=std::numeric_limits<unsigned>::max();
    for(auto color:Styles::Palette(large))for(auto c:color)assert(c>=0 && c<=1);
    assert(Styles::Role("MAT_AOE_NPC_Hair")==PaletteRole::Hair);
    assert(Styles::Role("MAT_AOE_NPC_Moss")==PaletteRole::Cloth);
    assert(Styles::Role("MAT_AOE_GhoulSkin")==PaletteRole::None);
    assert(Styles::Role("MAT_AOE_Building_Roof")==PaletteRole::None);
}
