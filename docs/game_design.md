# 게임 기획서 — 잔향의 시대

## 1. 게임 개요

### 1.1 기본 정보

- **가제:** 잔향의 시대 (Age of Echoes)
- **장르:** 중세 암흑시대풍 2.5D 무한 확장 오픈 월드 RPG
- **배경:** 중세 다크 판타지
- **핵심 시스템:** 죽음, 계승, NPC 생성, 고유능력, 무한 월드, 세계 변화
- **최종 목표:** 계승의 법칙의 진실을 발견하고 최초의 계승자 에녹을 쓰러뜨린다.

### 1.2 핵심 문장

> 하나의 죽음은 하나의 생명을 만들고,  
> 그 생명은 죽은 자의 기억이 아니라 삶의 방향을 물려받는다.

플레이어는 정해진 크기의 맵을 클리어하는 것이 아니라 끝없이 확장되는 세계를 탐험한다.

플레이어가 새로운 장소로 이동할수록 새로운 지역, 인물, 사건, 갈등이 생성된다.

가장 중요한 특징은 **죽음이 게임의 실패가 아니라 세계를 변화시키는 하나의 시스템**이라는 점이다.

플레이어가 죽으면 새로운 NPC가 세계에 태어나며, 그 NPC는 플레이어가 살아왔던 삶의 방향성을 일부 계승한다.

---

# 2. 세계관

## 2.1 세계의 분위기

세계는 중세 암흑시대를 기반으로 한다.

세계에는 다음과 같은 문제가 존재한다.

- 전쟁
- 전염병
- 기근
- 계급 억압
- 종교적 광기
- 미신
- 약탈
- 영주의 착취
- 폐허가 된 마을
- 죽음

사람들은 서로를 쉽게 믿지 못하며 권력자들은 약자를 착취한다.

마을과 도시는 전쟁과 질병으로 조금씩 무너지고 있다.

하지만 세계가 완전히 절망적인 것은 아니다.

암울한 세계 속에서도 작은 희망이 존재한다.

### 희망 이벤트 예시

- 굶주린 사람이 마지막 빵을 다른 사람과 나눈다.
- 폐허가 된 교회에 촛불 하나가 남아 있다.
- 플레이어의 죽음에서 태어난 NPC가 누군가를 구한다.
- 버려진 마을이 다시 재건된다.
- 서로 적대하던 사람들이 화해한다.
- 위험을 감수하면서 다른 사람을 돕는 사람이 등장한다.

전체적인 세계는 암울하지만 희망이 존재하기 때문에 작은 선의가 더욱 크게 느껴지는 구조를 가진다.

---

# 3. 계승의 법칙

세계에는 모든 사람이 알고 있지만 누구도 완전히 이해하지 못하는 법칙이 존재한다.

이를 **계승의 법칙**이라고 부른다.

## 3.1 기본 법칙

> 하나의 죽음은 하나의 생명을 만든다.

누군가 죽으면 세계 어딘가에서 새로운 사람이 태어난다.

새롭게 태어난 사람은 죽은 사람의 기억을 가지고 있지 않다.

하지만 죽은 사람이 살아가면서 형성한 **삶의 방향성** 일부를 물려받는다.

예를 들어 평생 사람들을 도왔던 사람이 죽으면 다른 지역에서 이유 없이 타인을 돕는 성향을 가진 사람이 태어날 수 있다.

반대로 폭력적인 삶을 살았던 사람이 죽으면 공격적인 성향을 가진 사람이 태어날 가능성이 높아진다.

---

# 4. 플레이어의 죽음

플레이어의 죽음은 단순한 Game Over가 아니다.

플레이어가 죽으면 세계에 새로운 변화가 발생한다.

## 4.1 사망 처리 과정

```text
Player Death
    ↓
Death Record 생성
    ↓
플레이어 행동 기록 분석
    ↓
플레이어 성향 분석
    ↓
계승 NPC 생성
    ↓
NPC를 World에 저장
    ↓
플레이어 재시작
    ↓
변화된 세계에서 게임 계속
```

플레이어는 다시 게임을 시작하지만 이전 세계는 초기화되지 않는다.

기존 NPC, 사건, 마을, 관계, 소문 등의 기록은 계속 유지된다.

---

# 5. 플레이어 성향 시스템

플레이어의 행동은 보이지 않는 성향 수치로 기록된다.

