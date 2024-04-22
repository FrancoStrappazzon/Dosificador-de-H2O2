// 20_Color_Blob_Detector.ino

/**
 * Detect blobs of given color
 */
 //ESP32 con camara que envia el tiempo de activacion a la otra ESP32 mediante LoRa
#include "esp32cam.h"
#include "esp32cam/JpegDecoder.h"
#include "esp32cam/apps/ColorBlobDetector.h"
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Arduino.h>

#define MAX_RESOLUTION_VGA
#define MAX_RECURSION_DEPTH 13


// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  char a[32];
  int b;
} struct_message;

// Create a struct_message called myData
struct_message myData;
esp_now_peer_info_t peerInfo;
using namespace Eloquent::Esp32cam;

Preferences preferences;


//Parametros Configurables desde WebServer
String broadcastAddressStr = "94E6863CD63C";

uint8_t broadcastAddress[]= {0X94, 0xE6, 0x86, 0x3C, 0xD6, 0x3C};


int tolerance;
int minArea;



//Si los parametros fueron almacenados en EEPROM
int colorParam1;
int colorParam2;
int colorParam3;

//Limietes de cianosemaforo
int LIMITE_VERDE = 11;
int LIMITE_AMARILLO = 40;

Cam cam;
JpegDecoder decoder;
Applications::ColorBlobDetector detector(100, 200, 100); // green - ish   170, 115, 95  , 147, 102, 95    (CAMBIAR POR PARAMETROS DE EEPROM)


//Variables para la configuracion Wifi
const char *ssid = "Fibertel WiFi729 2.4GHz";
const char *password = "00440964142";
AsyncWebServer server(80);

void onBeatDetected()
{
  Serial.println("Beat!");
}

// Defino el número de timer (0-3)
#define TIMER_NUMBER 0
// Defino el tiempo en segundos para la interrupción
#define INTERRUPT_INTERVAL_SECONDS 15 // 86400 segundos = 1 dia

hw_timer_t * timer = NULL;
//flag de la interrupcion
bool isTimerInterruptTriggered = false;
int tiempoVal=0;

/*.---------------------------------------------------------------------------------------------------------
    CALLBACK de la informacion enviada
//.---------------------------------------------------------------------------------------------------------*/
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
/*--------------------------------------------Handler de la interrupción del temporizador------------------------------------------------------------------------    
*/
void IRAM_ATTR onTimer() {
  isTimerInterruptTriggered = true; // Establecer la bandera cuando ocurre la interrupción
}

/*-----------------------------------------------------------------------------------------------------------
  Metodos del SETUP
//-----------------------------------------------------------------------------------------------------------*/
//----------------------------------------TIMER CONFIG--------------------------------------------------
void timerConfig(){
  //Config. e inicializa el Timer
  timer = timerBegin(TIMER_NUMBER, 80, true); // El divisor 80 da una frecuencia de 1 MHz
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, INTERRUPT_INTERVAL_SECONDS * 1000000, true); // Convertir segundos a microsegundos ya que la funcion trabaja en microsegundos

}
//----------------------------------------WIFI CONFIG--------------------------------------------------
void wiFiConfig(){
  WiFiManager wiFiManager;
  bool res;
  res = wiFiManager.autoConnect("AutoConnectAP","12345678");

  if(!res){
    Serial.println("Fallo la conexion !");
  }
  else{
    Serial.println("Conectado");
  }

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }

  // ssid = wiFiManager.getWiFiSSID(true).c_str();
  // password = wiFiManager.getWiFiPass(true).c_str();

  // WiFi.begin(ssid, password);
  // Serial.println("Connectando al WiFi ");
  // while(WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print("No conectado");
  // }
  // Serial.println("");
  // Serial.print("Se ha conectado al wifi con la ip: ");
  // Serial.println(WiFi.localIP());
}
//------------------------------------------WEB SERVER CONFIG------------------------------------------
void webServerConfig(){
  // Configuración del servidor web
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", SendHTML(colorParam1, colorParam2, colorParam3, broadcastAddressStr, tolerance, minArea, LIMITE_VERDE, LIMITE_AMARILLO));
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    // Manejar la solicitud POST y actualizar parámetros
    handle_SaveConfig(request);
  });

  server.onNotFound([](AsyncWebServerRequest *request){
    handle_NotFound(request);
  });

  server.begin();

  Serial.println("HTTP server started");
}

