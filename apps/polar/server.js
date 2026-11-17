// Servidor central en el Mac. Guarda el estado y lo comparte entre TODAS las pantallas
// (Mac e iPhone a la vez) y envia el MIDI a Logic Pro por el puerto virtual "REAL360".
const http = require('http'), fs = require('fs'), path = require('path'), os = require('os');
const { exec, execSync } = require('child_process');
const { WebSocketServer } = require('ws');
const midi = require('@julusian/midi');
const dgram = require('dgram');

const PORT = 8080, N = 16; // 16 huecos = 16 canales MIDI (pistas y efectos)
const CC = { x: 20, y: 21, v: 22, p: 23 }; // p = Spread // por pista: X, Y y volumen. Cada pista = un canal MIDI (1-8)
const MAPF = path.join(process.env.PANNER_DATA || __dirname, 'mapping.json');
let map = {}; try { map = JSON.parse(fs.readFileSync(MAPF)); } catch (e) {}
const S = Array.from({ length: N }, (_, i) => ({ x: .5, y: .5, v: 90, name: '', icon: '', on: i < 8, type: 'pista', src: -1, follow: true, sp: 64, color: '', mute: false, solo: false, guard: false, asp: 0, mono: true, pm: 0, rpan: false, rspr: false, rps: 5, rss: 5, hid: false })); // estado compartido. hid: pista quitada del pad y silenciada. pm: 0 Balance (como Logic y Looper Búho) · 1 Mono · 2 Apertura (L y R separados por el Spread); mono = pm distinto de 2
const TRF = path.join(process.env.PANNER_DATA || __dirname, 'tracks.json'); // nombres e iconos persistentes
try { JSON.parse(fs.readFileSync(TRF)).forEach((o, i) => { if (S[i]) { S[i].name = o.name || ''; S[i].icon = o.icon || ''; if (o.on !== undefined) S[i].on = !!o.on; if (o.type) S[i].type = o.type; if (o.src !== undefined) S[i].src = o.src; if (o.follow !== undefined) S[i].follow = !!o.follow; if (typeof o.color === 'string') S[i].color = o.color; if (typeof o.guard === 'boolean') S[i].guard = o.guard; S[i].hid = !!o.hid; S[i].pm = o.pm !== undefined ? Math.min(2, Math.max(0, o.pm | 0)) : (o.mono === true ? 1 : 0); S[i].mono = S[i].pm !== 2; if (S[i].pm !== 2) S[i].guard = false; S[i].asp = o.asp !== undefined ? (o.asp | 0) : -1; /* -1 = fichero antiguo: se rellena mas abajo con el modo global anterior */ } }); } catch (e) {}
const saveTr = () => fs.writeFileSync(TRF, JSON.stringify(S.map(({ name, icon, on, type, src, follow, color, guard, asp, mono, pm, hid }) => ({ name, icon, on, type, src, follow, color, guard, asp, mono, pm, hid }))));
let cur = 0, learning = null;