## 5.1 기본 성향

```text
Aggression
Mercy
Fear
Curiosity
Sacrifice
Greed
Hope
```

| 성향 | 의미 |
|---|---|
| Aggression | 공격성 |
| Mercy | 자비 |
| Fear | 공포 |
| Curiosity | 호기심 |
| Sacrifice | 희생 |
| Greed | 탐욕 |
| Hope | 희망 |

성향 값은 플레이어에게 직접 숫자로 보여주지 않는다.

플레이어는 자신의 행동으로 인해 발생하는 결과를 통해 자신의 성향을 확인하게 된다.

성향은 다음 요소에 영향을 준다.

- NPC 성격
- NPC 직업
- NPC 말투
- NPC 행동 패턴
- 등장 위치
- 인간관계
- 세력
- 사건
- 메인 스토리
- 최종 보스전
- 엔딩

---

# 6. 계승 NPC

플레이어가 죽으면 플레이어의 성향을 기반으로 새로운 NPC가 생성된다.

계승 NPC는 플레이어의 복제나 환생이 아니다.

완전히 독립적인 인간이지만 플레이어의 죽음에서 만들어진 **삶의 잔향**을 가지고 있다.

## 6.1 성향별 생성 예시

### 공격성

```text
높은 Aggression
→ 호전적인 용병
→ 복수심 강한 기사
→ 폭력적인 탈영병
→ 산적
```

### 자비

```text
높은 Mercy
→ 자비로운 순례자
→ 치료사
→ 수도사
→ 가난한 사람을 돕는 주민
```

### 공포

```text
높은 Fear
→ 피난민
→ 은둔자
→ 의심이 많은 주민
```

### 호기심

```text
높은 Curiosity
→ 학자
→ 탐험가
→ 유적 조사자
→ 폐허를 떠도는 순례자
```

### 희생

```text
높은 Sacrifice
→ 기사
→ 구조자
→ 타인을 보호하는 주민
```

### 탐욕

```text
높은 Greed
→ 상인
→ 도적
→ 부패한 관리
```

### 희망

```text
높은 Hope
→ 지도자
→ 성직자
→ 마을 재건자
```

---

# 7. 계승 NPC의 성장

계승 NPC는 생성된 뒤 사라지는 일회성 이벤트가 아니다.

NPC는 세계 속에서 계속 살아간다.

NPC는 다음 행동을 할 수 있다.

- 다른 지역으로 이동
- 직업 획득
- 다른 NPC와 관계 형성
- 세력 가입
- 새로운 세력 창설
- 소문 생성
- 플레이어와 재회
- 플레이어의 동료가 됨
- 플레이어의 적이 됨
- 상인이 됨
- 지도자가 됨
- 광신도가 됨
- 범죄자가 됨
- 새로운 사건 발생
- 새로운 퀘스트 생성

강력하게 성장한 계승 NPC는 이후 메인 스토리의 주요 인물이나 보스가 될 수도 있다.

---

# 8. 고유능력 시스템

세계의 모든 사람은 태어날 때부터 하나의 **고유능력**을 가지고 태어난다.

고유능력은 배워서 얻는 일반적인 스킬이나 마법이 아니다.

태어날 때 정해지는 선천적인 능력이다.

능력은 크게 6개의 계열로 나뉜다.

---

## 8.1 육체 계열

몸 자체를 강화하거나 변화시키는 능력.

예시:

- 힘 증가
- 피부 강화
- 빠른 회복
- 빠른 이동
- 낙하 저항

---

## 8.2 감각 계열

일반적인 인간이 감지할 수 없는 것을 느끼는 능력.

예시:

- 기척 감지
- 거짓말 감지
- 죽음의 냄새 감지
- 위험 예감
- 어둠 속 시야

---

## 8.3 정신 계열

마음, 기억, 의지, 공포 등에 영향을 주는 능력.

예시:

- 공포 억제
- 분노 유발
- 기억의 조각 읽기
- 꿈을 통한 교신
- 약한 정신 지배

---

## 8.4 생명 계열

생명력, 회복, 질병, 성장과 관련된 능력.

예시:

- 상처 회복
- 질병 진행 지연
- 식물 성장
- 생명력 감지
- 자신의 생명력을 대가로 한 치료

---

## 8.5 물질 계열

