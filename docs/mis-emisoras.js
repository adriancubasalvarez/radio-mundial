// «My stations / Mis emisoras»: the user's own stations, added from the
// install page and sent to the radio over USB (Web Serial).
//
// Protocol (see src/hal/esp32/mis_emisoras_usb.cpp), one line each way:
//   RM?             -> "RM=name\turl\tcodec\tkbps" per station, then "RM."
//   RM-             -> "RM>0"     start a new list
//   RM+n\tu\tc\tb   -> "RM>k"     add a station
//   RM!             -> "RMOK k"   save it (or "RMERR ...")
// Station search: radio-browser.info, straight from the browser (no keys).

const MAX = 20;
const SERVIDORES = ["de1.api.radio-browser.info", "fi1.api.radio-browser.info", "at1.api.radio-browser.info"];
const CODECS = new Set(["MP3", "AAC", "AAC+"]);
const KBPS_MAX = 192;  // same limit as the station database

const TEXTOS = {
  en: {
    conectar: "Connect to the radio",
    conectando: "Connecting… pick the radio's port in the browser window.",
    esperando: "Waiting for the radio to answer…",
    conectada: "Connected. These are the stations saved on your radio:",
    sinRespuesta: "The radio doesn't answer. Install the latest version above and try again.",
    ocupado: "The port is busy. Close the install window (or other programs using the radio) and try again.",
    noSoportado: "Your browser can't talk to the radio: use Chrome or Edge on a computer.",
    vacia: "No stations yet. Search below or paste a URL.",
    buscar: "Search by name (e.g. jazz, BBC, Radio Nacional)",
    botonBuscar: "Search",
    buscando: "Searching…",
    sinResultados: "No stations found that the radio can play.",
    errorBusqueda: "The station search isn't available right now. Try again later.",
    escuchar: "▶ Listen",
    parar: "■ Stop",
    anadir: "+ Add",
    quitar: "Remove",
    nombre: "Name",
    url: "Stream URL (http:// or https://)",
    anadirUrl: "Add URL",
    guardar: "Save to the radio",
    guardando: "Saving…",
    guardado: (n) => `Saved: ${n} station${n === 1 ? "" : "s"}. On the radio: Search › My stations.`,
    errorGuardar: "Couldn't save on the radio. Check the cable and try again.",
    llena: `The list is full (${MAX} stations).`,
    repetida: "That station is already in your list.",
    urlMala: "The URL must start with http:// or https:// (and be shorter than 256 characters).",
    nota: "The preview may not work for some http:// stations in the browser; the radio plays them anyway.",
    cambios: "Unsaved changes",
  },
  es: {
    conectar: "Conectar con la radio",
    conectando: "Conectando… elige el puerto de la radio en la ventana del navegador.",
    esperando: "Esperando a que responda la radio…",
    conectada: "Conectada. Estas son las emisoras guardadas en tu radio:",
    sinRespuesta: "La radio no responde. Instala arriba la última versión y prueba otra vez.",
    ocupado: "El puerto está ocupado. Cierra la ventana de instalación (u otros programas que usen la radio) y prueba otra vez.",
    noSoportado: "Tu navegador no puede hablar con la radio: usa Chrome o Edge en un ordenador.",
    vacia: "Aún no hay emisoras. Busca abajo o pega una URL.",
    buscar: "Busca por nombre (p. ej. jazz, BBC, Radio Nacional)",
    botonBuscar: "Buscar",
    buscando: "Buscando…",
    sinResultados: "No hay emisoras que la radio pueda reproducir.",
    errorBusqueda: "El buscador de emisoras no está disponible ahora. Prueba más tarde.",
    escuchar: "▶ Escuchar",
    parar: "■ Parar",
    anadir: "+ Añadir",
    quitar: "Quitar",
    nombre: "Nombre",
    url: "URL del stream (http:// o https://)",
    anadirUrl: "Añadir URL",
    guardar: "Guardar en la radio",
    guardando: "Guardando…",
    guardado: (n) => `Guardado: ${n} emisora${n === 1 ? "" : "s"}. En la radio: Buscar › Mis emisoras.`,
    errorGuardar: "No se pudo guardar en la radio. Revisa el cable y prueba otra vez.",
    llena: `La lista está llena (${MAX} emisoras).`,
    repetida: "Esa emisora ya está en tu lista.",
    urlMala: "La URL debe empezar por http:// o https:// (y tener menos de 256 caracteres).",
    nota: "En el navegador puede que no se oigan algunas emisoras http://; la radio las reproduce igual.",
    cambios: "Cambios sin guardar",
  },
};
const T = TEXTOS[document.documentElement.lang === "es" ? "es" : "en"];

const $ = (id) => document.getElementById(id);
let puerto = null, escritor = null, lineas = [], esperas = [];
let lista = [];        // [{n, u, c, b}]
let cambios = false;
const audio = new Audio();
let sonando = null;    // botón ▶ que está sonando

// ---------- Texts ----------

