/*
ESP32 Cheap Yellow Display Electronic Shelf Label with Google Firebase
Distributed under the MIT License
© Copyright Maxim Bortnikov 2024
For more information please visit
https://sourceforge.net/projects/esp32-cyd-esl-with-firebase/
https://github.com/Northstrix/ESP32-Cheap-Yellow-Display-Electronic-Shelf-Label-with-Google-Firebase
Required libraries:
https://github.com/mobizt/Firebase-ESP-Client
https://github.com/Northstrix/AES_in_CBC_mode_for_microcontrollers
https://github.com/intrbiz/arduino-crypto
*/
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include "aes.h"
#include <FS.h>
#include <SPIFFS.h>
#include "Crypto.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

#define BUTTON_PIN 22 // Ground this pin to enter the configuration mode

String wifi_ssid = "";
String wifi_password = "";
String api_key = "";
String database_url = "";

int delay_in_seconds = 600; // request data from Firebase every 600 seconds

int m;
int x;
int y;

String string_for_data;
byte tmp_st[8];
int decract;
uint8_t array_for_CBC_mode[16];
uint8_t back_aes_key[32]; 
uint32_t aes_mode[3] = {128, 192, 256};
uint8_t aes_key[32];

String esl_id;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
RNG random_number;

unsigned long dataMillis = 0;
int count = 0;
bool signupOK = false;

String dec_st;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

void back_aes_k() {
  for (int i = 0; i < 32; i++) {
    back_aes_key[i] = aes_key[i];
  }
}

void rest_aes_k() {
  for (int i = 0; i < 32; i++) {
    aes_key[i] = back_aes_key[i];
  }
}

void incr_aes_key() {
  if (aes_key[15] == 255) {
    aes_key[15] = 0;
    if (aes_key[14] == 255) {
      aes_key[14] = 0;
      if (aes_key[13] == 255) {
        aes_key[13] = 0;
        if (aes_key[12] == 255) {
          aes_key[12] = 0;
          if (aes_key[11] == 255) {
            aes_key[11] = 0;
            if (aes_key[10] == 255) {
              aes_key[10] = 0;
              if (aes_key[9] == 255) {
                aes_key[9] = 0;
                if (aes_key[8] == 255) {
                  aes_key[8] = 0;
                  if (aes_key[7] == 255) {
                    aes_key[7] = 0;
                    if (aes_key[6] == 255) {
                      aes_key[6] = 0;
                      if (aes_key[5] == 255) {
                        aes_key[5] = 0;
                        if (aes_key[4] == 255) {
                          aes_key[4] = 0;
                          if (aes_key[3] == 255) {
                            aes_key[3] = 0;
                            if (aes_key[2] == 255) {
                              aes_key[2] = 0;
                              if (aes_key[1] == 255) {
                                aes_key[1] = 0;
                                if (aes_key[0] == 255) {
                                  aes_key[0] = 0;
                                } else {
                                  aes_key[0]++;
                                }
                              } else {
                                aes_key[1]++;
                              }
                            } else {
                              aes_key[2]++;
                            }
                          } else {
                            aes_key[3]++;
                          }
                        } else {
                          aes_key[4]++;
                        }
                      } else {
                        aes_key[5]++;
                      }
                    } else {
                      aes_key[6]++;
                    }
                  } else {
                    aes_key[7]++;
                  }
                } else {
                  aes_key[8]++;
                }
              } else {
                aes_key[9]++;
              }
            } else {
              aes_key[10]++;
            }
          } else {
            aes_key[11]++;
          }
        } else {
          aes_key[12]++;
        }
      } else {
        aes_key[13]++;
      }
    } else {
      aes_key[14]++;
    }
  } else {
    aes_key[15]++;
  }
}

int getNum(char ch) {
  int num = 0;
  if (ch >= '0' && ch <= '9') {
    num = ch - 0x30;
  } else {
    switch (ch) {
    case 'A':
    case 'a':
      num = 10;
      break;
    case 'B':
    case 'b':
      num = 11;
      break;
    case 'C':
    case 'c':
      num = 12;
      break;
    case 'D':
    case 'd':
      num = 13;
      break;
    case 'E':
    case 'e':
      num = 14;
      break;
    case 'F':
    case 'f':
      num = 15;
      break;
    default:
      num = 0;
    }
  }
  return num;
}

