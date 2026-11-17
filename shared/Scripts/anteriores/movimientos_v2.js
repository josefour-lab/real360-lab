// =====================================================================
//  LOOPER POLAR · Movimientos · REAL360
//  Modos de movimiento del polar.
//
//  Este archivo se recarga solo cada vez que lo guardas, con Logic abierto
//  y sonando. No hace falta reinstalar ni reiniciar nada.
//  Si hay un error, Looper Polar sigue con la versión anterior y te lo muestra.
//
//  Cada modo recibe:
//    p.id         0 = guitarra en directo, 1..8 = capas
//    p.angulo     grados (0 = frente, + derecha, ±180 = detrás)
//    p.radio      0 centro … 1 borde
//    p.x, p.y     la misma posición en ejes X/Y (-1…1, y positivo = frente)
//    p.apertura   separación entre L y R en grados
//    p.velocidad  el control «Velocidad» de la interfaz
//    p.altavoces  4 si la pista es cuadrafónica, 2 si es estéreo
//                 (si el plugin no lo dice, se asume 4)
//    dt           segundos desde la última llamada (~0,016)
//    t            segundos desde que arrancó el plugin
//
//  Y devuelve { angulo, radio } o { x, y }; opcionalmente también { apertura }.
//  Para añadir un modo, añade una entrada a «modos»: aparecerá en el menú.
// =====================================================================

var estado = {};

function mem(p) {
  if (!estado[p.id]) estado[p.id] = {};
  return estado[p.id];
}
function azar(a, b) { return a + Math.random() * (b - a); }
function limitar(v, a, b) { return Math.max(a, Math.min(b, v)); }
function dist(x1, y1, x2, y2) { return Math.sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)); }

// Avanza desde la posición actual hacia (tx, ty) como máximo «paso» unidades
function acercar(p, tx, ty, paso) {
  var d = dist(p.x, p.y, tx, ty);
  if (d <= paso || d === 0) return { x: tx, y: ty };
  return { x: p.x + (tx - p.x) / d * paso, y: p.y + (ty - p.y) / d * paso };
}

// Velocidad de la interfaz (°/s) pasada a unidades del círculo por segundo
function rapidez(p) { return Math.abs(p.velocidad) / 90; }

// L, R, Rs, Ls en ejes X/Y
var ALTAVOCES = [
  { x: -0.707, y: 0.707 },
  { x: 0.707, y: 0.707 },
  { x: 0.707, y: -0.707 },
  { x: -0.707, y: -0.707 }
];

// Los altavoces que hay de verdad: en estéreo solo izquierda y derecha
// (las dos esquinas de arriba del pad, donde están IZQUIERDA y DERECHA).
function altavoces(p) {
  return p.altavoces === 2 ? [ALTAVOCES[0], ALTAVOCES[1]] : ALTAVOCES;
}

