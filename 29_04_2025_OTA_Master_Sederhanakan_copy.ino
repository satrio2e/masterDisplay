#include "OTA.h"
#include <SPI.h>
#include <EthernetSPI2.h>
#include <EEPROM.h>
#include "MgsModbus.h"
#define EEPROM_SIZE 255

#include <FS.h>
#include <SPIFFS.h>

// Buat instance SPI baru
 SPIClass spiW5500(HSPI);   // untuk ESP32-S3 pakai HSPI / FSPI

//ethernet setting
  MgsModbus Mb;

  byte mac[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x21 };
  IPAddress ip1(192, 168, 99, 195);
  IPAddress gateway1(192, 168, 99, 81);
  IPAddress subnet1(255, 255, 255, 0);

unsigned long previousMillis = 0;  // Waktu sebelumnya
const int interval = 500;  // Interval pergantian teks (1 detik)

//String tipe = "TYPE";
String tipe2 = "TYPE";
String lastTipe = "";  // Simpan teks terakhir

int idx = 0;
  
char pesan1[50]; // 14 karakter + null terminator
char pesan1_last[50] = ""; // 14 karakter + null terminator
char pesan1Char[50] = ""; // 14 karakter + null terminator
char pesan2[50]; // 14 karakter + null terminator
char pesan2_last[50]; // 14 karakter + null terminator
char pesan2Char[50]; // 14 karakter + null terminator
char pesan3[50]; // 14 karakter + null terminator
char pesan3_last[50]; // 14 karakter + null terminator
char pesan3Char[50]; // 14 karakter + null terminator
char pesan4[50]; // 14 karakter + null terminator
char pesan4_last[50]; // 14 karakter + null terminator
char pesan4Char[50]; // 14 karakter + null terminator
char pesan5[50]; // 14 karakter + null terminator
char pesan5_last[50]; // 14 karakter + null terminator
char pesan5Char[50]; // 14 karakter + null terminator

//Fungsi start Ethernet
 void startEthernet(){
    //ethernet begin esp32
      SPI.begin(14, 12, 13, 27);
      Ethernet.init(27);
      Ethernet.begin(mac, ip1, gateway1, subnet1);


    //ethernet esp32s3     
    //   spiW5500.begin(11, 13, 12, 10);  // SCK=13, MISO=12, MOSI=11, CS=10
    //   Ethernet.init(10);
    //   Ethernet.begin(mac, ip1, gateway1, subnet1);

      // Cek jenis chip
      int hw = Ethernet.hardwareStatus();
      if (hw == EthernetNoHardware)       Serial.println("Tidak ada shield Ethernet");
      else if (hw == EthernetW5100)       Serial.println("W5100 terdeteksi");
      else if (hw == EthernetW5200)       Serial.println("W5200 terdeteksi");
      else if (hw == EthernetW5500)       Serial.println("W5500 terdeteksi");
      else                                Serial.println("Hardware tidak dikenal");

      // Cek link
      int link = Ethernet.linkStatus();
      if (link == LinkON)  Serial.println("Link OK");
      else if (link == LinkOFF) Serial.println("Kabel lepas");
      else Serial.println("Status link unknown");

      Serial.print("IP Address: ");
      Serial.println(Ethernet.localIP());
    
 }
//

//memulai SPIFFS
 void startSPIFFS(){
  if (!SPIFFS.begin(true)) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
 }
//

//fungsi menulis ke SPIFFS
 void writeFile(fs::FS &fs, const char * path, char * message){

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
 }
//

//Baca data dari SPIFFS ke variabel char
 void readFileChar(fs::FS &fs, const char *path, char* outputVar) {
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("- failed to open file for reading");
    return;
  }

  int idx = 0;
  while (file.available() && idx < 50) {  // Pastikan tidak melebihi ukuran array
    outputVar[idx++] = file.read();
  }
  outputVar[idx] = '\0';  // Pastikan null terminator

  file.close();
 }
//

// Fungsi umum untuk membaca dan mengembalikan data ke Mb.MbData
 void readAndRestorePesan(const char* filePath, char* pesanChar, int startIndex, int endIndex) {
  // Baca data dari SPIFFS ke dalam pesanChar
  readFileChar(SPIFFS, filePath, pesanChar);

  // Pulihkan data dari pesanChar ke Mb.MbData dalam rentang yang ditentukan
  int idx = 0;
  for (int i = startIndex; i <= endIndex; i++) {
    uint16_t val = (uint16_t(pesanChar[idx]) << 8) | uint16_t(pesanChar[idx + 1]);
    Mb.MbData[i] = val;
    idx += 2;  // Karena setiap nilai uint16_t disimpan dalam dua byte
  }
 }
//

