"use strict";

const $ = (id) => document.getElementById(id);
const names = {
  jobs: ["사냥꾼", "상인", "약탈자", "구도자", "수호자", "치유사"],
  activities: ["회복", "사냥", "거래", "이동", "약탈", "명상", "순찰", "치료", "방어"],
  goals: ["체력", "재산", "깨달음"],
  villages: ["잿빛 마을", "종탑 마을", "강변 마을", "안개숲 마을", "검은나루 마을"],
  abilities: ["육체", "감각", "정신", "운명", "물질", "생명"],
  events: ["사냥 수확", "물품 판매", "물품 구매", "공격", "사망", "계승자 탄생", "깨달음", "치료", "편지 도착", "이주 출발", "이주 완료"],
  characters: ["무인 마을", "약탈자 거점", "선량한 마을", "상업 마을", "구도자 마을", "혼합 마을"],
  moods: ["적막", "혼란", "궁핍", "불안", "평온"],
};
let snapshot = null;
let selected = null;
let page = 0;
let affinityPage = 0;
const AFFINITY_PAGE_SIZE = 10;
let timer = null;
let polling = false;
let mapPoints = [];
let npcById = new Map();
let villageOptionsKey = "";
let simulationRate = null;
let latestStatus = "waiting";
const PAGE_SIZE = 50;
const number = (v) => typeof v === "number" && Number.isFinite(v) ? v.toLocaleString("ko-KR", {maximumFractionDigits: 1}) : "—";
const label = (list, index) => list[index] ?? "미상";
const actor = (id) => id === 0 || id === -1 ? "플레이어" : `${npcById.get(id)?.name ?? "NPC"} #${id}`;
const villageName = (id) => snapshot?.villages.find((v) => v.id === id)?.name ?? label(names.villages, id);
const target = (id) => id === 0 ? "없음" : actor(id);
const healthRatio = (n) => n.maxHealth > 0 && Number.isFinite(n.health) ? Math.max(0, Math.min(1, n.health / n.maxHealth)) : 0;
const signed = (v) => Number.isFinite(v) ? Math.abs(v) < .05 ? "0" : `${v > 0 ? "+" : ""}${number(v)}` : "—";
const affinityClass = (v) => v < 0 ? "affinity-negative" : v > 0 ? "affinity-positive" : "muted";
const affinityTier = (n) => !Number.isFinite(n.relationship) ? "미수신" : n.relationship >= 75 ? "깊은 유대" :
  n.relationship >= 50 ? "친밀" : n.relationship >= 25 ? "우호" : n.relationship >= 0 ? "중립" : n.relationship > -50 ? "경계" : "적대";