function limpiarNombre(texto) {
  // No tabs or line breaks (they split the protocol) and at most 60 bytes
  let n = (texto || "").replace(/[\t\r\n]+/g, " ").replace(/\s+/g, " ").trim();
  const cod = new TextEncoder();
  while (cod.encode(n).length > 60) n = n.slice(0, -1);
  return n || "?";
}

function urlValida(u) {
  return /^https?:\/\/\S+$/i.test(u) && new TextEncoder().encode(u).length <= 255;
}

function estado(texto, error = false) {
  const e = $("me-estado");
  e.textContent = texto;
  e.classList.toggle("aviso", error);
}

// ---------- Serial port ----------

async function leerSiempre() {
  const lector = puerto.readable.pipeThrough(new TextDecoderStream()).getReader();
  let resto = "";
  try {
    for (;;) {
      const { value, done } = await lector.read();
      if (done) break;
      resto += value;
      let i;
      while ((i = resto.indexOf("\n")) >= 0) {
        const linea = resto.slice(0, i).replace(/\r$/, "");
        resto = resto.slice(i + 1);
        if (!linea.startsWith("RM")) continue;  // the radio's normal log
        lineas.push(linea);
        esperas.splice(0).forEach((f) => f());
      }
    }
  } catch (e) {
    // cable unplugged: the next command will fail and say so
  }
}

function enviar(texto) {
  return escritor.write(new TextEncoder().encode(texto + "\n"));
}

// Waits for a reply line that passes `vale`, up to `ms` milliseconds
async function esperar(vale, ms) {
  const fin = Date.now() + ms;
  for (;;) {
    const i = lineas.findIndex(vale);
    if (i >= 0) return lineas.splice(0, i + 1).pop();
    const queda = fin - Date.now();
    if (queda <= 0) return null;
    await new Promise((r) => { esperas.push(r); setTimeout(r, Math.min(queda, 250)); });
  }
}

async function conectar() {
  if (!("serial" in navigator)) {
    estado(T.noSoportado, true);
    return;
  }
  try {
    estado(T.conectando);
    puerto = await navigator.serial.requestPort();
    await puerto.open({ baudRate: 115200 });
  } catch (e) {
    puerto = null;
    estado(e && e.name === "InvalidStateError" ? T.ocupado : "", !!(e && e.name === "InvalidStateError"));
    return;
  }
  escritor = puerto.writable.getWriter();
  leerSiempre();
  estado(T.esperando);
  const leida = await leerLista();
  if (leida === null) {
    estado(T.sinRespuesta, true);
    return;
  }
  lista = leida;
  cambios = false;
  $("me-conectar").hidden = true;
  $("me-editor").hidden = false;
  estado(T.conectada);
  pintarLista();
}

// Asks for the list and collects every "RM=" line until "RM."
async function leerLista() {
  for (let intento = 0; intento < 20; intento++) {
    lineas = [];
    await enviar("RM?");
    const fin = Date.now() + 1000;
    const recogidas = [];
    for (;;) {
      const l = await esperar(() => true, fin - Date.now());
      if (l === null) break;
      if (l === "RM.") {
        return recogidas.map((x) => {
          const [n, u, c, b] = x.slice(3).split("\t");
          return { n, u, c: c || "", b: Number(b) || 0 };
        });
      }
      if (l.startsWith("RM=")) recogidas.push(l);
    }
  }
  return null;
}

async function guardar() {
  const boton = $("me-guardar");
  boton.disabled = true;
  estado(T.guardando);
  try {
    lineas = [];
    await enviar("RM-");
    if (!(await esperar((l) => l.startsWith("RM>") || l.startsWith("RMERR"), 3000))?.startsWith("RM>")) throw 0;
    for (const e of lista) {
      await enviar(`RM+${e.n}\t${e.u}\t${e.c || ""}\t${e.b || 0}`);
      const r = await esperar((l) => l.startsWith("RM>") || l.startsWith("RMERR"), 3000);
      if (!r || !r.startsWith("RM>")) throw 0;
    }
    await enviar("RM!");
    const r = await esperar((l) => l.startsWith("RMOK") || l.startsWith("RMERR"), 5000);
    if (!r || !r.startsWith("RMOK")) throw 0;
    cambios = false;
    estado(T.guardado(lista.length));
  } catch (e) {
    estado(T.errorGuardar, true);
  }
  boton.disabled = false;
  pintarLista();
}

// ---------- List ----------

function botonEscuchar(url) {
  const b = document.createElement("button");
  b.type = "button";
  b.className = "me-pequeno";
  b.textContent = T.escuchar;
  b.addEventListener("click", () => {
    if (sonando === b) {
      audio.pause();
      b.textContent = T.escuchar;
      sonando = null;
      return;
    }
    if (sonando) sonando.textContent = T.escuchar;
    audio.src = url;
    audio.play().catch(() => {});
    b.textContent = T.parar;
    sonando = b;
  });
  return b;
}

function detalle(e) {
  return [e.c, e.b ? `${e.b} kbps` : "", e.pais].filter(Boolean).join(" · ");
}

