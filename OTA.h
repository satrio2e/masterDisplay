#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <EthernetSPI2.h>

// Ganti SSID dan password sesuai kebutuhan
const char* ssid = "ESP_Master";
const char* password = "12345678";

IPAddress newIP;

IPAddress IP;
IPAddress last_IP;
  
bool otaStarted = false;   // Menandakan apakah OTA sudah dimulai
bool lastOTAState = true; // Untuk deteksi perubahan dari LOW ke HIGH
bool currentState;

WebServer server(80);

const char* loginIndex = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Master OTA Update</title>
  <style>
    body {
      font-family: 'Helvetica Neue', Arial, sans-serif;
      background-color: #f5f7fa;
      color: #333;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 30px 20px;
      margin: 0;
    }
    h2 {
      color: #2c3e50;
      margin-bottom: 10px;
      text-align: center;
    }
    form {
      background: #fff;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 4px 8px rgba(0,0,0,0.05);
      margin-bottom: 30px;
      width: 100%;
      max-width: 400px;
      box-sizing: border-box;
    }
    input[type="file"], input[type="text"] {
      width: 100%;
      padding: 10px;
      margin-top: 10px;
      margin-bottom: 20px;
      border: 1px solid #ccc;
      border-radius: 8px;
      box-sizing: border-box;
    }
    input[type="submit"], button {
      background-color: #3498db;
      color: white;
      border: none;
      padding: 10px 15px;
      font-size: 16px;
      border-radius: 8px;
      cursor: pointer;
      width: 100%;
      box-sizing: border-box;
      transition: background-color 0.3s ease;
    }
    input[type="submit"]:hover, button:hover {
      background-color: #2980b9;
    }
    progress {
      width: 100%;
      margin-top: 10px;
      height: 20px;
    }
    p {
      margin-top: 10px;
      font-size: 14px;
      text-align: center;
    }
    hr {
      width: 100%;
      max-width: 400px;
      margin: 40px 0;
      border: none;
      border-top: 1px solid #ccc;
    }

    /* Responsiveness untuk handphone */
    @media (max-width: 480px) {
      body {
        padding: 20px 10px;
      }
      form {
        padding: 15px;
      }
      input[type="submit"], button {
        font-size: 14px;
        padding: 10px;
      }
      progress {
        height: 18px;
      }
    }
  </style>
</head>
<body>

  <h2>OTA Firmware Upload</h2>
  <form id="upload_form" method="POST" action="/update" enctype="multipart/form-data">
    <input type="file" name="update" required>
    <input type="submit" value="Upload Firmware">
    <progress id="progressBar" value="0" max="100"></progress>
    <p id="status"></p>
  </form>

  <hr>

  <h2>Set IP Ethernet</h2>
  <form id="ip_form">
    <input type="text" id="new_ip" placeholder="Masukkan IP baru" required>
    <button type="submit">Set IP</button>
    <p id="ip_status"></p>
  </form>

<script>
  // Handle OTA upload
  const form = document.getElementById('upload_form');
  const progressBar = document.getElementById('progressBar');
  const status = document.getElementById('status');

  form.addEventListener('submit', function(e) {
    e.preventDefault();
    const fileInput = form.querySelector('input[name="update"]');
    const file = fileInput.files[0];
    const xhr = new XMLHttpRequest();

    xhr.upload.addEventListener("progress", function(e) {
      if (e.lengthComputable) {
        const percent = (e.loaded / e.total) * 100;
        progressBar.value = Math.round(percent);
      }
    }, false);

    xhr.upload.addEventListener("load", function(e) {
      status.innerHTML = "Upload complete.";
    }, false);

    xhr.upload.addEventListener("error", function(e) {
      status.innerHTML = "Upload failed.";
    }, false);

    xhr.open("POST", "/update", true);
    const formData = new FormData();
    formData.append("update", file);
    xhr.send(formData);
  });

  // Handle Set IP Form
  const ipForm = document.getElementById('ip_form');
  const ipStatus = document.getElementById('ip_status');

  ipForm.addEventListener('submit', function(e) {
    e.preventDefault();
    const newIP = document.getElementById('new_ip').value;

    fetch('/setip', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ ip: newIP })
    })
    .then(response => response.text())
    .then(data => {
      ipStatus.innerText = "Server Response: " + data;
    })
    .catch(error => {
      ipStatus.innerText = "Error: " + error;
    });
  });