function renderAffinity() {
  if (!snapshot) return;
  const known = snapshot.npcs.filter((n) => Number.isFinite(n.relationship));
  const alive = known.filter((n) => n.alive);
  const mean = alive.length ? alive.reduce((s, n) => s + n.relationship, 0) / alive.length : null;
  $("affinity-summary").textContent = `생존 평균 ${signed(mean)} · 우호 이상 ${alive.filter(n => n.relationship >= 25).length}명 · 경계/적대 ${alive.filter(n => n.relationship < 0).length}명`;
  const query = $("affinity-search").value.trim().toLowerCase();
  const village = $("affinity-village").value, tier = $("affinity-tier").value, scope = $("affinity-scope").value;
  const direction = $("affinity-sort").value === "asc" ? 1 : -1;
  const filtered = known.filter((n) => (scope === "all" || n.alive) && (scope !== "met" || n.met === true) &&
    (village === "all" || n.village === Number(village)) && (tier === "all" || affinityTier(n) === tier) &&
    `${n.name ?? "NPC"} #${n.id}`.toLowerCase().includes(query));
  filtered.sort((a, b) => direction * (a.relationship - b.relationship) || a.id - b.id);
  const pages = Math.ceil(filtered.length / AFFINITY_PAGE_SIZE);
  affinityPage = Math.max(0, Math.min(affinityPage, pages - 1));
  $("affinity-page").textContent = `${filtered.length}명 · ${pages ? affinityPage + 1 : 0} / ${pages}`;
  $("affinity-prev").disabled = affinityPage === 0;
  $("affinity-next").disabled = affinityPage + 1 >= pages;
  const rows = document.createDocumentFragment();
  const start = affinityPage * AFFINITY_PAGE_SIZE;
  for (const [index, n] of filtered.slice(start, start + AFFINITY_PAGE_SIZE).entries()) {
    const row = node("tr", undefined, n.id === selected ? "selected" : "");
    cell(row, start + index + 1);
    const who = cell(row);
    const button = node("button", actor(n.id), "npc-link");
    button.addEventListener("click", () => selectNpc(n.id));
    who.append(button, node("small", `${villageName(n.village)} · ${n.alive ? label(names.jobs, n.job) : "사망"}`));
    const score = cell(row, signed(n.relationship));
    score.className = affinityClass(n.relationship);
    const bar = node("meter");
    bar.min = -100; bar.max = 100; bar.low = 0; bar.high = 25; bar.optimum = 100; bar.value = n.relationship;
    bar.setAttribute("aria-label", `호감도 ${n.relationship}`);
    score.append(bar);
    cell(row, affinityTier(n));
    const memory = n.affinity;
    cell(row, memory?.lastReason ? `${memory.lastReason} ${signed(memory.lastDelta)}` : "기록 없음",
      Number.isFinite(memory?.lastChange) ? `${number(Math.max(0, snapshot.simulationTime - memory.lastChange))}초 전` : "");
    cell(row, !Number.isFinite(memory?.talkCooldown) ? "미수신" : memory.talkCooldown > .001 ? `${Math.ceil(memory.talkCooldown)}초` : n.relationship >= 100 ? "최대 호감도" : "+5 가능");
    rows.append(row);
  }
  if (filtered.length) $("affinity-ranking").replaceChildren(rows);
  else emptyRow($("affinity-ranking"), 6, known.length ? "조건에 맞는 NPC 없음" : "호감도 데이터 미수신");

  const history = snapshot.affinity?.events;
  const onlySelected = $("affinity-selected-only").checked;
  const entries = (Array.isArray(history) ? history : []).filter((e) => !onlySelected || e.npcId === selected).slice(-128).reverse();
  $("affinity-history-info").textContent = !Array.isArray(history) ? "변화 내역 미수신 · 새 게임 실행 파일이 필요합니다." :
    `최근 128건 중 ${entries.length}건 · ${onlySelected ? selected === null ? "NPC를 선택하세요" : actor(selected) : "모든 NPC"} · 시뮬레이션 시간 기준`;
  const events = document.createDocumentFragment();
  for (const e of entries) {
    const row = node("tr");
    cell(row, `${number(e.time)}s`, `${number(Math.max(0, snapshot.simulationTime - e.time))}초 전`);
    const button = node("button", actor(e.npcId), "npc-link");
    button.addEventListener("click", () => selectNpc(e.npcId));
    cell(row).append(button);
    cell(row, e.reason);
    cell(row, signed(e.delta)).className = affinityClass(e.delta);
    cell(row, `${signed(e.before)} → ${signed(e.after)}`);
    events.append(row);
  }
  if (entries.length) $("affinity-history").replaceChildren(events);
  else emptyRow($("affinity-history"), 5, onlySelected && selected === null ? "순위에서 NPC를 선택하세요" : "기록된 호감도 변화 없음");
}

function node(tag, text, className) {
  const value = document.createElement(tag);
  if (text !== undefined) value.textContent = text;
  if (className) value.className = className;
  return value;
}

function cell(row, text, secondary) {
  const td = node("td", text);
  if (secondary !== undefined) td.append(node("small", secondary));
  row.append(td);
  return td;
}

function emptyRow(body, columns, message) {
  const row = node("tr");
  const td = node("td", message, "empty");
  td.colSpan = columns;
  row.append(td);
  body.replaceChildren(row);
}

function healthCell(row, n) {
  const td = cell(row);
  const box = node("div", undefined, `health${healthRatio(n) < .35 ? " critical" : ""}`);
  box.append(node("span", `${number(n.health)} / ${number(n.maxHealth)}`));
  const bar = document.createElement("progress");
  bar.max = 1;
  bar.value = healthRatio(n);
  bar.setAttribute("aria-label", "체력");
  box.append(bar);
  td.append(box);
}

