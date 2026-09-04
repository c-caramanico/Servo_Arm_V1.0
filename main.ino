#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_PWMServoDriver.h>
#include <Adafruit_I2CDevice.h>
#include <Preferences.h>
// --- WiFi Credentials ---

// Put wrt what ever your WiFi SSID and password are in the quotes below. This is the network that the ESP32 will connect to, 
//and you will use this same network to connect to the ESP32 from your computer or phone.
const char* ssid = "YourWiFiSSID"; 
const char* password = "YouRWifiPassword";

// --- Server & Hardware Setup ---
WebServer server(80);
Adafruit_PWMServoDriver board1 = Adafruit_PWMServoDriver(0x40);
Preferences prefs;

#define SERVOMIN  125 // Min pulse (0 degrees)
#define SERVOMAX  625 // Max pulse (180 degrees)
#define NUM_SERVOS 4

const int DEFAULT_SAFE_ANGLE = 90; // Fallback for brand-new flash

int currentAngle[NUM_SERVOS]; // In-memory mirror of each servo's current angle

// --- HTML & JavaScript (The Webpage) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Servo Control</title>
<style>
:root{
  --bg:#0d0e12;
  --surface:#16181d;
  --border:#262930;
  --text:#f4f4f5;
  --text-dim:#8b8d94;
  --accent:#00E599;
  --accent-dim:rgba(0,229,153,0.15);
}
*{box-sizing:border-box;}
body{margin:0;padding:32px 20px 60px;background:var(--bg);color:var(--text);font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;}
.wrap{max-width:520px;margin:0 auto;}
.top{display:flex;align-items:center;justify-content:space-between;margin-bottom:28px;}
h1{font-size:17px;font-weight:600;letter-spacing:-.01em;margin:0;}
.status{display:flex;align-items:center;gap:6px;font-size:12px;color:var(--text-dim);}
.dot{width:7px;height:7px;border-radius:50%;background:var(--accent);box-shadow:0 0 8px var(--accent);animation:pulse 2s ease-in-out infinite;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:.4;}}
.grid{display:grid;grid-template-columns:1fr;gap:12px;}
@media(min-width:480px){.grid{grid-template-columns:1fr 1fr;}}
.card{background:var(--surface);border:1px solid var(--border);border-radius:14px;padding:18px 18px 20px;transition:border-color .15s ease;}
.card:focus-within{border-color:var(--accent);}
.card-head{display:flex;align-items:baseline;justify-content:space-between;margin-bottom:16px;}
.joint{font-size:14px;font-weight:600;color:var(--text);}
.pin{display:block;font-size:11px;font-weight:400;color:var(--text-dim);margin-top:2px;}
.badge{font-family:'JetBrains Mono','SF Mono',Consolas,'Roboto Mono',monospace;font-size:13px;font-variant-numeric:tabular-nums;color:var(--accent);background:var(--accent-dim);border:1px solid rgba(0,229,153,0.25);padding:3px 8px;border-radius:6px;}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:5px;border-radius:3px;background:var(--border);outline:none;cursor:pointer;}
input[type=range]::-webkit-slider-runnable-track{-webkit-appearance:none;height:5px;border-radius:3px;background:transparent;}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:16px;height:16px;border-radius:50%;background:var(--accent);border:2px solid var(--bg);margin-top:-5.5px;box-shadow:0 0 0 0 rgba(0,229,153,0);transition:box-shadow .15s ease,transform .15s ease;}
input[type=range]:active::-webkit-slider-thumb,input[type=range]:focus::-webkit-slider-thumb{transform:scale(1.2);box-shadow:0 0 0 6px var(--accent-dim),0 0 14px rgba(0,229,153,.6);}
input[type=range]::-moz-range-track{height:5px;border-radius:3px;background:var(--border);}
input[type=range]::-moz-range-progress{height:5px;border-radius:3px;background:var(--accent);}
input[type=range]::-moz-range-thumb{width:16px;height:16px;border-radius:50%;background:var(--accent);border:2px solid var(--bg);transition:box-shadow .15s ease,transform .15s ease;}
input[type=range]:active::-moz-range-thumb{transform:scale(1.2);box-shadow:0 0 14px rgba(0,229,153,.6);}
</style>
</head>
<body>
<div class="wrap">
  <div class="top">
    <h1>Servo Control</h1>
    <div class="status"><span class="dot"></span>ONLINE</div>
  </div>
  <div class="grid">
    <div class="card">
      <div class="card-head">
        <span class="joint">Base<span class="pin">PIN 0</span></span>
        <span class="badge" id="val0">090deg;</span>
      </div>
      <input id="slider0" type="range" min="0" max="180" value="90" oninput="updateServo(0,this.value)">
    </div>
    <div class="card">
      <div class="card-head">
        <span class="joint">Shouder<span class="pin">PIN 1</span></span>
        <span class="badge" id="val1">090deg;</span>
      </div>
      <input id="slider1" type="range" min="0" max="180" value="90" oninput="updateServo(1,this.value)">
    </div>
    <div class="card">
      <div class="card-head">
        <span class="joint">Elbow<span class="pin">PIN 2</span></span>
        <span class="badge" id="val2">090deg;</span>
      </div>
      <input id="slider2" type="range" min="0" max="180" value="90" oninput="updateServo(2,this.value)">
    </div>
    <div class="card">
      <div class="card-head">
        <span class="joint">Gripper<span class="pin">PIN 3</span></span>
        <span class="badge" id="val3">090deg;</span>
      </div>
      <input id="slider3" type="range" min="0" max="180" value="90" oninput="updateServo(3,this.value)">
    </div>
  </div>
