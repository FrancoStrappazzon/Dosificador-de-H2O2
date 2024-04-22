#include <Arduino.h>
#include <esp_now.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <stdlib.h> 
#include<esp_timer.h> 

//-------------------------------- DEFINICION DE VARIABLES -------------------------------------

const int valvePin = 15;   // Pin donde está conectada la electroválvula
const int trigPin = 5;// Pin de Trigger del HC-SR04
const int echoPin = 18;// Pin de Echo del HC-SR04
const int ledAlarma = 4; //Led que indica cuando el tanque esta a nivel bajo
const int botonPulsador = 14; // Pulsador que sirve para configurar parametros desde el web server

float volumen = 0;
bool valveState = LOW; // Estado actual de la electroválvula
float tiempoEv =0.0; //Tiempo de apertura de la electrovalvula

float porcentajeCiano = 0.0;//tanto % de ciano
String cianosemaforo ="";

//defino velocidad del sonido en cm/us y gravedad en m/s^2
#define SOUND_SPEED 0.034
#define gravity 9.81

//constantes para volumen y concentracion
float cte_volumen = 0.03125;
float cte_concentracion = 0.0001; //esto sale de hace 0.015/150

long duration;
float distanceCm;

Preferences preferences;

hw_timer_t *timer = NULL;

//------------------------------------- COMUNICACION WIFI -------------------------------------

//COLOCAMOS EL TOKEN QUE NOS ENTREGA META
String token="Bearer EAADwVZALaTG4BO8WFear89UBxwdcfRa2cHEHGkbKcAo17vMdIUqRPryz5CHbi2EEeAcenujr0yv32jNxePHB4g73UMOw6uW9MJoiEQ0v0py9lq16GkWhiLXXiKKGpeozsjV7ZAZAwUbcf9w4rzgXQXNLvRR3LUqPXTKgVRHSj6ToUah54Gc8KLpZB9f0UGnj";

//COLOCAMOS LA URL A DONDE SE ENVIAN LOS MENSAJES DE WHATSAPP
String servidor = "https://graph.facebook.com/v17.0/173985965802722/messages";

WiFiManager wiFiManager;


// Valores iniciales de pametros del web server
float diametroValv = 1.27; //valor de 1/2 pulgada expresado en cm
int alturaTanque = 1;
String numeroCelular ="543541705103" ;
const uint8_t broadcastAddressDefault[] = {0X94, 0xE6, 0x86, 0x3C, 0xD6, 0x3C};

WebServer server(80);


void onBeatDetected()
{
  Serial.println("Beat!");
}

typedef struct struct_message {
    char a[32];
    int b;

} struct_message;

// Create a struct_message called myData
struct_message myData;

bool newDataAvailable = false; // Variable para indicar si hay nuevos datos por ESP-NOW
bool wppEnviado = false;

// Declaracion de funcion para manejar la electrovalvula 
void handleDistanceAndValve();


//--------INTERRUPCION CUANDO LLEGA UN DATO ----------------------
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  newDataAvailable = true;    //Flag para indicar que hay un nuevo dato disponible
    
}

//------------------INTERRUPCION POR TIMER-------------------------------
bool timerActivo = false;
void IRAM_ATTR onTimer() {
  // Cuando interrumpe pongo en bajo la salida de la electrovalvula 
  Serial.println("==========================");
  Serial.println("INTERRUPCION DEL TIMER");
  Serial.println("==========================");
  valveState = LOW;
  digitalWrite(valvePin, valveState);
  //invierto el valor de la bandera
  timerActivo=false;

}


//--------------------CONFIGURO TIMER---------------------------------
void configTimer(float tiempoEv){
  // Configuración del timer
  timer = timerBegin(0, 80, true); // Timer 0, prescaler de 80 (1MHz), cuenta ascendente
  timerAttachInterrupt(timer, &onTimer, true); // Adjunta la función onTimer como interrupción

}


//-------------------CONFIGURO PINES ----------------------------------
void configPines(){
  pinMode(valvePin, OUTPUT);
  pinMode(ledAlarma, OUTPUT);
  digitalWrite(valvePin, valveState);

  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input

  pinMode(botonPulsador, INPUT_PULLUP); // Configuro el pulsador
  
}

//----------------------CONFIGURACION PARAMETROS --------------------------
void configParametros(){
  preferences.begin("my-app", false);
  diametroValv = preferences.getFloat("diametroValv", 1.27);
  alturaTanque = preferences.getInt("alturaTanque", 1);
  numeroCelular = preferences.getString("numeroCelular", "543541705103");

  preferences.end();

}

//----------------------------------------WIFI MANAGER CONFIG--------------------------------------------------
void configWifiManager(){
  
  bool res;
  res = wiFiManager.autoConnect("AutoConnectAP","12345678");

  if(!res){
    Serial.println("Fallo la conexion !");
  }
  else{
    Serial.println("Conectado");
  }
  Serial.println("Connecting to ");

  //chequea que este conectado al wifi
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());
 
  server.on("/", handle_OnConnect);
  server.on("/save", HTTP_POST, handle_SaveConfig);

  server.onNotFound(handle_NotFound);
 
  server.begin();
  Serial.println("HTTP server started");
  }