function pintarLista() {
  const ul = $("me-lista");
  ul.innerHTML = "";
  if (lista.length === 0) {
    const li = document.createElement("li");
    li.className = "me-vacia";
    li.textContent = T.vacia;
    ul.append(li);
  }
  lista.forEach((e, i) => {
    const li = document.createElement("li");
    const texto = document.createElement("div");
    texto.innerHTML = "<strong></strong><span></span>";
    texto.querySelector("strong").textContent = e.n;
    texto.querySelector("span").textContent = detalle(e) || e.u;
    const quitar = document.createElement("button");
    quitar.type = "button";
    quitar.className = "me-pequeno";
    quitar.textContent = T.quitar;
    quitar.addEventListener("click", () => {
      lista.splice(i, 1);
      cambios = true;
      pintarLista();
    });
    li.append(texto, botonEscuchar(e.u), quitar);
    ul.append(li);
  });
  $("me-contador").textContent = `${lista.length} / ${MAX}` + (cambios ? ` · ${T.cambios}` : "");
  $("me-guardar").classList.toggle("me-pendiente", cambios);
}

function anadir(e) {
  if (lista.length >= MAX) {
    estado(T.llena, true);
    return false;
  }
  if (lista.some((x) => x.u === e.u)) {
    estado(T.repetida, true);
    return false;
  }
  lista.push({ n: limpiarNombre(e.n), u: e.u, c: e.c || "", b: e.b || 0 });
  cambios = true;
  estado("");
  pintarLista();
  return true;
}

// ---------- Search (radio-browser.info) ----------

async function buscar(ev) {
  ev.preventDefault();
  const texto = $("me-texto").value.trim();
  if (texto.length < 2) return;
  const ul = $("me-resultados");
  ul.innerHTML = `<li class="me-vacia">${T.buscando}</li>`;
  let datos = null;
  for (const servidor of SERVIDORES) {
    try {
      const r = await fetch(`https://${servidor}/json/stations/search?name=${encodeURIComponent(texto)}` +
                            "&limit=60&hidebroken=true&order=votes&reverse=true");
      if (r.ok) { datos = await r.json(); break; }
    } catch (e) { /* next server */ }
  }
  ul.innerHTML = "";
  if (datos === null) {
    ul.innerHTML = `<li class="me-vacia aviso">${T.errorBusqueda}</li>`;
    return;
  }
  const vistas = new Set();
  const validas = datos.filter((s) => {
    const u = s.url_resolved || s.url;
    const codec = (s.codec || "").toUpperCase();
    if (!CODECS.has(codec) || s.hls || s.bitrate > KBPS_MAX || !urlValida(u) || vistas.has(u)) return false;
    vistas.add(u);
    return true;
  }).slice(0, 25);
  if (validas.length === 0) {
    ul.innerHTML = `<li class="me-vacia">${T.sinResultados}</li>`;
    return;
  }
  for (const s of validas) {
    const e = { n: s.name.trim(), u: s.url_resolved || s.url, c: (s.codec || "").toUpperCase(),
                b: s.bitrate || 0, pais: s.countrycode || "" };
    const li = document.createElement("li");
    const textoLi = document.createElement("div");
    textoLi.innerHTML = "<strong></strong><span></span>";
    textoLi.querySelector("strong").textContent = e.n;
    textoLi.querySelector("span").textContent = detalle(e);
    const mas = document.createElement("button");
    mas.type = "button";
    mas.className = "me-pequeno me-anadir";
    mas.textContent = T.anadir;
    mas.addEventListener("click", () => { if (anadir(e)) mas.disabled = true; });
    li.append(textoLi, botonEscuchar(e.u), mas);
    ul.append(li);
  }
}

function anadirUrl(ev) {
  ev.preventDefault();
  const u = $("me-url").value.trim();
  if (!urlValida(u)) {
    estado(T.urlMala, true);
    return;
  }
  let n = $("me-nombre").value.trim();
  if (!n) { try { n = new URL(u).hostname; } catch (e) { n = u; } }
  if (anadir({ n, u })) {
    $("me-url").value = "";
    $("me-nombre").value = "";
  }
}

// ---------- Start ----------

$("me-conectar").textContent = T.conectar;
$("me-conectar").addEventListener("click", conectar);
$("me-texto").placeholder = T.buscar;
$("me-boton-buscar").textContent = T.botonBuscar;
$("me-buscar").addEventListener("submit", buscar);
$("me-nombre").placeholder = T.nombre;
$("me-url").placeholder = T.url;
$("me-boton-url").textContent = T.anadirUrl;
$("me-manual").addEventListener("submit", anadirUrl);
$("me-guardar").textContent = T.guardar;
$("me-guardar").addEventListener("click", guardar);
$("me-nota").textContent = T.nota;
if (!("serial" in navigator)) estado(T.noSoportado, true);
window.addEventListener("beforeunload", (ev) => { if (cambios) ev.preventDefault(); });
