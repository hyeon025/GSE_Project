# 렌더링 배치·메시 캐시 개선 보고서

작성일: 2026-09-22
상태: 구현 및 정적 검토. 빌드, 게임 실행, GPU 측정, 회귀 테스트 실행은 하지 않음.

## 1. 관측 자료와 한계

사용자가 전달한 기존 로그:

| 프레임 | 해당 프레임 draw call | 구간 평균 | 최소 | 최대 | FPS | 구간 프레임 수 |
|---:|---:|---:|---:|---:|---:|---:|
| 1708 | 337 | 342.7 | 337 | 351 | 60.0 | 60 |
| 1765 | 351 | 345.8 | 337 | 355 | 56.1 | 57 |
| 1826 | 353 | 349.1 | 345 | 359 | 60.0 | 61 |
| 1886 | 349 | 350.6 | 347 | 357 | 60.0 | 60 |

이 자료만으로 CPU 제출 비용, GPU 처리 비용, VSync 대기 중 어느 것이 병목인지 확정할 수 없다.
특히 60 FPS는 VSync 제한일 가능성도 있다. 이번 변경의 실제 감소율과 FPS 개선은 아직 측정하지 않았다.
기존 자료는 구간 요약이라 새 프레임 CSV와 동일한 원시 데이터는 아니다.

## 2. 기존 코드에서 확인한 원인

1. CachedMesh가 메시마다 Flush 후 glDrawArrays를 호출했다.
   CPU에서 메시를 재생성하지 않는 것과 draw call을 합치는 것은 별개다.
2. 그림자 등 동적 도형과 정적 메시가 번갈아 나오면서 작은 배치가 반복 제출됐다.
3. Text가 문자열마다 Flush하고 별도 텍스처를 그렸다.
   HUD에서 도형과 문자가 교차할 때 추가 배치 분리가 발생했다.
4. Ellipse는 호출할 때마다 32개 삼각형, 96개 정점을 CPU 배열에 추가했다.
   선·사각형·캐릭터 구성 도형도 화면 좌표 정점을 매 프레임 만들었다.
5. 기존 메시 캐시는 실행 중 GPU 메모리에만 있었고, 재실행 시 다시 생성했다.
6. 전체 호출 수만으로는 어느 패스와 배치 단절 원인을 개선해야 할지 알기 어려웠다.

## 3. 적용한 방법

### 순서 보존 인스턴스 배치

Renderer의 Triangle/Rect/Ellipse/Line/CachedMesh/Text 인터페이스는 유지했다.
각 호출은 즉시 draw하지 않고 인스턴스 큐에 들어간다.
인스턴스는 변환, 색상, 메시 범위, 텍스트 UV를 담은 112바이트 데이터다.

GPU의 공용 texture buffer에 메시 정점을 저장하고, 인스턴스별 범위를 셰이더에서 읽는다.
서로 다른 메시도 같은 배치에 넣을 수 있다.
최종 제출은 glDrawArraysInstanced 한 번이며 실제 GL 호출 한 번으로 집계한다.
기존 지형과 후처리 glDrawArrays도 별도로 계속 집계한다.