// ----------------------------------------CAM CONFIG--------------------------------------------------
void camConfig(){
  cam.aithinker();
  cam.highQuality();
  cam.vga();
  cam.highestSaturation();
  cam.disableAutomaticWhiteBalance();
  cam.disableAutomaticExposureControl();
  cam.disableGainControl();
 
  //Se toman los valores de la memoria NVS
  preferences.begin("parametros", false);
  tolerance = preferences.getInt("tolerance", 20);
  minArea = preferences.getInt("minArea", 100);
  //broadcastAddressStr = preferences.getString("broadcastAddress", "94E6863CD63C");
  preferences.end();
  // strtoul(broadcastAddressStr.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);

  /**
    * Set detector tollerance
    * The higher, the more shade of colors it will pick
    */
  detector.tollerate(tolerance);
  /**
    * Skip blob localization (slow) if not enough
    * pixels match color
    */
  detector.setMinArea(minArea);

  while (!cam.begin())
      Serial.println(cam.getErrorMessage());
}
//----------------------------------------ESP-NOW CONFIG--------------------------------------------------
void espNowConfig(){
  //WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  esp_now_init();
  if (esp_now_init() != ESP_OK) {
  Serial.println("Error initializing ESP-NOW");
  return;
  }
   // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  
  esp_now_register_send_cb(OnDataSent);

 	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
 	peerInfo.channel = 0;
 	peerInfo.encrypt = false;
  
  // Add peer
  esp_now_add_peer(&peerInfo);        
  if (esp_now_add_peer(&peerInfo) == ESP_ERR_ESPNOW_NOT_INIT) { 
      Serial.println(F("ESPNOW NO INICIADA"));
      return;
  }
  if (esp_now_add_peer(&peerInfo) == ESP_ERR_ESPNOW_ARG) { 
      Serial.println(F("invalid argument en add_peer"));
      return;
  }
  if (esp_now_add_peer(&peerInfo) == ESP_ERR_ESPNOW_NOT_FOUND) { 
      Serial.println(F("peer is not found"));
      return;
  }
}

void setup() {
       
  timerConfig();
    //Lo defino en modo Wi-Fi Station
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  delay(3000);

  wiFiConfig();
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
  //wiFiManager.resetSettings(); //Para resetear los valores almacenados de SSID y contraseña wifi (SOLO SIRVE PARA DEBUG Y TESTING)

  webServerConfig();
  camConfig();
  espNowConfig();

  timerAlarmEnable(timer);
  Serial.println("FIN DE SETUP");
}
//--------------------------------------------FIN SETUP--------------------------------------------------

