// Hidden OpenGL regression: crossed faces, isolated actor order, and all modular hair/head directions.
// Run from the repository root. Does not open or write any game save.
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/ArtPresentation.h"
#include "GlfwTestContext.h"

namespace
{
    constexpr int Width = 900, Height = 760;
    using Vertex = RenderMeshCache::Vertex;
    using Pixels = std::vector<unsigned char>;

    void Check(bool ok, const char* message)
    {
        if (!ok) throw std::runtime_error(message);
    }

    Pixels Read()
    {
        glFinish();
        Check(glGetError() == GL_NO_ERROR, "OpenGL error in hair/depth render");
        Pixels pixels(Width * Height * 4);
        glReadPixels(0, 0, Width, Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    }

    std::array<int, 3> Pixel(const Pixels& pixels, int x, int y)
    {
        size_t p = ((Height - y - 1) * Width + x) * 4;
        return {{pixels[p], pixels[p + 1], pixels[p + 2]}};
    }

    std::vector<Vertex> Face(Color color, float a, float b, float c)
    {
        return {{10, 10, color.r, color.g, color.b, 1, a, 0},
                {150, 10, color.r, color.g, color.b, 1, b, 0},
                {10, 150, color.r, color.g, color.b, 1, c, 0}};
    }

    void Finish(Renderer& renderer)
    {
        renderer.EndWorld();
        renderer.EndFrame();
    }

    Pixels Crossed(Renderer& renderer, bool reverse, bool crowded = false)
    {
        auto white = Face(Color(1, 1, 1), .9f, .1f, .1f);
        auto black = Face(Color(0, 0, 0), .1f, .9f, .9f);
        std::vector<Vertex> vertices;
        const auto& first = reverse ? black : white;
        const auto& second = reverse ? white : black;
        vertices.insert(vertices.end(), first.begin(), first.end());
        vertices.insert(vertices.end(), second.begin(), second.end());
        renderer.BeginFrame();
        renderer.BeginWorld();
        if (crowded)
            for (int i=0;i<4096;++i) renderer.Rect(Width+20.f,Height+20.f,2,2,Color(1,0,1));
        renderer.CachedVertices(Renderer::MeshKind::Imported, reverse ? 900002 : 900001,
                                0, 0, 1, 1, vertices, false, nullptr, true);
        Finish(renderer);
        return Read();
    }

    void CheckDepthRules(Renderer& renderer, bool hdr)
    {
        auto settings = renderer.GetPostProcessing();
        settings.enabled = hdr;
        settings.bloomStrength = 0;
        settings.edgeBlurStrength = 0;
        settings.vignetteStrength = 0;
        renderer.SetPostProcessing(settings);
        auto ordered = Crossed(renderer, false), reversed = Crossed(renderer, true);
        auto l = Pixel(ordered,25,25), r = Pixel(ordered,105,25);
        std::cout << "crossed hdr=" << hdr << " left=" << l[0] << ',' << l[1] << ',' << l[2]
                  << " right=" << r[0] << ',' << r[1] << ',' << r[2] << '\n';
        Check(Pixel(ordered, 25, 25)[0] > 180 && Pixel(ordered, 105, 25)[0] < 12,
              "Crossing triangles did not choose the nearest face per pixel");
        auto crowded = Crossed(renderer,false,true);
        Check(Pixel(crowded,25,25)[0]>180 && Pixel(crowded,105,25)[0]<12,
              "Depth bands lost hair-scale precision after 4096 painter instances");
        for (int y = 16; y < 125; ++y)
            for (int x = 16; x < 125 - y; ++x)
            {
                if (std::abs(x + y - 89) <= 1) continue; // Exact geometric ties have no unique nearest face.
                auto a = Pixel(ordered, x, y), b = Pixel(reversed, x, y);
                Check(std::abs(a[0] - b[0]) <= 2, "Depth depends on triangle submission order");
            }

        // Closely spaced depths need more than a coarse face bucket. The nearest gray is 1.0.
        std::vector<Vertex> precision;
        for (int i = 31; i >= 0; --i)
        {
            float gray = i / 31.f, depth = .4f + i * .0015f;
            auto face = Face(Color(gray, gray, gray), depth, depth, depth);
            precision.insert(precision.end(), face.begin(), face.end());
        }
        renderer.BeginFrame();
        renderer.BeginWorld();
        renderer.CachedVertices(Renderer::MeshKind::Imported, 900003, 0, 0, 1, 1,
                                precision, false, nullptr, true);
        Finish(renderer);
        Check(Pixel(Read(), 30, 30)[0] > 180, "Close per-pixel depths lost precision");

        // An actor's local geometry depth must not reorder different painter layers.
        renderer.BeginFrame();
        renderer.BeginWorld();
        auto red = Face(Color(1, 0, 0), 1, 1, 1), green = Face(Color(0, 1, 0), 0, 0, 0);
        renderer.CachedVertices(Renderer::MeshKind::Imported, 900004, 0, 0, 1, 1, red, false, nullptr, true);
        renderer.CachedVertices(Renderer::MeshKind::Imported, 900005, 0, 0, 1, 1, green, false, nullptr, true);
        // Force an intervening flush as world occluders/textures also do.
        renderer.Flush();
        auto blue = Face(Color(0, 0, 1), 0, 0, 0);
        renderer.CachedVertices(Renderer::MeshKind::Imported, 900006, 0, 0, 1, 1, blue, false, nullptr, true);
        renderer.EndWorld();
        renderer.Rect(20, 20, 20, 20, Color(1, 1, 0));
        renderer.EndFrame();
        auto isolation = Read();
        Check(Pixel(isolation, 50, 30)[2] > 180 && Pixel(isolation, 50, 30)[0] < 12,
              "Local depth escaped its actor painter layer across a flush");
        auto overlay = Pixel(isolation, 30, 30);
        Check(overlay[0] > 180 && overlay[1] > 180 && overlay[2] < 12,
              "World depth occluded the interface overlay");
    }

    void CheckDefaultFramebuffer(Renderer& renderer)
    {
        auto vertices=Face(Color(1,1,1),.9f,.1f,.1f);
        auto black=Face(Color(0,0,0),.1f,.9f,.9f);
        vertices.insert(vertices.end(),black.begin(),black.end());
        // Avatar previews use the default, single-sample framebuffer after HDR composition.
        renderer.BeginFrame();
        glClearColor(0,0,0,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        for(int i=0;i<4096;++i)renderer.Rect(Width+20.f,Height+20.f,2,2,Color(1,0,1));
        renderer.CachedVertices(Renderer::MeshKind::Imported,900007,0,0,1,1,vertices,false,nullptr,true);
        renderer.EndFrame();
        auto pixels=Read();
        Check(Pixel(pixels,25,25)[0]>180 && Pixel(pixels,105,25)[0]<12,
              "Default framebuffer lost local depth after many overlay instances");
        std::cout<<"default_single_sample_depth preceding_instances=4096 passed\n";
    }

    Art::Vec3 Convert(Art::Vec3 p, float yaw)
    {
        const float x = p.x, y = -p.z;
        return {(x * std::cos(yaw) - y * std::sin(yaw)) * 30,
                (x * std::sin(yaw) + y * std::cos(yaw)) * 30, p.y * 30};
    }

    double Cross(const Vertex& a, const Vertex& b, double x, double y)
    {
        return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
    }

    struct Reference
    {
        std::vector<double> depth = std::vector<double>(Width * Height, -1e30);
        std::vector<int> role = std::vector<int>(Width * Height, 0);
        std::vector<int> fixture = std::vector<int>(Width * Height, -1);
        std::vector<double> otherDepth = std::vector<double>(Width * Height, -1e30);
        std::vector<bool> interior = std::vector<bool>(Width * Height, false);
    };

    void Raster(Reference& ref, const std::vector<Vertex>& vertices, int fixture)
    {
        for (size_t i = 0; i < vertices.size(); i += 3)
        {
            const auto &a = vertices[i], &b = vertices[i + 1], &c = vertices[i + 2];
            double denominator = Cross(a, b, c.x, c.y);
            if (std::abs(denominator) < 1e-9) continue;
            int left = std::max(0, (int)std::floor(std::min({a.x, b.x, c.x})));
            int right = std::min(Width - 1, (int)std::ceil(std::max({a.x, b.x, c.x})));
            int top = std::max(0, (int)std::floor(std::min({a.y, b.y, c.y})));
            int bottom = std::min(Height - 1, (int)std::ceil(std::max({a.y, b.y, c.y})));
            for (int y = top; y <= bottom; ++y)
                for (int x = left; x <= right; ++x)
                {
                    double u = Cross(a, b, x + .5, y + .5) / denominator;
                    double v = Cross(b, c, x + .5, y + .5) / denominator;
                    double w = 1 - u - v;
                    if (u < 0 || v < 0 || w < 0) continue;
                    double depth = a.u * v + b.u * w + c.u * u;
                    size_t pixel = y * Width + x;
                    const int role = a.r > .5f ? 1 : a.g > .5f ? 2 : 3;
                    if (depth <= ref.depth[pixel])
                    {
                        if (role != ref.role[pixel]) ref.otherDepth[pixel] = std::max(ref.otherDepth[pixel], depth);
                        continue;
                    }
                    if (role != ref.role[pixel]) ref.otherDepth[pixel] = ref.depth[pixel];
                    ref.depth[pixel] = depth;
                    ref.role[pixel] = role;
                    ref.fixture[pixel] = fixture;
                    // Skip triangle boundaries, whose top-left rules differ from a CPU inclusive raster.
                    double edgeAB = std::abs(Cross(a,b,x+.5,y+.5)) / std::hypot(b.x-a.x,b.y-a.y);
                    double edgeBC = std::abs(Cross(b,c,x+.5,y+.5)) / std::hypot(c.x-b.x,c.y-b.y);
                    double edgeCA = std::abs(Cross(c,a,x+.5,y+.5)) / std::hypot(a.x-c.x,a.y-c.y);
                    ref.interior[pixel] = std::min({edgeAB,edgeBC,edgeCA}) > 1.1;
                }
        }
    }

    std::vector<Vertex> Diagnostic(const Art::Asset& head, const Art::Asset& hair, float yaw, float x, float y)
    {
        auto model = Art::Assets().Load(head);
        auto triangles = model->SampleMeshes({head.mesh, hair.mesh}, "Idle", .13, true, 0);
        const auto view = Art::Vec3{-.5411961f, -1.30656296f, 2 * .65f / 1.0747093f};
        const float length = std::sqrt(view.x * view.x + view.y * view.y + view.z * view.z);
        std::vector<Vertex> result;
        float nearest = -1e9f, farthest = 1e9f;
        for (const auto& triangle : triangles)
        {
            Art::Vec3 p[3];
            for (int k = 0; k < 3; ++k) p[k] = Convert(triangle.points[k], yaw);
            Art::Vec3 ab{p[1].x-p[0].x, p[1].y-p[0].y, p[1].z-p[0].z}, ac{p[2].x-p[0].x,p[2].y-p[0].y,p[2].z-p[0].z};
            float visible = (ab.y*ac.z-ab.z*ac.y)*view.x+(ab.z*ac.x-ab.x*ac.z)*view.y+(ab.x*ac.y-ab.y*ac.x)*view.z;
            if (!triangle.doubleSided && visible <= 0) continue;
            const bool isHair = triangle.paletteRole == Art::PaletteRole::Hair || triangle.paletteRole == Art::PaletteRole::HairLight;
            const bool isSkin = triangle.paletteRole == Art::PaletteRole::Skin || triangle.paletteRole == Art::PaletteRole::Ear || triangle.paletteRole == Art::PaletteRole::Blush;
            for (int k = 0; k < 3; ++k)
            {
                auto projected = IsometricView::Project({p[k].x, p[k].y, p[k].z});
                float depth = (p[k].x*view.x+p[k].y*view.y+p[k].z*view.z)/length;
                nearest = std::max(nearest, depth); farthest = std::min(farthest, depth);
                result.push_back({x+(float)projected.x*4.f,y+(float)projected.y*4.f,
                                  isHair ? 1.f : 0.f,isSkin ? 1.f : 0.f,(!isHair&&!isSkin) ? 1.f : 0.f,1,depth,0});
            }
        }
        for (auto& vertex : result) vertex.u = (vertex.u - farthest) / (nearest - farthest);
        return result;
    }

    void CheckHairReference(Renderer& renderer)
    {
        auto settings = renderer.GetPostProcessing(); settings.enabled = false; renderer.SetPostProcessing(settings);
        for (int head = 1; head <= 2; ++head)
        {
        Reference ref;
        renderer.BeginFrame(); renderer.BeginWorld();
        int test = (head-1)*20;
            for (int facing = 0; facing < 4; ++facing)
                for (int hair = 1; hair <= 5; ++hair, ++test)
                {
                    auto vertices = Diagnostic(*Art::Assets().Part("Head", head), *Art::Assets().Part("Hair", hair),
                                               Art::FacingYaw((CharacterFacing)facing), 90.f + (hair-1)*180.f,
                                               280.f + facing*190.f);
                    for (const auto& v:vertices)
                        Check(v.x>2 && v.x<Width-2 && v.y>2 && v.y<Height-2,"Hair fixture was clipped by the framebuffer");
                    Raster(ref, vertices, test%20);
                    renderer.CachedVertices(Renderer::MeshKind::Imported, 910000+test, 0,0,1,1,vertices,false,nullptr,true);
                }
        Finish(renderer);
        auto pixels = Read();
        size_t tested = 0, wrongHair = 0, wrongAll = 0, ties = 0, mixed = 0;
        int confusion[4][4] = {};
        std::array<int,20> perFixture = {};
        std::map<std::array<int,3>,int> mismatchedColors;
        for (int y = 0; y < Height; ++y)
            for (int x = 0; x < Width; ++x)
            {
                size_t i = y * Width + x;
                if (!ref.interior[i] || !ref.role[i]) continue;
                if (ref.depth[i]-ref.otherDepth[i]<.0003) { ++ties; continue; }
                auto p = Pixel(pixels,x,y);
                auto channels=p;std::sort(channels.begin(),channels.end());
                // A CPU center sample has no unique color at an MSAA coverage boundary.
                // Exclude only visibly blended samples; a wrongly occluding solid scalp remains tested.
                if (channels[1]>8 || (channels[2]>8 && channels[2]<245)) { ++mixed;continue; }
                int actual = p[0] > 180 ? 1 : p[1] > 180 ? 2 : p[2] > 180 ? 3 : 0;
                ++tested;
                ++perFixture[ref.fixture[i]];
                wrongAll += actual != ref.role[i];
                if (actual!=ref.role[i]) { ++confusion[ref.role[i]][actual]; ++mismatchedColors[p]; }
                wrongHair += ref.role[i] == 1 && actual == 2;
            }
        std::cout << "hair_depth_reference head=" << head << " variants=20 interior_pixels=" << tested
                  << " mismatch=" << wrongAll << " skin_over_hair=" << wrongHair << " depth_ties_skipped=" << ties
                  << " msaa_mixed_skipped=" << mixed << '\n';
        for (int e=1;e<=3;++e) for(int a=0;a<=3;++a) if(confusion[e][a])
            std::cout<<"reference_class="<<e<<" actual_class="<<a<<" count="<<confusion[e][a]<<'\n';
        for(auto item:mismatchedColors)std::cout<<"mismatch_rgb="<<item.first[0]<<','<<item.first[1]<<','<<item.first[2]<<" count="<<item.second<<'\n';
        Check(tested > 50000, "Hair reference did not test enough interior pixels");
        for(auto count:perFixture)Check(count>1000,"A hair/head/direction fixture did not have enough visible tested pixels");
        Check(wrongHair == 0 && wrongAll < 10, "GPU hair visibility disagrees with nearest-depth reference");
        }
    }

    Pixels ActualHair(Renderer& renderer, int head, bool reverse)
    {
        renderer.BeginFrame(); renderer.BeginWorld();
            for (int facing = 0; facing < 4; ++facing)
                for (int hair = 1; hair <= 5; ++hair)
                {
                    Art::DrawRequest r;
                    r.assets = {Art::Assets().Part("Head",head)->id,Art::Assets().Part("Hair",hair)->id};
                    if (reverse) std::reverse(r.assets.begin(),r.assets.end());
                    r.clip="Idle";r.seconds=.13;r.style=Art::Styles::Player();r.style.hair=1;
                    r.yaw=Art::FacingYaw((CharacterFacing)facing);
                    r.x=90.f+(hair-1)*180.f;r.y=280.f+facing*190.f;r.scale=4.f;
                    Check(Art::Draw(renderer,r),"Production hair draw failed");
                }
        Finish(renderer);
        return Read();
    }
}

int main()
{
    try
    {
        GlfwTestContext context(Width,Height,0,"HairDepthRenderTests");
        Check(context.window!=nullptr,"No GL context");
        int actualWidth=0,actualHeight=0;
        glfwGetFramebufferSize(context.window,&actualWidth,&actualHeight);
        std::cout << "framebuffer=" << actualWidth << 'x' << actualHeight << '\n';
        glewExperimental=GL_TRUE;Check(glewInit()==GLEW_OK,"GLEW initialization failed");
        while(glGetError()!=GL_NO_ERROR){}
        Renderer renderer(Width,Height);Check(renderer.IsInitialized(),"Renderer initialization failed");
        renderer.SetProfilingLogInterval(1000);
        CheckDepthRules(renderer,false);CheckDepthRules(renderer,true);
        CheckDefaultFramebuffer(renderer);
        CheckHairReference(renderer);
        for (int head=1;head<=2;++head)
        {
        auto normal=ActualHair(renderer,head,false),reversed=ActualHair(renderer,head,true);
        size_t differences=0;
        for(size_t i=0;i<normal.size();i+=4)
            if(std::max({std::abs((int)normal[i]-reversed[i]),std::abs((int)normal[i+1]-reversed[i+1]),std::abs((int)normal[i+2]-reversed[i+2])})>4)++differences;
        std::cout<<"production_hair_order head="<<head<<" variant_pairs=20 changed_pixels="<<differences<<'\n';
        Check(differences<20,"Head/hair part submission order changed production visibility");
        std::ofstream out("x64/hair-fix/production-hair-head"+std::to_string(head)+"-900x760.rgba",std::ios::binary);
        out.write((const char*)normal.data(),normal.size());
        }
        std::cout<<"Hair depth render tests passed\n";
        return 0;
    }
    catch(const std::exception& error)
    {
        std::cerr<<"HairDepthRenderTests: "<<error.what()<<'\n';
        return 1;
    }
}