</script>

</body>
</html>
)rawliteral";


void checkFlashSpace() {
  Serial.println("=== Flash Memory Info ===");
  
  uint32_t sketchSize = ESP.getSketchSize();
  uint32_t freeSpace = ESP.getFreeSketchSpace();
  uint32_t flashChipSize = ESP.getFlashChipSize();
  uint32_t flashChipSpeed = ESP.getFlashChipSpeed();
  uint32_t flashChipMode = ESP.getFlashChipMode();

  Serial.printf("Current Sketch Size    : %u bytes\n", sketchSize);
  Serial.printf("Free Space for OTA      : %u bytes\n", freeSpace);
  Serial.printf("Total Flash Chip Size   : %u bytes\n", flashChipSize);
  Serial.printf("Flash Chip Speed        : %u Hz\n", flashChipSpeed);
  
  String flashMode = "UNKNOWN";
  switch (flashChipMode) {
    case FM_QIO: flashMode = "QIO"; break;
    case FM_QOUT: flashMode = "QOUT"; break;
    case FM_DIO: flashMode = "DIO"; break;
    case FM_DOUT: flashMode = "DOUT"; break;
  }
  Serial.printf("Flash Chip Mode         : %s\n", flashMode.c_str());

  Serial.println("==========================");
}


void setupOTA() {

  // mDNS hanya jika dibutuhkan, biasanya tidak terpakai di mode AP
  if (!MDNS.begin("esp32")) {
    Serial.println("Error setting up MDNS responder!");
  }

  // Halaman upload
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", loginIndex);
  });

  // Endpoint upload firmware
  server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
      // delay(1000);   --> INI HAPUS SAJA
      // ESP.restart(); --> INI HAPUS SAJA, supaya tidak restart dua kali
  }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
          Serial.printf("Update Start: %s\n", upload.filename.c_str());
          if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
              Update.printError(Serial);
          }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
              Update.printError(Serial);
          }
      } else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
              Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
              server.send(200, "text/plain", "OK");
              delay(1500); // kasih waktu ke browser sebelum reboot
              ESP.restart();  // <-- RESTART di sini saja, setelah benar-benar selesai
          } else {
              Update.printError(Serial);
              server.send(500, "text/plain", "Update failed");
          }
      }
  });

  server.on("/setip", HTTP_POST, []() {
    String body = server.arg("plain");
    DynamicJsonDocument doc(128);
    DeserializationError error = deserializeJson(doc, body);
    if (error) {
      server.send(400, "text/plain", "Invalid JSON");
      return;
    }

    String ipStr = doc["ip"];
    if (newIP.fromString(ipStr)) {
      // Simpan ke EEPROM
      for (int i = 0; i < 4; i++) {
        EEPROM.write(i, newIP[i]);
      }
      EEPROM.commit();
      server.send(200, "text/plain", "IP saved. Restart to apply.");
      Serial.print("New IP saved: ");
      Serial.println(newIP);
    } else {
      server.send(400, "text/plain", "Invalid IP");
    }
  });

  server.begin();
  Serial.println("OTA server ready. Connect to ESP WiFi and upload firmware.");

  checkFlashSpace();
}

void modeOTA(){
  
bool currentTrigger = false;

// Saat PIN33 HIGH dan OTA belum aktif
if (!currentTrigger && lastOTAState) {
  Serial.println("OTA Triggered!");
  WiFi.softAP(ssid, password);
  delay(100);
  IP = WiFi.softAPIP();
  Serial.print("Access Point IP: ");
  Serial.println(IP);
  setupOTA();
  otaStarted = true;

}

// Saat PIN33 kembali LOW → matikan OTA
if (currentTrigger && !lastOTAState && otaStarted) {
  Serial.println("OTA Deactivated - Turning off AP");
  WiFi.softAPdisconnect(true);        
  otaStarted = false;
}

lastOTAState = currentTrigger;
server.handleClient();

}