function validSnapshot(s) {
  return s && s.type === "world.snapshot" && s.schemaVersion === 1 && typeof s.session === "string" &&
    Number.isSafeInteger(s.sequence) && Number.isFinite(s.timestampMs) && s.counts && s.player && s.statistics &&
    s.combat && Array.isArray(s.npcs) && Array.isArray(s.villages) && Array.isArray(s.events) &&
    s.npcs.every((n) => n && Number.isInteger(n.id) && typeof n.alive === "boolean") &&
    s.villages.every((v) => v && Number.isInteger(v.id) && Array.isArray(v.jobs) && Array.isArray(v.activities)) &&
    s.events.every((e) => e && Number.isInteger(e.kind)) &&
    (s.rumors === undefined || (Array.isArray(s.rumors) && s.rumors.every((r) => r &&
      Number.isInteger(r.id) && Number.isInteger(r.reachMask) && r.reachMask >= 1 && r.reachMask <= 127 &&
      Number.isInteger(r.actor) && Number.isInteger(r.victim) && Number.isInteger(r.village) &&
      Number.isInteger(r.witnesses) && typeof r.active === "boolean")));
}

function connection(status, age) {
  latestStatus = status;
  const text = {live: "실시간", paused: "게임 일시정지", stopped: "게임 종료", stale: "갱신 지연", waiting: "게임 대기", error: "데이터 오류", offline: "서버 연결 끊김"};
  $("connection").textContent = text[status] ?? "데이터 오류";
  $("connection").className = `status ${status === "offline" ? "error" : status}`;
  const messages = {
    waiting: snapshot ? "게임 스냅샷 대기 중. 마지막 수신 데이터가 유지됩니다." : "게임 스냅샷 대기 중",
    stopped: "게임이 종료되었습니다. 마지막 상태입니다.",
    stale: `마지막 스냅샷 이후 ${number(age)}초 경과. 표시된 데이터는 실시간 상태가 아닙니다.`,
    error: "JSON 스냅샷을 읽을 수 없습니다. 마지막 수신 데이터가 유지됩니다.",
    offline: "로컬 서버에 연결할 수 없습니다. 마지막 수신 데이터가 유지됩니다.",
  };
  $("notice").hidden = !messages[status];
  $("notice").textContent = messages[status] ?? "";
  $("metric-state").textContent = status === "live" ? "실시간" : "마지막 수신 상태";
  if (status !== "live") $("simulation-rate").textContent = status === "paused" ? "0 tick/s · 일시정지" : "—";
}

function renderDiagnostics(data, responseMs) {
  $("received-at").textContent = new Date().toLocaleTimeString("ko-KR");
  $("source-age").textContent = typeof data.ageSeconds === "number" ? `${number(data.ageSeconds)}초` : "—";
  $("transfer").textContent = `${number((data.bridge?.snapshotBytes ?? 0) / 1024)} KB / ${number(responseMs)} ms`;
  $("source-path").textContent = data.bridge?.snapshotPath ?? "경로 정보 없음";
  const error = data.bridge?.error;
  $("source-error").hidden = !error;
  $("source-error").textContent = error === "source_missing" ? "게임이 생성한 snapshot.json 파일이 없습니다." : error ?? "";
}