var modos = {

  // Gira alrededor del centro a la distancia actual. Velocidad negativa = al revés.
  "Órbita": function (p, dt) {
    return { angulo: p.angulo + p.velocidad * dt, radio: p.radio };
  },

  // Deambula por todo el círculo hacia destinos al azar, sin saltos.
  "Paseo aleatorio": function (p, dt) {
    var m = mem(p);
    if (m.tx === undefined || dist(p.x, p.y, m.tx, m.ty) < 0.03) {
      var a = azar(0, 2 * Math.PI), r = Math.sqrt(azar(0.1, 1));
      m.tx = Math.sin(a) * r;
      m.ty = Math.cos(a) * r;
    }
    return acercar(p, m.tx, m.ty, rapidez(p) * dt);
  },

  // Paseo aleatorio que se queda cerca del borde (más sensación de recorrer la sala).
  "Paseo por el borde": function (p, dt) {
    var m = mem(p);
    var dif = m.ta === undefined ? 0 : ((m.ta - p.angulo) % 360 + 540) % 360 - 180;
    if (m.ta === undefined || Math.abs(dif) < 2) {
      m.ta = p.angulo + azar(-160, 160);
      dif = ((m.ta - p.angulo) % 360 + 540) % 360 - 180;
    }
    var paso = Math.abs(p.velocidad) * dt;
    var ang = p.angulo + Math.sign(dif) * Math.min(Math.abs(dif), paso);
    return { angulo: ang, radio: p.radio + (azar(0.8, 1) - p.radio) * Math.min(1, dt) };
  },

  // Viaja de altavoz en altavoz al azar, deslizándose entre ellos.
  // En estéreo va de izquierda a derecha y vuelta; en cuadrafónico, por los cuatro.
  "Paseo por altavoces": function (p, dt) {
    var m = mem(p);
    var lista = altavoces(p);
    if (m.destino === undefined || m.destino >= lista.length)   // también si la pista pasa de 4 a 2
      m.destino = Math.floor(azar(0, lista.length));
    var s = lista[m.destino];
    if (dist(p.x, p.y, s.x, s.y) < 0.03) {
      if (m.espera === undefined) m.espera = azar(0.3, 1.5);   // se queda un momento en el altavoz
      m.espera -= dt;
      if (m.espera > 0) return { x: s.x, y: s.y };
      m.espera = undefined;
      var n;
      do { n = Math.floor(azar(0, lista.length)); } while (n === m.destino);
      m.destino = n;
      s = lista[n];
    }
    return acercar(p, s.x, s.y, rapidez(p) * dt);
  },

  // Rebota por las paredes del círculo como una bola de billar.
  "Rebote": function (p, dt) {
    var m = mem(p);
    if (m.vx === undefined) { var a = azar(0, 2 * Math.PI); m.vx = Math.sin(a); m.vy = Math.cos(a); }
    var v = rapidez(p);
    var x = p.x + m.vx * v * dt, y = p.y + m.vy * v * dt;
    var r = Math.sqrt(x * x + y * y);
    if (r > 0.98) {
      var nx = x / r, ny = y / r;                 // normal en el punto de choque
      var dot = m.vx * nx + m.vy * ny;
      m.vx -= 2 * dot * nx;
      m.vy -= 2 * dot * ny;
      x = nx * 0.98; y = ny * 0.98;
    }
    return { x: x, y: y };
  },

  // Dibuja un ocho de delante a atrás.
  "Ocho": function (p, dt) {
    var m = mem(p);
    if (m.fase === undefined) m.fase = 0;
    m.fase += p.velocidad * dt * Math.PI / 180;
    return { x: 0.85 * Math.sin(m.fase) * Math.cos(m.fase) * 2, y: 0.85 * Math.sin(m.fase) };
  },

  // Va y viene a izquierda y derecha alrededor de donde lo dejaste.
  "Péndulo": function (p, dt) {
    var m = mem(p);
    if (m.centro === undefined) { m.centro = p.angulo; m.fase = 0; }
    m.fase += Math.abs(p.velocidad) * dt * Math.PI / 180;
    return { angulo: m.centro + 60 * Math.sin(m.fase), radio: p.radio };
  },

  // Se acerca y se aleja del centro sin cambiar de dirección; la apertura respira con él.
  "Respiración": function (p, dt) {
    var m = mem(p);
    if (m.fase === undefined) { m.fase = 0; m.ap = p.apertura; }
    m.fase += Math.abs(p.velocidad) * dt * Math.PI / 180;
    var s = Math.sin(m.fase);
    return { angulo: p.angulo, radio: 0.55 + 0.4 * s, apertura: limitar(m.ap + 40 * s, 0, 180) };
  }
};

// ---- Llamadas desde Looper Polar (no hace falta tocarlas) ----

function nombresDeModos() { return Object.keys(modos); }

function mover(modo, p, dt, t) {
  var m = mem(p);
  if (m.modo !== modo) estado[p.id] = { modo: modo };   // al cambiar de modo, empieza de cero
  var f = modos[modo];
  return f ? f(p, dt, t) : null;
}
