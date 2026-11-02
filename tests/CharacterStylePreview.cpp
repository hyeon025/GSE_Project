// Production renderer preview: player + stable NPC identities, and all expressions.
#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include "../SimpleGame/Renderer.h"
#include "../SimpleGame/Dependencies/freeglut.h"
#include "../SimpleGame/ArtPresentation.h"

int main(int argc,char** argv)
{
    SetEnvironmentVariableA("LOCALAPPDATA",argc>1?argv[1]:"x64/perf/character-variation-data");
    glutInit(&argc,argv);glutInitContextVersion(3,3);glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_MULTISAMPLE);glutSetOption(GLUT_MULTISAMPLE,4);
    glutInitWindowSize(1440,1000);int window=glutCreateWindow("Character variation review");glutHideWindow();
    glewExperimental=GL_TRUE;if(glewInit()!=GLEW_OK)return 2;
    while(glGetError()!=GL_NO_ERROR){}
    glEnable(GL_MULTISAMPLE);
    {
        Renderer renderer(1440,1000);if(!renderer.IsInitialized())return 3;
        auto post=renderer.GetPostProcessing();post.enabled=false;renderer.SetPostProcessing(post);
        for(int sheet=0;sheet<3;++sheet)
        {
            renderer.BeginFrame();renderer.BeginWorld();
            renderer.Rect(0,0,1440,1000,Color(.88f,.86f,.80f));
            const bool expressions=sheet==1;
            const int count=expressions?6:15;
            for(int i=0;i<count;++i)
            {
                Art::DrawRequest request;
                request.assets=sheet || i==0?Art::Assets().PlayerRecipe():Art::Assets().Recipe(i);
                request.style=i==0?Art::Styles::Player():Art::Styles::Npc(i);
                if(expressions){request.style=Art::Styles::Player();request.style.expression=i;}
                if(sheet==2)request.style.expression=2; // Same geometry, fifteen instance palettes.
                request.preciseDepth=true;request.clip="Idle";request.seconds=.2;
                request.yaw=Art::FacingYaw(CharacterFacing::Front);
                request.x=(expressions?240.f:145.f)+(i%(expressions?3:5))*(expressions?480.f:285.f);
                request.y=(expressions?420.f:310.f)+(i/(expressions?3:5))*(expressions?460.f:320.f);
                request.scale=expressions?5.f:3.f;
                if(!Art::Draw(renderer,request))throw std::runtime_error("Character style fallback");
                std::wstring label=expressions ? std::wstring(Art::Styles::Expression(i),Art::Styles::Expression(i)+std::strlen(Art::Styles::Expression(i))) : i==0?L"PLAYER":L"NPC #"+std::to_wstring(i);
                renderer.Text(request.x-68,request.y+18,label,Color(.16f,.17f,.15f),18);
            }
            renderer.EndWorld();renderer.EndFrame();glFinish();
            if(glGetError()!=GL_NO_ERROR)return 4;
            std::vector<unsigned char> pixels(1440*1000*4);
            glReadPixels(0,0,1440,1000,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
            std::ofstream file("x64/perf/character-variation-"+std::to_string(sheet)+".rgba",std::ios::binary);
            file.write((const char*)pixels.data(),pixels.size());
        }
    }
    glutDestroyWindow(window);
}