function renderOperations() {
  const alive = snapshot.npcs.filter((n) => n.alive);
  const critical = alive.filter((n) => healthRatio(n) < .35);
  const engaged = alive.filter((n) => n.target !== 0);
  const pursuers = alive.filter((n) => n.target === -1);
  const activeRumors = (snapshot.rumors ?? []).filter((r) => r.active);
  const alerts = [];
  if (snapshot.player.health < snapshot.player.maxHealth * .35) alerts.push("플레이어 체력 위급");
  if (pursuers.length) alerts.push(`플레이어 추적 ${pursuers.length}명: ${pursuers.slice(0, 5).map((n) => actor(n.id)).join(", ")}`);
  if (critical.length) alerts.push(`위급 NPC ${critical.length}명: ${critical.slice(0, 5).map((n) => actor(n.id)).join(", ")}`);
  for (const v of snapshot.villages) {
    if (v.residents === 0) alerts.push(`${villageName(v.id)}: 생존 소속 주민 없음`);
    if (v.stock < 10) alerts.push(`${villageName(v.id)}: 물품 부족 (${number(v.stock)})`);
    if (v.treasury < 20) alerts.push(`${villageName(v.id)}: 공동 금고 부족 (${number(v.treasury)})`);
    if (v.recentDeaths > 0) alerts.push(`${villageName(v.id)}: 최근 인근 사망 ${number(v.recentDeaths)}건`);
  }
  $("alert-count").textContent = `${alerts.length}건`;
  $("alerts").replaceChildren(...(alerts.length ? alerts : ["현재 감지된 위험 없음"]).map((text) => node("li", text)));
  const sum = (values, key) => values.reduce((total, value) => total + (Number.isFinite(value[key]) ? value[key] : 0), 0);
  const populationHealth = sum(alive, "maxHealth");
  const fields = [
    ["전체 교전 / 플레이어 추적", `${engaged.length}명 / ${pursuers.length}명`],
    ["주민 체력 / 위급 인원", `${populationHealth ? number(sum(alive, "health") / populationHealth * 100) : "—"}% / ${critical.length}명`],
    ["전체 은화 (사망자 포함)", number(sum(snapshot.npcs, "gold") + sum(snapshot.villages, "treasury") + snapshot.player.gold)],
    ["마을 물품 / 생존 주민 물품", `${number(sum(snapshot.villages, "stock"))} / ${number(sum(alive, "goods"))}`],
    ["생존 주민 깨달음", number(sum(alive, "insight"))],
    ["활성 살인 소문", `${activeRumors.length}건`],
    ["선제 약탈 보호 잔여", Number.isFinite(snapshot.player.raidGraceSeconds) ? `${number(snapshot.player.raidGraceSeconds)}초` : "미수신"],
    ["연속 피격 보호 잔여", Number.isFinite(snapshot.player.hitRecoverySeconds) ? `${number(snapshot.player.hitRecoverySeconds)}초` : "미수신"],
    ["배달 중 편지", number((snapshot.letters ?? []).length)],
    ["이주 중 주민", number(snapshot.npcs.filter(n => n.alive && n.movingTo >= 0).length)],
  ];
  const dl = document.createDocumentFragment();
  for (const [title, value] of fields) dl.append(node("dt", title), node("dd", value));
  $("world-metrics").replaceChildren(dl);
}

function renderSummary() {
  const s = snapshot;
  $("alive").textContent = number(s.counts.npcAlive);
  $("population").textContent = `전체 ${number(s.counts.npcTotal)} / 사망 ${number(s.counts.npcDead)}`;
  $("mobs").textContent = number(s.counts.mobs);
  $("combat").textContent = s.combat.active ? `${s.combat.story ? "스토리 전투" : "경작지"} · 등장 중 ${number(s.counts.spawning)} · 보스 ${number(s.counts.bosses)}` : "오픈월드 · 개체형 야생 몹 없음";
  $("player-health").textContent = `${number(s.player.health)} / ${number(s.player.maxHealth)}`;
  $("player-gold").textContent = `은화 ${number(s.player.gold)} / 사망 ${number(s.player.deaths)}`;
  $("trades").textContent = number(s.statistics.trades);
  $("raids").textContent = `약탈 명중 ${number(s.statistics.raidHits)} / NPC 사망 ${number(s.statistics.npcDeaths)}`;
  $("clock").textContent = `${new Date(s.timestampMs).toLocaleString("ko-KR")} · 월드 ${number(s.worldTime)}s · tick ${number(s.tick)}`;
  $("session").textContent = `세션 ${s.session} · 메시지 #${s.sequence}`;
  $("download").disabled = false;
}