const out = new midi.Output();
out.openVirtualPort('REAL360');
const sent = {};
let mcount = 0; // mensajes MIDI enviados desde el último aviso a la interfaz (luz «MIDI»)
function cc(t, k, val) { const key = t + k; if (sent[key] === val) return; sent[key] = val; if (MODE.out === 'plugin') return; mcount++; out.sendMessage([0xB0 | t, CC[k], val & 127]); }
// Modo de posicion: 'polar' envia Angulo (CC20) y Diversidad (CC21), que es lo que Logic graba y acepta en todo el circulo;
// 'xy' envia X e Y directos (Logic solo acepta pares dentro del circulo).
const MODEF = path.join(process.env.PANNER_DATA || __dirname, 'mode.json');
const MODE = { polar: true, invA: false, invD: false, autoSp: 0, guardR: 0.5, guardSp: 44, out: 'plugin' }; // out: 'midi' | 'plugin' | 'both' (a dónde va el paneo). autoSp: 0 manual, 1 abierto en el centro, 2 abierto en los bordes
try { Object.assign(MODE, JSON.parse(fs.readFileSync(MODEF))); } catch (e) {}
MODE.out = 'plugin'; // POLAR arranca siempre con la salida al plugin (se puede cambiar a MIDI o Ambos mientras está abierto)
// Spread mínimo de las pistas estéreo protegidas: el Spread es libre, pero nunca baja de MODE.guardSp grados (se conserva el signo).
S.forEach(o => { if (o.asp < 0) o.asp = [0, 1, 2].includes(MODE.autoSp) ? MODE.autoSp : 0; }); // las pistas de un fichero antiguo heredan el modo global que había
function minSp() { return Math.max(0, Math.min(180, MODE.guardSp == null ? 44 : +MODE.guardSp)); }
function clampSpG(o, c) {
  if (!o || !o.guard) return c; const sd = (c - 63.5) / 63.5 * 180, m = minSp();
  return Math.abs(sd) >= m ? c : Math.round(63.5 + (sd < 0 ? -m : m) / 180 * 63.5);
}
S.forEach(o => { if (o.guard) o.sp = clampSpG(o, o.sp == null ? 64 : o.sp); });
// Protección del centro: una pista estéreo no puede entrar en el cuadrado central de radio MODE.guardR (en el centro exacto, Logic junta L y R y se cancela).
const gdir = [];
function guardXY(t, x, y) {
  const o = S[t]; if (!o || !o.guard || o.mono || !MODE.polar || !(MODE.guardR > 0)) return [x, y];
  const R = MODE.guardR; let u = (x - .5) * 2, w = (.5 - y) * 2; const r = Math.max(Math.abs(u), Math.abs(w));
  if (r > 1e-3) gdir[t] = [u / r, w / r];
  if (r >= R) return [x, y];
  const d = gdir[t] || [0, 1]; return [clamp(.5 + d[0] * R / 2), clamp(.5 - d[1] * R / 2)];
}
function posCC(o) {
  if (!MODE.polar) return [Math.round(o.x * 127), Math.round((1 - o.y) * 127)];
  const u = (o.x - .5) * 2, v = (.5 - o.y) * 2;                 // v>0 = delante
  let r = Math.min(1, Math.max(Math.abs(u), Math.abs(v)));       // el borde del pad = el borde del circulo
  if (o.guard && MODE.guardR > 0 && r < MODE.guardR) r = MODE.guardR; // seguridad: nunca se manda un centro exacto a una pista protegida
  let ang = (u === 0 && v === 0) ? 0 : Math.atan2(u, v) * 180 / Math.PI; // 0 delante, + derecha, +-180 detras
  if (MODE.invA) ang = -ang;
  const div = MODE.invD ? r : 1 - r;
  return [Math.round((ang + 180) / 360 * 127), Math.round(div * 127)];
}
const emitPos = t => { const [a, b] = posCC(S[t]); cc(t, 'x', a); cc(t, 'y', b); if (S[t].asp) emitSp(t); };
// mute y solo: se mandan a Logic como volumen 0 (el volumen real se conserva). El solo apaga las demás pistas; los efectos solo obedecen a su propio mute.
const anySolo = () => S.some(o => o.on && !o.hid && o.type === 'pista' && o.solo);
const silent = t => (S[t].hid ? true : S[t].type === 'pista' && anySolo() ? !S[t].solo : !!S[t].mute); // una pista quitada del pad no suena // como en Logic: el solo manda sobre el mute
const effVol = t => (silent(t) ? 0 : S[t].v);
// ---- Salida al plugin REAL360 (UDP local) ----
// Un paquete con las 16 pistas: ángulo, radio, spread y ganancia en decimales (sin los 128 pasos del MIDI). Se manda cuando algo cambia, con un latido cada 250 ms.
// Formato (little-endian): cabecera de 12 bytes = 'PLR1' + uint32 secuencia + uint8 nº de pistas (16) + 3 bytes libres;
// después, por pista (20 bytes): float32 ángulo° (0 delante, + derecha, ±180 detrás) · float32 radio (0 centro … 1 borde, distancia cuadrada)
// · float32 spread° (−180…180, el signo intercambia L y R) · float32 ganancia lineal (mute/solo ya aplicados) · uint8 banderas
// (1 activa · 2 mono · 4 silenciada · 8 efecto · 16 protegida · 32 balance; si no es ni mono ni balance, es «con apertura») · 3 bytes libres.
// El spread solo vale en «con apertura»; en balance y mono llega a 0.
const PLUG_PORT = +(process.env.POLAR_PLUGIN_PORT || 9360), PLUG_HOST = '127.0.0.1';
const plugSock = dgram.createSocket('udp4'); plugSock.on('error', () => {});
const plugOn = () => MODE.out === 'plugin' || MODE.out === 'both';
const PLUG = { seq: 0, body: null, at: 0, pkts: 0 };
function plugBody() {
  const b = Buffer.alloc(N * 20);
  S.forEach((o, t) => {
    const p = t * 20, u = (o.x - .5) * 2, w = (.5 - o.y) * 2; let r = Math.min(1, Math.max(Math.abs(u), Math.abs(w)));
    if (o.guard && !o.mono && MODE.guardR > 0 && r < MODE.guardR) r = MODE.guardR;
    const ang = (u === 0 && w === 0) ? (o.guard && gdir[t] ? Math.atan2(gdir[t][0], gdir[t][1]) * 180 / Math.PI : 0) : Math.atan2(u, w) * 180 / Math.PI;
    const sd = o.mono ? 0 : (spVal(o) - 63.5) / 63.5 * 180, vv = effVol(t), g = vv > 0 ? (vv / 90) * (vv / 90) : 0; // 40·log10(v/90) dB: la curva de POLAR con 0 dB exacto en 90 (+5,98 dB en 127)
    b.writeFloatLE(ang, p); b.writeFloatLE(r, p + 4); b.writeFloatLE(sd, p + 8); b.writeFloatLE(g, p + 12);
    b[p + 16] = (o.on ? 1 : 0) | (o.pm === 1 ? 2 : 0) | (silent(t) ? 4 : 0) | (o.type === 'efecto' ? 8 : 0) | (o.guard ? 16 : 0) | (o.pm === 0 ? 32 : 0);
  });
  return b;
}
setInterval(() => {
  if (!plugOn()) return;
  const body = plugBody(), now = Date.now();
  if (PLUG.body && body.equals(PLUG.body) && now - PLUG.at < 250) return;
  PLUG.body = body; PLUG.at = now;
  const h = Buffer.alloc(12); h.write('PLR1', 0, 'ascii'); h.writeUInt32LE(PLUG.seq++ >>> 0, 4); h[8] = N;
  plugSock.send(Buffer.concat([h, body]), PLUG_PORT, PLUG_HOST, () => {}); PLUG.pkts++;
}, 16);
setInterval(() => { bc({ plug: { pps: PLUG.pkts, out: MODE.out } }); PLUG.pkts = 0; }, 1000);
const emitVol = t => cc(t, 'v', effVol(t));
const emitAllVol = () => S.forEach((o, t) => { if (o.on) emitVol(t); });
const spVal0 = o => { const s0 = (o.sp == null ? 64 : o.sp); if (!o.asp) return s0; const u = (o.x - .5) * 2, w = (.5 - o.y) * 2, r = Math.min(1, Math.max(Math.abs(u), Math.abs(w))); return Math.round(63.5 + (s0 - 63.5) * (o.asp === 1 ? 1 - r : r)); }; // el Spread de Logic va de -180 a +180: 63,5 = 0
const spVal = o => clampSpG(o, spVal0(o));
const emitSp = t => { if (!S[t].mono) cc(t, 'p', spVal(S[t])); }; // en una pista mono el Spread no existe: no se manda
// Reenviar todo: POLAR recuerda lo último que mandó y no lo repite; esto borra esa memoria y manda de nuevo todos los valores (útil tras reiniciar Logic).
const resendAll = () => { Object.keys(sent).forEach(k => delete sent[k]); S.forEach((o, t) => { if (o.on) { emitPos(t); emitVol(t); emitSp(t); } }); };
const clamp = v => Math.min(1, Math.max(0, +v || 0));

