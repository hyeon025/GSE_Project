# 잔향의 시대 — 로우폴리 모듈 모델 세트

이 폴더는 모델링 산출물이다. 최신 건물 수정은 게임의 표시 배율·충돌 범위·가림 처리에도 함께 적용했다. 저장 형식은 유지한다.

## 산출물

- `Previews/Rigged/10_Character_Individuality.png`: 최신 플레이어와 NPC별 색상·표정 비교. 생산 게임 렌더러로 그렸다.
- `Previews/Rigged/11_Character_Expressions.png`: 미소·무표정·의지·걱정·졸림·활짝 웃음 6종.
- `Catalog/character_style_catalog.json`: 색상/표정 인덱스, 플레이어 전용 배색, NPC 영속 ID 연결 규칙.
- `References/Approved_Cute_Modular_Concept.png`: 사용자가 승인한 캐릭터 디자인 시안. 실제 모델 렌더와 구분한다.
- `Previews/Rigged/08_Cute_Modular_NPC_Lineup.png`: 최신 revision 4 실제 모델 4종.
- `Previews/Rigged/04_Quadruped_Hind_Before_After.png`: 참고 사진을 반영한 4족 6종의 뒷다리 수정 전후 비교. 4족은 리그 revision 2이며 GLB와 애니메이션도 갱신했다.
- `AgeOfEchoes_Characters_Rigged.blend`: 사람 공용 파트와 몹 7종의 리깅·키프레임 애니메이션 원본. 이번 추가 작업은 이 파일을 사용한다.
- `Exports/Rigged/`: 사람 모듈 라이브러리, 주민 조립 예시, 몹별 애니메이션 GLB 7개. 각 파일에 18개 클립이 들어 있다.
- `RIGGING_ANIMATION_KO.md`: 몹 종류, 리깅 규격, 클립 목록, 회전·점프·이동 적용 규칙과 검사 결과.
- `AgeOfEchoes_LowPoly_Modular.blend`: 편집 가능한 원본. 건물 비교 컬렉션 `07_BUILDING_SCALE_REVIEW`가 추가되어 있다.
- `Exports/NPC_Modular_Library.glb`: NPC 공용 파트 37개. 모두 공통 조립 원점에 겹쳐 있으므로 필요한 슬롯의 파트만 선택한다.
- `Exports/Environment_Library.glb`: 배경 모델 27개. 라이브러리의 원본은 각각 지면 원점에 있으므로 겹쳐 보이는 것이 정상이다.
- `Exports/NPC_Parts/`: NPC 메시별 개별 GLB 37개.
- `Exports/Environment/`: 배경 메시별 개별 GLB 27개.
- `Catalog/mesh_catalog.json`, `mesh_catalog.csv`: 메시 이름, 슬롯별 고정 인덱스, 삼각형 수, 크기. JSON에는 개별 GLB 경로와 재질 목록도 포함한다.
- `Catalog/npc_appearance_recipes.json`: 외형 조합 예시 40개. 실제 저장된 NPC에 연결하지 않은 자료이며 `npc_id`는 비어 있다.
- `Catalog/assembly_contract.json`: 조립 좌표, 필수/선택 슬롯, 조합 제약, 팔레트 및 좌표 규격.
- `Catalog/material_palette.json`: 재질 전체의 선형 RGBA 색상, 거칠기, 금속성, 발광 값. 기본 GLB에서 사용되지 않은 피부·머리 대체색도 포함한다.
- `Catalog/asset_statistics.json`, `validation_report.json`: 메시 수, 조합별 삼각형 수, 모델 데이터 검사 결과.
- `Previews/`: 실제 3D 모델에서 렌더링한 NPC 라인업, 분해도, 마을 배치, 배경 목록, 머리 앞/뒤 비교.

배경 목록 미리보기는 작은 풀과 큰 건물을 함께 확인하기 위해 표시 크기를 정규화했다. 실제 모델의 미터 단위 크기는 원본·GLB·카탈로그에 보존되어 있다.

## 아트 방향