SceneGraph의 painter order를 유지하며 메시 종류별로 임의 재정렬하지 않는다.
따라서 반투명 그림자, 불빛, UI와 객체의 앞뒤 관계를 유지하는 방향이다.
이 인스턴싱 모델은 OpenGL 3.3의 인스턴스별 속성 및 순차 인스턴스 실행 정의를 따른다.
기술 근거: [Khronos OpenGL 3.3 Core 명세, 2.8절](https://registry.khronos.org/OpenGL/specs/gl/glspec33.core.pdf).

### 배치 분리 조건과 비용

배치는 렌더 패스 변경, 텍스처 페이지 변경, 명시적 Flush, 인스턴스 2,048개 한도,
GPU 메시 저장소 초기화, 과도한 정점 패딩에서 분리된다.

하나의 instanced draw는 같은 정점 개수를 사용하므로 배치의 가장 큰 메시를 기준으로 제출한다.
작은 메시의 남는 정점은 셰이더에서 퇴화 삼각형으로 처리한다.
배치 전체의 제출 정점 / 유효 정점 비율이 8배를 넘으려 하면 배치를 나눈다.

따라서 draw call 감소가 GPU 시간 감소를 보장하지 않는다.
submitted_vertices/useful_vertices와 CPU 시간, 프레임 간격을 함께 비교해야 한다.
필요하면 RenderBatchLimits의 Instances/PaddingRatio를 조정한다.
큰 메시와 아주 작은 도형이 반복 교차하는 경우 더 작은 비율이 유리할 수도 있다.

### 기본 메시와 디스크 캐시

삼각형, 사각형, 타원은 단위 메시를 사용한다.
선은 단위 사각형의 affine 변환으로 표현한다.
그림자·불꽃·인물의 움직이는 부분도 기본 메시의 크기·위치·색상 변화로 처리한다.
애니메이션 상태나 화면 좌표를 파일 캐시 키로 만들지 않는다.

기존 바위·나무·벽·레벨 블록은 종류와 variant를 키로 사용한다.
조회 순서는 메모리 캐시 → 파일 검증·로드 → 생성·파일 저장 → GPU 업로드다.
첫 실행에서는 최초로 사용한 메시가 생성되고, 다음 실행부터 파일을 재사용한다.
따라서 이번 작업 중 메시 파일을 미리 생성한 것은 아니다.

경로: %LOCALAPPDATA%\GSE_Project\RendererCache\v2\<kind>_<variant>.mesh

파일에는 버전, endian 표식, 종류, variant, 정점 stride/개수와 체크섬이 있다.
크기 한도, 삼각형 단위 개수, 유한한 좌표·색상, 체크섬, 잔여 데이터를 검사한다.
검사에 실패하면 해당 파일을 사용하지 않고 다시 생성한다.
임시 파일을 완성한 뒤 원자적으로 교체하며 저장 실패는 메모리 렌더링을 막지 않는다.
메시 생성 규칙이 바뀌면 RenderMeshCache::Revision을 올려야 한다.

GPU 저장소는 최대 8 MiB, 장치 한도에 따라 더 작을 수 있다.
메모리 캐시 2,048개 또는 저장소 용량에 도달하면 대기 배치를 제출하고 저장소를 초기화한다.
LRU 개별 해제가 아니라 bounded arena 초기화 방식이며 mesh_evictions로 추적한다.
파일 캐시는 최대 2,048개까지 새 파일을 저장한다. 한도 도달 시 기존 파일은 유지하고 신규 저장은 건너뛴다.
무한 월드를 오래 탐험하면 이 한도의 영향을 받으므로 disk_budget_skip 이벤트를 확인해야 한다.
현재 생성기는 작은 메시지만, 포맷 자체는 메시당 최대 65,536개 정점으로 제한한다.

### 텍스트 아틀라스

GDI의 한글 글꼴 렌더링은 유지하고 문자열 비트맵을 최대 2,048×2,048 아틀라스에 넣는다.
같은 페이지의 글자와 일반 도형은 제출 순서를 유지하며 같은 배치에 들어갈 수 있다.
텍스트 캐시는 최대 512개 항목·4개 페이지다.
페이지보다 큰 문자열은 장치 한도 안에서 별도 텍스처로 처리한다.
캐시 초기화 시 기존 배치를 먼저 제출해 삭제된 텍스처 참조를 방지한다.
새 문자열의 GDI 비트맵 생성은 여전히 발생하며, 글꼴 비트맵은 이번 메시 파일 캐시 대상이 아니다.

## 4. 분석 자료

### 콘솔

기본 1초 간격으로 이전 draw call 평균·최소·최대·FPS에 다음이 추가된다.

- world/post/ui: 마지막 프레임의 패스별 실제 draw 수
- instances: 해당 프레임에 제출한 인스턴스 수
- uploadKB: 인스턴스·메시·텍스처 업로드량
- build/diskHit: 생성 메시 수 / 디스크 적중 수
- renderCPUms: 해당 Display 렌더링 구간의 CPU 경과 시간

평균·최소·최대·FPS만 구간 통계이며 나머지는 마지막 프레임 수치다.
전체 프레임 분석은 CSV를 사용한다.

### 프레임 CSV

경로: %LOCALAPPDATA%\GSE_Project\RenderLogs\render_<시각_PID>_frames_0.csv 등

매 프레임 기록하며 8 MiB마다 분할하고 실행당 4개 파일을 순환 사용한다.
각 파일의 # 메타데이터에 스키마, 메시 버전, 배치 한도, GPU/GL, 빌드 시각,
셰이더 소스 fingerprint와 segment 번호를 기록한다.
fingerprint는 재현 비교용이며 보안 서명이 아니다.

| 항목 | 분석 목적 |
|---|---|
| frame, elapsed_ms, frame_interval_ms | 순환 파일 병합·프레임 시간·FPS 분석 |
| cpu_render_ms | CPU 렌더 준비/제출 및 cold cache 처리 비용 |
| previous_log_ms | 직전 EndFrame의 CSV·콘솔 출력 부하 |
| width, height, hdr, post_enabled, combat, camera_x/y, zoom, seed | 비교 장면 조건 |
| draw_calls, draw_arrays, draw_instanced | GL 호출 수와 실제 instanced 제출 수 |
| world_draws, post_draws, ui_draws | 패스별 비용 분리 |
| instances, useful_vertices, submitted_vertices | 배치 크기·정점 패딩 비용 |
| upload_bytes, texture_upload_bytes | CPU→GPU 전송량 |
| mesh_hits/misses/builds, mesh_build_us | 메모리 적중·재생성 비용 |
| disk_hits/misses/rejected/writes/write_errors, cache_io_us | 파일 캐시 상태와 IO 비용 |
| mesh_evictions, mesh_count, mesh_used_bytes, mesh_capacity_bytes | GPU 캐시 압박 |
| text_hits/misses, text_entries, text_pages | 글꼴 캐시 변동 |
| flush_explicit/pass/texture/capacity/padding/cache, empty_flushes | 배치 단절 원인 |

draw_calls = draw_arrays + draw_instanced = world_draws + post_draws + ui_draws여야 한다.
정점 수는 인스턴스까지 반영하며, GL 호출 수와 혼동하지 않는다.
seed는 기존 Terrain에 전달되는 표시용 seed 값으로 전체 세이브/RNG 재현 정보는 아니다.

cpu_render_ms는 GPU 실행 시간이 아니며 게임 Update와 SwapBuffers 대기를 포함하지 않는다.
frame_interval_ms에는 두 Display 시작 사이의 업데이트·VSync·로그 부하 등이 포함된다.
매 프레임 glFinish나 GPU 결과 대기는 추가하지 않았다.
현재 CSV만으로 GPU 병목을 확정할 수 없고, 필요하면 비동기 GPU timer query를 후속 추가한다.

### 캐시 이벤트 CSV

같은 실행 접두사의 _events_0.csv, _events_1.csv에 기록한다.
4 MiB마다 순환하며 frame,event,mesh_kind,variant,vertices,elapsed_us를 제공한다.
build, disk_hit/miss/rejected/write/write_error, disk_budget_skip, arena_reset,
text_atlas_page/reset, text_entry_reset 등을 통해 문제 객체를 추적한다.
mesh_kind: 0 바위, 1 나무, 2 벽, 3 블록, 4 삼각형, 5 사각형/선/텍스트, 6 타원.

시작 시 기존 render_*.csv를 오래된 순서대로 정리해 128 MiB·64개 이하로 맞춘다.
이번 실행은 추가로 프레임 약 32 MiB·이벤트 약 8 MiB를 유지한다.
전체 실행의 무제한 영구 보관이 아니라 최근 분석 자료를 유지하는 정책이다.
비교용 자료는 실행 접두사 전체를 따로 보존해야 순환으로 덮어쓰이지 않는다.

### 요약 도구

저장소 루트에서 아래처럼 한 실행의 프레임 CSV를 선택한다.
이벤트 CSV를 입력하지 않는다.

```powershell
$latest = Get-ChildItem "$env:LOCALAPPDATA\GSE_Project\RenderLogs" -Filter "*_frames_*.csv" |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
$pattern = $latest.FullName -replace '_frames_\d+\.csv$', '_frames_*.csv'
.\tools\Summarize-RenderProfile.ps1 -Path $pattern -SkipSeconds 5
```

도구는 GPU를 실행하지 않고 CSV를 읽어 평균, p95, 최대 draw call, 프레임 간격,
CPU 시간, 패딩 비율, 캐시·배치 단절 누적치를 JSON으로 출력한다.
SkipSeconds는 실행 시작 기준이며 워밍업이 실제 끝났다는 보장은 아니다.

## 5. 검증 및 후속 우선순위

이번에는 포맷·프로젝트 등록·소스 정합성만 확인한다.
tests/RenderBatchTests.cpp에 파일 round-trip, 손상·잘림·버전·키 불일치,
실패 시 기존 데이터 보존과 배치 한도 검사를 추가했다. 실행하지 않았다.

사용자 실행 시 다음을 비교한다.

1. 같은 세이브·위치·해상도·줌·후처리 설정으로 정지/이동/전투/HUD 화면을 각각 측정.
2. 첫 실행의 cold cache와 재실행의 disk cache를 분리해서 측정.
3. 정지 장면 워밍업 후 mesh_builds가 계속 발생하지 않는지 확인.
4. draw call 평균·p95와 cpu_render_ms, frame_interval_ms를 함께 비교.
5. 그림자·투명 불빛·NPC 가림·체력바·한글 UI·줌·리사이즈·후처리 토글의 시각 회귀 확인.
6. 불필요한 조기 Flush는 flush_explicit, 텍스처 분리는 flush_texture,
   저장소 압박은 flush_cache, GPU 패딩은 submitted/useful 비율로 판단.

예상 방향은 객체 수에 비례하던 draw를 배치 수에 가깝게 줄이는 것이다.
지형 1회와 HDR 후처리는 별도 패스라 완전히 하나의 draw로 만들지 않는다.
기본 설정에서 후처리는 bloom 5회 + soft blur 3회 + composite 1회이며,
이 수치는 코드 경로 계산이지 실행 측정 결과가 아니다.

후속 후보:
- cache_io_us가 크면 로딩 단계의 사전 준비 또는 비동기 파일 읽기/쓰기.
- text_misses와 texture_upload_bytes가 크면 문자열 캐시 대신 glyph atlas.
- mesh_evictions가 반복되면 저장소 페이지/LRU·디스크 교체 정책.
- 패딩이 높고 프레임 시간이 악화되면 더 낮은 PaddingRatio 또는 별도 mesh-size 배치 전략.
- draw 감소 후에도 GPU 비용이 크면 비동기 GPU 타이머, 해상도·오버드로·후처리 분석.

인스턴싱은 제출 횟수 감소 기법이고, 디스크 캐시는 재생성 비용 감소 기법이다.
둘을 각각의 지표로 검증해야 한다.