let local = ''; try { local = execSync('scutil --get LocalHostName', { timeout: 1500 }).toString().trim(); } catch (e) {}
const ips = () => Object.values(os.networkInterfaces()).flat().filter(i => i.family === 'IPv4' && !i.internal).map(i => 'http://' + i.address + ':' + PORT);
const urls = () => [...(local ? ['http://' + local + '.local:' + PORT] : []), ...ips()]; // la primera (.local) no cambia con la wifi
const ICON = Buffer.from('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', 'base64');
const srv = http.createServer((req, res) => {
  if (/^\/(apple-)?touch-icon(-precomposed)?\.png/.test(req.url || '')) { // icono de la pantalla de inicio del iPhone (incluido en este archivo)
    res.writeHead(200, { 'Content-Type': 'image/png', 'Cache-Control': 'no-cache' }); return res.end(ICON);
  }
  fs.readFile(process.env.PANNER_WEB || path.join(__dirname, 'index.html'), (e, d) => {
    res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8', 'Cache-Control': 'no-store' }); res.end(d);
  });
});
const wss = new WebSocketServer({ server: srv });
const send = (c, o) => c.readyState === 1 && c.send(JSON.stringify(o));
const bc = (o, except) => wss.clients.forEach(c => c !== except && send(c, o));
setInterval(() => { if (mcount) { bc({ mi: mcount }); mcount = 0; } }, 150);