function renderVillages() {
  const key = JSON.stringify(snapshot.villages.map((v) => [v.id, villageName(v.id)]));
  if (key !== villageOptionsKey) {
    villageOptionsKey = key;
    for (const id of ["village-filter", "rumor-village", "affinity-village"]) {
      const select = $(id);
      const selectedValue = select.value;
      const all = node("option", "모든 마을");
      all.value = "all";
      const options = snapshot.villages.map((v) => {
        const option = node("option", villageName(v.id));
        option.value = String(v.id);
        return option;
      });
      select.replaceChildren(all, ...options);
      select.value = snapshot.villages.some((v) => String(v.id) === selectedValue) ? selectedValue : "all";
    }
  }
  const rows = document.createDocumentFragment();
  const details = document.createDocumentFragment();
  for (const v of snapshot.villages) {
    const row = node("tr");
    cell(row, villageName(v.id));
    cell(row, label(names.characters, v.character), label(names.moods, v.mood)).className = v.mood === 1 ? "critical" : "";
    cell(row, `${number(v.residents)} / ${number(v.present)}`);
    cell(row, v.maxHealth ? `${number(v.health / v.maxHealth * 100)}%` : "—");
    cell(row, `${number(v.injured)} / ${number(v.critical)}`).className = v.critical ? "critical" : "";
    cell(row, number(v.treasury));
    cell(row, number(v.stock));
    cell(row, number(v.gold), `물품 ${number(v.goods)} · 깨달음 ${number(v.insight)}`);
    rows.append(row);
    const detail = node("p");
    detail.append(node("b", villageName(v.id)));
    detail.append(document.createTextNode(`약탈 성향 ${number((v.averageRaidWeight ?? 0) * 100)}% · 약탈 성향 주민 ${number(v.raiders)} · 최근 사망 ${number(v.recentDeaths)} · 유통 소문 ${number(v.heardRumors)}`), node("br"));
    detail.append(document.createTextNode(v.jobs.map((count, i) => `${label(names.jobs, i)} ${number(count)}`).join(" · ")));
    detail.append(node("br"), document.createTextNode(`교전 ${number(v.engaged)} · ` + v.activities.map((count, i) => count ? `${label(names.activities, i)} ${number(count)}` : "").filter(Boolean).join(" · ")));
    details.append(detail);
  }
  $("villages").replaceChildren(rows);
  $("village-details").replaceChildren(details);
}

function selectNpc(id) {
  selected = id;
  renderNpcs();
  renderMap();
  renderAffinity();
}

function renderNpcs() {
  if (!snapshot) return;
  const search = $("search").value.trim().toLowerCase();
  const village = $("village-filter").value;
  const job = $("job-filter").value;
  const life = $("life-filter").value;
  const sort = $("sort").value;
  const filtered = snapshot.npcs.filter((n) =>
    (village === "all" || n.village === Number(village)) && (job === "all" || n.job === Number(job)) &&
    (life === "all" || n.alive === (life === "alive")) &&
    (`npc #${n.id} ${n.name ?? ""} ${villageName(n.village)} ${label(names.jobs, n.job)} ${label(names.activities, n.activity)}`).toLowerCase().includes(search));
  filtered.sort((a, b) => {
    if (sort === "health") return healthRatio(a) - healthRatio(b) || a.id - b.id;
    if (sort === "gold" || sort === "insight" || sort === "relationship") return (b[sort] ?? 0) - (a[sort] ?? 0) || a.id - b.id;
    return a.id - b.id;
  });
  const pages = Math.ceil(filtered.length / PAGE_SIZE);
  page = Math.max(0, Math.min(page, pages - 1));
  $("result-count").textContent = `${number(filtered.length)}명 / 전체 ${number(snapshot.npcs.length)}명`;
  $("page-label").textContent = `${pages ? page + 1 : 0} / ${pages}`;
  $("previous").disabled = page === 0;
  $("next").disabled = page + 1 >= pages;
  const rows = document.createDocumentFragment();
  for (const n of filtered.slice(page * PAGE_SIZE, (page + 1) * PAGE_SIZE)) {
    const row = node("tr", undefined, n.id === selected ? "selected" : "");
    const first = cell(row);
    const button = node("button", n.name ?? `NPC #${n.id}`, "npc-link");
    button.addEventListener("click", () => selectNpc(n.id));
    first.append(button, node("small", `#${n.id} · ${n.alive ? "생존" : "사망"}`));
    cell(row, label(names.jobs, n.job), villageName(n.village));
    cell(row, n.alive ? label(names.activities, n.activity) : "사망", label(names.goals, n.goal));
    healthCell(row, n);
    cell(row, number(n.gold));
    cell(row, number(n.goods));
    cell(row, number(n.insight));
    cell(row, target(n.target));
    rows.append(row);
  }
  if (filtered.length) $("npcs").replaceChildren(rows);
  else emptyRow($("npcs"), 8, "조건에 맞는 NPC 없음");
  renderDetail();
}