무기, 도구, 금속, 돌, 흙, 불 등의 물질에 영향을 주는 능력.

예시:

- 칼날 강화
- 돌벽 약화
- 불씨 증폭
- 잠긴 문 약화
- 금속 균열 감지

---

## 8.6 운명 계열

확률, 인연, 징조, 불운, 선택의 결과에 영향을 주는 능력.

예시:

- 위험한 상황의 생존 확률 증가
- 재회의 운명 강화
- 다른 사람의 불운 대신 받기
- 미래 사건의 징조 확인

---

# 9. 주 계열과 파생능력

모든 사람에게는 하나의 주 계열이 존재한다.

주 계열은 태어날 때 결정되며 평생 바뀌지 않는다.

주 계열의 능력은 가장 강하게 성장할 수 있다.

하지만 인접한 계열의 능력은 훈련을 통해 약하게 사용할 수 있다.

## 9.1 계열 구조

```text
육체 <-> 감각 <-> 정신 <-> 운명 <-> 물질 <-> 생명 <-> 육체
```

## 9.2 파생 가능 계열

```text
육체 → 감각 / 생명
감각 → 육체 / 정신
정신 → 감각 / 운명
운명 → 정신 / 물질
물질 → 운명 / 생명
생명 → 물질 / 육체
```

### 기본 규칙

```text
주 계열
→ 선천적으로 결정
→ 가장 강하게 성장 가능

인접 계열
→ 훈련을 통해 약하게 습득 가능

비인접 계열
→ 거의 습득 불가능

파생능력
→ 주 능력보다 약함
→ 사용 조건이나 제한 존재
```

---

# 10. 무한 오픈 월드

월드에는 정해진 크기가 없다.

플레이어가 이동하면서 새로운 지역이 계속 생성된다.

기본적으로 **Chunk 기반 월드 생성 방식**을 사용한다.

## 10.1 기본 구조

```text
Player 이동
    ↓
주변 Chunk 확인
    ↓
미생성 Chunk 발견
    ↓
새로운 Chunk 생성
    ↓
지형 생성
    ↓
지역 생성
    ↓
NPC 생성
    ↓
사건 생성
```

플레이어에게서 멀어진 Chunk는 저장 후 메모리에서 제거한다.

```text
Nearby Chunk
→ Load

Far Chunk
→ Save
→ Unload
```

중요한 점은 세계가 단순히 넓어지는 것이 아니라 플레이어의 삶과 죽음으로 인해 점점 더 복잡하고 풍부해져야 한다는 것이다.

---

# 11. 영구적으로 저장되는 요소

모든 것을 영구 저장할 필요는 없다.

세계에 중요한 영향을 미치는 요소를 중심으로 저장한다.

```text
Important NPC
Important Location
Important Event
Faction
Relationship
Rumor
War
Destroyed Village
Restored Village
Player Death
Successor NPC
```

핵심 원칙:

> 재미는 소비되는 것이 아니라 세계에 축적된다.

---

# 12. 메인 스토리 전체 구조

```text
PROLOGUE
이름 없는 죽음
        ↓
ACT 1
죽음이 남긴 사람들
        ↓
ACT 2
성계교단
        ↓
BOSS 1
붉은 성자 아벨론
        ↓
ACT 3
태어나지 말았어야 할 아이
        ↓
WORLD TRUTH
계승의 법칙의 진실
        ↓
ACT 4
플레이어가 만든 세계
        ↓
ACT 5
최초의 계승자
        ↓
FINAL AREA
태초의 묘지
        ↓
FINAL BOSS
에녹
        ↓
ENDING
```

---

# 13. PROLOGUE — 이름 없는 죽음

## Quest ID

```text
MAIN_000
```

플레이어는 가난한 변방 마을에서 시작한다.

플레이어는 선택받은 영웅이 아니다.

농민, 용병, 떠돌이 등 평범한 인간으로 시작한다.

플레이어에게는 하나의 고유능력이 존재하지만 아직 제대로 사용하는 방법을 알지 못한다.

마을에는 다음 문제가 발생하고 있다.

- 전염병
- 기근
- 높은 세금
- 주민 갈등
- 도적의 위협

플레이어는 자유롭게 행동할 수 있다.

### 선택 예시

```text
병자를 도와준다.
음식을 훔친다.
주민에게 음식을 나눠준다.
도적과 싸운다.
위험한 주민을 버리고 도망간다.
```

