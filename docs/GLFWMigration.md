# GLFW 프레임워크 전환

## 빌드와 의존성

현재 솔루션은 Windows용이다. Visual Studio 2022의 Desktop development with C++와 Windows 10/11 SDK를 설치하고 `SimpleGame.sln`을 빌드한다. Debug/Release, Win32/x64 네 구성이 같은 저장소의 소스로 빌드된다. 게임의 GDI 텍스트·저장 API가 Windows 전용이므로 Linux/macOS 전체 게임 빌드를 지원한다는 의미는 아니다.

- GLFW 3.4: `SimpleGame/Dependencies/GLFW/glfw-3.4`에 공식 소스·헤더·라이선스를 보관한다. `GLFW.vcxproj`를 프로젝트 참조로 자동 빌드·정적 링크한다. CMake 설치와 인터넷 연결이 필요 없다.
- GLEW 2.2.0: `SimpleGame/Dependencies/GLEW/glew-2.2.0`의 소스와 같은 버전 헤더를 함께 사용한다. 기존 정적 바이너리는 LIBCMT로 만들어져 있었으므로 프로젝트의 DLL CRT 설정과 충돌하지 않도록 현재 구성으로 직접 컴파일한다.
- FreeGLUT 링크와 런타임 DLL 의존성을 제거했다. GLFW/GLEW DLL 복사도 필요 없다. Windows 시스템 라이브러리는 SDK를 사용한다.
- 저장소 Art 폴더 또는 `GSE_ART_ROOT` 지정은 기존과 같다. 게임 실행에 필요한 OpenGL 3.3 드라이버는 별도 시스템 요건이다.

공식 소스는 그대로 보관하고 앱 설정은 별도 프로젝트에 둔다. 생성되는 `Dependencies/GLFW/build`는 Git에서 제외한다. 저장소 경로는 프로젝트 기준 상대 경로로 계산한다.

## 실행과 성능

OpenGL 3.3 core, double buffer, MSAA 4 설정과 기존 렌더러·메시/포즈 캐시·프로파일을 유지한다. 렌더러·게임 로직 변경으로 프레임워크 전환 결과를 섞지 않는다.

메인 루프는 이벤트 처리 → Update/모니터/자동 저장 → Render → Swap 순서로 한 번씩 수행한다. 시간은 `glfwGetTime()`의 초 단위 double로 계산하고 기존 50ms dt 상한을 유지한다. 최소화 또는 0 크기 프레임버퍼에서는 최대 0.1초 이벤트 대기로 CPU 공회전과 렌더링을 줄인다. 종료 시 저장·모니터 종료·GPU 자원 해제를 마친 뒤 창과 GL 컨텍스트를 해제한다.

기본은 `glfwSwapInterval(1)`이다. 모니터 주사율에 맞춰 동기화하므로 무제한 실행과 FPS를 직접 비교하면 안 된다. 상한 없는 비교는 `SimpleGame.exe --no-vsync`로 수행한다. 드라이버 설정이 VSync를 강제로 지정할 수도 있다.

키 반복 이벤트는 무시하고 기존 키 누름 상태와 단발 명령을 보존한다. ESC/F1/F6/F8, 마우스 클릭·휠도 연결했다. 포커스를 잃으면 키 상태를 비워 이동 고착을 방지한다. 마우스는 논리 창 좌표를 프레임버퍼 픽셀 좌표로 변환하며 리사이즈는 프레임버퍼 콜백을 사용한다.

## 확인 항목

기존 `tools/Measure-ArtRenderer.ps1`, `Measure-Gameplay.ps1`의 숨겨진 OpenGL 컨텍스트도 GLFW로 전환했다. 이 스크립트는 사용자 실행 시 라이브러리·벤치마크를 빌드하고 별도 데이터 디렉터리를 사용한다. 테스트 컨텍스트는 VSync 0이며 실제 플레이 루프 벤치마크는 production Display/Swap 경로를 사용한다.

전환 전후 동일한 Release/x64, GPU, 드라이버, 저장 복사본, 해상도, HDR, 이동 경로와 VSync 조건에서 측정한다. 시작 워밍업을 제외하고 기존 `Summarize-RenderProfile.ps1`로 프레임 간격 p95, CPU render, game update, previous swap, draw calls, uploads와 cache misses를 비교한다. 기본 VSync 실행과 무제한 벤치마크는 따로 비교한다.

코드·프로젝트 정적 점검: 애플리케이션/테스트/측정 도구의 FreeGLUT 사용 제거, 네 구성의 소스 경로와 XML, PowerShell 구문, 기존 draw call 및 Update/Swap 계측 유지 여부를 확인한다.

사용자의 기존 선호에 따라 게임 빌드·실행과 실제 FPS 비교는 자동 수행하지 않았다. 따라서 정량적인 성능 저하 없음은 아직 실측으로 확정하지 않았다. 사용자 측 실행 시 입력·줌·포커스 복귀·최소화 복귀·DPI 변경·종료 저장도 함께 확인한다.