function renderDetail() {
  const n = snapshot.npcs.find((value) => value.id === selected);
  if (!n) {
    $("detail").replaceChildren(node("h3", "선택한 NPC"), node("p", "선택 없음", "muted"));
    return;
  }
  const fields = [
    ["직업", label(names.jobs, n.job)], ["상태", n.alive ? label(names.activities, n.activity) : "사망"],
    ["목표", label(names.goals, n.goal)], ["소속 마을", villageName(n.village)],
    ["현재 위치", n.presentVillage < 0 ? "마을 밖" : villageName(n.presentVillage)],
    ["행선 마을", villageName(n.destination)], ["고유능력", label(names.abilities, n.ability)],
    ["주택 필지", n.houseLot >= 0 ? `${villageName(n.village)} ${n.houseLot + 1}번지` : "배정 대기"],
    ["이주 목적지", n.movingTo >= 0 ? villageName(n.movingTo) : "없음"],
    ["집 입구", Number.isFinite(n.homeX) ? `${n.homeX.toFixed(0)}, ${n.homeY.toFixed(0)}` : "미수신"],
    ["전투력", number(n.power)], ["플레이어 호감도", `${signed(n.relationship)} · ${affinityTier(n)}`],
    ["신뢰 / 만남", `${number(n.trust)} / ${n.met === undefined ? "미수신" : n.met ? "대화한 사이" : "대화 전"}`],
    ["최근 호감도 변화", n.affinity?.lastReason ? `${n.affinity.lastReason} ${signed(n.affinity.lastDelta)}` : "기록 없음"],
    ["약탈 가중치", number(n.raidWeight)],
    ["좌표", `${number(n.x)}, ${number(n.y)}`], ["대상", target(n.target)],
    ["기원", label(["플레이어 계승", "NPC 계승", "일반 주민"], n.origin)],
    ["죽음 기록", n.originDeathId ? `#${n.originDeathId}` : "없음"],
    ["이전 인물", n.parentId ? actor(n.parentId) : "없음"],
    ["계승자", n.successorId ? actor(n.successorId) : "없음"],
  ];
  const dl = node("dl");
  for (const [title, value] of fields) dl.append(node("dt", title), node("dd", value));
  $("detail").replaceChildren(node("h3", actor(n.id)), dl);
}

function renderEvents() {
  const kinds = {violent: [3, 4], economy: [0, 1, 2], life: [5, 6, 7]};
  const filter = $("event-kind").value;
  const rows = document.createDocumentFragment();
  const events = snapshot.events.filter((e) => filter === "all" || kinds[filter].includes(e.kind));
  for (const e of events.slice(-128).reverse()) {
    const row = node("tr");
    cell(row, `${number(e.time)}s`);
    cell(row, label(names.events, e.kind));
    cell(row, actor(e.actor));
    cell(row, e.other ? actor(e.other) : "—");
    cell(row, number(e.amount));
    rows.append(row);
  }
  if (events.length) $("events").replaceChildren(rows);
  else emptyRow($("events"), 5, "기록된 사건 없음");
}

function renderRumors() {
  if (!snapshot) return;
  const village = $("rumor-village").value;
  const activeOnly = $("rumor-active").checked;
  const rumors = (snapshot.rumors ?? []).filter((r) => (!activeOnly || r.active) &&
    (village === "all" || (r.reachMask & (1 << Number(village)))));
  $("rumor-count").textContent = `${rumors.length}건 / 기록 ${(snapshot.rumors ?? []).length}건`;
  const rows = document.createDocumentFragment();
  for (const r of [...rumors].reverse()) {
    const row = node("tr");
    cell(row, `살인 소문 #${r.id}`, r.active ? "유통 중" : "지난 소문");
    cell(row, `${number(r.age)}s`);
    cell(row, actor(r.actor), `피해자 ${actor(r.victim)}`);
    cell(row, villageName(r.village), `${number(r.x)}, ${number(r.y)}`);
    cell(row, r.witnesses ? `${r.witnesses}명 목격` : "미확인 전언");
    cell(row, snapshot.villages.filter((v) => r.reachMask & (1 << v.id)).map((v) => villageName(v.id)).join(" · "));
    rows.append(row);
  }
  if (rumors.length) $("rumors").replaceChildren(rows);
  else emptyRow($("rumors"), 6, "조건에 맞는 소문 없음");
}