char getChar(int num) {
  char ch;
  if (num >= 0 && num <= 9) {
    ch = char(num + 48);
  } else {
    switch (num) {
    case 10:
      ch = 'a';
      break;
    case 11:
      ch = 'b';
      break;
    case 12:
      ch = 'c';
      break;
    case 13:
      ch = 'd';
      break;
    case 14:
      ch = 'e';
      break;
    case 15:
      ch = 'f';
      break;
    }
  }
  return ch;
}

void back_key() {
  back_aes_k();
}

void rest_key() {
  rest_aes_k();
}

void clear_variables() {
  string_for_data = "";
  decract = 0;
}

void split_for_decr(char ct[], int ct_len, int p) {
  int br = false;
  int res[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
  };
  byte prev_res[] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
  };
  for (int i = 0; i < 32; i += 2) {
    if (i + p > ct_len - 1) {
      br = true;
      break;
    }
    if (i == 0) {
      if (ct[i + p] != 0 && ct[i + p + 1] != 0)
        res[i] = 16 * getNum(ct[i + p]) + getNum(ct[i + p + 1]);
      if (ct[i + p] != 0 && ct[i + p + 1] == 0)
        res[i] = 16 * getNum(ct[i + p]);
      if (ct[i + p] == 0 && ct[i + p + 1] != 0)
        res[i] = getNum(ct[i + p + 1]);
      if (ct[i + p] == 0 && ct[i + p + 1] == 0)
        res[i] = 0;
    } else {
      if (ct[i + p] != 0 && ct[i + p + 1] != 0)
        res[i / 2] = 16 * getNum(ct[i + p]) + getNum(ct[i + p + 1]);
      if (ct[i + p] != 0 && ct[i + p + 1] == 0)
        res[i / 2] = 16 * getNum(ct[i + p]);
      if (ct[i + p] == 0 && ct[i + p + 1] != 0)
        res[i / 2] = getNum(ct[i + p + 1]);
      if (ct[i + p] == 0 && ct[i + p + 1] == 0)
        res[i / 2] = 0;
    }
  }

  for (int i = 0; i < 32; i += 2) {
    if (i + p - 32 > ct_len - 1) {
      br = true;
      break;
    }
    if (i == 0) {
      if (ct[i + p - 32] != 0 && ct[i + p - 32 + 1] != 0)
        prev_res[i] = 16 * getNum(ct[i + p - 32]) + getNum(ct[i + p - 32 + 1]);
      if (ct[i + p - 32] != 0 && ct[i + p - 32 + 1] == 0)
        prev_res[i] = 16 * getNum(ct[i + p - 32]);
      if (ct[i + p - 32] == 0 && ct[i + p - 32 + 1] != 0)
        prev_res[i] = getNum(ct[i + p - 32 + 1]);
      if (ct[i + p - 32] == 0 && ct[i + p - 32 + 1] == 0)
        prev_res[i] = 0;
    } else {
      if (ct[i + p - 32] != 0 && ct[i + p - 32 + 1] != 0)
        prev_res[i / 2] = 16 * getNum(ct[i + p - 32]) + getNum(ct[i + p - 32 + 1]);
      if (ct[i + p - 32] != 0 && ct[i + p - 32 + 1] == 0)
        prev_res[i / 2] = 16 * getNum(ct[i + p - 32]);
      if (ct[i + p - 32] == 0 && ct[i + p - 32 + 1] != 0)
        prev_res[i / 2] = getNum(ct[i + p - 32 + 1]);
      if (ct[i + p - 32] == 0 && ct[i + p - 32 + 1] == 0)
        prev_res[i / 2] = 0;
    }
  }
  
  if (br == false) {
    if(decract > 16){
      for (int i = 0; i < 16; i++){
        array_for_CBC_mode[i] = prev_res[i];
      }
    }
    uint8_t ret_text[16];
    uint8_t cipher_text[16];
    for(int i = 0; i<16; i++){
      cipher_text[i] = res[i];
    }
    uint32_t aes_mode[3] = {128, 192, 256};
    int i = 0;
    aes_context ctx;
    set_aes_key(&ctx, aes_key, aes_mode[m]);
    aes_decrypt_block(&ctx, ret_text, cipher_text);
    incr_aes_key();
    if (decract > 2) {
              for (int i = 0; i < 16; i++) {
                ret_text[i] ^= array_for_CBC_mode[i];
              }

              for (i = 0; i < 16; i += 2) {
                // tft.drawPixel(x, y, (ret_text[i + 1] << 8) | ret_text[i]); //Display image on display as it's being received and decrypted
                /*
                uint16_t color = (ret_text[i + 1] << 8) | ret_text[i];
                char hexString[5];
                sprintf(hexString, "%04X", color);
                string_for_data += String(hexString);
                */
                if (ret_text[i + 1] < 16)
                  string_for_data += "0";
                string_for_data += String(ret_text[i + 1], HEX);
                if (ret_text[i] < 16)
                  string_for_data += "0";
                string_for_data += String(ret_text[i], HEX);
                x++;
                if (x == DISPLAY_WIDTH) {
                  x = 0;
                  y++;
                }
              }
    }

    if (decract == -1){
      for (i = 0; i < 16; ++i) {
        array_for_CBC_mode[i] = int(ret_text[i]);
      }
    }
    decract++;
  }
}