2026-11-02 개성 추가: 파츠 37개를 공유하면서 피부 5종, 머리색 8종, NPC 의상 배색 12종, 표정 6종으로 구분한다. 플레이어는 짙은 푸른 머리·청록/금색 의상·별도 파츠 조합을 사용한다. NPC의 기존 파츠는 유지하고 색상 번호만 첫 동기화 때 영속 ID에서 배정해 저장한다. 표정은 같은 ID에서 다시 계산하므로 실행/이동/직업 변경으로 바뀌지 않는다. 머리 2개의 상대 모프 5개로 표정을 조절하며 색상 때문에 메시를 복제하지 않는다. 검증 결과는 `Catalog/character_style_validation.json`, 실제 게임 화면은 `Previews/Rigged/12_Character_Individuality_In_Game.png`이다.

기준은 `docs/game_design.md`의 중세 다크 판타지 「잔향의 시대」다. 재빛 천, 짙은 가죽, 무광 철, 낡은 목재, 이끼 낀 돌과 낮은 채도의 식생을 사용했다. 폐허 예배당의 촛불과 치유사의 등불은 작은 희망을 표현한다. 재질은 이미지 텍스처 없이 면 색과 거친 표면으로 구성했다. 모든 자연물은 입체 메시이며 풀도 두께가 있는 작은 각기둥이다.

캐릭터는 머리부터 발바닥까지 기본 키 1.8m, 머리 높이 0.6m인 3등신이다. 머리카락과 모자 높이는 기본 키에 추가된다. 성인 NPC의 스타일화된 비율이며 아동 캐릭터를 의미하지 않는다. 모든 파트는 같은 편안한 A 포즈와 공통 원점을 사용한다.

남성형/여성형 머리 2종은 의상, 장비, 머리카락을 공유한다. 성별에 따른 의상 제한은 없다. 파트의 연결 치수는 공통이며 성별마다 옷 메시를 복제하지 않는다.

## 파트 이름과 고정 인덱스

이름 형식은 `SM_NPC_{Slot}_{Index:02}_{Variant}` 및 `SM_ENV_{Category}_{Index:02}_{Variant}`다. Blender의 오브젝트 이름과 메시 데이터 이름을 동일하게 지정했다. GLB 노드 이름과 사용자 속성에도 에셋 ID, 슬롯, 인덱스를 보존했다.

번호는 **슬롯 안에서만** 유효하다. `Hair=1`과 `Torso=1`은 서로 다른 파트다. 선택 슬롯의 `0`은 없음이며 실제 빈 메시를 만들지 않는다. 새 파트는 뒤 번호에 추가하고 기존 번호는 변경하거나 재사용하지 않는다. Blender 목록 순서나 GLB 내부 배열 순서를 영속 인덱스로 쓰지 않는다.

| 슬롯 | 인덱스 |
|---|---|
| Head | 1 남성형, 2 여성형 |
| Hair | 0 없음, 1 짧은 머리, 2 단발, 3 긴 머리, 4 땋은 머리, 5 옆으로 넘긴 머리 |
| Torso | 1 주민 튜닉, 2 수호자 흉갑, 3 궁수 조끼, 4 구도자 로브, 5 상인 겹옷, 6 치유사 앞치마 |
| Arms | 1 천 소매, 2 판금 소매, 3 가죽 소매, 4 로브 소매; 손 포함 |
| Legs | 1 기본 바지, 2 기운 바지, 3 판금 하의 |
| Feet | 1 장화, 2 천을 감은 장화 |
| FacialHair | 0 없음, 1 짧은 수염, 2 긴 수염 |
| Headgear | 0 없음, 1 후드, 2 투구, 3 모자 |
| Back | 0 없음, 1 망토, 2 화살통, 3 배낭 |
| Belt | 1 허리띠, 2 주머니 허리띠 |
| Mainhand | 0 없음, 1 직검, 2 활, 3 지팡이 |
| Offhand | 0 없음, 1 방패, 2 등불 |

필수 슬롯은 Head/Torso/Arms/Legs/Feet/Belt다. 투구·후드·모자를 쓰면 Hair=0을 선택해 머리카락 관통을 피한다. 판금 흉갑은 판금 소매, 로브는 로브 소매와 조립한다. 로브 안쪽 바지는 유지할 수 있다. 피부 3색, 머리카락 3색, 천 3색을 조합하되 메시를 복제하지 않고 재질만 교체한다.

## 배경 모델

- 건물 6종: 서민 초가집, 대장간, 시장 가판대, 촛불이 남은 폐허 예배당, 종탑, 목골 주택.
- 자연물 17종: 풀 3, 바위 3, 관목 2, 나무 4(참나무/침엽수/고사목/어린 나무), 그루터기/통나무 2, 폐허 돌벽 2, 돌길 1.
- 마을 소품 4종: 목책, 상자, 나무 통, 우물.