void loop(){

  if (isTimerInterruptTriggered) {
    preferences.begin("parametros", false);
    colorParam1 = preferences.getInt("colorParam1", 100);
    colorParam2 = preferences.getInt("colorParam2", 200);
    colorParam3 = preferences.getInt("colorParam3", 100);
    tolerance = preferences.getInt("tolerance", 10);
    minArea = preferences.getInt("minArea", 100);
    broadcastAddressStr = preferences.getString("broadcastAddressStr","94E6863CD63C");  
    LIMITE_VERDE = preferences.getInt("LIMITE_VERDE", LIMITE_VERDE);
    LIMITE_AMARILLO = preferences.getInt("LIMITE_AMARILLO", LIMITE_AMARILLO);
    //broadcastAddressStr = preferences.getString("broadcastAddress", "94E6863CD63C");
    preferences.end();

    //---------------------------------------------------------------
    Serial.println("Parametros de foto actual:");
    Serial.println(colorParam1);
    Serial.println(colorParam2);
    Serial.println(colorParam3);
    Serial.println(tolerance);
    Serial.println(minArea);
    Serial.println(broadcastAddressStr);
    Serial.println(LIMITE_VERDE);
    Serial.println(LIMITE_AMARILLO);
    //---------------------------------------------------------------
    detector.updateColor(colorParam1, colorParam2, colorParam3);
    detector.set("tol", tolerance);
    detector.set("min-area", minArea);
    //---------------------------------------DISPARO CAMARA-------------------------------------------
    if (!cam.capture()) {
        Serial.println(cam.getErrorMessage());
        return;
    }
    if (!decoder.decode(cam)) {
        Serial.println(decoder.getErrorMessage());
        return;
    }
    if (detector.detect(decoder)) {
        Serial.print(detector.maskCount);
        Serial.println(" pixels match target color");
        Serial.print("Blob detection run in ");
        Serial.print(detector.getExecutionTimeInMillis());
        Serial.println("ms");
        Serial.println("Anchura de la imagen:");
        Serial.println(detector.getWidth());
        Serial.println("Altura de la imagen:");
        Serial.println(detector.getHeight());
    }
    else {
        Serial.println(detector.getErrorMessage());
    }
    //Serial.println(detector.toString());
    detector.printTo(Serial);
    //detector.printTo(Serial);

    // Registro de pixeles en la imagen
    uint16_t contaPixel = detector.maskCount;
    
    
    //-----------------------------------CIANOSEMAFORO------------------------------------------------
    float limiteVerdeAbs = LIMITE_VERDE * 48 ; // Regla de 3 simple tira esto para pasar de porcentaje a absoluto
    float limiteAmarilloAbs = LIMITE_AMARILLO * 48 ; // Regla de 3 simple tira esto para pasar de porcentaje a absoluto

    Serial.println("Limites de semaforo:");
    Serial.println(limiteVerdeAbs);
    Serial.println(limiteAmarilloAbs);
    char cianoSemaforo[32];
    if (contaPixel < limiteVerdeAbs) {
      strcpy(cianoSemaforo, "Verde");
    } else if (contaPixel >= limiteVerdeAbs && contaPixel <= limiteAmarilloAbs) {
      strcpy(cianoSemaforo, "Amarillo");
    } else {
      strcpy(cianoSemaforo, "Rojo");
    }
    Serial.println("Cianosemaforo:");
    Serial.println(cianoSemaforo);
    //------------------------------------ESP NOW---------------------------------------------------
    WiFi.disconnect(true);
    espNowConfig();
    
    // Set values to send
    strcpy(myData.a, cianoSemaforo);
    myData.b = contaPixel;
    
    preferences.begin("parametros", false);
    broadcastAddressStr = preferences.getString("broadcastAddressStr", "94E6863CD63C");
    stringToHexArray(broadcastAddressStr, broadcastAddress);
    // Imprimir el resultado
    Serial.println("Array resultante:");
    for (int i = 0; i < 6; ++i) {
      Serial.printf("0x%02X ", broadcastAddress[i]);
    }

    // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
    
    Serial.print("Send Status: ");
    if (result == ESP_OK) {
      Serial.println("Success");
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      // How did we get so far!!
      Serial.println("ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    } else {
      Serial.println("Not sure what happened");
    }

    preferences.end();
    wiFiConfig();
    isTimerInterruptTriggered = false; // Restablecer la bandera
    }

}

//-------------------------------------------Web Server--------------------------------------------------
void handle_SaveConfig(AsyncWebServerRequest *request){
  Serial.print("Cambio de configuracion");
  preferences.begin("parametros", false);
  bool hasParam1 = request->hasParam("param1", true);
  bool hasParam2 = request->hasParam("param2", true);
  bool hasParam3 = request->hasParam("param3", true);
  bool hasBroadcastAddressStr = request->hasParam("broadcastAddressStr", true);
  bool hasTolerance = request->hasParam("tolerance", true);
  bool hasMinArea = request->hasParam("minArea", true);
  bool hasLimiteVerde = request->hasParam("limiteVerde", true);
  bool hasLimiteAmarillo = request->hasParam("limiteAmarillo", true);

  if (hasLimiteVerde) {
      LIMITE_VERDE = request->getParam("limiteVerde", true)->value().toInt();
      Serial.println(LIMITE_VERDE);
      preferences.putInt("LIMITE_VERDE", LIMITE_VERDE);
  }

  if (hasLimiteAmarillo) {
      LIMITE_AMARILLO = request->getParam("limiteAmarillo", true)->value().toInt();
      Serial.println(LIMITE_AMARILLO);
      preferences.putInt("LIMITE_AMARILLO", LIMITE_AMARILLO);
  }
  if (hasParam1) {
    colorParam1 = request->getParam("param1", true)->value().toInt();
    Serial.println(colorParam1);
    preferences.putInt("colorParam1", colorParam1);
  }

  if (hasParam2) {
    colorParam2 = request->getParam("param2", true)->value().toInt();
    Serial.println(colorParam2);
    preferences.putInt("colorParam2", colorParam2);
  }

  if (hasParam3) {
    colorParam3 = request->getParam("param3", true)->value().toInt();
    Serial.println(colorParam3);
    preferences.putInt("colorParam3", colorParam3);
  }

  if (hasBroadcastAddressStr) {
    broadcastAddressStr = request->getParam("broadcastAddressStr", true)->value();
    Serial.println(broadcastAddressStr);
    preferences.putString("broadcastAddressStr", broadcastAddressStr);
  }

  if (hasTolerance) {
    tolerance = request->getParam("tolerance", true)->value().toInt();
    Serial.println(tolerance);
    preferences.putInt("tolerance", tolerance);
  }

  if (hasMinArea) {
    minArea = request->getParam("minArea", true)->value().toInt();
    Serial.println(minArea);
    preferences.putInt("minArea", minArea);
  }

  preferences.end();
  request->send(200, "text/html", ConfirmacionHTML());
}

void handle_NotFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

String SendHTML(int colorParam1, int colorParam2, int colorParam3, String broadcastAddressStr, int tolerance, int minArea, int LIMITE_VERDE, int LIMITE_AMARILLO){
  String html = "<!DOCTYPE html>";
  html += "<html lang='es'>";
  html += "<head>";
  html += "  <meta charset='UTF-8'>";
  html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "  <title>Configuración de Parámetros</title>";
  html += "  <style>";
  html += "    body {";
  html += "      font-family: 'Arial', sans-serif;";
  html += "      background-image: url('https://i.ytimg.com/vi/QAvFguIzoRk/maxresdefault.jpg');";
  html += "      background-size: cover;";
  html += "      background-position: center;";
  html += "      color: #333;";
  html += "      margin: 0;";
  html += "      padding: 20px;";
  html += "      display: flex;";
  html += "      justify-content: center;";
  html += "      align-items: center;";
  html += "      height: 100vh;";
  html += "      flex-direction: column;";
  html += "    }";
  html += "    h1 {";
  html += "      margin-bottom: 20px;";
  html += "      color: #4CAF50;";
  html += "    }";
  html += "    form {";
  html += "      background-color: rgba(255, 255, 255, 0.7);";
  html += "      padding: 20px;";
  html += "      border-radius: 10px;";
  html += "      width: 300px;";
  html += "      text-align: left;";
  html += "      box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);";
  html += "    }";
  html += "    input {";
  html += "      width: calc(100% - 20px);";
  html += "      margin-bottom: 10px;";
  html += "      padding: 10px;";
  html += "      border: 1px solid #ccc;";
  html += "      border-radius: 5px;";
  html += "    }";
  html += "    input[type='range'] {";
  html += "      width: calc(100% - 20px);";
  html += "      margin-bottom: 10px;";
  html += "    }";
  html += "    input[type='submit'] {";
  html += "      background-color: #4CAF50;";
  html += "      color: #fff;";
  html += "      cursor: pointer;";
  html += "    }";
  html += "  </style>";
  html += "</head>";
  html += "<body>";
  html += "  <h1>Configuración de Parámetros</h1>";
  html += "  <form action='/save' method='post'>";
  html += "    <label for='param1'>Luma (Y):</label><br>";
  html += "    <input type='text' id='param1' name='param1' value='" + String(colorParam1) + "'><br>";
  html += "    <label for='param2'>Cromancia Blue (Cb):</label><br>";
  html += "    <input type='text' id='param2' name='param2' value='" + String(colorParam2) + "'><br>";
  html += "    <label for='param3'>Cromancia Red (Cr):</label><br>";
  html += "    <input type='text' id='param3' name='param3' value='" + String(colorParam3) + "'><br>";
  html += "    <label for='broadcastAddressStr'>Broadcast Address:</label><br>";
  html += "    <input type='text' id='broadcastAddressStr' name='broadcastAddressStr' value='" + String(broadcastAddressStr) + "'><br>"; //94E6863CD63C
  html += "    <label for='tolerance'>Tolerancia:</label><br>";
  html += "    <input type='text' id='tolerance' name='tolerance' value='" + String(tolerance) + "'><br>";
  html += "    <label for='minArea'>Área Mínima:</label><br>";
  html += "    <input type='text' id='minArea' name='minArea' value='" + String(minArea) + "'><br>";
  html += "    <label for='limiteVerde'>Límite Verde(%):</label><br>";
  html += "    <input type='range' id='limiteVerde' name='limiteVerde' min='0' max='100' value='" + String(LIMITE_VERDE) + "'><br>";
  html += "    <label for='limiteAmarillo'>Límite Amarillo(%):</label><br>";
  html += "    <input type='range' id='limiteAmarillo' name='limiteAmarillo' min='0' max='100' value='" + String(LIMITE_AMARILLO) + "'><br>";
  html += "    <input type='submit' value='Guardar'>";
  html += "  </form>";
  html += "</body>";
  html += "</html>";

  return html;
}



  String ConfirmacionHTML(){
  String html = "<!DOCTYPE html>";
  html += "<html lang='es'>";
  html += "<head>";
  html += "  <meta charset='UTF-8'>";
  html += "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "  <title>Actualización Exitosa</title>";
  html += "  <style>";
  html += "    body {";
  html += "      font-family: 'Arial', sans-serif;";
  html += "      background-color: #f7f7f7;";
  html += "      color: #333;";
  html += "      margin: 0;";
  html += "      padding: 20px;";
  html += "      display: flex;";
  html += "      justify-content: center;";
  html += "      align-items: center;";
  html += "      height: 100vh;";
  html += "      flex-direction: column;";
  html += "    }";
  html += "    h1 {";
  html += "      color: #4CAF50;";
  html += "    }";
  html += "    p {";
  html += "      font-size: 18px;";
  html += "    }";
  html += "    button {";
  html += "      margin-top: 20px;";
  html += "      padding: 10px 20px;";
  html += "      background-color: #4CAF50;";
  html += "      color: #fff;";
  html += "      border: none;";
  html += "      border-radius: 5px;";
  html += "      cursor: pointer;";
  html += "    }";
  html += "  </style>";
  html += "</head>";
  html += "<body>";
  html += "  <h1>¡Actualización Exitosa!</h1>";
  html += "  <p>Las variables se han actualizado correctamente.</p>";
  html += "  <button onclick='window.location.href=\"/\"'>Volver a la Página Principal</button>";
  html += "</body>";
  html += "</html>";

  return html;
}  


void stringToHexArray(const String &hexString, uint8_t *hexArray) {
  for (int i = 0; i < 6; ++i) {
    // Obtener el índice correspondiente en la cadena para cada par de caracteres
    int charIndex = i * 2;

    // Extraer el par de caracteres de la cadena y convertirlo a un valor hexadecimal
    hexArray[i] = strtol(hexString.substring(charIndex, charIndex + 2).c_str(), nullptr, 16);
    
  }
}