#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace Art
{
    // The numbers are also used by Instanced.vs. Geometry is shared across colors.
    enum class PaletteRole : unsigned char
    {
        None, Skin, Ear, Blush, Hair, HairLight, Cloth, Trim, Linen, LinenShade
    };
    struct CharacterStyle
    {
        bool enabled = false;
        unsigned skin = 0, hair = 0, cloth = 0, expression = 0;
    };
    namespace Styles
    {
        constexpr unsigned SkinCount=5, HairCount=8, NpcClothCount=12, ExpressionCount=6;
        constexpr unsigned CombinationCount=SkinCount*HairCount*NpcClothCount*ExpressionCount;
        constexpr unsigned Revision=1;
        inline CharacterStyle Npc(unsigned identity)
        {
            CharacterStyle s;s.enabled=true;
            unsigned serial=(std::uint64_t(identity ? identity-1 : 0)*137)%CombinationCount;
            s.cloth=1+serial%NpcClothCount;serial/=NpcClothCount;
            s.hair=serial%HairCount;serial/=HairCount;
            s.skin=serial%SkinCount;serial/=SkinCount;
            s.expression=(serial+s.cloth*5+s.hair*3+s.skin*2)%ExpressionCount;
            return s;
        }
        inline CharacterStyle Player()
        {
            CharacterStyle s;s.enabled=true;s.skin=1;s.hair=7;s.cloth=0;s.expression=2;return s;
        }
        inline float Linear(float v) { return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f); }
        using Rgb=std::array<float,3>;
        inline std::array<Rgb,4> Palette(const CharacterStyle& s)
        {
            static const Rgb skin[SkinCount]={{{.93f,.75f,.56f}},{{.86f,.63f,.43f}},{{.73f,.47f,.29f}},{{.55f,.33f,.20f}},{{.37f,.23f,.16f}}};
            static const Rgb hair[HairCount]={{{.12f,.09f,.075f}},{{.32f,.16f,.075f}},{{.74f,.55f,.25f}},{{.60f,.25f,.115f}},{{.43f,.40f,.34f}},{{.22f,.13f,.10f}},{{.76f,.75f,.69f}},{{.13f,.20f,.25f}}};
            // Index zero is reserved for the player: deep teal with brass trim.
            static const Rgb cloth[NpcClothCount+1]={{{.14f,.40f,.46f}},{{.63f,.27f,.19f}},{{.28f,.40f,.61f}},{{.36f,.48f,.25f}},{{.53f,.29f,.48f}},{{.71f,.53f,.26f}},{{.33f,.47f,.45f}},{{.48f,.23f,.28f}},{{.50f,.43f,.30f}},{{.43f,.37f,.57f}},{{.73f,.58f,.43f}},{{.23f,.34f,.40f}},{{.47f,.49f,.46f}}};
            static const Rgb trim[NpcClothCount+1]={{{.83f,.65f,.32f}},{{.83f,.69f,.43f}},{{.71f,.79f,.82f}},{{.67f,.65f,.40f}},{{.78f,.63f,.62f}},{{.40f,.31f,.21f}},{{.74f,.70f,.48f}},{{.69f,.55f,.38f}},{{.77f,.73f,.59f}},{{.74f,.68f,.81f}},{{.42f,.31f,.26f}},{{.68f,.68f,.57f}},{{.76f,.66f,.48f}}};
            std::array<Rgb,4> result={{skin[s.skin%SkinCount],hair[s.hair%HairCount],cloth[s.cloth%(NpcClothCount+1)],trim[s.cloth%(NpcClothCount+1)]}};
            for(auto& rgb:result) for(auto& v:rgb)v=Linear(v);
            return result;
        }
        inline PaletteRole Role(const std::string& name)
        {
            if(name=="MAT_AOE_NPC_Skin")return PaletteRole::Skin;
            if(name=="MAT_AOE_NPC_Ear")return PaletteRole::Ear;
            if(name=="MAT_AOE_NPC_Blush")return PaletteRole::Blush;
            if(name=="MAT_AOE_NPC_Hair")return PaletteRole::Hair;
            if(name=="MAT_AOE_NPC_HairLight")return PaletteRole::HairLight;
            if(name=="MAT_AOE_NPC_Teal" || name=="MAT_AOE_NPC_Moss" || name=="MAT_AOE_NPC_Wine" || name=="MAT_AOE_NPC_Rust" || name=="MAT_AOE_NPC_IronDark")return PaletteRole::Cloth;
            if(name=="MAT_AOE_NPC_TealTrim" || name=="MAT_AOE_NPC_MossLight")return PaletteRole::Trim;
            if(name=="MAT_AOE_NPC_Linen")return PaletteRole::Linen;
            if(name=="MAT_AOE_NPC_LinenShade")return PaletteRole::LinenShade;
            return PaletteRole::None;
        }
        inline const char* Expression(unsigned index)
        {
            static const char* names[]={"Gentle","Neutral","Determined","Concerned","Sleepy","Cheerful"};
            return names[index%ExpressionCount];
        }
    }
}