건물은 외관용 모델이다. 최신 주택의 게임 표시 배율과 기초 충돌 범위는 아래 건물 수정 절에 기록했다. 문·창 위치의 벽체를 나누어 겹침을 줄였으며, 실제 실내 이동 기능은 없다.

## 다른 작업에서 적용할 사항

1. 개별 GLB 또는 라이브러리에서 `asset_id`로 공용 메시를 등록한다. 라이브러리 파일과 개별 파일은 같은 메시의 두 전달 방식이므로 중복으로 등록할 필요가 없다.
2. 각 NPC의 영속 ID에 `body_type_index`, 슬롯별 파트 인덱스, 피부/머리/천 팔레트 인덱스와 카탈로그 버전을 저장한다. NPC 사망·이동·직업 변경 또는 저장/로드 중 외형을 매번 다시 뽑지 않는다.
3. 외형 예시의 `appearance_id`는 조합 프리셋 식별자이며 게임의 실제 `npc_id`와 다르다. 40개 예시는 현재 주민 수를 고려한 참고 자료일 뿐, 주민 이름·직업·저장 ID에 이미 매핑된 데이터가 아니다.
4. 공용 메시를 참조해 조립하고 NPC마다 새 메시를 생성하지 않는다. 실제 동시 표현 수와 재질 수에 맞춘 인스턴싱·드로콜 처리는 적용 작업 범위다. 한 메시에도 여러 재질이 있으므로 메시 수와 드로콜 수는 같지 않다.
5. 기존 `LowPoly_Modular.blend`와 `Exports/NPC_Parts/`는 정적 원본으로 유지했다. 리깅과 키프레임 애니메이션은 새 `Characters_Rigged.blend` 및 `Exports/Rigged/`에 있다. 손 장비도 손뼈에 스키닝했다. LOD, 전용 충돌 메시, 게임 임포터와 게임 적용은 포함하지 않았다.

Blender 원본은 +Z 위, -Y 앞이며 GLB는 내보내기 시 +Y 위, +Z 앞으로 변환했다. NPC 모든 파트의 원점은 양발 사이 지면이고 배경 원점은 각 모델의 지면 기준이다. 모든 파트는 절대 조립 좌표를 포함하므로 파트별 중심으로 다시 정렬하면 연결이 깨진다. `assembly_contract.json`의 소켓 좌표는 Blender 원본 좌표이며 GLB 적용 시 축 변환이 필요하다.

원본 컬렉션은 `01_ASSET_LIBRARY`(숨겨진 원본), `02_NPC_ASSEMBLY_EXAMPLES`, `03_PARTS_BREAKDOWN`, `04_ENVIRONMENT_DIORAMA`, `05_ENVIRONMENT_CONTACT_SHEET`, `06_HAIR_AND_HEAD_VARIATIONS`로 구성했다. 02~06은 검토용이며 서로 같은 메시 데이터를 공유한다. `PREVIEW_ONLY_Ground`, 카메라, 조명은 게임 에셋이 아니다.

## 최신 캐릭터 수정 · 2026-10-29

품질 revision 3: 사람 팔의 역방향 꺾임을 수정하고 소매 4종의 연결 구조·손·스키닝을 다시 만들었다. 사람 얼굴·헤어·상의와 동물 6종의 윤곽·발·귀를 보완했다. 정적 원본과 리깅 원본, 해당 GLB 및 카탈로그를 함께 갱신했다. 기존 조립 인덱스와 메시 이름은 유지한다.

수정 전후: `Previews/Rigged/07_NPC_Before_After.png`

팔 동작: `Previews/Rigged/Humanoid_Run.gif`

검사 결과: `Catalog/character_quality_revision.json`, `Catalog/glb_roundtrip_quality.json`
상세 인계: `RIGGING_ANIMATION_KO.md`의 품질 revision 3 절.

## 귀여운 조립식 캐릭터 · revision 4 · 2026-11-01

승인한 시안을 기준으로 사람 공용 파트 37개를 다시 모델링했다. 둥근 사각 얼굴, 큰 타원형 눈과 작은 코·미소, 볼과 귀, 덩어리 형태의 머리카락, 넓은 몸통, 엄지가 있는 손과 둥근 부츠를 사용한다. 튜닉·갑옷·궁수·로브·상인·치유사 의상과 모자·수염·가방·무기를 같은 연결 치수로 맞췄다. 시안의 세밀한 장식은 게임 화면에 필요한 큰 형태로 단순화했다.