각 행동은 플레이어 성향에 영향을 준다.

---

## 13.1 첫 번째 죽음

마을에 대규모 습격이 발생한다.

플레이어는 전투 과정에서 첫 번째 죽음을 경험한다.

```text
PlayerHP <= 0

Trigger:
FIRST_DEATH_EVENT
```

화면이 암전된다.

그러나 `GAME OVER`가 나타나지 않는다.

대신 세계 어딘가에서 한 아이가 태어나는 장면이 나온다.

```text
GenerateSuccessorNPC()
```

메시지:

> 당신의 삶은 끝났다.  
> 그러나 당신이 남긴 방향은 끝나지 않았다.

이후 플레이어는 다시 시작한다.

하지만 기존 세계는 그대로 유지된다.

---

# 14. ACT 1 — 죽음이 남긴 사람들

## Quest ID

```text
MAIN_100
```

플레이어는 세계를 여행하면서 자신의 죽음에서 만들어진 NPC들을 만나기 시작한다.

NPC들은 플레이어를 기억하지 못한다.

그러나 플레이어의 과거 행동과 비슷한 성향을 보여준다.

### 대사 예시

> 왜 그런지는 모르겠지만 누군가 곤란해하는 모습을 그냥 지나치지 못합니다.

또는:

> 이상하게 칼을 잡으면 마음이 편해집니다.

특수 계승 NPC 조건:

```text
if NPC.origin == PLAYER_DEATH:
    EnableSpecialDialogue()
```

일정 수 이상의 계승 NPC를 만나면 다음 메인 퀘스트가 활성화된다.

```text
SuccessorNPCEncounter >= 3

Unlock:
MAIN_200
```

---

# 15. ACT 2 — 성계교단

## Quest ID

```text
MAIN_200
```

플레이어는 거대한 종교 세력인 **성계교단**을 만나게 된다.

성계교단은 계승의 법칙을 신의 섭리라고 주장한다.

교리는 다음과 같다.

> 죽음은 죄가 아니다.  
> 죽음은 새로운 생명의 씨앗이다.

성계교단은 겉으로는 선한 집단처럼 보인다.

- 병자 치료
- 장례 진행
- 고아 보호
- 식량 제공

그러나 조사할수록 다른 모습이 발견된다.

교단은 일부 지역에서 의도적으로 갈등을 일으키고 있다.

```text
전쟁 유도
영주 갈등 조장
질병 방치
종교 갈등 조장
```

그들의 논리는 다음과 같다.

```text
Death 증가
    ↓
Birth 증가
    ↓
Succession 증가
    ↓
인류 유지
```

---

# 16. BOSS 1 — 붉은 성자 아벨론

## Boss ID

```text
BOSS_ABELON
```

아벨론은 성계교단의 고위 지도자다.

과거에는 장군이었다.

수많은 사람을 죽였지만 동시에 수많은 사람을 구한 인물이다.

아벨론은 말한다.

> 한 사람이 죽으면 다른 사람이 태어난다.  
> 그렇다면 죽음은 정말 악인가?

아벨론은 계승을 유지하기 위해 의도적으로 전쟁을 일으키고 있다.

플레이어는 아벨론과 싸운다.

```text
AbelonHP <= 0
```

아벨론은 죽는다.

그러나 아벨론이 죽는 순간 세계 어딘가에서 새로운 생명이 태어난다.

플레이어는 중요한 사실을 깨닫는다.

```text
Kill Evil
    ↓
Death
    ↓
New Life
    ↓
Succession Continues
```

악인을 죽이는 것조차 계승의 법칙을 유지시키고 있다.

---

# 17. ACT 3 — 태어나지 말았어야 할 아이

## Quest ID

```text
MAIN_300
```

플레이어는 특별한 계승 NPC를 만난다.

```text
NPC_SPECIAL_CHILD
```

이 아이 역시 플레이어의 죽음에서 태어난 존재다.

하지만 일반적인 계승자와 다르다.

```text
Normal Successor:
Memory = NONE

Special Child:
Memory = PARTIAL
```

아이는 플레이어를 보자마자 말한다.

> 당신이 죽던 순간을 본 적 있어.

계승자는 원래 죽은 사람의 기억을 가질 수 없다.

따라서 이 아이의 존재 자체가 계승의 법칙에 발생한 오류다.

