#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

struct DatosRCP {
  long fuerza = 0;
  int16_t aceleracionZ = 0;
  int compresiones = 0;
  float ritmo = 0;
  bool malaPostura = false;
  bool centroActivo = false;
};
DatosRCP datosRCP;

const char PAGINA_RCP[] PROGMEM = R"HTML(
<!DOCTYPE html><html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Simulador RCP</title><style>
*{box-sizing:border-box}body{font-family:system-ui,Arial;background:#0b1220;color:#e9f0fa;margin:0;padding:18px}main{max-width:900px;margin:auto}h1{font-size:1.6rem;margin-bottom:4px}.muted{color:#a4b2c9}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:20px 0}.card{background:#172339;border:1px solid #293a53;border-radius:12px;padding:17px}.value{font-size:2rem;font-weight:750;margin:8px 0}.ok{color:#4ade80}.bad{color:#fb7185}canvas{width:100%;height:220px;display:block}a{color:#93c5fd}
</style></head><body><main><h1>Simulador RCP</h1><div class="muted">Monitoreo local de sensores · <span id="conexion">Conectando...</span></div>
<div class="grid"><div class="card">Fuerza (cuentas HX711)<div class="value" id="fuerza">--</div></div><div class="card">Aceleración Z (cuentas)<div class="value" id="acel">--</div></div><div class="card">Compresiones<div class="value" id="comp">--</div></div><div class="card">Ritmo (comp/min)<div class="value" id="ritmo">--</div></div><div class="card">Postura<div class="value" id="postura">--</div></div><div class="card">Contacto central<div class="value" id="centro">--</div></div></div>
<div class="card"><strong>Historial de fuerza (últimas 100 muestras)</strong><canvas id="grafico" width="800" height="220"></canvas></div><p class="muted">Las lecturas de fuerza y aceleración son valores sin calibrar. No representan kgf ni profundidad de compresión.</p><p><a href="/">Configuración WiFi</a></p></main>
<script>
const $=id=>document.getElementById(id),hist=[];const canvas=$('grafico'),ctx=canvas.getContext('2d');
function dibujar(){const w=canvas.width,h=canvas.height;ctx.clearRect(0,0,w,h);ctx.strokeStyle='#41536b';ctx.beginPath();ctx.moveTo(0,h/2);ctx.lineTo(w,h/2);ctx.stroke();if(hist.length<2)return;let max=Math.max(1,...hist.map(x=>Math.abs(x)));ctx.strokeStyle='#38bdf8';ctx.lineWidth=2;ctx.beginPath();hist.forEach((v,i)=>{let x=i*(w/(99)),y=h/2-(v/max)*(h*.43);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y)});ctx.stroke()}
async function actualizar(){try{const r=await fetch('/datos',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const d=await r.json();$('conexion').textContent='Conectado';$('conexion').className='ok';$('fuerza').textContent=d.fuerza;$('acel').textContent=d.aceleracionZ;$('comp').textContent=d.compresiones;$('ritmo').textContent=d.ritmo.toFixed(1);$('postura').textContent=d.malaPostura?'Incorrecta':'Sin contacto lateral';$('postura').className='value '+(d.malaPostura?'bad':'ok');$('centro').textContent=d.centroActivo?'Detectado':'No detectado';hist.push(d.fuerza);if(hist.length>100)hist.shift();dibujar()}catch(e){$('conexion').textContent='Sin conexión';$('conexion').className='bad'}}
actualizar();setInterval(actualizar,200);
</script></body></html>
)HTML";

void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width,initial-scale=1'></head><body>";
  html += "<h2>Configuracion WiFi</h2><a href='/dashboard'>Abrir dashboard RCP</a><hr>";
  html += "<form method='POST' action='/wifi'>";
  html += "Red Wi-Fi: <input type='text' name='ssid'><br>";
  html += "Contrasena: <input type='password' name='password'><br>";
  html += "<input type='submit' value='Conectar'></form></body></html>";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleWifi() {
  String ssid = server.arg("ssid"), password = server.arg("password");
  if(ssid.isEmpty()){server.send(400,"text/plain","Falta SSID");return;}
  // Conservar el AP mientras se intenta conectar a otra red.
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long inicio=millis();
  while(WiFi.status()!=WL_CONNECTED && millis()-inicio<8000){delay(100);}
  if(WiFi.status()==WL_CONNECTED){
    Serial.print("IP en red externa: ");Serial.println(WiFi.localIP());
    server.send(200,"text/plain","Conexion establecida. Dashboard: http://"+WiFi.localIP().toString()+"/dashboard");
  }else{server.send(200,"text/plain","No se establecio conexion. El AP local sigue disponible.");}
}

void handleDatos() {
  String json="{";
  json+="\"fuerza\":"+String(datosRCP.fuerza)+",";
  json+="\"aceleracionZ\":"+String(datosRCP.aceleracionZ)+",";
  json+="\"compresiones\":"+String(datosRCP.compresiones)+",";
  json+="\"ritmo\":"+String(datosRCP.ritmo,1)+",";
  json+="\"malaPostura\":"+String(datosRCP.malaPostura?"true":"false")+",";
  json+="\"centroActivo\":"+String(datosRCP.centroActivo?"true":"false")+"}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",json);
}

void initAP(const char* apSsid,const char* apPassword) {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid,apPassword);
  server.on("/",HTTP_GET,handleRoot);
  server.on("/wifi",HTTP_POST,handleWifi);
  server.on("/dashboard",HTTP_GET,[](){server.send_P(200,"text/html; charset=utf-8",PAGINA_RCP);});
  server.on("/datos",HTTP_GET,handleDatos);
  server.begin();
  Serial.print("IP del AP: ");Serial.println(WiFi.softAPIP());
  Serial.println("Dashboard: http://192.168.4.1/dashboard");
}
void loopAP(){server.handleClient();}