//------------------------------------- CONFIGURACION WIFI -------------------------------------
void configWifi(){
  WiFi.begin(wiFiManager.getWiFiSSID(), wiFiManager.getWiFiPass());
  Serial.println("\Conectando al WiFi ");
  while(WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print("No conectado");
  }
  Serial.println("");
  Serial.print("Se ha conectado al wifi con la ip: ");
  Serial.println(WiFi.localIP());
}

//------------------------------------- ESP-NOW -------------------------------------  
void configEspNow(){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  // Inicializo ESP-NOW
  esp_now_init();
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    ESP.restart();
    return;
  }else {
    Serial.print("ESPNOW inicializado con exito ");
  }
      // Registra la función de recepción de datos
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_peer_info_t peerInfo;
  peerInfo.channel = 0;  // Channel on which to communicate
  peerInfo.encrypt = false;  // Enable or disable encryption
}
//------------------------------------- SETUP -------------------------------------

void setup() {
  
  configPines();
   Serial.begin(115200); // Inicializa la comunicación serial para depuración
  // Seteo el dispositico como Wi-Fi Station
  WiFi.mode(WIFI_STA);  

  configParametros();
  configWifiManager();
  
  configWifi();
  configEspNow();
  configTimer(tiempoEv);  //configuro timer pero no lo activo
  timerAlarmDisable(timer);  // Deshabilita el temporizador al principio

  Serial.println("FIN DEL SETUP");
}


//------------------------ LOOP ----------------------------
void loop() {
  server.handleClient();
  //Si llego un nuevo dato se activa la bandera
    if (newDataAvailable) {
    volumen = myData.b*cte_volumen; //transforma los pixeles en volumen;

    Serial.print("Dato recibido: ");
    Serial.println(volumen);
   
    Serial.println("Cianosemaforo: ");
    Serial.print(myData.a);
    Serial.println("*******Valor:************ ");
    Serial.println(myData.b);
    porcentajeCiano = myData.b/48;
    cianosemaforo = String(myData.a) + "(" + porcentajeCiano + "%)";
    handleDistanceAndValve();

  //Habilito el timer cuando no esta activo para no sobreescribir el valor
    if (!timerActivo) {
      Serial.println("*****Activando el temporizador*****");
      // Reinicia el valor de cuenta del temporizador a 0
      timerWrite(timer, 0);
      timerAlarmWrite(timer, tiempoEv * 1000000, false);
      timerAlarmEnable(timer);
      timerActivo=true;
      }

    if(wppEnviado){
      configEspNow();
      wppEnviado= false;
    }
    newDataAvailable = false; // Reinicio bandera

  }
   // Verifica si el botón pulsador ha sido presionado
  if (digitalRead(botonPulsador) == LOW) {
    delay(50); // Antirrebote 
    if (digitalRead(botonPulsador) == LOW) {
      // Llama a la función configWifiManager() cuando el botón es presionado
      configWifiManager();
    }
  }
}

void handle_OnConnect() {
  
  server.send(200, "text/html", SendHTML(diametroValv,alturaTanque,numeroCelular)); 
}
 