플레이어는 아이와 함께 계승의 법칙을 조사한다.

---

# 18. 여섯 고대 유적

플레이어는 여섯 고유능력 계열과 관련된 고대 유적을 찾아야 한다.

```text
Ancient Ruin - Body
Ancient Ruin - Sense
Ancient Ruin - Mind
Ancient Ruin - Life
Ancient Ruin - Matter
Ancient Ruin - Fate
```

각 유적에서는 최초 능력자와 계승의 법칙에 대한 기록을 발견할 수 있다.

여섯 유적을 모두 조사하면 세계의 진실이 공개된다.

---

# 19. 계승의 법칙의 진실

과거에는 계승의 법칙이 존재하지 않았다.

사람이 죽으면 그것으로 끝이었다.

그러나 거대한 전쟁과 전염병, 기근이 동시에 발생하면서 인간은 멸종 직전까지 몰렸다.

당시 여섯 명의 강력한 능력자가 나타났다.

```text
육체의 최초 능력자
감각의 최초 능력자
정신의 최초 능력자
생명의 최초 능력자
물질의 최초 능력자
운명의 최초 능력자
```

그들은 인간을 멸종에서 구하기 위해 여섯 능력을 결합했다.

그 결과 새로운 법칙이 탄생했다.

```text
Death
    ↓
Life Energy
    ↓
New Human
```

이것이 **계승의 법칙**이다.

---

# 20. 계승의 부작용

처음에는 계승이 성공한 것처럼 보였다.

사람이 죽어도 새로운 사람이 태어났기 때문에 인간은 멸종하지 않았다.

하지만 문제가 발생했다.

새로운 생명은 죽은 사람의 기억은 가지지 않았지만 감정과 삶의 방향성을 물려받았다.

```text
Warrior Death
→ Aggression

Victim Death
→ Fear

Hero Death
→ Sacrifice

Explorer Death
→ Curiosity
```

결국 세계는 순환에 빠진다.

```text
War
    ↓
Death
    ↓
Aggressive Successors
    ↓
Conflict
    ↓
War
    ↓
Death
```

전쟁에서 죽은 사람들의 증오가 다음 세대로 이어지고 그 세대가 다시 새로운 전쟁을 만든다.

---

# 21. ACT 4 — 플레이어가 만든 세계

## Quest ID

```text
MAIN_400
```

지금까지 플레이어의 죽음에서 태어난 NPC들이 메인 스토리에 본격적으로 참여하기 시작한다.

플레이어가 어떤 삶을 살았는지에 따라 세계의 세력 구도가 달라진다.

### Mercy가 높은 경우

```text
Mercy > Threshold

Spawn:
HealerFaction
RefugeeSupport
VillageRestoration
```

### Aggression이 높은 경우

```text
Aggression > Threshold

Spawn:
MercenaryFaction
BanditFaction
WarEvent
```

### Greed가 높은 경우

```text
Greed > Threshold

Spawn:
MerchantFaction
CrimeFaction
CorruptNoble
```

### Curiosity가 높은 경우

```text
Curiosity > Threshold

Spawn:
ScholarFaction
ExplorerFaction
AncientRuinQuest
```

플레이어는 여기에서 중요한 사실을 깨닫는다.

> 자신은 세계를 탐험하고 있었던 것이 아니다.  
> 자신의 행동으로 다음 시대를 만들고 있었다.

---

# 22. ACT 5 — 최초의 계승자

## Quest ID

```text
MAIN_500
```

여섯 최초 능력자 중 다섯 명은 이미 죽었다.

하지만 한 명은 아직 살아 있다.

그의 이름은 **에녹(Enoch)**이다.

```text
Name: Enoch
Role: First Successor
PrimaryAbility: Fate
Age: Unknown
```

에녹은 운명 계열의 최초 능력자이며 현재 계승의 법칙을 유지하고 있는 존재다.

---

# 23. 에녹의 목적

에녹의 목적은 세계 정복이 아니다.

그의 목표는 단 하나다.

```text
Prevent Human Extinction
```

에녹에게 인간 개개인의 행복은 중요하지 않다.

```text
Human Happiness = Optional
Human Survival = Mandatory
```

따라서 인간이 멸종하지 않는다면 다음과 같은 사건도 허용한다.

- 전쟁
- 질병
- 기근
- 갈등
- 죽음

