/*
 * stiebel-cards.js — two Lovelace cards for ha-stiebel-control, no dependencies.
 *
 *   custom:stiebel-heating-curve-card   heating curve diagram as in the WPMW II manual
 *   custom:stiebel-week-program-card    time program (7 text entities) as week bars, editable
 *
 * Install: copy to /config/www/stiebel-cards.js and add the resource
 * /local/stiebel-cards.js (JavaScript module). Examples: docs/CONFIGURATION.md, "Dashboards".
 *
 * Heating curve model. Stiebel publishes the curves only as a diagram (WPMW II manual,
 * "Heizkurven-Diagramm": slopes 0.2 ... 3 for a room setpoint of 20 °C; another room setpoint
 * shifts the curve in parallel). The curves follow
 *     T = R + S * 20 * ((20 - Ta) / 20) ^ 0.63        (Ta < 20 °C, else T = R)
 * with room setpoint R, slope S and outdoor temperature Ta: at 0 °C the rise is S * 20 K, at
 * -20 °C S * 31 K. This reproduces the display points of the manual (0.60, R 20: 20/32/39 °C
 * at +20/0/-20 °C) and of a WPL13E (0.30, R 21: 21/27/30; 0.30, R 20: 20/26/29), and ten days of
 * its return setpoint (KESSELSOLLTEMP) with a constant +0.5 K offset (rms 0.28 K). The setpoint
 * marker of the card shows how well the model fits a given installation.
 */

const CURVE_EXPONENT = 0.63;

function curveValue(slope, room, outdoor, exponent = CURVE_EXPONENT) {
  const d = 20 - outdoor;
  if (d <= 0) return room;
  return room + slope * 20 * Math.pow(d / 20, exponent);
}

// Numeric value of an entity; for a climate entity its target temperature
function num(hass, entityId) {
  const st = entityId && hass.states[entityId];
  if (!st) return NaN;
  if (entityId.startsWith("climate.")) return parseFloat(st.attributes.temperature);
  return parseFloat(st.state);
}

function fmt(v, digits = 1) {
  return Number.isFinite(v) ? v.toLocaleString(undefined, { minimumFractionDigits: digits, maximumFractionDigits: digits }) : "–";
}

const BASE_STYLE = `
  ha-card { padding: 12px 16px 16px; }
  .head { display: flex; align-items: baseline; justify-content: space-between; gap: 8px; }
  .title { font-size: 1.1em; font-weight: 500; color: var(--primary-text-color); }
  .sub { color: var(--secondary-text-color); font-size: 0.85em; }
  button.pill { font: inherit; font-size: 0.9em; cursor: pointer; border-radius: 16px; padding: 4px 12px;
    border: 1px solid var(--divider-color); background: var(--card-background-color); color: var(--primary-text-color); }
  button.pill.primary { background: var(--primary-color); color: var(--text-primary-color, #fff); border-color: var(--primary-color); }
  button.icon { font: inherit; cursor: pointer; border: none; background: none; color: var(--secondary-text-color);
    width: 32px; height: 32px; border-radius: 50%; font-size: 1.1em; }
  button.icon:hover { background: var(--secondary-background-color); }
  svg text { fill: var(--secondary-text-color); font-size: 11px; font-family: inherit; }
`;

// ---------------------------------------------------------------------------
// Heating curve
// ---------------------------------------------------------------------------
class StiebelHeatingCurveCard extends HTMLElement {
  setConfig(config) {
    if (!config.slope || !config.room) throw new Error("slope and room are required");
    this._config = {
      title: "Heizkurve",
      y_label: "Soll",
      compare: [0.2, 0.4, 0.6, 0.8],
      color: "var(--state-climate-heat-color, #ff8100)",
      exponent: CURVE_EXPONENT,
      ...config,
    };
    this._preview = null;
    if (!this.shadowRoot) this.attachShadow({ mode: "open" });
  }

  set hass(hass) {
    this._hass = hass;
    this._render();
  }

  getCardSize() { return 6; }
  getGridOptions() { return { columns: 12, min_columns: 6, rows: "auto" }; }

