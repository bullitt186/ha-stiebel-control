/*
 * stiebel-cards.js — two Lovelace cards for ha-stiebel-control, no dependencies.
 *
 *   custom:stiebel-heating-curve-card   heating curve diagram as in the WPMW II manual
 *   custom:stiebel-week-program-card    time program (7 text entities) as week bars, editable
 *
 * Install: copy to /config/www/stiebel-cards.js and add the resource
 * /local/stiebel-cards.js (JavaScript module). Examples: docs/CONFIGURATION.md, "Dashboards".
 *
 * Theming: the cards draw only with Home Assistant theme variables (text, divider, card and
 * primary colors, font and spacing tokens), so any theme and the dark mode style them like
 * the built-in cards. `color` takes the same values as the tile card: a theme color name
 * (orange, blue, primary, accent, ...), a CSS variable or a hex value. Texts follow the
 * user's language (German or English), numbers the user's number format.
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

// Theme color names of Home Assistant (as accepted by the tile card's `color`)
const THEME_COLORS = new Set([
  "primary", "accent", "red", "pink", "purple", "deep-purple", "indigo", "blue", "light-blue",
  "cyan", "teal", "green", "light-green", "lime", "yellow", "amber", "orange", "deep-orange",
  "brown", "light-grey", "grey", "dark-grey", "blue-grey", "black", "white", "disabled",
]);
const HEAT_COLOR = "var(--state-climate-heat-color, var(--orange-color, #ff9800))";

function cssColor(color) {
  if (!color) return HEAT_COLOR;
  if (THEME_COLORS.has(color)) return `var(--${color}-color, var(--primary-color))`;   // theme without it: primary
  return color;   // CSS variable, hex, rgb(...)
}

const TEXT = {
  de: {
    over: "über Außentemperatur", room: "Raumsoll", night: "Nacht", outdoor: "Außentemperatur °C",
    current: "aktuell", preview: "Vorschau", compare: "Vergleich", now: "Außen", curve: "Kurve",
    controller: "Regler", flatter: "Flacher", steeper: "Steiler", discard: "Verwerfen", apply: "Übernehmen",
    setpoint: "Soll laut Regler", edit: "Bearbeiten", save: "Speichern", cancel: "Abbrechen",
    copy: "Mo auf Di–Fr", none: "kein Zeitraum", unavailable: "nicht verfügbar",
    hint: "Format „06:00-22:00“, bis zu drei Zeiträume mit Komma, leer = kein Zeitraum. Die Wärmepumpe liest jede Änderung zurück.",
    days: ["Mo", "Di", "Mi", "Do", "Fr", "Sa", "So"], curveTitle: "Heizkurve", programTitle: "Programm", soll: "Soll",
  },
  en: {
    over: "over outdoor temperature", room: "Room setpoint", night: "night", outdoor: "Outdoor temperature °C",
    current: "current", preview: "preview", compare: "compare", now: "Outdoor", curve: "curve",
    controller: "controller", flatter: "Flatter", steeper: "Steeper", discard: "Discard", apply: "Apply",
    setpoint: "Setpoint reported by the controller", edit: "Edit", save: "Save", cancel: "Cancel",
    copy: "Mon to Tue–Fri", none: "no period", unavailable: "unavailable",
    hint: "Format \"06:00-22:00\", up to three periods separated by commas, empty = no period. The heat pump reads every change back.",
    days: ["Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"], curveTitle: "Heating curve", programTitle: "Program", soll: "Setpoint",
  },
};

function texts(hass) {
  const lang = (hass && (hass.locale?.language || hass.language)) || "en";
  return lang.startsWith("de") ? TEXT.de : TEXT.en;
}

function fmt(hass, v, digits = 1) {
  if (!Number.isFinite(v)) return "–";
  const lang = hass?.locale?.language || hass?.language || undefined;
  return v.toLocaleString(lang, { minimumFractionDigits: digits, maximumFractionDigits: digits });
}

function esc(s) {
  return String(s ?? "").replace(/[&<>"]/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[ch]));
}

// MDI paths (Material Design Icons, Apache 2.0), drawn by <ha-icon-button>
const MDI_MINUS = "M19,13H5V11H19V13Z";
const MDI_PLUS = "M19,13H13V19H11V13H5V11H11V5H13V11H19V13Z";
const MDI_PENCIL = "M20.71,7.04C21.1,6.65 21.1,6 20.71,5.63L18.37,3.29C18,2.9 17.35,2.9 16.96,3.29L15.12,5.12L18.87,8.87M3,17.25V21H6.75L17.81,9.93L14.06,6.18L3,17.25Z";

// Shared styles: Home Assistant theme variables and design tokens, with the default theme as
// fallback. <ha-card> brings background, border, radius and shadow of the active theme.
const BASE_STYLE = `
  :host { display: block; }
  ha-card { padding: var(--ha-space-4, 16px); height: 100%; box-sizing: border-box; }
  .head { display: flex; align-items: flex-start; justify-content: space-between; gap: var(--ha-space-2, 8px); }
  .title { font-size: var(--ha-font-size-l, 16px); font-weight: var(--ha-font-weight-medium, 500);
    line-height: var(--ha-line-height-normal, 1.5); color: var(--primary-text-color); }
  .sub { font-size: var(--ha-font-size-s, 12px); line-height: var(--ha-line-height-condensed, 1.3);
    color: var(--secondary-text-color); }
  ha-icon-button { --mdc-icon-button-size: 36px; --mdc-icon-size: 20px; color: var(--secondary-text-color); }
  .actions { display: flex; gap: var(--ha-space-2, 8px); justify-content: flex-end; flex-wrap: wrap;
    margin-top: var(--ha-space-3, 12px); }
  button.btn { font-family: inherit; font-size: var(--ha-font-size-m, 14px); font-weight: var(--ha-font-weight-medium, 500);
    height: 36px; padding: 0 var(--ha-space-4, 16px); border-radius: var(--ha-border-radius-pill, 18px); cursor: pointer;
    border: none; background: transparent; color: var(--primary-color); }
  button.btn:hover { background: color-mix(in srgb, var(--primary-color) 8%, transparent); }
  button.btn.filled { background: var(--primary-color); color: var(--text-primary-color, #fff); }
  button.btn.filled:hover { background: color-mix(in srgb, var(--primary-color) 88%, var(--primary-text-color)); }
  button.btn:focus-visible { outline: 2px solid var(--primary-color); outline-offset: 2px; }
  svg text { fill: var(--secondary-text-color); font-family: inherit; font-size: 11px; }
`;

// ---------------------------------------------------------------------------
// Heating curve
// ---------------------------------------------------------------------------
class StiebelHeatingCurveCard extends HTMLElement {
  setConfig(config) {
    if (!config.slope || !config.room) throw new Error("slope and room are required");
    this._config = { compare: [0.2, 0.4, 0.6, 0.8], exponent: CURVE_EXPONENT, ...config };
    this._preview = null;
    this._key = null;
    if (!this.shadowRoot) this.attachShadow({ mode: "open" });
    if (this._hass) this._render(true);
  }

  set hass(hass) {
    this._hass = hass;
    this._render(false);
  }

  getCardSize() { return 7; }
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
    if (!Number.isFinite(base)) return;
    const next = Math.round((base + dir * step) / step) * step;
    this._preview = Math.min(max, Math.max(min, Number(next.toFixed(3))));
    if (Math.abs(this._preview - actual) < 1e-6) this._preview = null;
    this._render(true);
  }

  _apply() {
    if (this._preview == null) return;
    this._hass.callService("number", "set_value", { entity_id: this._config.slope, value: this._preview });
    this._preview = null;
    this._render(true);
  }

  _render(force) {
    if (!this._hass || !this._config) return;
    const c = this._config;
    const h = this._hass;
    const t = texts(h);
    const ids = [c.slope, c.room, c.room_night, c.outdoor, c.setpoint];
    const key = JSON.stringify([ids.map((e) => (e && h.states[e] ? [h.states[e].state, h.states[e].attributes.temperature] : null)),
                                this._preview, h.locale?.language]);
    if (!force && key === this._key) return;   // nothing this card shows has changed
    this._key = key;

    const color = cssColor(c.color);
    const slope = num(h, c.slope);
    const room = num(h, c.room);
    const night = num(h, c.room_night);
    const outdoor = num(h, c.outdoor);
    const setpoint = num(h, c.setpoint);
    const shown = this._preview ?? slope;
    const preview = this._preview != null;
    const title = c.title ?? t.curveTitle;
    const yLabel = c.y_label ?? t.soll;

    // geometry: outdoor +20 (left) ... -20 (right), like the manual
    const W = 360, H = 228, L = 34, R = 10, T = 10, B = 34;
    const xMin = -20, xMax = 20;
    const rRef = Number.isFinite(room) ? room : 20;
    const pts = (s, r) => {
      const out = [];
      for (let ta = xMax; ta >= xMin; ta -= 1) out.push([ta, curveValue(s, r, ta, c.exponent)]);
      return out;
    };
    const curves = c.compare.map((s) => ({ s, p: pts(s, rRef), kind: "compare" }));
    if (Number.isFinite(shown)) {
      if (Number.isFinite(night)) curves.push({ p: pts(shown, night), kind: "night" });
      if (preview && Number.isFinite(slope)) curves.push({ p: pts(slope, rRef), kind: "old" });
      curves.push({ p: pts(shown, rRef), kind: "main" });
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
      grid += `<line class="grid" x1="${sx(ta)}" x2="${sx(ta)}" y1="${T}" y2="${H - B}"/>`;
      grid += `<text x="${sx(ta)}" y="${H - B + 14}" text-anchor="middle">${ta}</text>`;
    }
    for (let y = Math.ceil(yMin / 5) * 5; y <= yMax; y += 5) {
      grid += `<line class="grid" x1="${L}" x2="${W - R}" y1="${sy(y)}" y2="${sy(y)}"/>`;
      grid += `<text x="${L - 4}" y="${sy(y) + 4}" text-anchor="end">${y}</text>`;
    }
    let lines = "";
    for (const cv of curves) {
      if (cv.kind === "compare") {
        const last = cv.p[cv.p.length - 1];
        lines += `<path class="cmp" d="${path(cv.p)}"/>`;
        lines += `<text class="cmp-label" x="${sx(last[0]) - 2}" y="${sy(last[1]) - 3}" text-anchor="end">${fmt(h, cv.s, 1)}</text>`;
      } else {
        lines += `<path class="${cv.kind}" d="${path(cv.p)}"/>`;
      }
    }
    let marker = "";
    const modelNow = Number.isFinite(outdoor) && Number.isFinite(shown) ? curveValue(shown, rRef, outdoor, c.exponent) : NaN;
    if (Number.isFinite(outdoor) && outdoor <= xMax && outdoor >= xMin) {
      marker += `<line class="now" x1="${sx(outdoor)}" x2="${sx(outdoor)}" y1="${T}" y2="${H - B}"/>`;
      if (Number.isFinite(modelNow)) marker += `<circle class="dot" cx="${sx(outdoor)}" cy="${sy(modelNow)}" r="3.5"/>`;
      if (Number.isFinite(setpoint))
        marker += `<circle class="ring" cx="${sx(outdoor)}" cy="${sy(setpoint)}" r="5"><title>${t.setpoint} ${fmt(h, setpoint)} °C</title></circle>`;
    }
    const at = (ta) => (Number.isFinite(shown) ? Math.round(curveValue(shown, rRef, ta, c.exponent)) : "–");

    this.shadowRoot.innerHTML = `
      <style>${BASE_STYLE}
        .ctl { display: flex; align-items: center; flex-shrink: 0; }
        .slope { font-size: var(--ha-font-size-2xl, 24px); font-weight: var(--ha-font-weight-normal, 400);
          min-width: 2.6em; text-align: center; color: var(--primary-text-color); font-variant-numeric: tabular-nums; }
        .slope.preview { color: ${color}; font-weight: var(--ha-font-weight-medium, 500); }
        svg { width: 100%; height: auto; display: block; margin-top: var(--ha-space-2, 8px); }
        .grid { stroke: var(--divider-color); stroke-width: 0.5; }
        .cmp { fill: none; stroke: var(--secondary-text-color); stroke-opacity: 0.35; stroke-width: 1; }
        .cmp-label { opacity: 0.8; }
        .main { fill: none; stroke: ${color}; stroke-width: 2.5; stroke-linecap: round; }
        .old { fill: none; stroke: ${color}; stroke-opacity: 0.35; stroke-width: 2; }
        .night { fill: none; stroke: ${color}; stroke-opacity: 0.6; stroke-width: 1.5; stroke-dasharray: 4 3; }
        .now { stroke: ${color}; stroke-opacity: 0.5; stroke-dasharray: 2 3; }
        .dot { fill: ${color}; }
        .ring { fill: none; stroke: var(--primary-text-color); stroke-width: 1.5; }
        .points { display: flex; justify-content: space-between; margin-top: var(--ha-space-1, 4px);
          font-size: var(--ha-font-size-m, 14px); color: var(--secondary-text-color); font-variant-numeric: tabular-nums; }
        .points b { color: var(--primary-text-color); font-weight: var(--ha-font-weight-medium, 500); }
        .legend { display: flex; flex-wrap: wrap; gap: var(--ha-space-1, 4px) var(--ha-space-3, 12px);
          margin-top: var(--ha-space-2, 8px); font-size: var(--ha-font-size-s, 12px); color: var(--secondary-text-color); }
        .sw { display: inline-block; width: 16px; height: 0; vertical-align: middle; margin-right: 4px; border-top: 2.5px solid ${color}; }
        .sw.n { border-top: 2px dashed ${color}; opacity: 0.7; }
        .sw.c { border-top: 1px solid var(--secondary-text-color); opacity: 0.5; }
      </style>
      <ha-card>
        <div class="head">
          <div><div class="title">${esc(title)}</div>
            <div class="sub">${esc(yLabel)} ${t.over} · ${t.room} ${fmt(h, room)} °C${Number.isFinite(night) ? ` / ${t.night} ${fmt(h, night)} °C` : ""}</div></div>
          <div class="ctl">
            <ha-icon-button id="minus" label="${t.flatter}"></ha-icon-button>
            <div class="slope ${preview ? "preview" : ""}">${fmt(h, shown, 2)}</div>
            <ha-icon-button id="plus" label="${t.steeper}"></ha-icon-button>
          </div>
        </div>
        <svg viewBox="0 0 ${W} ${H}" role="img" aria-label="${esc(title)}">${grid}${lines}${marker}
          <text x="${W - R}" y="${H - 2}" text-anchor="end">${t.outdoor}</text>
        </svg>
        <div class="points">
          <span>+20 °C → <b>${at(20)}</b></span><span>0 °C → <b>${at(0)}</b></span><span>−20 °C → <b>${at(-20)}</b></span>
        </div>
        <div class="legend">
          <span><i class="sw"></i>${preview ? t.preview : t.current}</span>
          ${Number.isFinite(night) ? `<span><i class="sw n"></i>${t.night}</span>` : ""}
          <span><i class="sw c"></i>${t.compare} ${c.compare.map((s) => fmt(h, s, 1)).join(" · ")}</span>
          ${Number.isFinite(outdoor) ? `<span>${t.now} ${fmt(h, outdoor)} °C → ${t.curve} ${fmt(h, modelNow)} °C${Number.isFinite(setpoint) ? `, ${t.controller} ○ ${fmt(h, setpoint)} °C` : ""}</span>` : ""}
        </div>
        ${preview ? `<div class="actions"><button class="btn" id="reset">${t.discard}</button>
          <button class="btn filled" id="apply">${t.apply} (${fmt(h, slope, 2)} → ${fmt(h, shown, 2)})</button></div>` : ""}
      </ha-card>`;
    const $ = (id) => this.shadowRoot.getElementById(id);
    $("minus").path = MDI_MINUS;
    $("plus").path = MDI_PLUS;
    $("minus").addEventListener("click", () => this._step(-1));
    $("plus").addEventListener("click", () => this._step(1));
    if (preview) {
      $("apply").addEventListener("click", () => this._apply());
      $("reset").addEventListener("click", () => { this._preview = null; this._render(true); });
    }
  }

  static getStubConfig() {
    return { slope: "number.heatingpump_manager_heizkurve_set", room: "climate.heatingpump_climate_heizung" };
  }
}

// ---------------------------------------------------------------------------
// Week program
// ---------------------------------------------------------------------------

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

function fmtH(h) {
  const hh = Math.floor(h), mm = Math.round((h - hh) * 60);
  return `${String(hh).padStart(2, "0")}:${String(mm).padStart(2, "0")}`;
}

class StiebelWeekProgramCard extends HTMLElement {
  setConfig(config) {
    if (!Array.isArray(config.entities) || config.entities.length !== 7)
      throw new Error("entities: seven text entities, Monday first");
    this._config = { ...config };
    this._edit = false;
    this._key = null;
    if (!this.shadowRoot) this.attachShadow({ mode: "open" });
    if (this._hass) this._render(true);
  }

  set hass(hass) {
    this._hass = hass;
    if (!this._edit) this._render(false);   // keep what the user is typing
  }

  connectedCallback() {
    // move the "now" marker once a minute
    this._timer = setInterval(() => { if (!this._edit) this._render(false); }, 60000);
  }

  disconnectedCallback() { clearInterval(this._timer); }

  getCardSize() { return this._edit ? 9 : 5; }
  getGridOptions() { return { columns: 12, min_columns: 6, rows: "auto" }; }

  _save() {
    for (const el of this.shadowRoot.querySelectorAll("input[data-entity]")) {
      const st = this._hass.states[el.dataset.entity];
      const value = el.value.trim();
      if (st && st.state !== value)
        this._hass.callService("text", "set_value", { entity_id: el.dataset.entity, value });
    }
    this._edit = false;
    this._render(true);
  }

  _copyMonday() {
    const inputs = [...this.shadowRoot.querySelectorAll("input[data-entity]")];
    for (let i = 1; i < 5; i++) inputs[i].value = inputs[0].value;
  }

  _render(force) {
    if (!this._hass || !this._config) return;
    const c = this._config;
    const h = this._hass;
    const t = texts(h);
    const now = new Date();
    const today = (now.getDay() + 6) % 7;
    const hourNow = now.getHours() + now.getMinutes() / 60;
    const states = c.entities.map((e) => h.states[e]);
    const key = JSON.stringify([states.map((s) => (s ? s.state : null)), this._edit, today, Math.floor(hourNow * 12), h.locale?.language]);
    if (!force && key === this._key) return;
    this._key = key;

    const color = cssColor(c.color);
    const rows = states.map((st, i) => {
      const periods = parsePeriods(st ? st.state : "");
      const blocks = periods.map(([a, b]) =>
        `<div class="blk" style="left:${(a / 24) * 100}%;width:${(Math.max(0, b - a) / 24) * 100}%"></div>`).join("");
      const nowLine = i === today ? `<div class="nowline" style="left:${(hourNow / 24) * 100}%"></div>` : "";
      const label = periods.length ? periods.map(([a, b]) => `${fmtH(a)}–${fmtH(b)}`).join(", ")
                                   : (st && st.state !== "unavailable" ? t.none : t.unavailable);
      const value = st && !["unknown", "unavailable"].includes(st.state) ? st.state : "";
      const right = this._edit
        ? `<input data-entity="${esc(c.entities[i])}" value="${esc(value)}" placeholder="06:00-22:00" aria-label="${t.days[i]}">`
        : `<div class="txt">${label}</div>`;
      return `<div class="row ${i === today ? "today" : ""}"><div class="day">${t.days[i]}</div>
        <div class="bar">${blocks}${nowLine}</div>${right}</div>`;
    }).join("");

    this.shadowRoot.innerHTML = `
      <style>${BASE_STYLE}
        .head { align-items: center; margin-bottom: var(--ha-space-1, 4px); }
        .grid { display: grid; grid-template-columns: 2.2em minmax(0, 1fr) minmax(7.5em, max-content); align-items: center;
          column-gap: var(--ha-space-2, 8px); row-gap: 6px; }
        .grid.editing { grid-template-columns: 2.2em minmax(0, 1fr) minmax(0, 1fr); }
        .row { display: contents; }
        .day { font-size: var(--ha-font-size-s, 12px); color: var(--secondary-text-color); }
        .today .day { color: var(--primary-text-color); font-weight: var(--ha-font-weight-bold, 600); }
        /* track like a tile feature: the program color at 20 % */
        .bar { position: relative; height: 16px; border-radius: 8px; overflow: hidden; }
        .bar::before { content: ""; position: absolute; inset: 0; background: ${color}; opacity: 0.2; }
        .blk { position: absolute; top: 0; bottom: 0; background: ${color}; border-radius: 8px; }
        .nowline { position: absolute; top: 0; bottom: 0; width: 2px; margin-left: -1px; background: var(--primary-text-color); }
        .txt { font-size: var(--ha-font-size-s, 12px); color: var(--primary-text-color); text-align: right;
          white-space: nowrap; font-variant-numeric: tabular-nums; }
        .ticks { font-size: var(--ha-font-size-xs, 10px); color: var(--secondary-text-color); display: flex; justify-content: space-between; }
        input { font-family: inherit; font-size: var(--ha-font-size-m, 14px); width: 100%; box-sizing: border-box;
          height: 32px; padding: 0 var(--ha-space-2, 8px); border: none; border-radius: 4px 4px 0 0; outline: none;
          border-bottom: 1px solid var(--input-idle-line-color, var(--secondary-text-color));
          background: var(--input-fill-color, var(--secondary-background-color));
          color: var(--input-ink-color, var(--primary-text-color)); }
        input:focus { border-bottom: 2px solid var(--primary-color); }
        input::placeholder { color: var(--input-label-ink-color, var(--secondary-text-color)); }
        .hint { font-size: var(--ha-font-size-s, 12px); color: var(--secondary-text-color); margin-top: var(--ha-space-3, 12px); }
      </style>
      <ha-card>
        <div class="head"><div class="title">${esc(c.title ?? t.programTitle)}</div>
          ${this._edit ? "" : `<ha-icon-button id="edit" label="${t.edit}"></ha-icon-button>`}</div>
        <div class="grid ${this._edit ? "editing" : ""}">
          <div></div><div class="ticks"><span>0</span><span>6</span><span>12</span><span>18</span><span>24</span></div><div></div>
          ${rows}
        </div>
        ${this._edit ? `<div class="hint">${t.hint}</div>
          <div class="actions"><button class="btn" id="copy">${t.copy}</button><button class="btn" id="cancel">${t.cancel}</button>
          <button class="btn filled" id="save">${t.save}</button></div>` : ""}
      </ha-card>`;
    const $ = (id) => this.shadowRoot.getElementById(id);
    if (this._edit) {
      $("save").addEventListener("click", () => this._save());
      $("cancel").addEventListener("click", () => { this._edit = false; this._render(true); });
      $("copy").addEventListener("click", () => this._copyMonday());
    } else {
      $("edit").path = MDI_PENCIL;
      $("edit").addEventListener("click", () => { this._edit = true; this._render(true); });
    }
  }

  static getStubConfig() {
    const days = ["mo", "di", "mi", "do", "fr", "sa", "so"];
    return { entities: days.map((d) => `text.heatingpump_heiz_programm_${d}`) };
  }
}

customElements.define("stiebel-heating-curve-card", StiebelHeatingCurveCard);
customElements.define("stiebel-week-program-card", StiebelWeekProgramCard);
window.customCards = window.customCards || [];
window.customCards.push(
  { type: "stiebel-heating-curve-card", name: "Stiebel heating curve", description: "Heating curve diagram with slope preview" },
  { type: "stiebel-week-program-card", name: "Stiebel week program", description: "Time program as week bars, editable" },
);