죽음의 수가 부족해질 경우 에녹은 직접 갈등을 만들어낸다.

```text
if DeathRate < RequiredDeathRate:
    GenerateConflict()
```

수천 년 동안 에녹은 죽음과 탄생의 균형을 관리해왔다.

---

# 24. FINAL AREA — 태초의 묘지

## Area ID

```text
AREA_FIRST_GRAVE
```

에녹은 세계의 중심에 존재하는 **태초의 묘지**에 있다.

일반적인 월드 좌표로는 접근할 수 없다.

플레이어가 여섯 유적을 모두 조사해야 입구가 열린다.

```text
BodyRelic   = TRUE
SenseRelic  = TRUE
MindRelic   = TRUE
LifeRelic   = TRUE
MatterRelic = TRUE
FateRelic   = TRUE
```

조건 충족:

```text
UnlockFirstGrave()
```

---

# 25. 태초의 묘지

태초의 묘지는 일반적인 던전과 다르다.

이곳에는 지금까지 플레이어가 경험한 죽음의 흔적이 존재한다.

```text
LoadPlayerDeathHistory()
```

과거 사망 장소가 왜곡된 모습으로 나타난다.

```text
Death #1
→ Destroyed Village

Death #2
→ Battlefield

Death #3
→ Ancient Ruin
```

플레이어는 자신이 과거에 죽었던 장소를 다시 지나가야 한다.

그리고 지금까지 플레이어의 죽음에서 태어난 NPC들이 등장한다.

---

# 26. 계승 NPC의 최종전 참여

NPC가 플레이어를 어떻게 생각하는지에 따라 최종전의 역할이 달라진다.

```text
if NPC.Relationship > 70:
    JoinPlayer()

if NPC.Relationship < -70:
    JoinEnoch()

if NPC.Sacrifice > 80:
    SacrificeEvent()

if NPC.Power > 80:
    BossSupportEvent()
```

따라서 플레이어가 게임 전체에서 어떤 행동을 했는지가 최종전에 직접 영향을 준다.

---

# 27. FINAL BOSS — 에녹

## Boss ID

```text
BOSS_ENOCH
```

에녹은 운명 계열의 극한에 도달한 능력자다.

## Phase 1

에녹은 자신의 고유능력을 사용한다.

```text
ProbabilityManipulation
FuturePrediction
FateAttack
```

## Phase 2

에녹은 계승의 법칙을 사용하여 플레이어가 지금까지 살아온 방식을 공격에 이용한다.

```text
AnalyzePlayerTrait()
```

### Aggression이 높은 경우

```text
SpawnPastWarriors()
```

플레이어의 공격성을 계승한 전사들이 나타난다.

### Fear가 높은 경우

```text
ReplayPlayerDeaths()
```

플레이어의 과거 죽음이 환영으로 재현된다.

### Greed가 높은 경우

```text
DisableStrongEquipment()
```

강력한 장비 일부가 봉인된다.

### Mercy가 높은 경우

```text
SpawnAllySuccessor()
```

플레이어가 만들어낸 선한 계승자가 전투를 돕는다.

### Sacrifice가 높은 경우

```text
SuccessorSacrificeEvent()
```

계승 NPC가 플레이어를 위해 희생한다.

### Hope가 높은 경우

```text
PartyRevival()
```

쓰러졌던 아군이 다시 일어난다.

---

# 28. FINAL PHASE

에녹의 체력이 10% 이하가 되면 전투가 멈춘다.

```text
EnochHP <= 10%
```

에녹이 말한다.

> 내가 사라지면 죽음은 다시 죽음이 된다.

> 다시는 그 죽음에서 누군가 태어나지 않는다.

> 그래도 나를 죽이겠는가?

이후 마지막 전투가 시작된다.

```text
FINAL_PHASE = TRUE
```

플레이어는 에녹을 쓰러뜨린다.

---

# 29. 엔딩

에녹을 쓰러뜨리면 플레이어는 계승의 법칙을 어떻게 할 것인지 결정해야 한다.

```text
BOSS_ENOCH = DEAD

OpenEndingSelection()
```

세 가지 엔딩이 존재한다.

---

# 30. ENDING A — 계승의 종말

## Ending ID

```text
ENDING_DESTROY
```

플레이어는 계승의 법칙을 완전히 파괴한다.

```text
SuccessionSystem = FALSE
```