  _slopeAttrs() {
    const st = this._hass.states[this._config.slope];
    const a = (st && st.attributes) || {};
    return { min: a.min ?? 0.1, max: a.max ?? 3, step: a.step ?? 0.05 };
  }

  _step(dir) {
    const actual = num(this._hass, this._config.slope);
    const { min, max, step } = this._slopeAttrs();
    const base = this._preview ?? actual;
    const next = Math.round((base + dir * step) / step) * step;
    this._preview = Math.min(max, Math.max(min, Number(next.toFixed(3))));
    if (Math.abs(this._preview - actual) < 1e-6) this._preview = null;
    this._render();
  }

  _apply() {
    if (this._preview == null) return;
    this._hass.callService("number", "set_value", { entity_id: this._config.slope, value: this._preview });
    this._preview = null;
    this._render();
  }

  _render() {
    if (!this._hass || !this._config) return;
    const c = this._config;
    const slope = num(this._hass, c.slope);
    const room = num(this._hass, c.room);
    const night = num(this._hass, c.room_night);
    const outdoor = num(this._hass, c.outdoor);
    const setpoint = num(this._hass, c.setpoint);
    const shown = this._preview ?? slope;

    // geometry: outdoor +20 (left) ... -20 (right), like the manual
    const W = 360, H = 228, L = 34, R = 10, T = 10, B = 34;
    const xMin = -20, xMax = 20;
    const curves = [];
    const pts = (s, r) => {
      const out = [];
      for (let ta = xMax; ta >= xMin; ta -= 1) out.push([ta, curveValue(s, r, ta, c.exponent)]);
      return out;
    };
    const rRef = Number.isFinite(room) ? room : 20;
    for (const s of c.compare) curves.push({ s, p: pts(s, rRef), kind: "compare" });
    if (Number.isFinite(shown)) {
      if (Number.isFinite(night)) curves.push({ s: shown, p: pts(shown, night), kind: "night" });
      if (this._preview != null && Number.isFinite(slope)) curves.push({ s: slope, p: pts(slope, rRef), kind: "old" });
      curves.push({ s: shown, p: pts(shown, rRef), kind: "main" });
    }
    let yMax = 20;
    for (const cv of curves) for (const [, y] of cv.p) yMax = Math.max(yMax, y);
    if (Number.isFinite(setpoint)) yMax = Math.max(yMax, setpoint);
    const yMin = Math.min(15, Number.isFinite(night) ? Math.floor(night) - 1 : 15);
    yMax = Math.ceil((yMax + 2) / 5) * 5;
    const sx = (ta) => L + ((xMax - ta) / (xMax - xMin)) * (W - L - R);
    const sy = (y) => T + ((yMax - y) / (yMax - yMin)) * (H - T - B);
    const path = (p) => p.map(([x, y], i) => `${i ? "L" : "M"}${sx(x).toFixed(1)},${sy(y).toFixed(1)}`).join("");

    let grid = "";
    for (let ta = xMax; ta >= xMin; ta -= 5) {
      grid += `<line x1="${sx(ta)}" x2="${sx(ta)}" y1="${T}" y2="${H - B}" stroke="var(--divider-color)" stroke-width="0.5"/>`;
      grid += `<text x="${sx(ta)}" y="${H - B + 14}" text-anchor="middle">${ta}</text>`;
    }
    for (let y = Math.ceil(yMin / 5) * 5; y <= yMax; y += 5) {
      grid += `<line x1="${L}" x2="${W - R}" y1="${sy(y)}" y2="${sy(y)}" stroke="var(--divider-color)" stroke-width="0.5"/>`;
      grid += `<text x="${L - 4}" y="${sy(y) + 4}" text-anchor="end">${y}</text>`;
    }
    let lines = "";
    for (const cv of curves) {
      if (cv.kind === "compare") {
        const last = cv.p[cv.p.length - 1];
        lines += `<path d="${path(cv.p)}" fill="none" stroke="var(--secondary-text-color)" stroke-opacity="0.35" stroke-width="1"/>`;
        lines += `<text x="${sx(last[0]) - 2}" y="${sy(last[1]) - 3}" text-anchor="end" opacity="0.7">${fmt(cv.s, 1)}</text>`;
      } else if (cv.kind === "night") {
        lines += `<path d="${path(cv.p)}" fill="none" stroke="${c.color}" stroke-opacity="0.6" stroke-width="1.5" stroke-dasharray="4 3"/>`;
      } else if (cv.kind === "old") {
        lines += `<path d="${path(cv.p)}" fill="none" stroke="${c.color}" stroke-opacity="0.35" stroke-width="2"/>`;
      } else {
        lines += `<path d="${path(cv.p)}" fill="none" stroke="${c.color}" stroke-width="2.5"/>`;
      }
    }
    let marker = "";
    if (Number.isFinite(outdoor) && outdoor <= xMax && outdoor >= xMin) {
      const model = Number.isFinite(shown) ? curveValue(shown, rRef, outdoor, c.exponent) : NaN;
      marker += `<line x1="${sx(outdoor)}" x2="${sx(outdoor)}" y1="${T}" y2="${H - B}" stroke="${c.color}" stroke-opacity="0.5" stroke-dasharray="2 3"/>`;
      if (Number.isFinite(model)) marker += `<circle cx="${sx(outdoor)}" cy="${sy(model)}" r="3.5" fill="${c.color}"/>`;
      if (Number.isFinite(setpoint))
        marker += `<circle cx="${sx(outdoor)}" cy="${sy(setpoint)}" r="5" fill="none" stroke="var(--primary-text-color)" stroke-width="1.5"><title>Soll laut Regler ${fmt(setpoint)} °C</title></circle>`;
    }

    const at = (ta) => (Number.isFinite(shown) ? Math.round(curveValue(shown, rRef, ta, c.exponent)) : "–");
    const preview = this._preview != null;
    const modelNow = Number.isFinite(outdoor) && Number.isFinite(shown) ? curveValue(shown, rRef, outdoor, c.exponent) : NaN;

    this.shadowRoot.innerHTML = `
      <style>${BASE_STYLE}
        .ctl { display: flex; align-items: center; gap: 4px; }
        .slope { font-size: 1.6em; font-weight: 500; min-width: 3.2em; text-align: center; color: var(--primary-text-color); }
        .slope.preview { color: ${c.color}; }
        .points { display: flex; justify-content: space-between; margin-top: 4px; font-size: 0.9em; color: var(--primary-text-color); }
        .points span b { font-weight: 500; }
        .legend { display: flex; flex-wrap: wrap; gap: 4px 14px; margin-top: 6px; font-size: 0.8em; color: var(--secondary-text-color); }
        .sw { display: inline-block; width: 16px; height: 0; vertical-align: middle; margin-right: 4px; border-top: 2.5px solid ${c.color}; }
        .sw.n { border-top: 2px dashed ${c.color}; opacity: 0.7; }
        .sw.c { border-top: 1px solid var(--secondary-text-color); opacity: 0.5; }
        .actions { display: flex; gap: 8px; justify-content: flex-end; margin-top: 8px; }
        svg { width: 100%; height: auto; display: block; margin-top: 6px; }
      </style>
      <ha-card>
        <div class="head">
          <div><div class="title">${c.title}</div>
            <div class="sub">${c.y_label} über Außentemperatur · Raumsoll ${fmt(room)} °C${Number.isFinite(night) ? ` / Nacht ${fmt(night)} °C` : ""}</div></div>
          <div class="ctl">
            <button class="icon" id="minus" title="flacher">−</button>
            <div class="slope ${preview ? "preview" : ""}">${fmt(shown, 2)}</div>
            <button class="icon" id="plus" title="steiler">+</button>
          </div>
        </div>
        <svg viewBox="0 0 ${W} ${H}" role="img" aria-label="${c.title}">${grid}${lines}${marker}
          <text x="${W - R}" y="${H - 2}" text-anchor="end">Außentemperatur °C</text>
        </svg>
        <div class="points">
          <span>+20 °C → <b>${at(20)}</b></span><span>0 °C → <b>${at(0)}</b></span><span>−20 °C → <b>${at(-20)}</b></span>
        </div>
        <div class="legend">
          <span><i class="sw"></i>${preview ? "Vorschau" : "aktuell"}</span>
          ${Number.isFinite(night) ? `<span><i class="sw n"></i>Nacht</span>` : ""}
          <span><i class="sw c"></i>Vergleich ${c.compare.map((s) => fmt(s, 1)).join(" · ")}</span>
          ${Number.isFinite(outdoor) ? `<span>Außen ${fmt(outdoor)} °C → Kurve ${fmt(modelNow)} °C${Number.isFinite(setpoint) ? `, Regler ○ ${fmt(setpoint)} °C` : ""}</span>` : ""}
        </div>
        ${preview ? `<div class="actions"><button class="pill" id="reset">Verwerfen</button><button class="pill primary" id="apply">Übernehmen (${fmt(slope, 2)} → ${fmt(shown, 2)})</button></div>` : ""}
      </ha-card>`;
    const $ = (id) => this.shadowRoot.getElementById(id);
    $("minus").onclick = () => this._step(-1);
    $("plus").onclick = () => this._step(1);
    if (preview) {
      $("apply").onclick = () => this._apply();
      $("reset").onclick = () => { this._preview = null; this._render(); };
    }
  }

