/* The benchmarks page (doppler#2372): headline cards, one family at a time,
   and a chart per row, drawn from the data that `scripts/bench_report.py
   --page` embeds in docs/benchmarks.md as `window.dpBenchData`. Every value
   is seconds per unit; each row also carries how many units of its domain
   one unit is (`cnt`, `dom`), so a stream row reads in input samples per
   second and a setup row (`dom` null) in time per call. Loaded on every
   page; does nothing on a page without the cards. */
(function () {
  "use strict";

  // The rate suffix per domain comes from the page's JSON (bench_report's
  // DOMAIN_SUFFIX), so the script and the static table print the same.
  let SUFFIX = {};
  const NOUN = { Sa: "input sample", sym: "symbol", bit: "bit", codeword: "codeword", frame: "frame" };
  const PRESETS = [1, 2.4, 10, 20, 61.44, 125, 250];
  const GROUPS = [
    ["Sa", "Sample throughput", "input samples per second, higher is faster"],
    ["other", "Symbols, bits and other units", "their own rate, higher is faster"],
    ["ctl", "Setup, control and per-call", "time per call, lower is faster"],
  ];
  const COLS = 7;

  /* ---- formatting ---- */
  function fmtNum(v) {
    return v >= 100 ? v.toFixed(0) : v >= 10 ? v.toFixed(1) : v.toFixed(2);
  }
  function fmtTime(s) {
    const T = [[1e-12, "ps"], [1e-9, "ns"], [1e-6, "µs"], [1e-3, "ms"], [1, "s"]];
    let u = T[0];
    for (const x of T) if (s >= x[0]) u = x;
    return fmtNum(s / u[0]) + " " + u[1];
  }
  function fmtRate(perSec, dom) {
    const S = [[1e9, "G"], [1e6, "M"], [1e3, "k"], [1, ""]];
    let u = S[S.length - 1];
    for (const x of S) if (perSec >= x[0]) { u = x; break; }
    return fmtNum(perSec / u[0]) + " " + u[1] + (SUFFIX[dom] || dom + "/s");
  }
  const esc = (s) => String(s).replace(/[&<>"]/g, (c) =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" })[c]);
  // A row's value in its own terms: a rate, or time per call for setup and
  // read-outs, whose `cnt` is how many recorded units one call is.
  const rateOf = (r, s) => (r.dom && s != null ? r.cnt / s : null);
  function fmtVal(r, s, plain) {
    if (s == null) return "—";
    const out = r.dom ? fmtRate(r.cnt / s, r.dom) : fmtTime(s * r.cnt) + " / call";
    if (plain) return out;
    return r.dom ? out.replace(/ (\S+)$/, " <small>$1</small>")
      : out.replace(" / call", " <small>/ call</small>");
  }
  const group = (r) => (!r.dom ? "ctl" : r.dom === "Sa" ? "Sa" : "other");
  const unitWord = (r) => r.unit.replace(/_/g, " ");
  // a, b in seconds: how much faster b is than a.
  const speedup = (a, b) => (a != null && b != null ? a / b - 1 : null);
  function pct(x) {
    return (x >= 0 ? "+" : "−") +
      Math.abs(x * 100).toFixed(Math.abs(x) < 0.1 ? 1 : 0) + "%";
  }
  // `sym` leads, so a card reads "%Δ +30%" rather than "+30% %Δ".
  function chip(x, sym) {
    if (x == null) return '<span class="dpb-flat">—</span>';
    const cls = Math.abs(x) < 0.03 ? "dpb-flat" : x > 0 ? "dpb-up" : "dpb-down";
    return '<span class="dpb-delta ' + cls + '">' + (sym ? sym + " " : "") +
      pct(x) + "</span>";
  }

  /* ---- the reader's sample rate, remembered per viewer ---- */
  let RATE = 20;
  try {
    const v = parseFloat(localStorage.getItem("dpb-rate"));
    if (v > 0) RATE = v;
  } catch (e) { /* storage unavailable: the default */ }
  // Throughput / your rate, for stream rows only.
  const headroom = (r, s) => (r.dom === "Sa" && s != null ? r.cnt / s / (RATE * 1e6) : null);
  function headChip(x) {
    if (x == null) return '<span class="dpb-flat">—</span>';
    const cls = x < 1 ? "dpb-down" : x < 1.5 ? "dpb-warn" : "dpb-up";
    const txt = "×" + (x >= 10 ? Math.round(x).toLocaleString() : x.toFixed(x < 1 ? 2 : 1));
    return '<span class="dpb-delta ' + cls + '" title="' +
      (x < 1 ? "falls behind" : "keeps up") + " at " + RATE + ' MSa/s">' + txt +
      (x < 1 ? " behind" : "") + "</span>";
  }

  /* An unlabelled row's name, from its ID: `obj · op` plus its params. */
  function autoName(r) {
    const raw = r.id.slice(r.id.indexOf(":") + 1);
    let [obj, rest] = raw.split("::");
    rest = (rest || "").replace(/^test_bench_|^test_/, "");
    const params = [];
    rest = rest.replace(/[\[(]([^\])]*)[\])]/g, (_, p) => {
      p.split(",").forEach((x) => params.push(x.trim().replace("=", " = ")));
      return "";
    });
    return { obj, op: rest.replace(/_/g, " ").replace(/\s+/g, " ").trim(), params };
  }

  /* ---- charts ---- */
  function segments(a, brk) {
    const segs = [];
    let cur = [];
    for (let i = 0; i < a.length; i++) {
      if (a[i] == null || brk.indexOf(i) >= 0) {
        if (cur.length) segs.push(cur);
        cur = [];
      }
      if (a[i] != null) cur.push(i);
    }
    if (cur.length) segs.push(cur);
    return segs;
  }
  function spark(r, NV, w, h) {
    const sp = r.p.map((s) => (s == null ? null : 1 / s));   // speed
    const vals = sp.filter((x) => x != null);
    if (!vals.length) return "";
    let lo = Math.min(...vals), hi = Math.max(...vals);
    if (hi - lo < hi * 0.02) { const m = (hi + lo) / 2; lo = m * 0.99; hi = m * 1.01; }
    const x = (i) => 2 + (w - 4) * i / Math.max(1, NV - 1);
    const y = (v) => h - 3 - (h - 6) * (v - lo) / (hi - lo);
    let out = "";
    for (const seg of segments(sp, r.brk)) {
      if (seg.length === 1) {
        const i = seg[0];
        out += '<circle cx="' + x(i).toFixed(1) + '" cy="' + y(sp[i]).toFixed(1) +
          '" r="1.4" class="dpb-dot"/>';
        continue;
      }
      out += '<polyline class="dpb-line-p" points="' +
        seg.map((i) => x(i).toFixed(1) + "," + y(sp[i]).toFixed(1)).join(" ") + '"/>';
    }
    for (const b of r.brk) {
      out += '<line class="dpb-brk" x1="' + x(b - 0.5).toFixed(1) + '" x2="' +
        x(b - 0.5).toFixed(1) + '" y1="2" y2="' + (h - 2) + '"/>';
    }
    const L = NV - 1;
    if (sp[L] != null) {
      out += '<circle class="dpb-last" cx="' + x(L).toFixed(1) + '" cy="' +
        y(sp[L]).toFixed(1) + '" r="2.6"/>';
    }
    return '<svg viewBox="0 0 ' + w + " " + h + '" preserveAspectRatio="none"' +
      ' role="img" aria-label="speed across releases">' + out + "</svg>";
  }
  // W is the width the chart is shown at, so its 10px text stays 10px:
  // a fixed 640-wide viewBox shrank it to about 5px on a phone.
  function bigChart(r, V, tip, W) {
    const NV = V.length, LAST = NV - 1;
    const H = 240, L = 72, R = 12, T = 14, B = 34;
    const conv = (s) => (s == null ? null : r.dom ? r.cnt / s : s * r.cnt);
    const P = r.p.map(conv), N = r.n.map(conv);
    const all = P.concat(N).filter((v) => v != null);
    let lo = Math.min(...all), hi = Math.max(...all);
    const pad = (hi - lo) * 0.12 || hi * 0.05;
    lo = Math.max(0, lo - pad); hi = hi + pad;
    // The reader's rate. Within about 2x of the range the axis stretches to
    // take it in; further out an edge marker says where it is. A chart that
    // said nothing would read as "no constraint" -- exactly when the reader
    // is most at ease.
    const rv = r.dom === "Sa" ? RATE * 1e6 : null;
    if (rv != null && rv < lo && rv >= lo / 2) lo = rv * 0.9;
    if (rv != null && rv > hi && rv <= hi * 2) hi = rv * 1.1;
    const x = (i) => L + (W - L - R) * i / Math.max(1, NV - 1);
    const y = (v) => T + (H - T - B) * (1 - (v - lo) / (hi - lo));
    const lab = (v) => (r.dom ? fmtRate(v, r.dom) : fmtTime(v));
    let g = "";
    if (rv != null && rv >= lo && rv <= hi) {
      const yr = y(rv).toFixed(1);
      g += '<line class="dpb-rate" x1="' + L + '" x2="' + (W - R) + '" y1="' + yr +
        '" y2="' + yr + '"/><text class="dpb-rate-t" x="' + (W - R - 4) + '" y="' +
        (+yr - 5) + '" text-anchor="end">your rate, ' + RATE + " MSa/s</text>";
    } else if (rv != null) {
      const below = rv < lo, h = headroom(r, r.p[LAST]);
      const ye = below ? H - B - 4 : T + 10;
      const arrow = below ? "▼" : "▲";
      g += '<text class="dpb-rate-t dpb-rate-edge" x="' + (W - R - 4) + '" y="' + ye +
        '" text-anchor="end">' + arrow + " your rate " + RATE + " MSa/s is " +
        (below ? "below" : "above") + (h != null ? ", ×" + (h >= 10 ? Math.round(h)
          : h.toFixed(h < 1 ? 2 : 1)) + (h < 1 ? " behind" : "") : "") + "</text>";
    }
    for (let k = 0; k <= 4; k++) {
      const v = lo + (hi - lo) * k / 4, yy = y(v).toFixed(1);
      g += '<line class="dpb-grid" x1="' + L + '" x2="' + (W - R) + '" y1="' + yy +
        '" y2="' + yy + '"/><text class="dpb-axis" x="' + (L - 6) + '" y="' +
        (+yy + 4) + '" text-anchor="end">' + esc(lab(v)) + "</text>";
    }
    const step = Math.ceil(NV / Math.max(2, Math.floor((W - L) / 70)));
    // Regular ticks stop short of the last release, whose label is drawn
    // on its own and would otherwise overprint the tick before it.
    for (let i = 0; i <= LAST - step; i += step) {
      g += '<text class="dpb-axis" x="' + x(i).toFixed(1) + '" y="' + (H - 12) +
        '" text-anchor="middle">' + V[i] + "</text>";
    }
    g += '<text class="dpb-axis dpb-axis-last" x="' + x(LAST).toFixed(1) + '" y="' +
      (H - 12) + '" text-anchor="end">' + V[LAST] + "</text>";
    for (const b of r.brk) {
      const xx = x(b - 0.5).toFixed(1);
      g += '<line class="dpb-brk" x1="' + xx + '" x2="' + xx + '" y1="' + T +
        '" y2="' + (H - B) + '"/><text class="dpb-axis" x="' + (+xx + 4) +
        '" y="' + (T + 10) + '">meaning changed</text>';
    }
    const line = (a, cls) => segments(a, r.brk).map((seg) => seg.length === 1
      ? '<circle class="' + cls + '-dot" cx="' + x(seg[0]).toFixed(1) + '" cy="' +
        y(a[seg[0]]).toFixed(1) + '" r="2.5"/>'
      : '<polyline class="' + cls + '" points="' +
        seg.map((i) => x(i).toFixed(1) + "," + y(a[i]).toFixed(1)).join(" ") +
        '"/>').join("");
    g += line(N, "dpb-big-n") + line(P, "dpb-big-p");
    g += '<line class="dpb-hair" x1="0" x2="0" y1="' + T + '" y2="' + (H - B) +
      '" visibility="hidden"/>';
    g += '<rect class="dpb-hit" x="' + L + '" y="' + T + '" width="' + (W - L - R) +
      '" height="' + (H - T - B) + '" fill="transparent"/>';
    const svg = '<svg viewBox="0 0 ' + W + " " + H + '" role="img" aria-label="' +
      esc(r.id) + ' across releases">' + g + "</svg>";
    function attach(el) {
      const s = el.querySelector("svg"), hit = s.querySelector(".dpb-hit");
      const hair = s.querySelector(".dpb-hair");
      hit.addEventListener("mousemove", (ev) => {
        const bb = s.getBoundingClientRect();
        const sx = (ev.clientX - bb.left) * W / bb.width;
        const i = Math.max(0, Math.min(LAST,
          Math.round((sx - L) / (W - L - R) * (NV - 1))));
        hair.setAttribute("x1", x(i)); hair.setAttribute("x2", x(i));
        hair.setAttribute("visibility", "visible");
        tip.innerHTML = "<b>" + V[i] + '</b><br><span class="dpb-k-p">portable</span> ' +
          (P[i] == null ? "—" : esc(lab(P[i]))) +
          '<br><span class="dpb-k-n">native</span> ' + (N[i] == null ? "—" : esc(lab(N[i])));
        tip.hidden = false;
        // Kept inside the viewport: near the right edge it flips left.
        const room = window.scrollX + document.documentElement.clientWidth;
        const left = ev.pageX + 12 + tip.offsetWidth > room - 8
          ? ev.pageX - 12 - tip.offsetWidth : ev.pageX + 12;
        tip.style.left = Math.max(window.scrollX + 4, left) + "px";
        tip.style.top = ev.pageY + 12 + "px";
      });
      hit.addEventListener("mouseleave", () => {
        hair.setAttribute("visibility", "hidden");
        tip.hidden = true;
      });
    }
    return { svg, attach };
  }

  function init() {
    // The data is a script on the page that sets this global
    // (bench_report.py's PAGE_DATA), not an element found by id: instant
    // navigation re-creates an inline script with its text alone, and runs
    // it before document$ fires (#2391).
    const D = window.dpBenchData;
    const cards = document.getElementById("dp-bench-cards");
    const browser = document.getElementById("dp-bench-browser");
    const ratebox = document.getElementById("dp-bench-rate");
    if (!D || !cards || !browser || browser.dataset.ready) return;
    browser.dataset.ready = "1";
    SUFFIX = D.suffix || {};
    const V = D.versions, NV = V.length, LAST = NV - 1;
    const rows = D.rows, labels = D.labels;
    const byId = new Map(rows.map((r) => [r.id, r]));
    const head = D.head.filter((id) => byId.has(id));
    const headIdx = new Map(head.map((id, i) => [id, i]));
    let tip = document.getElementById("dpb-tip");
    if (!tip) {
      tip = document.createElement("div");
      tip.id = "dpb-tip"; tip.className = "dpb-tip"; tip.hidden = true;
      document.body.appendChild(tip);
    }
    const nameOf = (r) => (labels[r.id] ? labels[r.id].name : null);

    /* the rate box */
    if (ratebox) {
      ratebox.classList.add("dpb-ratebox");
      ratebox.innerHTML =
        '<label for="dpb-rate-in">Your sample rate</label>' +
        '<span><input id="dpb-rate-in" type="number" min="0.001" step="any"' +
        ' inputmode="decimal"> MSa/s</span><span class="dpb-presets"></span>' +
        '<p>Every stream kernel reads in samples per second, so its' +
        ' <a href="#headroom">headroom</a> is its throughput ÷ your rate. Below' +
        " ×1 it falls behind in real time on this machine.</p>";
    }
    const rateIn = ratebox && ratebox.querySelector("#dpb-rate-in");
    // `typed` leaves the box alone: rewriting it mid-entry would eat a
    // trailing "." while the reader types 61.44.
    function syncPresets(typed) {
      if (!ratebox) return;
      if (!typed) rateIn.value = RATE;
      ratebox.querySelector(".dpb-presets").innerHTML = PRESETS.map((v) =>
        '<button type="button" data-v="' + v + '" aria-pressed="' + (v === RATE) +
        '">' + v + "</button>").join("");
    }

    /* headline cards replace the static table that follows them */
    const fallback = cards.nextElementSibling;
    if (fallback && fallback.querySelector && fallback.querySelector("table")) {
      fallback.hidden = true;
    } else if (fallback && fallback.tagName === "TABLE") {
      fallback.hidden = true;
    }
    cards.classList.add("dpb-cards");
    function renderCards() {
      cards.innerHTML = head.map((id, i) => {
        const r = byId.get(id), p = r.p[LAST], n = r.n[LAST];
        return '<button type="button" class="dpb-card" data-id="' + esc(id) + '">' +
          '<span class="dpb-what">' + esc(nameOf(r)) + '<sup><a href="#d' + (i + 1) +
          '" tabindex="-1">' + (i + 1) + "</a></sup></span>" +
          '<span class="dpb-big">' + fmtVal(r, p) + "</span>" + spark(r, NV, 240, 38) +
          '<span class="dpb-row"><span>native ' + esc(fmtVal(r, n, true)) + "</span>" +
          chip(speedup(p, n), "%Δ") + "</span>" +
          (r.dom === "Sa" ? '<span class="dpb-row"><span>at ' + RATE + " MSa/s</span>" +
            headChip(headroom(r, p)) + "</span>" : "") + "</button>";
      }).join("");
    }

    /* family browser */
    const count = {};
    rows.forEach((r) => (count[r.fam] = (count[r.fam] || 0) + 1));
    const fams = D.families.filter((f) => count[f.id]);
    const state = { fam: fams[0].id, lang: "all", q: "", sort: "name", open: null };
    try {
      const s = JSON.parse(localStorage.getItem("dpb-state") || "{}");
      if (s.fam && count[s.fam]) state.fam = s.fam;
      if (s.lang) state.lang = s.lang;
    } catch (e) { /* storage unavailable: defaults */ }
    const save = () => {
      try {
        localStorage.setItem("dpb-state", JSON.stringify({ fam: state.fam, lang: state.lang }));
      } catch (e) { /* storage unavailable */ }
    };
    browser.innerHTML =
      '<div class="dpb-tabs" role="tablist">' + fams.map((f) =>
        '<button type="button" class="dpb-tab" role="tab" data-fam="' + f.id + '">' +
        esc(f.title) + "<span>" + count[f.id] + "</span></button>").join("") + "</div>" +
      '<div class="dpb-tools"><input type="search" class="dpb-q"' +
      ' placeholder="Filter by name, e.g. fft or n=4096" aria-label="Filter benchmarks">' +
      '<span class="dpb-seg" role="group" aria-label="Language">' +
      '<button type="button" data-lang="all">C + Python</button>' +
      '<button type="button" data-lang="c">C</button>' +
      '<button type="button" data-lang="py">Python</button></span>' +
      '<label>Sort <select class="dpb-sort"><option value="name">by name</option>' +
      '<option value="tput">by throughput</option>' +
      '<option value="head">by headroom</option>' +
      '<option value="delta">by change since last release</option>' +
      '<option value="src">by %Δ</option></select></label>' +
      '<span class="dpb-count"></span></div>' +
      '<div class="dpb-tablebox"><table class="dpb-table"><thead><tr>' +
      '<th scope="col">Benchmark</th><th scope="col">Portable</th>' +
      '<th scope="col">Native</th><th scope="col"><a href="#pct-delta">%Δ</a></th>' +
      '<th scope="col"><a href="#since-last">Since last</a></th>' +
      '<th scope="col"><a href="#headroom">Headroom</a></th>' +
      '<th scope="col"><a href="#trend">Trend</a></th></tr></thead>' +
      "<tbody></tbody></table></div>";
    const tbody = browser.querySelector("tbody");
    const box = browser.querySelector(".dpb-tablebox");

    function rowHTML(r) {
      const lab = nameOf(r), hi = headIdx.get(r.id), a = autoName(r);
      const name = lab
        ? '<span class="dpb-nm">' + esc(lab) +
          (hi != null ? '<sup><a href="#d' + (hi + 1) + '">' + (hi + 1) + "</a></sup>" : "") +
          "</span>"
        : '<span class="dpb-nm"><span class="dpb-obj">' + esc(a.obj) + "</span>" +
          (a.op ? " · " + esc(a.op) : "") + "</span>";
      const params = (lab ? [] : a.params).map((p) =>
        '<span class="dpb-pm">' + esc(p) + "</span>").join("");
      return '<tr class="dpb-r" data-id="' + esc(r.id) + '" tabindex="0" aria-expanded="' +
        (state.open === r.id) + '"><td>' + name + '<span class="dpb-lang">' +
        (r.k === "c" ? "C" : "Py") + "</span><br>" + params + '<div class="dpb-rid">' +
        esc(r.id.slice(r.id.indexOf(":") + 1)) + "</div></td>" +
        '<td class="dpb-v">' + fmtVal(r, r.p[LAST]) + '</td><td class="dpb-v">' +
        fmtVal(r, r.n[LAST]) + "</td><td>" + chip(speedup(r.p[LAST], r.n[LAST])) +
        "</td><td>" + (r.slb ? '<a class="dpb-flat" href="#trend" title="the' +
          ' value changed meaning since the last release (a new unit, or an' +
          ' old round of a new size), so the two are different quantities">' +
          "break</a>" : chip(r.sl)) + "</td><td>" + headChip(headroom(r, r.p[LAST])) +
        '</td><td class="dpb-trend">' + spark(r, NV, 132, 28) + "</td></tr>";
    }
    function detailHTML(r) {
      // Stacked (narrow) panels give the chart the box's width; side by
      // side it keeps 640.
      const avail = box.clientWidth - 40;
      const c = bigChart(r, V, tip,
        Math.round(avail < 900 ? Math.max(300, Math.min(640, avail)) : 640));
      const first = r.p.findIndex((v) => v != null);
      const lab = labels[r.id];
      const repro = r.k === "c"
        ? 'make bench BENCH_ARGS="--c-only ' + r.comp + '"'
        : "make bench-python PYTEST_BENCH_DIRS=src/doppler/" + r.mod + "/benchmarks";
      const noun = r.dom && (NOUN[r.dom] || r.dom);
      const counts = r.dom
        ? r.cnt.toLocaleString() + " " + noun + (r.cnt === 1 ? "" : "s") + " per " +
          unitWord(r) + (r.inf ? ", inferred from the name" : "")
        : "not a stream rate, so it reads as time per call" + (r.cnt > 1
          ? " (" + r.cnt.toLocaleString() + " " + unitWord(r) + "s per call)" : "");
      const dl = [
        ["What it times", lab ? esc(lab.def)
          : "No definition yet: this row is on benchmarks/.labels-exempt."],
        ["Counts", esc(counts)],
        ["Recorded unit", esc(unitWord(r)) + (r.k === "c" ? ", " + r.it.toLocaleString() +
          " per round" : "")],
        ["Measured since", V[first]],
        ["Breaks", r.brk.length ? r.brk.map((i) => V[i]).join(", ") +
          ': the value changed meaning (<a href="#trend">why</a>)' : "none"],
        ["Reproduce", "<code>" + esc(repro) + "</code>"],
        ["ID", "<code>" + esc(r.id.slice(r.id.indexOf(":") + 1)) + "</code>"],
      ];
      return {
        html: '<tr class="dpb-detail"><td colspan="' + COLS + '"><div class="dpb-dpanel"><div>' +
          '<div class="dpb-key"><span class="dpb-k-p">portable</span>' +
          '<span class="dpb-k-n">native</span></div>' + c.svg + "</div><dl>" +
          dl.map(([k, v]) => "<dt>" + k + "</dt><dd>" + v + "</dd>").join("") +
          "</dl></div></td></tr>",
        attach: c.attach,
      };
    }
    function render() {
      browser.querySelectorAll(".dpb-tab").forEach((b) =>
        b.setAttribute("aria-selected", String(b.dataset.fam === state.fam)));
      browser.querySelectorAll(".dpb-seg button").forEach((b) =>
        b.setAttribute("aria-pressed", String(b.dataset.lang === state.lang)));
      const list = rows.filter((r) => r.fam === state.fam &&
        (state.lang === "all" || r.k === state.lang) &&
        (!state.q || (r.id + " " + (nameOf(r) || "")).toLowerCase().indexOf(state.q) >= 0));
      const key = {
        name: (r) => [headIdx.has(r.id) ? 0 : nameOf(r) ? 1 : 2, r.comp.toLowerCase(), r.id],
        // Within a group a rate compares only with its own unit: bit/s
        // never ranks against codewords/s.
        tput: (r) => [r.dom || "", -(rateOf(r, r.p[LAST]) || 0), r.p[LAST]],
        head: (r) => [-(headroom(r, r.p[LAST]) || 0), r.p[LAST]],
        // `sl` is computed in bench_report.py, which honours breaks.
        delta: (r) => [-Math.abs(r.sl || 0)],
        src: (r) => [-(speedup(r.p[LAST], r.n[LAST]) || 0)],
      }[state.sort];
      list.sort((a, b) => {
        const ka = key(a), kb = key(b);
        for (let i = 0; i < ka.length; i++) {
          if (ka[i] < kb[i]) return -1;
          if (ka[i] > kb[i]) return 1;
        }
        return 0;
      });
      const behind = list.filter((r) => {
        const x = headroom(r, r.p[LAST]);
        return x != null && x < 1;
      }).length;
      browser.querySelector(".dpb-count").textContent = list.length + " rows" +
        (behind ? ", " + behind + " behind at " + RATE + " MSa/s" : "");
      if (!list.length) {
        tbody.innerHTML = '<tr><td colspan="' + COLS + '" class="dpb-empty">No benchmark' +
          " in this family matches “" + esc(state.q) + "”. Clear the filter or pick" +
          " another family.</td></tr>";
        return;
      }
      let html = "", att = null;
      for (const [gid, title, note] of GROUPS) {
        const part = list.filter((r) => group(r) === gid);
        if (!part.length) continue;
        html += '<tr class="dpb-sep"><th colspan="' + COLS + '" scope="rowgroup">' +
          title + " <span>· " + note + " · " + part.length + "</span></th></tr>";
        let dom = null;
        for (const r of part) {
          // Sorted by throughput, the "other" group runs in blocks of one
          // unit (bit/s, then chip/s...): a subheading marks each block.
          if (gid === "other" && state.sort === "tput" && r.dom !== dom) {
            dom = r.dom;
            html += '<tr class="dpb-sub"><td colspan="' + COLS + '">' +
              esc(SUFFIX[dom] || dom + "/s") + "</td></tr>";
          }
          html += rowHTML(r);
          if (state.open === r.id) { const d = detailHTML(r); html += d.html; att = d.attach; }
        }
      }
      tbody.innerHTML = html;
      const det = tbody.querySelector("tr.dpb-detail");
      if (det) {
        // As wide as the box shows, not as the table is: the panel is
        // pinned to the visible part of a table that scrolls sideways.
        const panel = det.querySelector(".dpb-dpanel");
        panel.style.width = (box.clientWidth - 30) + "px";
        // Too narrow for chart and definitions side by side: stack them.
        panel.classList.toggle("dpb-stack", box.clientWidth < 900);
        if (att) att(det);
      }
    }
    function setRate(v, typed) {
      if (!(v > 0)) return;
      RATE = v;
      try { localStorage.setItem("dpb-rate", String(v)); } catch (e) { /* no storage */ }
      syncPresets(typed); renderCards(); render();
    }
    const toggle = (id) => { state.open = state.open === id ? null : id; render(); };
    browser.querySelector(".dpb-tabs").addEventListener("click", (e) => {
      const b = e.target.closest(".dpb-tab");
      if (!b) return;
      state.fam = b.dataset.fam; state.open = null; save(); render();
    });
    browser.querySelector(".dpb-seg").addEventListener("click", (e) => {
      const b = e.target.closest("button");
      if (!b) return;
      state.lang = b.dataset.lang; save(); render();
    });
    browser.querySelector(".dpb-q").addEventListener("input", (e) => {
      state.q = e.target.value.trim().toLowerCase(); render();
    });
    browser.querySelector(".dpb-sort").addEventListener("change", (e) => {
      state.sort = e.target.value; render();
    });
    tbody.addEventListener("click", (e) => {
      if (e.target.closest("a")) return;
      const tr = e.target.closest("tr.dpb-r");
      if (tr) toggle(tr.dataset.id);
    });
    tbody.addEventListener("keydown", (e) => {
      if ((e.key === "Enter" || e.key === " ") && e.target.matches("tr.dpb-r")) {
        e.preventDefault(); toggle(e.target.dataset.id);
      }
    });
    cards.addEventListener("click", (e) => {
      const c = e.target.closest(".dpb-card");
      if (!c || e.target.closest("a")) return;
      const r = byId.get(c.dataset.id);
      state.fam = r.fam; state.lang = "all"; state.q = ""; state.sort = "name";
      state.open = r.id;
      browser.querySelector(".dpb-q").value = "";
      browser.querySelector(".dpb-sort").value = "name";
      render();
      box.scrollIntoView({ block: "start" });
      const tr = tbody.querySelector('tr.dpb-r[data-id="' + CSS.escape(r.id) + '"]');
      if (tr) box.scrollTop = tr.offsetTop - 44;
    });
    if (ratebox) {
      rateIn.addEventListener("input", () => setRate(parseFloat(rateIn.value), true));
      ratebox.querySelector(".dpb-presets").addEventListener("click", (e) => {
        const b = e.target.closest("button");
        if (b) setRate(parseFloat(b.dataset.v));
      });
    }
    syncPresets(); renderCards(); render();
    // The open panel's width and the chart's are measured when drawn, so
    // a rotate or a resize redraws it.
    onResize = () => { if (state.open) render(); };
  }

  let onResize = null, resizeTimer = 0;
  window.addEventListener("resize", () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => { if (onResize) onResize(); }, 150);
  });

  // Material's instant navigation swaps pages without a reload; document$
  // fires on every page it shows, including the first.
  if (window.document$ && window.document$.subscribe) {
    window.document$.subscribe(init);
  } else if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