기존 `SM_NPC_*` / `SKM_NPC_*` 오브젝트·메시 이름, 37개 슬롯별 인덱스, 40개 외형 조합, 공용 21개 뼈와 18개 키프레임 클립을 유지한다. 망자가 공유하는 사람 메시 7개도 동기화했다. 정적 파트 37개와 정적 라이브러리, 사람 리깅 라이브러리·주민 예시·망자 GLB를 갱신했다. 동물 6종과 건물·자연물은 이번 수정 범위 밖이다.

조합별 약 3,552~4,956삼각형이며 텍스처를 추가하지 않았다. 모든 신규 NPC 파트는 닫힌 입체 메시이며 단면 재질로 뒷면을 제외한다. 게임에서는 면의 실제 겹침 깊이를 비교해 머리카락·눈·옷·갑옷의 삼각형 가림 오류를 줄인다. 계산 결과는 기존 포즈 캐시와 비동기 작업자를 사용한다.

- `Catalog/cute_modular_revision.json`: 현재 형상·웨이트·원본 키프레임·GLB 재가져오기 검증. revision 3 기록은 이전 수정 이력이다.
- `Previews/Rigged/Cute_Humanoid_Walk.gif`, `Cute_Humanoid_Run.gif`, `Cute_Humanoid_Attack.gif`: 최신 네 조합의 실제 뼈대 애니메이션.
- `Previews/Rigged/09_Cute_NPC_In_Game.png`: 실제 게임 화면.
- `tools/Build-CuteModularNPC.py`, `Publish-CuteModularNPC.py`, `Review-CuteModularNPC.py`: 재현용 Blender 도구. 기본 빌드는 검토본만 만들고 `-- --publish`를 지정해야 배포 파일을 갱신한다.

기존 정적 미리보기와 4족 뒷다리 전후 비교는 해당 시점의 기록이다. 최신 캐릭터 외형은 `Previews/Rigged/06_NPC_Quality_Upgrade.png` 및 갱신된 동작 GIF를 기준으로 확인한다.

## 주택 확대 및 건물 품질 수정 · 2026-11-01

주택은 게임에서 사람 키의 약 1.8~1.9배 높이로 표시한다. 기존 초가집은 높이 26.4 월드 단위로 사람(54)보다 작았으나 103.6으로 확대했다. 주택 기초는 66×56, 마을 주택 간격은 112이며 충돌 범위·가림·깊이 정렬도 맞췄다. 시장의 작은 기초 충돌 범위는 50×46을 사용해 마을 중심 길을 유지한다.

초가집·대장간·가판대를 다시 다듬고 `SM_ENV_Building_06_TimberHouse`(Building 인덱스 6)를 추가했다. 주택 자리는 초가집/목골 주택을 공유하며, 가판대는 시장에 사용한다. 기존 이름과 인덱스는 유지했다. 벽의 문·창 공간, 목재 골조, 창틀·덧문, 지붕 색면, 굴뚝, 기초·문턱, 등불/대장간 간판 등을 보완했다. 새 모델은 건물당 약 600~1,500삼각형으로 유지한다.

- `Previews/08_Building_Quality_And_Scale.png`: 게임 표시 배율에 맞춘 사람·건물 비교 렌더. NPC 검토 복사본만 1.5배로 표시하여 게임의 사람30/건물20 배율을 재현한다.
- `Previews/09_Buildings_In_Game.png`: 실제 게임 화면.
- `Catalog/building_scale_revision.json`: 표시 배율·치수·검사 결과.
- `Exports/Environment_Library.glb`와 변경 건물 4종 개별 GLB, 원본 `.blend` 및 카탈로그를 갱신했다.

빌드된 게임과 새 Art 폴더를 함께 사용한다. 건물의 큰 면이 문·창문을 덮던 문제는 건물 전용 겹침 깊이 정렬로 보완했다. 이 정렬은 정적 건물의 캐시 생성 때만 계산한다. 모델 불러오기, 이동 경로, 깊이 정렬 검사 및 Release/Debug 빌드를 통과했다. 실제 이동 장면 검사에서 평균 약 60FPS였으며 초기 로딩과 일부 긴 프레임은 남는다. 이전 정적 미리보기는 당시 외형의 기록이다.