void decrypt_string_with_aes_in_cbc(String ct) { // Function for aes. Takes ciphertext as an input.
  clear_variables();
  int ct_len = ct.length() + 1;
  char ct_array[ct_len];
  ct.toCharArray(ct_array, ct_len);
  int ext = 0;
  decract = -1;
  while (ct_len > ext) {
    split_for_decr(ct_array, ct_len, 0 + ext);
    ext += 32;
    decract += 10;
  }
}

void disp_centered_text(String text, int h) {
  tft.drawCentreString(text, 160, h, 1);
}

String read_file(fs::FS & fs, String path) {
  File file = fs.open(path, "r");
  if (!file || file.isDirectory()) {
    return "-1";
  }
  String fileContent;
  while (file.available()) {
    fileContent += String((char) file.read());
  }
  file.close();
  return fileContent;
}

void write_to_file_with_overwrite(fs::FS & fs, String path, String content) {
  File file = fs.open(path, "w");
  if (!file) {
    return;
  }
  file.print(content);
  file.close();
}

bool bttn_is_pressed() {
  bool bt_state = false;
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(500);
    if (digitalRead(BUTTON_PIN) == LOW) {
      bt_state = true;
    }
  }
  return bt_state;
}

void display_line_on_display(String img_line) {
  if (img_line == "-1")
    return;
  for (int i = 0; i < (DISPLAY_WIDTH * 12); i += 4) {
    String hex = "";
    for (int j = 0; j < 4; j++) {
      hex += img_line.charAt(i + j);
    }
    uint16_t color = (uint16_t) strtol(hex.c_str(), NULL, 16);
    tft.drawPixel(x, y, color);
    x++;
    if (x == DISPLAY_WIDTH) {
      x = 0;
      y++;
    }
  }
}

String read_file_from_firebase(String filename){
  if(Firebase.RTDB.getString(&fbdo, filename.c_str()))
    return fbdo.to<String>();
  else
    return "-1";
}

