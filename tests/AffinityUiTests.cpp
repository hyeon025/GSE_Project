// Exercise the actual HUD and action routing in a hidden GL context, without the real save/monitor.
#define main EmbeddedGameEntry
#include "../SimpleGame/SimpleGame.cpp"
#undef main
#include "GlfwTestContext.h"
#include <cassert>
#include <fstream>

void DrawTestUi(const char* path = nullptr)
{
    g_Renderer->BeginFrame();
    g_Renderer->BeginWorld();
    g_Renderer->EndWorld();
    RenderUI();
    g_Renderer->EndFrame();
    glFinish();
    assert(glGetError() == GL_NO_ERROR);
    if (path)
    {
        std::vector<unsigned char> pixels(g_Width * g_Height * 4);
        glReadPixels(0, 0, g_Width, g_Height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::ofstream file(path, std::ios::binary);
        file.write((const char*)pixels.data(), pixels.size());
    }
}

void ClickAction(int action)
{
    auto it = std::find_if(g_Buttons.begin(), g_Buttons.end(), [&](const Button& b) { return b.action == action && b.enabled; });
    assert(it != g_Buttons.end());
    Mouse(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, (int)((it->x + it->w / 2) * g_UiScale), (int)((it->y + it->h / 2) * g_UiScale));
}

int main()
{
    GlfwTestContext context(1440, 900, 4, "AffinityUiTests");
    assert(context.window);
    g_Window = context.window;
    glewExperimental = GL_TRUE;
    assert(glewInit() == GLEW_OK);
    while (glGetError() != GL_NO_ERROR) {}
    glEnable(GL_MULTISAMPLE);
    g_SaveBlocked = true;
    g_World.levelOne.active = false;
    Society::Initialize(g_World);
    g_Renderer.reset(new Renderer(1440, 900));
    assert(g_Renderer->IsInitialized());
    g_Scene.reset(new GameScene(*g_Renderer, g_World));
    auto& n = g_World.npcs[0];
    n.p = g_World.player.p;
    n.life.health = 70;
    g_TargetKind = 2;
    g_TargetIndex = 0;
    g_Dialog = true;
    Act(Talk);
    assert(n.met && n.relationship == 5);
    Act(Open);
    Act(Share);
    assert(n.relationship == 30 && g_World.player.bread == 2);
    Act(Open);
    Act(RescueNpc);
    assert(n.relationship == 65 && n.saved && g_World.player.herbs == 0);
    Act(Open);
    Act(RescueNpc);
    assert(n.relationship == 65); // Disabled choice cannot consume resources or apply a second reward.
    Act(ThreatenNpc);
    assert(n.relationship == 25 && n.trust == 1);
    g_World.society.elapsed = 60;
    Act(Open);
    Act(Talk);
    assert(n.relationship == 30);
    Act(Open);
    DrawTestUi("x64/affinity-tests/dialog-1440x900.rgba");
    Act(Close);
    Affinity::Apply(g_World, g_World.npcs[1], Affinity::Threat);
    Affinity::Apply(g_World, g_World.npcs[2], Affinity::Bread);
    KeyDown('h', 0, 0);
    KeyUp('h', 0, 0);
    assert(g_Panel == 5);
    DrawTestUi("x64/affinity-tests/ranking-1440x900.rgba");
    assert(g_AffinityRanking.size() == 350 && g_World.npcs[g_AffinityRanking[0]].id == n.id);
    ClickAction(AffinitySort);
    DrawTestUi();
    assert(g_World.npcs[g_AffinityRanking[0]].id == 2);
    ClickAction(AffinitySelect + 2);
    DrawTestUi();
    assert(g_AffinitySelected == 2);
    ClickAction(AffinityNext);
    DrawTestUi();
    assert(g_AffinityPage == 1);
    ClickAction(AffinityMet);
    DrawTestUi();
    assert(g_AffinityPage == 0 && g_AffinityRanking.size() == 1);
    ClickAction(AffinityVillage);
    DrawTestUi();
    assert(g_AffinityVillage == 0 && g_AffinityRanking.size() == 1);
    ClickAction(AffinityVillage);
    DrawTestUi();
    assert(g_AffinityRanking.empty());
    ClickAction(Close);
    assert(g_Panel == 0);
    g_AffinityMetOnly = false;
    g_AffinityVillage = -1;
    g_AffinityDescending = true;
    glfwSetWindowSize(g_Window, 960, 640);
    glfwPollEvents();
    Reshape(960, 640);
    Act(AffinityOpen);
    DrawTestUi("x64/affinity-tests/ranking-960x640.rgba");
    for (const auto& b : g_Buttons)
        assert(b.x >= 0 && b.y >= 0 && b.x + b.w <= UiWidth() && b.y + b.h <= UiHeight());
    KeyDown(27, 0, 0);
    assert(g_Panel == 0);
    g_Scene.reset();
    g_Renderer.reset();
    g_Window = nullptr;
    std::cout << "Affinity UI tests passed: interaction actions, keyboard, clicks, selection, sort, filters, pages, 960x640/1440x900\n";
}