  static getStubConfig() {
    return { slope: "number.manager_heizkurve_steigung_einstellung", room: "climate.heatingpump_climate_heizung" };
  }
}

// ---------------------------------------------------------------------------
// Week program
// ---------------------------------------------------------------------------
const DAY_LABELS = ["Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"];

// "06:30-09:00, 11:00-17:00" -> [[6.5, 9], [11, 17]]; "", "-" -> []
function parsePeriods(text) {
  if (!text || text === "-" || text === "unknown" || text === "unavailable") return [];
  const out = [];
  for (const part of text.split(",")) {
    const m = part.trim().match(/^(\d{1,2}):(\d{2})\s*-\s*(\d{1,2}):(\d{2})$/);
    if (!m) continue;
    out.push([+m[1] + +m[2] / 60, +m[3] + +m[4] / 60]);
  }
  return out;
}

class StiebelWeekProgramCard extends HTMLElement {
  setConfig(config) {
    if (!Array.isArray(config.entities) || config.entities.length !== 7)
      throw new Error("entities: seven text entities, Monday first");
    this._config = { title: "Programm", color: "var(--state-climate-heat-color, #ff8100)", ...config };
    this._edit = false;
    if (!this.shadowRoot) this.attachShadow({ mode: "open" });
  }

  set hass(hass) {
    const first = !this._hass;
    this._hass = hass;
    if (first || !this._edit) this._render();
  }

