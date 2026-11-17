// POLAR - app de Mac (Electron). Admite actualizaciones sin reinstalar:
// menu REAL360 > "Instalar actualizacion..." (Cmd+U).
const { app, BrowserWindow, Menu, dialog } = require('electron');
const fs = require('fs'), path = require('path'), Module = require('module');
const { execFileSync } = require('child_process');

app.setPath('userData', path.join(app.getPath('appData'), 'REAL360')); // se mantiene la carpeta de datos anterior
process.env.PANNER_DATA = app.getPath('userData');
process.env.PANNER_ASSETS = __dirname;
process.argv.push('--no-open');
const UPD = path.join(app.getPath('userData'), 'update');
const BASE = path.join(UPD, 'base.txt');

function loadExt(file) { // ejecuta un server.js externo usando los modulos de la app
  const m = new Module(file, module);
  m.filename = file; m.paths = module.paths;
  m._compile(fs.readFileSync(file, 'utf8'), file);
}
function startServer() {
  const ok = fs.existsSync(BASE) && fs.readFileSync(BASE, 'utf8') === app.getVersion();
  const web = path.join(UPD, 'index.html'), srv = path.join(UPD, 'server.js');
  if (ok && fs.existsSync(web)) process.env.PANNER_WEB = web;
  if (ok && fs.existsSync(srv)) {
    try { return loadExt(srv); } catch (e) {
      console.error('Actualizacion fallida, uso la version original', e);
      try { fs.writeFileSync(path.join(app.getPath('userData'), 'update-error.txt'), String(e.stack || e)); } catch (_) {}
      dialog.showErrorBox('POLAR', 'La actualización no se pudo cargar y se usa la versión original: ' + (e.message || e));
    }
  }
  require('./server.js');
}
function collect(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) collect(p, out);
    else if (e.name === 'index.html' || e.name === 'server.js') out.push(p);
  }
  return out;
}
async function installUpdate() {
  const r = await dialog.showOpenDialog({
    title: 'Elige el archivo de actualizacion (zip, o index.html y server.js)',
    properties: ['openFile', 'multiSelections'],
    filters: [{ name: 'Actualizacion', extensions: ['zip', 'html', 'js'] }]
  });
  if (r.canceled || !r.filePaths.length) return;
  try {
    const files = [];
    for (const f of r.filePaths) {
      if (f.toLowerCase().endsWith('.zip')) {
        const tmp = fs.mkdtempSync(path.join(app.getPath('temp'), 'real360-'));
        execFileSync('/usr/bin/unzip', ['-o', '-q', f, '-d', tmp]);
        files.push(...collect(tmp));
      } else if (['index.html', 'server.js'].includes(path.basename(f))) files.push(f);
    }
    if (!files.length) throw new Error('No he encontrado index.html ni server.js en lo que has elegido.');
    fs.mkdirSync(UPD, { recursive: true });
    for (const f of files) fs.copyFileSync(f, path.join(UPD, path.basename(f)));
    fs.writeFileSync(BASE, app.getVersion());
  } catch (e) { dialog.showErrorBox('POLAR', String(e.message || e)); return; }
  app.relaunch(); app.exit(0);
}
function resetUpdate() { fs.rmSync(UPD, { recursive: true, force: true }); app.relaunch(); app.exit(0); }

let sesWin = null;
function openSes(focus) { // ventana de sesiones (guardar, cargar, renombrar, duplicar, borrar)
  if (sesWin && !sesWin.isDestroyed()) { sesWin.focus(); return; }
  sesWin = new BrowserWindow({ width: 560, height: 740, minWidth: 420, backgroundColor: '#07090c', title: 'Sesiones · POLAR' });
  sesWin.loadURL('http://localhost:8080/?ses=1' + (focus === 'name' ? '&focus=name' : ''));
  sesWin.on('closed', () => { sesWin = null; });
}
function runInPage(js) { const w = BrowserWindow.getFocusedWindow() || BrowserWindow.getAllWindows()[0]; if (w) w.webContents.executeJavaScript(js).catch(() => {}); }
const SESION_MENU = { label: 'Sesión', submenu: [
  { label: 'Gestionar sesiones…', accelerator: 'CmdOrCtrl+Shift+O', click: () => openSes() },
  { type: 'separator' },
  { label: 'Guardar sesión', accelerator: 'CmdOrCtrl+S', click: () => runInPage("typeof saveSes==='function'&&saveSes()") },
  { label: 'Guardar como…', accelerator: 'CmdOrCtrl+Shift+S', click: () => openSes('name') }
] };
function create() {
  const win = new BrowserWindow({ width: 1280, height: 840, minWidth: 700, backgroundColor: '#07090c', title: 'POLAR' });
  const load = () => win.loadURL('http://localhost:8080');
  win.webContents.on('did-fail-load', () => setTimeout(load, 300));
  load();
}
app.whenReady().then(() => {
  startServer();
  Menu.setApplicationMenu(Menu.buildFromTemplate([
    { label: 'POLAR', submenu: [
      { role: 'about' }, { type: 'separator' },
      { label: 'Instalar actualización…', accelerator: 'CmdOrCtrl+U', click: installUpdate },
      { label: 'Volver a la versión original', click: resetUpdate }, { type: 'separator' },
      { role: 'hide' }, { role: 'hideOthers' }, { role: 'unhide' }, { type: 'separator' }, { role: 'quit' } ] },
    SESION_MENU, { role: 'editMenu' }, { role: 'viewMenu' }, { role: 'windowMenu' }
  ]));
  create();
});
app.on('window-all-closed', () => app.quit());