이제 사람이 죽어도 새로운 사람이 태어나지 않는다.

죽음은 다시 진정한 죽음이 된다.

하지만 새로운 인간은 과거 세대의 성향에서 자유로워진다.

## 마지막 장면

한 아이가 자연스럽게 태어난다.

아이의 부모가 묻는다.

> 이 아이는 누구를 계승한 걸까요?

누군가 대답한다.

> 아무도.

> 자기 자신으로 태어난 거야.

---

# 31. ENDING B — 새로운 계승자

## Ending ID

```text
ENDING_SUCCESSOR
```

플레이어는 에녹의 자리를 대신한다.

```text
Player.Role = SUCCESSION_KEEPER
```

계승의 법칙은 계속 유지된다.

플레이어의 가장 강한 성향이 앞으로 만들어질 세계에 영향을 준다.

```text
WorldTrait = PlayerDominantTrait
```

## 마지막 장면

플레이어가 에녹의 왕좌에 앉는다.

```text
10 Years Later
100 Years Later
500 Years Later
```

시간이 흐를수록 플레이어의 모습과 눈빛이 조금씩 에녹과 닮아간다.

화면이 암전된다.

---

# 32. ENDING C — 계승의 해방

## Ending ID

```text
ENDING_FREEDOM
```

특정 조건을 만족해야 해금되는 진 엔딩이다.

예시 조건:

```text
SuccessorNPCCount >= 10
SavedSuccessorNPC >= 5
ChangedSuccessorDestiny >= 3
ExtremeTrait = FALSE
SpecialChildQuest = COMPLETE
```

플레이어는 계승의 법칙 자체를 없애지 않는다.

대신 죽은 사람의 삶의 방향을 새로운 인간이 반드시 물려받아야 하는 연결을 제거한다.

### 기존

```text
Death
    ↓
New Life
    ↓
Inherited Trait
```

### 변경

```text
Death
    ↓
New Life
    ↓
Free Will
```

새로운 인간은 과거 사람의 공격성, 탐욕, 공포, 희생 등에 강제로 지배되지 않는다.

## 마지막 메시지

> 물려받은 것이 우리가 누구인지를 결정하지 않는다.

> 마지막 선택은 언제나 살아 있는 자의 것이다.

---

# 33. 엔딩 이후 월드

보스를 쓰러뜨린 이후에도 게임은 끝나지 않는다.

```text
ContinueGame = TRUE
```

선택한 엔딩에 따라 이후 월드 생성 규칙이 변경된다.

## ENDING_DESTROY

```text
SuccessorGeneration = FALSE
DeathPenalty = HIGH
NaturalBirth = TRUE
```

죽음의 위험성이 크게 증가한다.

## ENDING_SUCCESSOR

```text
SuccessorGeneration = TRUE
WorldInfluence = PlayerTrait
```

플레이어의 성향이 세계 생성에 지속적으로 영향을 준다.

## ENDING_FREEDOM

```text
SuccessorGeneration = TRUE
InheritedTrait = OPTIONAL
FreeWillNPC = TRUE
```

NPC들이 자신의 선천적인 성향과 다른 선택을 할 가능성이 증가한다.

---

# 34. 전체 게임 루프

```text
Explore
    ↓
Meet NPC
    ↓
Quest / Combat / Choice
    ↓
Update Player Traits
    ↓
Explore New Chunk
    ↓
World Event
    ↓
Player Death
    ↓
Generate Successor NPC
    ↓
Respawn
    ↓
Successor NPC Lives In World
    ↓
NPC Creates Relationships
    ↓
NPC Creates Events
    ↓
Player Meets Successor
    ↓
World History Accumulates
    ↓
Continue Exploration
```

---

# 35. 메인 시스템 연결 구조

```text
[플레이어 행동]
        ↓
[성향 변화]
        ↓
[플레이어 죽음]
        ↓
[DeathRecord 생성]
        ↓
[계승 NPC 생성]
        ↓
[NPC 성장]
        ↓
[관계 / 사건 생성]
        ↓
[세계 변화]
        ↓
[메인 스토리 변화]
        ↓
[최종전 세력 변화]
        ↓
[에녹 보스전]
        ↓
[엔딩]
```

---

# 36. 구현용 기본 데이터 구조

## 36.1 Player