// ---- Sesiones guardadas (varias configuraciones de pistas, nombres, iconos y posiciones) ----
// ---- Movimiento aleatorio (paneo, spread y efectos, cada uno con su velocidad) ----
const RND = { pan: { on: false, speed: 5 }, sp: { on: false, speed: 5 }, fx: { on: false, speed: 5 }, vari: 5 };
const touched = {}, touchedSp = {}, rt = {}, rs = {}, spf = {}, fp = {}, fsp = {};
const fac = () => 1; // todas las pistas van a la misma velocidad
let rndTimer = null;
function setPos(t, x, y) {
  [x, y] = guardXY(t, x, y);
  const o = S[t], ox = o.x, oy = o.y; o.x = x; o.y = y; emitPos(t);
  S.forEach((e, k) => { if (e.on && e.type === 'efecto' && e.src === t && e.follow) { e.x = clamp(e.x + x - ox); e.y = clamp(e.y + y - oy); emitPos(k); bc({ t: k, x: e.x, y: e.y }); } });
  bc({ t, x, y });
}
function wtarget(t) { // punto al azar; en una pista protegida, fuera de la zona del centro
  const on = S[t].guard && MODE.polar && MODE.guardR > 0;
  for (let i = 0; i < 40; i++) { const x = .07 + Math.random() * .86, y = .07 + Math.random() * .86; if (!on || Math.max(Math.abs((x - .5) * 2), Math.abs((.5 - y) * 2)) >= MODE.guardR + .05) return { x, y }; }
  return { x: .07, y: .07 };
}
// Paneo aleatorio de una pista estéreo protegida: recorre el campo en órbita alrededor de la zona del centro, con velocidad y radio que
// cambian despacio y de vez en cuando cambia de sentido. No cruza la zona ni choca con ella, así que no rebota ni se queda en un lado.
const gst = [];
function wanderOrbit(t, o, speed) {
  const R = MODE.guardR, now = Date.now(), lo = R + .08, hi = .96;
  const u = (o.x - .5) * 2, w = (.5 - o.y) * 2; let rho = Math.max(Math.abs(u), Math.abs(w));
  let th = (u === 0 && w === 0) ? 0 : Math.atan2(u, w);                       // radianes, 0 = delante, + = derecha
  const st = gst[t] || (gst[t] = { s: Math.random() < .5 ? 1 : -1, k: 1, sv: 0, rt: Math.min(hi, Math.max(lo, rho)), next: 0 });
  if (now > st.next) {                                                          // cada 2-6 s: nuevo ritmo, nuevo radio y, a veces, otro sentido
    if (Math.random() < .25) st.s = -st.s;
    st.k = .85 + Math.random() * .65; st.rt = lo + Math.random() * (hi - lo); st.next = now + 2000 + Math.random() * 4000;
  }
  const base = (0.0012 * Math.pow(1.5, speed - 1)) / (0.5 * Math.max(.4, rho)); // radianes por paso con el mismo ritmo que el resto de movimientos
  st.sv += (st.s * st.k - st.sv) * .07; rho += (st.rt - rho) * .012;           // cambios suaves
  rho = Math.max(R + .04, Math.min(hi + .02, rho)); th += st.sv * base;
  const m = Math.max(Math.abs(Math.sin(th)), Math.abs(Math.cos(th)));
  setPos(t, clamp(.5 + rho * Math.sin(th) / m / 2), clamp(.5 - rho * Math.cos(th) / m / 2));
}
function wander(t, o, speed) {
  if (o.guard && MODE.polar && MODE.guardR > 0) return wanderOrbit(t, o, speed); // mueve el objeto hacia un destino al azar y, al llegar, elige otro (y otra velocidad)
  const step = 0.0012 * Math.pow(1.5, speed - 1), g = rt[t], dx = g ? g.x - o.x : 0, dy = g ? g.y - o.y : 0, dist = Math.hypot(dx, dy);
  if (!g || dist < step * 1.5) { rt[t] = wtarget(t); fp[t] = fac(); return; }
  setPos(t, o.x + dx / dist * step, o.y + dy / dist * step);
}
function rndTick() { // movimiento aleatorio por pista: cada una según sus propios interruptores y velocidades
  const now = Date.now();
  S.forEach((o, t) => {
    if (!o.on) return;
    if (o.rpan && now - (touched[t] || 0) > 1500) wander(t, o, o.rps || 5);
    if (o.rspr && !o.mono && now - (touchedSp[t] || 0) > 1500) {
      const step = 0.2 * Math.pow(1.5, (o.rss || 5) - 1); if (spf[t] == null) spf[t] = o.sp == null ? 64 : o.sp;
      const g = rs[t];
      if (g == null || Math.abs(g - spf[t]) < step * 1.5) { rs[t] = Math.random() * 127; fsp[t] = fac(); }
      else { spf[t] += Math.sign(g - spf[t]) * step; const w = clampSpG(o, Math.round(spf[t])); if (w !== o.sp) { o.sp = w; emitSp(t); bc({ t, sp: w }); } }
    }
  });
}
function rndSync() {
  const any = S.some(o => o.on && (o.rpan || o.rspr));
  if (any && !rndTimer) rndTimer = setInterval(rndTick, 50);
  if (!any && rndTimer) { clearInterval(rndTimer); rndTimer = null; }
}
const VER = 'polar-29';
const SESD = path.join(process.env.PANNER_DATA || __dirname, 'sesiones'); fs.mkdirSync(SESD, { recursive: true });
const clean = n => String(n).replace(/[^\p{L}\p{N} _-]/gu, '_').trim().slice(0, 40);
const sesFile = n => path.join(SESD, clean(n) + '.json');
const sesList = () => fs.readdirSync(SESD).filter(f => f.endsWith('.json')).map(f => f.slice(0, -5)).sort((a, b) => a.localeCompare(b));
let sesCur = ''; try { sesCur = fs.readFileSync(path.join(SESD, '_actual.txt'), 'utf8'); } catch (e) {}
const snap = () => S.map(({ x, y, v, sp, name, icon, on, type, src, follow, color, guard, asp, mono, pm, hid }) => ({ x, y, v, sp, name, icon, on, type, src, follow, color, guard, asp, mono, pm, hid }));
const sesInfo = () => ({ list: sesList(), cur: sesCur });
function sesSave(n) { n = clean(n); fs.writeFileSync(sesFile(n), JSON.stringify({ s: snap(), v: 2 })); sesCur = n; fs.writeFileSync(path.join(SESD, '_actual.txt'), n); }
function sesLoad(n) {
  const j = JSON.parse(fs.readFileSync(sesFile(n)));
  S.forEach((o, i) => { if (j.s[i]) { Object.assign(o, j.s[i]); o.hid = !!j.s[i].hid; if (j.s[i].pm === undefined) o.pm = j.s[i].mono ? 1 : 0; /* sesión antigua: estéreo pasa a Balance */ o.pm = Math.min(2, Math.max(0, o.pm | 0)); o.mono = o.pm !== 2; if (o.pm !== 2) o.guard = false; if (j.v !== 2 && o.sp === 0) o.sp = 64; } else o.on = false; });
  S.forEach(o => { if (o.guard) o.sp = clampSpG(o, o.sp == null ? 64 : o.sp); });
  if (!S[cur].on) cur = Math.max(0, S.findIndex(o => o.on));
  sesCur = clean(n); fs.writeFileSync(path.join(SESD, '_actual.txt'), sesCur); saveTr();
  for (const k in sent) delete sent[k];
  S.forEach((o, t) => { if (o.on) { emitPos(t); emitVol(t); emitSp(t); } });
  bc({ init: { S, cur, urls: urls(), mode: MODE, ver: VER, ses: sesInfo(), rnd: RND } });
}
if (!sesList().length) sesSave('Básica');