void setup()
{
  m = 2; // Set AES to 256-bit mode
  tft.begin();
  tft.fillScreen(0x0000);
  tft.setRotation(1);
  tft.setCursor(0, 5);
  if (!SPIFFS.begin()) {
    tft.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);
  if (bttn_is_pressed() == true) {
    disp_centered_text("Open The Serial Terminal", 10);
    disp_centered_text("and switch ESL to operating mode", 30);
    while (bttn_is_pressed() == true) {
      delay(100);
    }
    tft.fillScreen(0x0000);
    disp_centered_text("The Encryption Key", 10);
    disp_centered_text("will be printed to the Serial Terminal", 30);
    disp_centered_text("in 2 seconds", 50);
    delay(2000);
    tft.fillScreen(0x0000);
    disp_centered_text("Check the Serial Terminal", 10);
    String generated_encryption_key;
    for (int i = 0; i < 32; i++) {
      byte rbyte = random_number.get();
      if (rbyte < 0x10)
        generated_encryption_key += "0";
      generated_encryption_key += String(rbyte, HEX);
    }
    write_to_file_with_overwrite(SPIFFS, "/enckey", generated_encryption_key);
    Serial.println("\nEncryption Key:");
    Serial.println(read_file(SPIFFS, "/enckey"));
    delay(100);
    Serial.println("\nPaste the ESL ID to the Serial Terminal. And press the \"Send\" button.");
    while (!Serial.available()) {
      delayMicroseconds(400);
    }
    String read_esl_id = Serial.readString();
    write_to_file_with_overwrite(SPIFFS, "/id", read_esl_id);
    Serial.print("ESL ID is set to \"");
    Serial.print(read_file(SPIFFS, "/id"));
    Serial.println("\"");

    Serial.println("\nEnter Access Point Name:");
    while (!Serial.available()) {
      delayMicroseconds(400);
    }
    String wifi_ssid = Serial.readString();
    write_to_file_with_overwrite(SPIFFS, "/wifi_ssid", wifi_ssid);

    Serial.println("\nEnter Access Point Password:");
    while (!Serial.available()) {
      delayMicroseconds(400);
    }
    String wifi_password = Serial.readString();
    write_to_file_with_overwrite(SPIFFS, "/wifi_password", wifi_password);

    Serial.println("\nEnter WEB API KEY:");
    while (!Serial.available()) {
      delayMicroseconds(400);
    }
    String api_key = Serial.readString();
    write_to_file_with_overwrite(SPIFFS, "/api_key", api_key);

    Serial.println("\nEnter Database URL:");
    while (!Serial.available()) {
      delayMicroseconds(400);
    }
    String database_url = Serial.readString();
    write_to_file_with_overwrite(SPIFFS, "/database_url", database_url);
    tft.fillScreen(0x0000);
    Serial.println("\nConfiguration completed!\nPlease, reboot the device.");
    disp_centered_text("Reboot The Device", 10);

  } else {
    esl_id = read_file(SPIFFS, "/id");
    wifi_ssid = read_file(SPIFFS, "/wifi_ssid");
    wifi_password = read_file(SPIFFS, "/wifi_password");
    api_key = read_file(SPIFFS, "/api_key");
    database_url = read_file(SPIFFS, "/database_url");
    if (wifi_ssid == "-1" || wifi_password == "-1" || api_key == "-1" || database_url == "-1" || esl_id == "-1") {
      tft.fillScreen(0x0000);
      tft.setTextColor(0xf800);
      tft.setTextSize(2);
      disp_centered_text("ESL isn't configured!", 114);
      for (;;)
        delay(10000);
    }
    if (read_file(SPIFFS, "/Line2") != "-1") {
      x = 0;
      y = 0;
      for (int i = 2; i < DISPLAY_HEIGHT; i+=3) {
        display_line_on_display(read_file(SPIFFS, "/Line" + String(i)));
      }
    }
    else{
      tft.fillScreen(0x0000);
      tft.setTextSize(2);
      tft.setTextColor(65535);
      disp_centered_text("No image is set (yet)", 114);
    }
  x = 0;
  y = 0;
  // Set Key
  String extr_key = read_file(SPIFFS, "/enckey");
  if (extr_key == "-1"){
    tft.setTextColor(0xf800);
    tft.print("Encryption Key isn't set!!!");
    for(;;)
      delay(10000);
  }
  byte res[32];
  for (int i = 0; i < 64; i += 2) {
    if (i == 0) {
      if (extr_key.charAt(i) != 0 && extr_key.charAt(i + 1) != 0)
        res[i] = 16 * getNum(extr_key.charAt(i)) + getNum(extr_key.charAt(i + 1));
      if (extr_key.charAt(i) != 0 && extr_key.charAt(i + 1) == 0)
        res[i] = 16 * getNum(extr_key.charAt(i));
      if (extr_key.charAt(i) == 0 && extr_key.charAt(i + 1) != 0)
        res[i] = getNum(extr_key.charAt(i + 1));
      if (extr_key.charAt(i) == 0 && extr_key.charAt(i + 1) == 0)
        res[i] = 0;
    } else {
      if (extr_key.charAt(i) != 0 && extr_key.charAt(i + 1) != 0)
        res[i / 2] = 16 * getNum(extr_key.charAt(i)) + getNum(extr_key.charAt(i + 1));
      if (extr_key.charAt(i) != 0 && extr_key.charAt(i + 1) == 0)
        res[i / 2] = 16 * getNum(extr_key.charAt(i));
      if (extr_key.charAt(i) == 0 && extr_key.charAt(i + 1) != 0)
        res[i / 2] = getNum(extr_key.charAt(i + 1));
      if (extr_key.charAt(i) == 0 && extr_key.charAt(i + 1) == 0)
        res[i / 2] = 0;
    }
  }

  for (int i = 0; i < 32; i++) {
     aes_key[i] = (uint8_t) res[i];
  }
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    Serial.println("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(300);
        Serial.print("#");
    }
    Serial.println("");
    //Serial.println();
    //Serial.print("Connected with IP: ");
    //Serial.println(WiFi.localIP());
    //Serial.println();

    //Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);
    config.api_key = api_key.c_str();
    config.database_url = database_url.c_str();
  #if defined(ARDUINO_RASPBERRY_PI_PICO_W)
      config.wifi.clearAP();
      config.wifi.addAP(wifi_ssid.c_str(), wifi_password.c_str());
  #endif

    // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
    Firebase.reconnectNetwork(true);

    fbdo.setBSSLBufferSize(14336 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);

    Serial.println("Signing Up to Firebase");

    if (Firebase.signUp(&config, &auth, "", ""))
    {
        Serial.println("signed to firebase successfully");
        Serial.println("");
        signupOK = true;
    }
    else
        Serial.printf("%s\n", config.signer.signupError.message.c_str());

    config.token_status_callback = tokenStatusCallback;
    Firebase.begin(&config, &auth);
    back_key();
    try_to_retrieve_data_from_the_cloud();
    dataMillis = millis();
  }
}