```cpp
struct Player
{
    float aggression;
    float mercy;
    float fear;
    float curiosity;
    float sacrifice;
    float greed;
    float hope;

    AbilityType primaryAbility;
    AbilityType secondaryAbility;

    int deathCount;

    std::vector<DeathRecord> deathHistory;
};
```

---

## 36.2 SuccessorNPC

```cpp
struct SuccessorNPC
{
    int id;

    int originDeathID;

    TraitType primaryTrait;
    TraitType secondaryTrait;

    AbilityType primaryAbility;
    AbilityType secondaryAbility;

    PersonalityType personality;
    JobType job;
    FactionType faction;

    WorldPosition location;

    int relationship;

    int power;

    bool alive;
    bool important;
};
```

---

## 36.3 DeathRecord

```cpp
struct DeathRecord
{
    int deathID;

    WorldPosition location;

    DeathCause cause;

    PlayerTraits traits;

    ActionType lastAction;

    int generatedSuccessorID;
};
```

---

## 36.4 World

```cpp
struct World
{
    std::vector<Chunk> generatedChunks;

    std::vector<Location> importantLocations;

    std::vector<SuccessorNPC> successorNPCs;

    std::vector<Faction> factions;

    std::vector<WorldEvent> worldEvents;

    std::vector<Rumor> rumors;

    std::vector<War> wars;

    std::vector<Village> destroyedVillages;

    std::vector<Village> restoredVillages;
};
```

---

# 37. 플레이어에게 던지는 질문

게임은 플레이어에게 정답을 직접 알려주지 않는다.

플레이 과정에서 다음 질문을 자연스럽게 던진다.

- 나는 어떤 삶을 살았는가?
- 내 죽음에서 태어난 존재는 누구인가?
- 그 존재는 축복인가, 저주인가?
- 나는 그 생명에게 책임이 있는가?
- 악인을 죽이는 것은 정말 세계를 좋은 방향으로 만드는가?
- 과거의 삶을 물려받은 사람은 그 삶에 책임이 있는가?
- 인류를 살리기 위해 개인을 희생시키는 것은 옳은가?
- 죽음이 새로운 생명을 만드는 것은 축복인가?
- 물려받은 성향을 거부할 수 있는가?

---

# 38. 핵심 게임 철학

이 게임에서는 플레이어가 만든 재미있는 요소가 한 번 사용되고 사라지는 것이 아니라 세계에 축적된다.

> 재미는 소비되는 것이 아니라 세계에 축적된다.

플레이어가 죽으면 새로운 NPC가 생긴다.

그 NPC는 살아가면서 다른 NPC와 관계를 만든다.

관계는 사건을 만든다.

사건은 새로운 갈등이나 퀘스트를 만든다.

그 결과가 다시 플레이어에게 돌아온다.

```text
Player
    ↓
Action
    ↓
Death
    ↓
Successor
    ↓
Relationship
    ↓
Event
    ↓
World Change
    ↓
Player
```

따라서 플레이 시간이 길어질수록 단순히 맵의 크기만 증가하는 것이 아니다.

플레이어가 만들어낸 **사람, 관계, 갈등, 사건, 소문, 세력, 역사**가 증가한다.

---

# 39. 게임의 핵심 질문

플레이어는 죽을 때마다 자신의 플레이를 돌아보게 된다.

```text
내가 어떤 삶을 살았는가?

내 죽음에서 태어난 존재는 누구인가?

그 존재는 축복인가, 저주인가?

나는 그 생명에 책임이 있는가?
```

플레이어의 죽음은 실패 기록이 아니라 세계의 역사다.

---

# 40. 최종 핵심 메시지

게임을 처음 시작했을 때 플레이어는 암흑시대에 던져진 이름 없는 인간이다.

살아남기 위해 싸울 수도 있다.

도망칠 수도 있다.

사람을 죽일 수도 있다.

누군가를 구할 수도 있다.

어떤 선택을 하더라도 그 행동은 세계에 흔적을 남긴다.

플레이어가 죽어도 그 흔적은 사라지지 않는다.

플레이어의 죽음에서 새로운 사람이 태어나고,

그 사람이 다른 사람과 관계를 만들고,

그 관계가 사건을 만들고,

그 사건이 역사가 된다.

결국 플레이어는 깨닫는다.

> 처음에는 세계가 플레이어를 만들었다.

> 마지막에는 플레이어가 세계를 만든다.