  getCardSize() { return this._edit ? 8 : 4; }
  getGridOptions() { return { columns: 12, min_columns: 6, rows: "auto" }; }

  _save() {
    const inputs = this.shadowRoot.querySelectorAll("input[data-entity]");
    for (const el of inputs) {
      const st = this._hass.states[el.dataset.entity];
      const value = el.value.trim();
      if (st && st.state !== value)
        this._hass.callService("text", "set_value", { entity_id: el.dataset.entity, value });
    }
    this._edit = false;
    this._render();
  }

  _copyMonday() {
    const inputs = [...this.shadowRoot.querySelectorAll("input[data-entity]")];
    for (let i = 1; i < 5; i++) inputs[i].value = inputs[0].value;
  }

  _render() {
    if (!this._hass || !this._config) return;
    const c = this._config;
    const states = c.entities.map((e) => this._hass.states[e]);
    const now = new Date();
    const today = (now.getDay() + 6) % 7;
    const hourNow = now.getHours() + now.getMinutes() / 60;
    const rows = states.map((st, i) => {
      const text = st ? st.state : "unavailable";
      const periods = parsePeriods(text);
      const blocks = periods.map(([a, b]) =>
        `<div class="blk" style="left:${(a / 24) * 100}%;width:${(Math.max(0, b - a) / 24) * 100}%"></div>`).join("");
      const nowLine = i === today ? `<div class="now" style="left:${(hourNow / 24) * 100}%"></div>` : "";
      const label = periods.length ? periods.map(([a, b]) => `${fmtH(a)}–${fmtH(b)}`).join(", ") : (st ? "kein Zeitraum" : "nicht verfügbar");
      const edit = this._edit
        ? `<input data-entity="${c.entities[i]}" value="${st && !["unknown", "unavailable"].includes(st.state) ? st.state : ""}" placeholder="06:00-22:00">`
        : `<div class="txt">${label}</div>`;
      return `<div class="row ${i === today ? "today" : ""}"><div class="day">${DAY_LABELS[i]}</div>
        <div class="bar">${blocks}${nowLine}</div>${edit}</div>`;
    }).join("");
    this.shadowRoot.innerHTML = `
      <style>${BASE_STYLE}
        .row { display: grid; grid-template-columns: 2em 1fr minmax(7.5em, auto); align-items: center; gap: 8px; margin: 3px 0; }
        .day { color: var(--secondary-text-color); font-size: 0.9em; }
        .today .day { color: var(--primary-text-color); font-weight: 600; }
        .bar { position: relative; height: 14px; border-radius: 7px; background: var(--secondary-background-color); overflow: hidden; }
        .blk { position: absolute; top: 0; bottom: 0; background: ${c.color}; border-radius: 7px; }
        .now { position: absolute; top: -2px; bottom: -2px; width: 2px; background: var(--primary-text-color); }
        .txt { font-size: 0.85em; color: var(--primary-text-color); text-align: right; white-space: nowrap; }
        .ticks { display: grid; grid-template-columns: 2em 1fr minmax(7.5em, auto); gap: 8px; font-size: 0.75em; color: var(--secondary-text-color); }
        .ticks div:nth-child(2) { display: flex; justify-content: space-between; }
        input { font: inherit; font-size: 0.9em; width: 100%; box-sizing: border-box; padding: 4px 6px; border-radius: 6px;
          border: 1px solid var(--divider-color); background: var(--card-background-color); color: var(--primary-text-color); }
        .actions { display: flex; gap: 8px; justify-content: flex-end; margin-top: 8px; flex-wrap: wrap; }
        .hint { font-size: 0.8em; color: var(--secondary-text-color); margin-top: 6px; }
      </style>
      <ha-card>
        <div class="head"><div class="title">${c.title}</div>
          ${this._edit ? "" : `<button class="icon" id="edit" title="Bearbeiten">✎</button>`}</div>
        <div class="ticks"><div></div><div><span>0</span><span>6</span><span>12</span><span>18</span><span>24</span></div><div></div></div>
        ${rows}
        ${this._edit ? `<div class="hint">Format „06:00-22:00“, bis zu drei Zeiträume mit Komma, leer = kein Zeitraum. Die Wärmepumpe liest jede Änderung zurück.</div>
          <div class="actions"><button class="pill" id="copy">Mo auf Di–Fr</button><button class="pill" id="cancel">Abbrechen</button><button class="pill primary" id="save">Speichern</button></div>` : ""}
      </ha-card>`;
    const $ = (id) => this.shadowRoot.getElementById(id);
    if (this._edit) {
      $("save").onclick = () => this._save();
      $("cancel").onclick = () => { this._edit = false; this._render(); };
      $("copy").onclick = () => this._copyMonday();
    } else {
      $("edit").onclick = () => { this._edit = true; this._render(); };
    }
  }
}

function fmtH(h) {
  const hh = Math.floor(h), mm = Math.round((h - hh) * 60);
  return `${String(hh).padStart(2, "0")}:${String(mm).padStart(2, "0")}`;
}

customElements.define("stiebel-heating-curve-card", StiebelHeatingCurveCard);
customElements.define("stiebel-week-program-card", StiebelWeekProgramCard);
window.customCards = window.customCards || [];
window.customCards.push(
  { type: "stiebel-heating-curve-card", name: "Stiebel heating curve", description: "Heating curve diagram with slope preview" },
  { type: "stiebel-week-program-card", name: "Stiebel week program", description: "Time program as week bars, editable" },
);
