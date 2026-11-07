#pragma once
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <ostream>
#include <string>
#include "CharacterStyle.h"

namespace Game
{
    struct PlayerProfile
    {
        // Gender is a fixed rule, never an editable or deserialized field.
        std::wstring name = L"\uc5ec\ud589\uc790", motto;
        unsigned hairShape = 3, outfit = 1;
        Art::CharacterStyle style = Art::Styles::Player();

        static const wchar_t* Gender()
        {
            return L"\uc5ec\uc131";
        }

        static constexpr size_t NameLimit = 16, MottoLimit = 32;
    };

    inline bool ProfileTextValid(const std::wstring& text, size_t limit)
    {
        if (text.size() > limit)
        {
            return false;
        }
        for (size_t i = 0; i < text.size(); ++i)
        {
            unsigned code = (unsigned)text[i];
            if (code < 32 || code == 127)
            {
                return false;
            }
            if (code >= 0xd800 && code <= 0xdbff)
            {
                if (++i >= text.size() || text[i] < 0xdc00 || text[i] > 0xdfff)
                {
                    return false;
                }
            }
            else if (code >= 0xdc00 && code <= 0xdfff)
            {
                return false;
            }
        }
        return true;
    }

    inline bool ProfileValid(const PlayerProfile& p)
    {
        return !p.name.empty() && p.name.find_first_not_of(L" ") != std::wstring::npos &&
               ProfileTextValid(p.name, PlayerProfile::NameLimit) &&
               ProfileTextValid(p.motto, PlayerProfile::MottoLimit) && p.hairShape >= 1 && p.hairShape <= 5 &&
               p.outfit >= 1 && p.outfit <= 6 && p.style.skin < Art::Styles::SkinCount &&
               p.style.hair < Art::Styles::HairCount && p.style.cloth <= Art::Styles::NpcClothCount &&
               p.style.expression < Art::Styles::ExpressionCount;
    }

    inline void WriteProfile(std::ostream& out, const PlayerProfile& p)
    {
        out << "PROFILE 1 " << p.hairShape << ' ' << p.outfit << ' ' << p.style.skin << ' ' << p.style.hair << ' '
            << p.style.cloth << ' ' << p.style.expression << '\n';
        for (const auto* text : {&p.name, &p.motto})
        {
            out << text->size();
            for (wchar_t code : *text)
            {
                out << ' ' << (unsigned)code;
            }
            out << '\n';
        }
    }

    inline bool ReadProfile(std::istream& in, PlayerProfile& profile)
    {
        PlayerProfile p;
        std::string magic;
        unsigned version = 0;
        if (!(in >> magic >> version >> p.hairShape >> p.outfit >> p.style.skin >> p.style.hair >> p.style.cloth >>
              p.style.expression) ||
            magic != "PROFILE" || version != 1)
        {
            return false;
        }
        for (auto* text : {&p.name, &p.motto})
        {
            size_t size = 0;
            size_t limit = text == &p.name ? PlayerProfile::NameLimit : PlayerProfile::MottoLimit;
            if (!(in >> size) || size > limit)
            {
                return false;
            }
            text->clear();
            for (size_t i = 0; i < size; ++i)
            {
                unsigned code = 0;
                if (!(in >> code) || code > 0xffff)
                {
                    return false;
                }
                text->push_back((wchar_t)code);
            }
        }
        if (!ProfileValid(p))
        {
            return false;
        }
        profile = p;
        return true;
    }

    inline void AppendProfileCharacter(std::wstring& text, unsigned code, size_t limit)
    {
        if (code < 32 || code == 127 || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
        {
            return;
        }
        size_t units = code > 0xffff ? 2 : 1;
        if (text.size() + units > limit)
        {
            return;
        }
        if (units == 1)
        {
            text.push_back((wchar_t)code);
        }
        else
        {
            code -= 0x10000;
            text.push_back((wchar_t)(0xd800 + (code >> 10)));
            text.push_back((wchar_t)(0xdc00 + (code & 1023)));
        }
    }

    inline void EraseProfileCharacter(std::wstring& text)
    {
        if (text.empty())
        {
            return;
        }
        bool low = text.back() >= 0xdc00 && text.back() <= 0xdfff;
        text.pop_back();
        if (low && !text.empty() && text.back() >= 0xd800 && text.back() <= 0xdbff)
        {
            text.pop_back();
        }
    }
}