void try_to_retrieve_data_from_the_cloud() {
  Serial.println("Trying to retrieve data from Firebase...");
  String firebase_data = read_file_from_firebase(esl_id);
  String spiffs_data = read_file(SPIFFS, "/" + esl_id);
  Serial.print("Firebase image hash for the '");
  Serial.print(esl_id);
  Serial.print("' ESL: ");
  Serial.println(firebase_data);
  Serial.print("SPIFFS image hash for the '");
  Serial.print(esl_id);
  Serial.print("' ESL: ");
  Serial.println(spiffs_data);

  if (firebase_data != spiffs_data) {
    x = 0;
    y = 0;
    String firebase_line2 = read_file_from_firebase(esl_id + "2");

    if (firebase_line2 != "-1") {
      for (int i = 2; i < DISPLAY_HEIGHT; i += 3) {
        String extr_data = read_file_from_firebase(esl_id + String(i));
        Serial.print("Trying to retrieve file '");
        Serial.print(esl_id);
        Serial.print(i);
        Serial.println("'...");
        int retry_count = 0;
        while (extr_data == "-1") {
          extr_data = read_file_from_firebase(esl_id + String(i));
          retry_count++;
          if (retry_count > 3) {
            Serial.println("Failed to retrieve data from Firebase. Rebooting...");
            ESP.restart();
          }
          delay(100);
        }
        Serial.print("Retrieved file '");
        Serial.print(esl_id);
        Serial.print(i);
        Serial.println("'");
        decrypt_string_with_aes_in_cbc(extr_data);
        delay(120);
        write_to_file_with_overwrite(SPIFFS, "/NLine" + String(i), string_for_data);
        delay(24);
        rest_key();
        delay(120);
      }
      rest_key();
      write_to_file_with_overwrite(SPIFFS, "/" + esl_id, read_file_from_firebase(esl_id));
    }
    x = 0;
    y = 0;
    delay(200);
    for (int i = 2; i < DISPLAY_HEIGHT; i += 3) {
      Serial.print("Reading line N");
      Serial.println(i);
      String img_data = read_file(SPIFFS, "/NLine" + String(i));
      delay(120);
      Serial.println("Writing...");
      write_to_file_with_overwrite(SPIFFS, "/Line" + String(i), img_data);
      Serial.println("Done!");
    }
    for (int i = 2; i < DISPLAY_HEIGHT; i += 3) {
      display_line_on_display(read_file(SPIFFS, "/Line" + String(i)));
    }
  } else {
    Serial.println("Image hashes match. No update is needed.");
  }
  Serial.println("Done retrieving data from Firebase!");
}

void loop()
{
    if (millis() - dataMillis > (1000 * delay_in_seconds) && signupOK && Firebase.ready())
    {
        dataMillis = millis();
        try_to_retrieve_data_from_the_cloud();
    }
}