function renderMap() {
  if (!snapshot) return;
  const canvas = $("map");
  const bounds = canvas.getBoundingClientRect();
  const width = bounds.width, height = bounds.height;
  if (!width || !height) return;
  const ratio = window.devicePixelRatio || 1;
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext("2d");
  ctx.scale(ratio, ratio);
  ctx.fillStyle = "#e9eeeb";
  ctx.fillRect(0, 0, width, height);
  const extent = Math.max(800, ...snapshot.villages.map(v => Math.max(Math.abs(v.x), Math.abs(v.y)) + (v.radius ?? 160)));
  const scale = Math.min((width - 48) / (extent*2), (height - 48) / (extent*2));
  const point = (p) => ({x: width / 2 + p.x * scale, y: height / 2 - p.y * scale});
  ctx.strokeStyle = "#d9e1db";
  ctx.lineWidth = 1;
  for (let v = -extent; v <= extent; v += extent / 5) {
    const a = point({x: v, y: -extent}), b = point({x: v, y: extent});
    const c = point({x: -extent, y: v}), d = point({x: extent, y: v});
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b.x, b.y); ctx.moveTo(c.x, c.y); ctx.lineTo(d.x, d.y); ctx.stroke();
  }
  ctx.strokeStyle = "#b0b9ac";
  ctx.lineWidth = 6;
  const villages = snapshot.villages;
  ctx.beginPath();
  villages.forEach((v, i) => villages.slice(i + 1).forEach(other => {
    if (Math.hypot(v.x-other.x,v.y-other.y) > 1717) return;
    const a = point(v), b = point(other);
    ctx.moveTo(a.x,a.y); ctx.lineTo(b.x,b.y);
  }));
  ctx.stroke();
  ctx.font = '11px "Malgun Gothic", sans-serif';
  for (const v of villages) {
    const p = point(v);
    if (v.radius) {
      ctx.strokeStyle = "#a6b6a8"; ctx.lineWidth = 1;
      ctx.beginPath(); ctx.arc(p.x,p.y,v.radius*scale,0,Math.PI*2); ctx.stroke();
    }
    ctx.fillStyle = "#836426";
    ctx.fillRect(p.x - 5, p.y - 5, 10, 10);
    ctx.fillStyle = "#35463b";
    ctx.fillText(villageName(v.id), p.x + 9, p.y - 9);
  }
  mapPoints = [];
  let outside = 0;
  for (const n of snapshot.npcs) {
    if (!n.alive || !Number.isFinite(n.x) || !Number.isFinite(n.y)) continue;
    if (Math.abs(n.x) > extent || Math.abs(n.y) > extent) { outside++; continue; }
    const p = point(n);
    mapPoints.push({...p, id: n.id});
    ctx.fillStyle = n.target ? "#bc4949" : "#267b64";
    ctx.beginPath(); ctx.arc(p.x, p.y, n.id === selected ? 5 : 2.5, 0, Math.PI * 2); ctx.fill();
    if (n.id === selected) { ctx.strokeStyle = "#122d25"; ctx.lineWidth = 2; ctx.stroke(); }
  }
  if (!snapshot.combat.active && Number.isFinite(snapshot.player.x) && Number.isFinite(snapshot.player.y) && Math.abs(snapshot.player.x) <= extent && Math.abs(snapshot.player.y) <= extent) {
    const p = point(snapshot.player);
    ctx.fillStyle = "#316ac4";
    ctx.beginPath(); ctx.moveTo(p.x, p.y - 5); ctx.lineTo(p.x + 5, p.y); ctx.lineTo(p.x, p.y + 5); ctx.lineTo(p.x - 5, p.y); ctx.closePath(); ctx.fill();
  }
  $("off-map").textContent = outside ? `생활권 밖 ${outside}명` : "";
}