void handle_SaveConfig(){
  Serial.print("Cambio de configuracion");
  //Inicia preferencias con el nombre "my-app" y guardo los parametros de la pagina web en la memoria no volatil
  preferences.begin("my-app", false);
  if (server.hasArg("param1"))
  {
    diametroValv = server.arg("param1").toFloat();
    Serial.println(diametroValv);
    preferences.putFloat("diametroValv", diametroValv);
  }
  if (server.hasArg("param2"))
  {
    alturaTanque = server.arg("param2").toInt();
    Serial.println(alturaTanque);
    preferences.putInt("alturaTanque", alturaTanque);
  }
  if (server.hasArg("param3")) {
    numeroCelular = server.arg("param3");

    preferences.putString("numeroCelular", numeroCelular);

    Serial.println("El nuevo numero de celular es: ");
    Serial.println(numeroCelular);
  }

  preferences.end();
  server.send(200, "text/html", ConfirmacionHTML());
  delay(2000);
  
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

  String SendHTML(float diametroValv,int alturaTanque, String numeroCelular){

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
  html += "    Diámetro Valvula (m): <input type='text' name='param1' value='" + String(diametroValv) + "'><br>";
  html += "    Altura de tanque (m): <input type='text' name='param2' value='" + String(alturaTanque) + "'><br>";
  html += "    Número de Teléfono: <input type='text' name='param3' value='" + String(numeroCelular) + "'><br>";
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

void handleDistanceAndValve(){
    //------------------------------------- SENSOR ULTRASONIDO -------------------------------------
    // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Seteo el pin de trigger durante 10 us
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  //Leo el echoPin, retorna el tiempo de la onda en microsegundos
  duration = pulseIn(echoPin, HIGH);
  
  // Calculo la distancia en cm 
  distanceCm = duration * SOUND_SPEED/2;
  Serial.print("Prueba sensor ultrasonido, distancia: ");
  Serial.print(distanceCm);
 
  //---------------------------------------------------------------------------------------------------------------
  configWifi();

//---------------------------------------------------MENSAJE DE WHATSAPP------------------------------------------------

  //Creamos un JSON donde se coloca el numero de telefono y el valor del cianosemaforo

  String payload = "{\"messaging_product\":\"whatsapp\",\"to\":\"54" + numeroCelular + "\",\"type\":\"text\",\"text\": {\"body\": \"" + cianosemaforo + "\"}}";

  //envio mensaje del cianosemaforo.
    if(WiFi.status()== WL_CONNECTED){
      //Iniciamos el objeto HTTP que posteriormente enviara el mensaje
    HTTPClient http;
      //Colocamos la URIL del servidor a donde se enviara el mensaje
    http.begin(servidor.c_str());
      //Colocamos la cabecera donde indicamos que sera tipo JSON
    http.addHeader("Content-Type", "application/json"); 
      //Agregamos el token en la cabecera de datos a enviar
    http.addHeader("Authorization", token);    
      //Enviamos los datos via post
    int httpPostCode = http.POST(payload);

      //Si se lograron enviar los datos
      if (httpPostCode > 0) {
        //Recibimos la respuesta que nos entrega Meta
      int httpResponseCode = http.GET();
        //Si hay respuesta la mostramos
      if (httpResponseCode>0) {
         Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        }  
    }
    http.end();
    }

// si el wifi no esta conectado intento reconectarlo 

    int wifiReconnectAttempts = 0;

   if(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("No conectado, intento de reconexión ");
    Serial.println(wifiReconnectAttempts);
    WiFi.begin(wiFiManager.getWiFiSSID(), wiFiManager.getWiFiPass());
    wifiReconnectAttempts++;

    if(wifiReconnectAttempts > 10) {
        // Maneja la reconexión después de varios intentos
        wifiReconnectAttempts = 0;
    }
  }
    
//---------------------------------------------- TIEMPO ELECTROVALVULA --------------------------------------------

    //Calculo el tiempo de apertura de la EV de acuerdo la formula t=V/(A*sqrt(2*g*h))
    float h = alturaTanque - (distanceCm/100); ; //altura del liquido restante en metros
     Serial.print("diametroValv= " );
    Serial.println(diametroValv);
    float AreaEv = 3.1416*((diametroValv/200)*(diametroValv/200)); // Area = pi* rad^2 expresada en metros

    if(h<0){
      h=0.05;
    }
//------------------------------------- ---------ALARMA POR WHATSAPP -------------------------------------
   
  // Nuevo mensaje para la recarga del tanque si el liquido restante es menor que 18 cm del tanque 
  if (h < 0.18) {
//Mensaje de alarma que se enviara por WhatsApp 
    String payload2 = "{\"messaging_product\":\"whatsapp\",\"to\":\"54" + numeroCelular + "\",\"type\":\"text\",\"text\": {\"body\": \"Recargar tanque de H2O2 \"}}";
  
    if (WiFi.status() == WL_CONNECTED) {
      // Iniciamos el objeto HTTP que posteriormente enviara el mensaje
      HTTPClient http;
      // Colocamos la URIL del servidor a donde se enviara el mensaje
      http.begin(servidor.c_str());
      // Colocamos la cabecera donde indicamos que sera tipo JSON
      http.addHeader("Content-Type", "application/json");
      // Agregamos el token en la cabecera de datos a enviar
      http.addHeader("Authorization", token);
      // Enviamos los datos via post
      int httpPostCode = http.POST(payload2);

      // Si se lograron enviar los datos 
      if (httpPostCode > 0) {
        // Recibimos la respuesta que nos entrega Meta
        int httpResponseCode = http.GET();
        // Si hay respuesta la mostramos
        if (httpResponseCode > 0) {
          Serial.print("HTTP Response code: ");
          Serial.println(httpResponseCode);
          String responsePayload = http.getString();
          Serial.println(responsePayload);
        } else {
        }
      }
      http.end();
    }
    digitalWrite(ledAlarma, HIGH); // Enciende el LED de alarma
    Serial.println("ENCIENDO LED DE ALARMA");

  }else {

    digitalWrite(ledAlarma, LOW); // Apago el LED de alarma 
    Serial.println("APAGO LED DE ALARMA");

  }

//---------------------------------------------------------TIEMPO DE APERTURA ELECTROVALVULA--------------------------

 //Calculo el tiempo de apertura de la electrovalvula
    Serial.print("Volumen de PEROXIDO:");
    Serial.println(volumen*cte_concentracion);
    Serial.print("h:");
    Serial.println(h);
    tiempoEv = (volumen*cte_concentracion) / (AreaEv*sqrt(2* gravity * h));

    Serial.print("*******************");
    Serial.println("Tiempo Electrovalvula: ");
    Serial.print(tiempoEv);
     // Apertura de electrovalvula
    valveState = HIGH;
    digitalWrite(valvePin, valveState);

    
    Serial.println("\Distance (cm): ");
    Serial.println(distanceCm);

    Serial.println("Cianobacterias detectadas: tiempo de apertura");
    Serial.println(tiempoEv);

  wppEnviado=true;
  return;
  }
