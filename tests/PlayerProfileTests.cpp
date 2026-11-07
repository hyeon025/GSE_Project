#include "../SimpleGame/PlayerProfile.h"
#include <cassert>
#include <sstream>

int main()
{
    using namespace Game;
    PlayerProfile profile;
    assert(ProfileValid(profile));
    assert(std::wstring(PlayerProfile::Gender()) == L"\uc5ec\uc131");

    profile.name = L"\uc544\ub9b0 \"A\"";
    profile.motto = L"\ud76c\ub9dd\uc744 \ucc3e\uc544";
    profile.hairShape = 5;
    profile.outfit = 6;
    profile.style.skin = 4;
    profile.style.hair = 7;
    profile.style.cloth = 12;
    profile.style.expression = 5;
    std::stringstream save;
    WriteProfile(save, profile);
    PlayerProfile restored;
    assert(ReadProfile(save, restored));
    assert(restored.name == profile.name && restored.motto == profile.motto);
    assert(restored.hairShape == 5 && restored.outfit == 6 && restored.style.enabled);
    assert(restored.style.skin == 4 && restored.style.hair == 7);
    assert(restored.style.cloth == 12 && restored.style.expression == 5);

    // A malformed profile must never partially replace an existing profile.
    for (const char* corrupt :
         {"PROFILE 1 5 6 4 7 12 5\n1 55296\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n1 56320\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n1 1114112\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n999999\n",
          "PROFILE 1 5 6 4 7 12 5\n2 65",
          "PROFILE 1 5 6 4 7 12 5\n1 10\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n1 32\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n0\n0\n",
          "PROFILE 1 5 6 4 7 12 5\n1 65\n33\n",
          "PROFILE 1 5 6 5 7 12 5\n1 65\n0\n",
          "PROFILE 1 5 6 4 8 12 5\n1 65\n0\n",
          "PROFILE 1 5 6 4 7 13 5\n1 65\n0\n",
          "PROFILE 1 5 6 4 7 12 6\n1 65\n0\n",
          "PROFILE 1 0 6 4 7 12 5\n1 65\n0\n",
          "PROFILE 1 5 7 4 7 12 5\n1 65\n0\n",
          "PROFILE 2 5 6 4 7 12 5\n1 65\n0\n"})
    {
        std::stringstream damaged(corrupt);
        assert(!ReadProfile(damaged, restored));
        assert(restored.name == profile.name && restored.motto == profile.motto);
        assert(restored.hairShape == 5 && restored.outfit == 6 && restored.style.skin == 4);
    }

    std::wstring text(PlayerProfile::NameLimit - 1, L'A');
    AppendProfileCharacter(text, 0x1f642, PlayerProfile::NameLimit);
    assert(text.size() == PlayerProfile::NameLimit - 1);
    EraseProfileCharacter(text);
    AppendProfileCharacter(text, 0x1f642, PlayerProfile::NameLimit);
    assert(ProfileTextValid(text, PlayerProfile::NameLimit));
    assert(text.size() == PlayerProfile::NameLimit);
    EraseProfileCharacter(text);
    assert(text.size() == PlayerProfile::NameLimit - 2);
    for (unsigned invalid : {0u, 10u, 127u, 0xd800u, 0xdfffu, 0x110000u})
    {
        auto before = text;
        AppendProfileCharacter(text, invalid, PlayerProfile::NameLimit);
        assert(text == before);
    }
    profile.motto.clear();
    assert(ProfileValid(profile));
    profile.name.assign(PlayerProfile::NameLimit + 1, L'A');
    assert(!ProfileValid(profile));
}
