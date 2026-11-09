#include "stdafx.h"
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "Renderer.h"

void Renderer::ReleaseText()
{
    for (const auto& entry : m_Text)
    {
        if (entry.second.standalone)
        {
            glDeleteTextures(1, &entry.second.id);
        }
    }
    for (const auto& page : m_TextPages)
    {
        glDeleteTextures(1, &page.texture);
    }
    m_Text.clear();
    m_TextPages.clear();
}

void Renderer::ReleaseTextResources()
{
    ReleaseText();
    if (m_TextDc)
    {
        DeleteDC(static_cast<HDC>(m_TextDc));
        m_TextDc = nullptr;
    }
    for (const auto& item : m_TextFonts) DeleteObject(static_cast<HFONT>(item.second));
    m_TextFonts.clear();
}

bool Renderer::UploadText(TextTexture& text, const void* pixels)
{
    if (text.width > m_MaxTextureSize || text.height > m_MaxTextureSize)
    {
        CacheEvent("text_too_large", {MeshKind::UnitQuad, 0}, (size_t)text.width * text.height);
        return false;
    }
    glActiveTexture(GL_TEXTURE0);
    if (text.width + 2 > m_TextPageSize || text.height + 2 > m_TextPageSize)
    {
        text.standalone = true;
        glGenTextures(1, &text.id);
        glBindTexture(GL_TEXTURE_2D, text.id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, text.width, text.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    }
    else
    {
        if (!m_TextPages.empty())
        {
            auto& page = m_TextPages.back();
            if (page.x + text.width + 1 > m_TextPageSize)
            {
                page.x = 1;
                page.y += page.rowHeight + 2;
                page.rowHeight = 0;
            }
        }
        if (m_TextPages.empty() || m_TextPages.back().y + text.height + 1 > m_TextPageSize)
        {
            if (m_TextPages.size() >= 4)
            {
                FlushBatch(BatchBreak::Cache);
                ReleaseText();
                CacheEvent("text_atlas_reset", {MeshKind::UnitQuad, 0});
            }
            TextPage page;
            glGenTextures(1, &page.texture);
            glBindTexture(GL_TEXTURE_2D, page.texture);
            std::vector<unsigned char> empty((size_t)m_TextPageSize * m_TextPageSize * 4, 255);
            for (size_t i = 3; i < empty.size(); i += 4)
            {
                empty[i] = 0;
            }
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                m_TextPageSize,
                m_TextPageSize,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                empty.data()
            );
            m_Stats.Add(RenderCounter::TextureUploadBytes, empty.size());
            m_TextPages.push_back(page);
            CacheEvent("text_atlas_page", {MeshKind::UnitQuad, m_TextPages.size()});
        }
        auto& page = m_TextPages.back();
        text.id = page.texture;
        text.u = (float)page.x / m_TextPageSize;
        text.v = (float)page.y / m_TextPageSize;
        text.du = (float)text.width / m_TextPageSize;
        text.dv = (float)text.height / m_TextPageSize;
        glBindTexture(GL_TEXTURE_2D, page.texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, page.x, page.y, text.width, text.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        page.x += text.width + 2;
        page.rowHeight = std::max(page.rowHeight, text.height);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_Stats.Add(RenderCounter::TextureUploadBytes, (size_t)text.width * text.height * 4);
    return true;
}

void Renderer::Text(float x, float y, const std::wstring& text, Color color, int size)
{
    if (text.empty())
    {
        return;
    }
    auto key = std::make_pair(text, size);
    auto found = m_Text.find(key);
    m_Stats.Add(found == m_Text.end() ? RenderCounter::TextMisses : RenderCounter::TextHits);
    if (found == m_Text.end())
    {
        if (m_Text.size() >= 512)
        {
            FlushBatch(BatchBreak::Cache);
            ReleaseText();
            CacheEvent("text_entry_reset", {MeshKind::UnitQuad, 0});
        }
        if (!m_TextDc) m_TextDc = CreateCompatibleDC(nullptr);
        HDC dc = static_cast<HDC>(m_TextDc);
        if (!dc)
        {
            return;
        }
        auto cachedFont = m_TextFonts.find(size);
        if (cachedFont == m_TextFonts.end())
        {
            // A renderer uses only a handful of HUD sizes. Keep a hard bound for
            // arbitrary callers without throwing away fonts on text-atlas resets.
            if (m_TextFonts.size() >= 32)
            {
                auto victim = m_TextFonts.begin();
                DeleteObject(static_cast<HFONT>(victim->second));
                m_TextFonts.erase(victim);
            }
            cachedFont = m_TextFonts.emplace(size, nullptr).first;
            cachedFont->second = CreateFontW(
                -size,
                0,
                0,
                0,
                FW_NORMAL,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY,
                DEFAULT_PITCH,
                L"Malgun Gothic"
            );
            if (!cachedFont->second)
            {
                m_TextFonts.erase(cachedFont);
                return;
            }
        }
        HFONT font = static_cast<HFONT>(cachedFont->second);
        if (!font)
        {
            return;
        }
        HGDIOBJ oldFont = SelectObject(dc, font);
        if (!oldFont || oldFont == HGDI_ERROR) return;
        // Restore both selections on every exit, including upload/allocation
        // failures, so cached fonts can always be evicted or destroyed safely.
        struct RasterSelection
        {
            HDC dc;
            HGDIOBJ oldFont, oldBitmap;
            HBITMAP bitmap;
            ~RasterSelection()
            {
                if (oldBitmap) SelectObject(dc, oldBitmap);
                SelectObject(dc, oldFont);
                if (bitmap) DeleteObject(bitmap);
            }
        } selection{dc, oldFont, nullptr, nullptr};
        SIZE extent = {};
        if (!GetTextExtentPoint32W(dc, text.c_str(), (int)text.size(), &extent)) return;
        int w = std::max(1L, extent.cx + 4), h = std::max((LONG)size + 6, extent.cy + 4);
        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w;
        info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        selection.bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!selection.bitmap || !bits)
        {
            return;
        }
        selection.oldBitmap = SelectObject(dc, selection.bitmap);
        if (!selection.oldBitmap || selection.oldBitmap == HGDI_ERROR)
        {
            selection.oldBitmap = nullptr;
            return;
        }
        memset(bits, 0, (size_t)w * h * 4);
        SetBkColor(dc, RGB(0, 0, 0));
        SetTextColor(dc, RGB(255, 255, 255));
        TextOutW(dc, 1, 1, text.c_str(), (int)text.size());
        GdiFlush();
        auto pixels = static_cast<unsigned char*>(bits);
        for (int i = 0; i < w * h; ++i)
        {
            pixels[i * 4 + 3] = pixels[i * 4];
            pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = 255;
        }
        TextTexture t;
        t.width = w;
        t.height = h;
        bool uploaded = UploadText(t, bits);
        if (!uploaded)
        {
            return;
        }
        found = m_Text.insert(std::make_pair(key, t)).first;
    }
    const auto& t = found->second;
    const auto& mesh = GetMesh(
        MeshKind::UnitQuad,
        0,
        [this]
        {
            Rect(0, 0, 1, 1, Color(1, 0, 0));
        }
    );
    Instance instance;
    instance.matrix[0] = (float)t.width;
    instance.matrix[3] = (float)t.height;
    instance.placement[0] = x;
    instance.placement[1] = y;
    instance.colors[0] = color;
    instance.options[0] = 2;
    instance.uv[0] = t.u;
    instance.uv[1] = t.v;
    instance.uv[2] = t.du;
    instance.uv[3] = t.dv;
    QueueInstance(mesh, instance, t.id);
}