// Fungsi untuk membaca dan memulihkan data ke Mb.MbData dan memeriksa perubahan pesan
 void updatePesan(int slave,int startIndex, int endIndex, char* pesan, char* pesan_last, const char* filePath) {
  int idx = 0;

  // Loop melalui rentang data Mb.MbData
  for (int i = startIndex; i <= endIndex; i++) {
    uint16_t val = Mb.MbData[i];

    // Simpan data ke buffer pesan (konversi menjadi dua byte)
    pesan[idx++] = (val >> 8) & 0xFF;
    pesan[idx++] = val & 0xFF;
  }
  pesan[idx] = '\0';  // Pastikan null terminator pada akhir string pesan

  // Periksa jika pesan baru berbeda dari pesan terakhir
  if (strcmp(pesan, pesan_last) != 0) {
    // Kirim pesan jika berbeda
    kirimPesan(slave, pesan);
    strcpy(pesan_last, pesan);  // Simpan pesan terbaru untuk perbandingan nanti

    // Tulis pesan terbaru ke SPIFFS
    writeFile(SPIFFS, filePath, pesan);

    // Baca kembali pesan dari SPIFFS untuk memastikan
    char pesanChar[50];  // buffer untuk menyimpan pesan
    readFileChar(SPIFFS, filePath, pesanChar);

    // Tampilkan pesan yang dibaca
    Serial.println(pesanChar);
  }
 }
//

//Fungsi mengirim pesan
 void kirimPesan(int id, char* pesan) {
  String kirim = String(id) + "|" + pesan;
  Serial2.println(kirim);
 }
//

//
void updatePesanMaster1(int startIndex, int endIndex, char* pesan1, char* pesan1_last, char* pesan2, char* pesan2_last) {
  int idx = 0;

  // Ambil data baru dari MbData dan simpan ke pesan1
  for (int i = startIndex; i <= endIndex; i++) {
    uint16_t val = Mb.MbData[i];
    pesan1[idx++] = (val >> 8) & 0xFF;
    pesan1[idx++] = val & 0xFF;
  }
  pesan1[idx] = '\0';

  // Bandingkan pesan1 dengan pesan1_last
  if (strcmp(pesan1, pesan1_last) != 0) {
    // Simpan pesan1_last ke pesan2
    strcpy(pesan2, pesan1_last);
    strcpy(pesan2_last, pesan1_last);  // Update pesan2_last

    // Simpan pesan baru ke pesan1_last
    strcpy(pesan1_last, pesan1);

    // Kirim pesan-pesan
    kirimPesan(1, pesan1);
    kirimPesan(2, pesan2);

    // Simpan ke SPIFFS
    writeFile(SPIFFS, "/master1.txt", pesan1);
    writeFile(SPIFFS, "/master2.txt", pesan2);

    // Debug output
    Serial.println("Pesan master1: " + String(pesan1));
    Serial.println("Pesan master2 (dari histori): " + String(pesan2));
  }
}
//

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, 16, 17); // TX only, pin 17
  startSPIFFS();
  startEthernet();

  // Baca dan pulihkan data pesan dari file SPIFFS ke Mb.MbData
  readAndRestorePesan("/master1.txt", pesan1Char, 0, 29);
  readAndRestorePesan("/master2.txt", pesan2Char, 30, 59);
  readAndRestorePesan("/master3.txt", pesan3Char, 60, 89);
  readAndRestorePesan("/master4.txt", pesan4Char, 70, 99);
  readAndRestorePesan("/master5.txt", pesan5Char, 100, 129);


}

void loop() {
  unsigned long currentMillis = millis();
  // put your main code here, to run repeatedly:
 modeOTA();
 Mb.MbsRun();

//millis untuk update tulisan
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;  // Reset timer

      // Baca dari MbData[0–19] untuk master1
      int idx = 0;
      for (int i = 0; i <= 29; i++) {
        uint16_t val = Mb.MbData[i];
        pesan1[idx++] = (val >> 8) & 0xFF;
        pesan1[idx++] = val & 0xFF;
      }
      pesan1[idx] = '\0';

      if (strcmp(pesan1, pesan1_last) != 0) {
        // Geser histori ke bawah
        strcpy(pesan5, pesan4_last);
        strcpy(pesan5_last, pesan4_last);
        writeFile(SPIFFS, "/master5.txt", pesan5);
        kirimPesan(5, pesan5);

        strcpy(pesan4, pesan3_last);
        strcpy(pesan4_last, pesan3_last);
        writeFile(SPIFFS, "/master4.txt", pesan4);
        kirimPesan(4, pesan4);

        strcpy(pesan3, pesan2_last);
        strcpy(pesan3_last, pesan2_last);
        writeFile(SPIFFS, "/master3.txt", pesan3);
        kirimPesan(3, pesan3);

        strcpy(pesan2, pesan1_last);
        strcpy(pesan2_last, pesan1_last);
        writeFile(SPIFFS, "/master2.txt", pesan2);
        kirimPesan(2, pesan2);

        // Update master1
        strcpy(pesan1_last, pesan1);
        writeFile(SPIFFS, "/master1.txt", pesan1);
        kirimPesan(1, pesan1);
      }
   } // millis
} //void loop