</div>
<script>
function fillSlider(s){
  var p=(s.value-s.min)/(s.max-s.min)*100;
  s.style.background='linear-gradient(to right,var(--accent) '+p+'%,var(--border) '+p+'%)';
}
function pad(n){n=parseInt(n,10);return (n<10?'00':n<100?'0':'')+n;}
function updateServo(num,angle){
  document.getElementById('val'+num).innerText=pad(angle)+'\u00B0';
  fillSlider(document.getElementById('slider'+num));
  fetch('/set?servo='+num+'&angle='+angle);
}
document.querySelectorAll('input[type=range]').forEach(fillSlider);
window.addEventListener('DOMContentLoaded',function(){
  fetch('/status').then(function(r){return r.json();}).then(function(angles){
    angles.forEach(function(angle,num){
      var s=document.getElementById('slider'+num);
      var l=document.getElementById('val'+num);
      if(s){s.value=angle;fillSlider(s);}
      if(l){l.innerText=pad(angle)+'\u00B0';}
    });
  }).catch(function(e){console.error('status fetch failed',e);});
});
</script>
</body>
</html>
)rawliteral";

// Build the Preferences key for a given servo channel
String servoKey(int servoNum) {
  return "servo" + String(servoNum);
}

void setup() {
  Serial.begin(115200);

  // Initialize the PCA9685
  board1.begin();
  board1.setPWMFreq(60);

  // Restore last-known positions (or fall back to a safe centered angle
  // on a brand-new flash where no position has been saved yet)
  prefs.begin("arm_state", false);
  const char* jointNames[NUM_SERVOS] = {"Shoulder", "Elbow", "Wrist", "Base"};
  for (int i = 0; i < NUM_SERVOS; i++) {
    int lastAngle = prefs.getInt(servoKey(i).c_str(), DEFAULT_SAFE_ANGLE);
    board1.setPWM(i, 0, angleToPulse(lastAngle));
    currentAngle[i] = lastAngle;
    Serial.printf("Servo %d (%s) restored to %d deg\n", i, jointNames[i], lastAngle);
  }

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  // WiFi connected
  Serial.println("\nWiFi Connected!");
  Serial.print("Go to this IP address in your browser: ");
  Serial.println(WiFi.localIP());

  // Web Server Routing
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/set", []() {
    if (server.hasArg("servo") && server.hasArg("angle")) {
      int servo = server.arg("servo").toInt();
      int angle = server.arg("angle").toInt();

      if (servo < 0 || servo >= NUM_SERVOS || angle < 0 || angle > 180) {
        server.send(400, "text/plain", "Invalid servo or angle");
        return;
      }

      board1.setPWM(servo, 0, angleToPulse(angle));
      currentAngle[servo] = angle;

      // Persist so the arm resumes here after a reset/power cycle
      prefs.putInt(servoKey(servo).c_str(), angle);

      server.send(200, "text/plain", "OK");
    }
  });

  server.on("/status", []() {
    String json = "[";
    for (int i = 0; i < NUM_SERVOS; i++) {
      if (i > 0) json += ",";
      json += String(currentAngle[i]);
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.begin();
}

void loop() {
  // Listen for incoming web requests
  server.handleClient(); 
}

// Convert angle (0-180) to PCA9685 pulse width
int angleToPulse(int ang) {
  int pulse = map(ang, 0, 180, SERVOMIN, SERVOMAX);
  return pulse;
}