wss.on('connection', ws => {
  send(ws, { init: { S, cur, urls: urls(), mode: MODE, ver: VER, ses: sesInfo(), rnd: RND } });
  ws.on('message', m => {
    try {
      const j = JSON.parse(m);
      if (j.sel !== undefined) { cur = ((j.sel | 0) + N) % N; bc({ sel: cur }, ws); }
      else if (j.learn) { learning = j.learn; console.log('Aprendiendo:', learning); }
      else if (j.ses) {
        const n = String(j.name || '').trim().slice(0, 40);
        try {
          if (j.ses === 'save' && n) { sesSave(n); bc({ ses: sesInfo() }); }
          else if (j.ses === 'load' && n && fs.existsSync(sesFile(n))) sesLoad(n);
          else if (j.ses === 'rename' && n && fs.existsSync(sesFile(n))) {
            const to = clean(j.to || '');
            if (!to) send(ws, { msg: 'Nombre no válido' });
            else if (to !== clean(n) && fs.existsSync(sesFile(to))) send(ws, { msg: 'Ya existe una sesión llamada «' + to + '»' });
            else { fs.renameSync(sesFile(n), sesFile(to)); if (sesCur === clean(n)) { sesCur = to; fs.writeFileSync(path.join(SESD, '_actual.txt'), to); } bc({ ses: sesInfo() }); }
          }
          else if (j.ses === 'dup' && n && fs.existsSync(sesFile(n))) {
            const to = clean(j.to || '');
            if (!to) send(ws, { msg: 'Escribe el nombre de la copia' });
            else if (fs.existsSync(sesFile(to))) send(ws, { msg: 'Ya existe una sesión llamada «' + to + '»' });
            else { fs.copyFileSync(sesFile(n), sesFile(to)); bc({ ses: sesInfo() }); }
          }
          else if (j.ses === 'del' && n && fs.existsSync(sesFile(n))) { fs.unlinkSync(sesFile(n)); if (sesCur === clean(n)) sesCur = ''; bc({ ses: sesInfo() }); }
        } catch (e) { console.error('Sesion:', e.message); }
      }
      else if (j.rnd) {
        if (j.rnd.once) S.forEach((o, t) => { if (o.on && (j.rnd.all ? o.type !== 'efecto' : t === j.rnd.t)) { const g = o.guard ? wtarget(t) : { x: .06 + Math.random() * .88, y: .06 + Math.random() * .88 }; setPos(t, g.x, g.y); } });
        for (const k of ['pan', 'sp', 'fx']) if (j.rnd[k]) {
          if (j.rnd[k].on !== undefined) RND[k].on = !!j.rnd[k].on;
          if (j.rnd[k].speed) RND[k].speed = Math.min(10, Math.max(1, j.rnd[k].speed | 0));
        }
        if (j.rnd.vari !== undefined) RND.vari = Math.min(10, Math.max(0, j.rnd.vari | 0));
        for (const k in spf) delete spf[k]; for (const k in rs) delete rs[k];
        rndSync(); bc({ rnd: RND });
      }
      else if (j.mode) {
        Object.assign(MODE, { polar: !!j.mode.polar, invA: !!j.mode.invA, invD: !!j.mode.invD, autoSp: [0, 1, 2].includes(j.mode.autoSp) ? j.mode.autoSp : 0 });
        if (j.mode.out !== undefined) { const was = MODE.out; MODE.out = ['midi', 'plugin', 'both'].includes(j.mode.out) ? j.mode.out : 'midi'; PLUG.body = null; if (was === 'plugin' && MODE.out !== 'plugin') resendAll(); }
        if (j.mode.guardR !== undefined) MODE.guardR = Math.min(.6, Math.max(0, +j.mode.guardR || 0));
        if (j.mode.guardSp !== undefined) { MODE.guardSp = Math.min(180, Math.max(0, +j.mode.guardSp || 0)); S.forEach((p, t) => { if (p.on && p.guard) { p.sp = clampSpG(p, p.sp == null ? 64 : p.sp); emitSp(t); bc({ t, sp: p.sp }); } }); }
        S.forEach((p, t) => { if (p.on && p.guard) { const g = guardXY(t, p.x, p.y); if (g[0] !== p.x || g[1] !== p.y) setPos(t, g[0], g[1]); } });
        fs.writeFileSync(MODEF, JSON.stringify(MODE)); S.forEach((o, t) => { if (o.on) { emitPos(t); emitSp(t); } }); bc({ mode: MODE }, ws);
      }
      else if (j.cmd === 'probe') { // devuelve lo ÚLTIMO que se mandó de verdad a Logic por cada pista: [CC20 ángulo, CC21 diversidad, CC22 volumen, CC23 spread]
        const P = {}; S.forEach((o, t) => { P[t] = [sent[t + 'x'], sent[t + 'y'], sent[t + 'v'], sent[t + 'p']]; }); send(ws, { probe: P });
      }
      else if (j.cmd === 'rall') { // activar o quitar el movimiento aleatorio en todas las pistas del mismo tipo que la pista indicada (pistas con pistas, efectos con efectos)
        const fx = S[j.t].type === 'efecto', on = !!j.on, sp = Math.min(10, Math.max(1, (j.speed | 0) || 5));
        S.forEach((p, k) => { if (!p.on || (p.type === 'efecto') !== fx) return;
          if (j.kind === 'spr') { if (p.mono) return; p.rspr = on; p.rss = sp; delete spf[k]; delete rs[k]; bc({ t: k, rspr: on, rss: sp }); }
          else { p.rpan = on; p.rps = sp; delete rt[k]; bc({ t: k, rpan: on, rps: sp }); } });
        rndSync();
      }
      else if (j.cmd === 'rstop') { S.forEach((p, k) => { if (p.rpan || p.rspr) { p.rpan = p.rspr = false; bc({ t: k, rpan: false, rspr: false }); } }); rndSync(); }
      else if (j.cmd === 'resend') { resendAll(); bc({ toast: 'Estado reenviado a Logic ✓' }); }
      else if (j.cmd === 'reopen') {
        try { out.closePort(); } catch (e) {}
        try { out.openVirtualPort('REAL360'); } catch (e) { send(ws, { msg: 'No he podido recrear el puerto MIDI: ' + e.message }); }
        resendAll(); bc({ toast: 'Puerto MIDI recreado y estado reenviado ✓' });
      }
      else if (j.sweep && j.t >= 0 && j.t < N) { // barrido de un solo CC (0 a 127) para que Logic lo aprenda sin ambiguedad
        const t = j.t, k = ['x', 'y', 'v', 'p'].includes(j.sweep) ? j.sweep : 'x'; let v = 0;
        const iv = setInterval(() => { if (v > 127) { clearInterval(iv); const p = posCC(S[t]); delete sent[t + k]; cc(t, k, k === 'x' ? p[0] : k === 'y' ? p[1] : k === 'v' ? effVol(t) : spVal(S[t])); return; } mcount++; out.sendMessage([0xB0 | t, CC[k], v]); v += 4; }, 30);
      }
      else if (j.t >= 0 && j.t < N) {
        const o = { t: j.t };
        if (j.x !== undefined) {
          touched[j.t] = Date.now(); const ox = S[j.t].x, oy = S[j.t].y;
          { const g = guardXY(j.t, clamp(j.x), clamp(j.y)); S[j.t].x = o.x = g[0]; S[j.t].y = o.y = g[1]; } emitPos(j.t);
          if (S[j.t].type !== 'efecto') S.forEach((e, k) => { // los efectos enlazados acompañan a su pista
            if (e.on && e.type === 'efecto' && e.src === j.t && e.follow) {
              e.x = clamp(e.x + o.x - ox); e.y = clamp(e.y + o.y - oy); emitPos(k); bc({ t: k, x: e.x, y: e.y });
            }
          });
        }
        if (j.v !== undefined) { S[j.t].v = o.v = Math.min(127, Math.max(0, j.v | 0)); emitVol(j.t); }
        if (j.mute !== undefined) { S[j.t].mute = o.mute = !!j.mute; emitAllVol(); }
        if (j.solo !== undefined) { S[j.t].solo = o.solo = !!j.solo; emitAllVol(); }
        if (j.hid !== undefined) { S[j.t].hid = o.hid = !!j.hid; emitAllVol(); saveTr(); } // quitar la pista del pad (y silenciarla) o volver a ponerla
        if (j.sp !== undefined && !S[j.t].mono) { touchedSp[j.t] = Date.now(); delete spf[j.t]; S[j.t].sp = o.sp = clampSpG(S[j.t], Math.min(127, Math.max(0, j.sp | 0))); emitSp(j.t); }
        if (j.name !== undefined) { S[j.t].name = o.name = String(j.name).slice(0, 14); saveTr(); }
        if (j.color !== undefined) { S[j.t].color = o.color = /^#[0-9a-f]{6}$/i.test(j.color) ? j.color : ''; saveTr(); }
        if (j.icon !== undefined) { S[j.t].icon = o.icon = String(j.icon).slice(0, 12); saveTr(); }
        if (j.pm !== undefined) { const m = Math.min(2, Math.max(0, j.pm | 0)); S[j.t].pm = o.pm = m; S[j.t].mono = o.mono = m !== 2; if (m !== 2) { S[j.t].guard = o.guard = false; S[j.t].rspr = o.rspr = false; rndSync(); } saveTr(); if (m === 2) emitSp(j.t); } // modo de paneo de la pista
        if (j.mono !== undefined && j.pm === undefined) { S[j.t].pm = o.pm = j.mono ? 1 : 2; S[j.t].mono = o.mono = !!j.mono; if (S[j.t].mono) { S[j.t].rspr = o.rspr = false; rndSync(); } if (S[j.t].mono && S[j.t].guard) S[j.t].guard = o.guard = false; saveTr(); if (!S[j.t].mono) emitSp(j.t); } // pista mono: sin Spread y sin protección del centro
        if (j.guard !== undefined) { S[j.t].guard = o.guard = !!j.guard && !S[j.t].mono; saveTr(); if (S[j.t].guard) { S[j.t].sp = o.sp = clampSpG(S[j.t], S[j.t].sp == null ? 64 : S[j.t].sp); emitSp(j.t); const g = guardXY(j.t, S[j.t].x, S[j.t].y); if (g[0] !== S[j.t].x || g[1] !== S[j.t].y) setPos(j.t, g[0], g[1]); } }
        if (j.rpan !== undefined) { S[j.t].rpan = o.rpan = !!j.rpan; delete rt[j.t]; rndSync(); }
        if (j.rspr !== undefined) { S[j.t].rspr = o.rspr = !!j.rspr && !S[j.t].mono; delete spf[j.t]; delete rs[j.t]; rndSync(); }
        if (j.rps !== undefined) { S[j.t].rps = o.rps = Math.min(10, Math.max(1, j.rps | 0)); }
        if (j.rss !== undefined) { S[j.t].rss = o.rss = Math.min(10, Math.max(1, j.rss | 0)); }
        if (j.asp !== undefined && !S[j.t].mono) { S[j.t].asp = o.asp = [0, 1, 2].includes(j.asp | 0) ? j.asp | 0 : 0; saveTr(); emitSp(j.t); } // Spread automático de ESTA pista: 0 manual, 1 abierto en el centro, 2 abierto en los bordes
        if (j.follow !== undefined) { S[j.t].follow = o.follow = !!j.follow; saveTr(); }
        if (j.src !== undefined) { S[j.t].src = o.src = j.src | 0; saveTr(); }
        if (j.rem) { S[j.t].on = o.on = false; S[j.t].rpan = S[j.t].rspr = false; rndSync(); S[j.t].mute = S[j.t].solo = S[j.t].hid = false; emitAllVol(); S.forEach(p => { if (p.src === j.t) p.src = -1; }); saveTr(); }
        if (j.add) { Object.assign(S[j.t], { on: true, type: j.add === 'efecto' ? 'efecto' : 'pista', src: -1, follow: true, x: .5, y: .5, v: 90, sp: 64, name: '', icon: '', color: '', mute: false, solo: false, guard: false, asp: 0, mono: true, pm: 0, rpan: false, rspr: false, rps: 5, rss: 5, hid: false }); Object.assign(o, S[j.t]); emitPos(j.t); emitVol(j.t); saveTr(); cur = j.t; }
        bc(o, ws);
      }
    } catch (e) { console.error('Mensaje:', e.message); }
  });
});

// ---- Controlador fisico (cualquier entrada MIDI) ----
const step = d => { for (let k = 1; k <= N; k++) { const i = (((cur + d * k) % N) + N) % N; if (S[i].on) { cur = i; return; } } };
const keyOf = m => { const s = m[0] & 0xF0; return s === 0xB0 ? 'cc' + m[1] : (s === 0x90 && m[2] > 0 ? 'n' + m[1] : null); };
function onMidi(m) {
  const k = keyOf(m); if (!k) return;
  console.log('Entrada:', m.join(' '), '->', k);
  if (learning) {
    if (learning === 'vol' && k[0] !== 'c') return;
    map[learning] = k; fs.writeFileSync(MAPF, JSON.stringify(map));
    bc({ learned: learning + ' = ' + k }); learning = null; return;
  }
  const pressed = k[0] === 'n' || m[2] > 0;
  if (k === map.prev && pressed) { step(-1); bc({ sel: cur }); }
  else if (k === map.next && pressed) { step(1); bc({ sel: cur }); }
  else if (k === map.vol) { S[cur].v = m[2] & 127; emitVol(cur); bc({ t: cur, v: S[cur].v }); }
}
const probe = new midi.Input();
for (let i = 0; i < probe.getPortCount(); i++) {
  const n = probe.getPortName(i); if (/REAL360/i.test(n)) continue;
  const p = new midi.Input(); p.ignoreTypes(true, true, true);
  p.on('message', (dt, m) => onMidi(m)); p.openPort(i); console.log('Controlador detectado:', n);
}

srv.listen(PORT, () => {
  console.log('POLAR listo.\n  Mac:    http://localhost:' + PORT);
  for (const l of Object.values(os.networkInterfaces()))
    for (const i of l) if (i.family === 'IPv4' && !i.internal) console.log('  iPhone: http://' + i.address + ':' + PORT);
  if (!process.argv.includes('--no-open')) exec('open http://localhost:' + PORT);
});