async function poll() {
  clearTimeout(timer);
  if (polling) return;
  polling = true;
  $("refresh").disabled = true;
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 4000);
  const started = performance.now();
  let reachedServer = false;
  try {
    const response = await fetch("/api/state", {cache: "no-store", signal: controller.signal});
    reachedServer = true;
    if (!response.ok) throw new Error("HTTP status");
    const data = await response.json();
    renderDiagnostics(data, performance.now() - started);
    if (data.snapshot && !validSnapshot(data.snapshot)) {
      connection("error");
    } else {
      connection(data.status, data.ageSeconds);
      const s = data.snapshot;
      if (s && (!snapshot || s.session !== snapshot.session || s.sequence > snapshot.sequence)) {
        simulationRate = snapshot && s.session === snapshot.session && s.timestampMs > snapshot.timestampMs && s.tick >= snapshot.tick
          ? (s.tick - snapshot.tick) * 1000 / (s.timestampMs - snapshot.timestampMs) : null;
        if (snapshot && s.session !== snapshot.session) { selected = null; page = 0; affinityPage = 0; }
        snapshot = s;
        npcById = new Map(s.npcs.map((n) => [n.id, n]));
        renderSummary(); renderVillages(); renderNpcs(); renderEvents(); renderRumors(); renderMap(); renderOperations();
        renderAffinity();
      }
      if (latestStatus === "live") $("simulation-rate").textContent = simulationRate === null ? "계산 중" : `${number(simulationRate)} tick/s`;
    }
  } catch (error) {
    connection(reachedServer ? "error" : "offline");
    $("source-error").hidden = false;
    $("source-error").textContent = reachedServer ? `응답 처리 오류: ${error.message}` : "로컬 모니터 서버에 연결할 수 없습니다.";
  } finally {
    clearTimeout(timeout);
    polling = false;
    $("refresh").disabled = false;
    timer = setTimeout(poll, Number($("interval").value));
  }
}

for (const [i, name] of names.jobs.entries()) {
  const option = node("option", name);
  option.value = String(i);
  $("job-filter").append(option);
}
for (const id of ["village-filter", "job-filter", "life-filter", "sort", "search"]) {
  $(id).addEventListener(id === "search" ? "input" : "change", () => { page = 0; renderNpcs(); });
}
$("previous").addEventListener("click", () => { page--; renderNpcs(); });
for (const id of ["affinity-search", "affinity-village", "affinity-tier", "affinity-scope", "affinity-sort"]) {
  $(id).addEventListener(id === "affinity-search" ? "input" : "change", () => { affinityPage = 0; renderAffinity(); });
}
$("affinity-prev").addEventListener("click", () => { affinityPage--; renderAffinity(); });
$("affinity-next").addEventListener("click", () => { affinityPage++; renderAffinity(); });
$("affinity-selected-only").addEventListener("change", renderAffinity);
$("event-kind").addEventListener("change", () => { if (snapshot) renderEvents(); });
for (const id of ["rumor-village", "rumor-active"]) $(id).addEventListener("change", renderRumors);
$("next").addEventListener("click", () => { page++; renderNpcs(); });
$("refresh").addEventListener("click", poll);
$("interval").addEventListener("change", poll);
$("map").addEventListener("click", (event) => {
  const rect = $("map").getBoundingClientRect();
  const nearby = mapPoints.map((p) => ({id: p.id, distance: Math.hypot(p.x - event.clientX + rect.left, p.y - event.clientY + rect.top)})).filter((p) => p.distance < 12).sort((a, b) => a.distance - b.distance);
  if (nearby.length) selectNpc(nearby[0].id);
});
$("download").addEventListener("click", () => {
  if (!snapshot) return;
  const url = URL.createObjectURL(new Blob([JSON.stringify(snapshot, null, 2)], {type: "application/json"}));
  const link = node("a");
  link.href = url;
  link.download = `world-${snapshot.session}-${snapshot.sequence}.json`;
  document.body.append(link);
  link.click();
  link.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
new ResizeObserver(renderMap).observe($("map"));
document.addEventListener("visibilitychange", () => { if (!document.hidden) poll(); });
if (location.protocol === "file:") {
  $("connection").textContent = "HTTP 서버 필요";
  $("notice").textContent = "접속 주소: http://127.0.0.1:8765/";
} else {
  poll();
}
