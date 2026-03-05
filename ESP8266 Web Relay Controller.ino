#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <EEPROM.h> // <-- NUEVO

#ifndef STASSID
#define STASSID "SSID"
#define STAPSK "PASSWORD"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
WiFiServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000);  // UTC-3 Argentina

String header;
String D1State = "off";
String D2State = "off";
String D5State = "off";
String D6State = "off";

#define RELAY1_PIN D1
#define RELAY2_PIN D2
#define RELAY3_PIN D5
#define RELAY4_PIN D6

// NUEVO - Variables de horario automático
int horaInicio1, minutoInicio1, horaFin1, minutoFin1;
int horaInicio2, minutoInicio2, horaFin2, minutoFin2;

// Guardar y leer de EEPROM
void guardarHorarioEEPROM() {
  EEPROM.write(0, horaInicio1);
  EEPROM.write(1, minutoInicio1);
  EEPROM.write(2, horaFin1);
  EEPROM.write(3, minutoFin1);

  EEPROM.write(4, horaInicio2);
  EEPROM.write(5, minutoInicio2);
  EEPROM.write(6, horaFin2);
  EEPROM.write(7, minutoFin2);

  EEPROM.commit();
}

void cargarHorarioEEPROM() {
  horaInicio1 = EEPROM.read(0);
  minutoInicio1 = EEPROM.read(1);
  horaFin1 = EEPROM.read(2);
  minutoFin1 = EEPROM.read(3);

  horaInicio2 = EEPROM.read(4);
  minutoInicio2 = EEPROM.read(5);
  horaFin2 = EEPROM.read(6);
  minutoFin2 = EEPROM.read(7);

}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(500);
    ESP.restart();
  }
  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupOTA() {
  ArduinoOTA.begin();
  Serial.println("OTA Ready");
}

void setup() {
  Serial.begin(9600);
  digitalWrite(RELAY1_PIN, HIGH);
  pinMode(RELAY1_PIN, OUTPUT);

  digitalWrite(RELAY2_PIN, HIGH);
  pinMode(RELAY2_PIN, OUTPUT);

  digitalWrite(RELAY3_PIN, HIGH);
  pinMode(RELAY3_PIN, OUTPUT);

  digitalWrite(RELAY4_PIN, HIGH);
  pinMode(RELAY4_PIN, OUTPUT);

  EEPROM.begin(512); // <-- NUEVO
  cargarHorarioEEPROM(); // <-- NUEVO

  setupWiFi();
  setupOTA();
  timeClient.begin();
  server.begin();
}

String obtenerFecha() {
  time_t rawTime = timeClient.getEpochTime();
  struct tm *tiempo = localtime(&rawTime);
  char fecha[11];
  sprintf(fecha, "%02d/%02d/%04d", tiempo->tm_mday, tiempo->tm_mon + 1, tiempo->tm_year + 1900);
  return String(fecha);
}

// NUEVO - Chequeo de horarios por rele
bool enRangoAutomatico(int h, int m, int hi, int mi, int hf, int mf) {
  int actual = h * 60 + m;
  int inicio = hi * 60 + mi;
  int fin = hf * 60 + mf;
  if (inicio < fin) {
    return actual >= inicio && actual < fin;
  } else {
    return actual >= inicio || actual < fin;
  }
}

unsigned long mensajeGuardadoHasta1 = 0;  // <-- NUEVO
bool mostrarMensajeGuardado1 = false;    // <-- NUEVO
unsigned long mensajeGuardadoHasta2 = 0;  // <-- NUEVO
bool mostrarMensajeGuardado2 = false;    // <-- NUEVO

void loop() {
  ArduinoOTA.handle();
  timeClient.update();
  String horaActual = timeClient.getFormattedTime();
  String fechaActual = obtenerFecha();
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();

  // Control automático para relay 1
  bool auto1 = enRangoAutomatico(currentHour, currentMinute, horaInicio1, minutoInicio1, horaFin1, minutoFin1);
  if (auto1) {
    digitalWrite(RELAY1_PIN, LOW);
    D1State = "auto";
  } else if (D1State == "auto") {
    digitalWrite(RELAY1_PIN, HIGH);
    D1State = "off";
  }

  // Control automático para relay 2
  bool auto2 = enRangoAutomatico(currentHour, currentMinute, horaInicio2, minutoInicio2, horaFin2, minutoFin2);
  if (auto2) {
    digitalWrite(RELAY2_PIN, LOW);
    D2State = "auto";
  } else if (D2State == "auto") {
    digitalWrite(RELAY2_PIN, HIGH);
    D2State = "off";
  }

  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    header = "";

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        header += c;

        if (c == '\n') {
          if (currentLine.length() == 0) {

            if (header.indexOf("GET /configurar?") >= 0) {
              // Detectamos si se presionó el botón de guardar1 o guardar2
              bool guardar1 = header.indexOf("guardar1") > 0;
              bool guardar2 = header.indexOf("guardar2") > 0;

              if (guardar1) {
                int hi1 = header.indexOf("hi1=");
                int mi1 = header.indexOf("mi1=");
                int hf1 = header.indexOf("hf1=");
                int mf1 = header.indexOf("mf1=");

                if (hi1 > 0 && mi1 > 0 && hf1 > 0 && mf1 > 0) {
                  horaInicio1 = header.substring(hi1 + 4, header.indexOf('&', hi1)).toInt();
                  minutoInicio1 = header.substring(mi1 + 4, header.indexOf('&', mi1)).toInt();
                  horaFin1 = header.substring(hf1 + 4, header.indexOf('&', hf1)).toInt();
                  minutoFin1 = header.substring(mf1 + 4, header.indexOf('&', mf1)).toInt();
                  guardarHorarioEEPROM(); // Guarda ambos, pero solo se modificaron los datos del rele 1
                  mostrarMensajeGuardado1 = true;  // Mostrar el mensaje
                  mensajeGuardadoHasta1 = millis() + 5000; // El mensaje desaparecerá en 5 segundos
                }
              }

              if (guardar2) {
                int hi2 = header.indexOf("hi2=");
                int mi2 = header.indexOf("mi2=");
                int hf2 = header.indexOf("hf2=");
                int mf2 = header.indexOf("mf2=");

                if (hi2 > 0 && mi2 > 0 && hf2 > 0 && mf2 > 0) {
                  horaInicio2 = header.substring(hi2 + 4, header.indexOf('&', hi2)).toInt();
                  minutoInicio2 = header.substring(mi2 + 4, header.indexOf('&', mi2)).toInt();
                  horaFin2 = header.substring(hf2 + 4, header.indexOf('&', hf2)).toInt();
                  minutoFin2 = header.substring(mf2 + 4).toInt();
                  guardarHorarioEEPROM(); // Guarda ambos, pero solo se modificaron los datos del rele 2
                  mostrarMensajeGuardado2 = true;  // Mostrar el mensaje
                  mensajeGuardadoHasta2 = millis() + 5000; // El mensaje desaparecerá en 5 segundos
                }
              }
            }

            if (header.indexOf("GET /hora") >= 0) {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/plain");
              client.println("Connection: close");
              client.println();
              client.println(horaActual);
              break;
            }

            // HTML
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close\n");

            if (!auto1) {
              if (header.indexOf("GET /D1/on") >= 0) {
                D1State = "on";
                digitalWrite(RELAY1_PIN, LOW);
              } else if (header.indexOf("GET /D1/off") >= 0) {
                D1State = "off";
                digitalWrite(RELAY1_PIN, HIGH);
              }
            }

            if (!auto2) {
              if (header.indexOf("GET /D2/on") >= 0) {
                D2State = "on";
                digitalWrite(RELAY2_PIN, LOW);
              } else if (header.indexOf("GET /D2/off") >= 0) {
                D2State = "off";
                digitalWrite(RELAY2_PIN, HIGH);
              }
            }

            if (header.indexOf("GET /D5/on") >= 0) {
              D5State = "on";
              digitalWrite(RELAY3_PIN, LOW);
            } else if (header.indexOf("GET /D5/off") >= 0) {
              D5State = "off";
              digitalWrite(RELAY3_PIN, HIGH);
            }

            if (header.indexOf("GET /D6/on") >= 0) {
              D6State = "on";
              digitalWrite(RELAY4_PIN, LOW);
            } else if (header.indexOf("GET /D6/off") >= 0) {
              D6State = "off";
              digitalWrite(RELAY4_PIN, HIGH);
            }

            // HTML completo
            client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
            client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
            client.println("<style>html{font-family:Helvetica;text-align:center;background:black;color:white;}");
            client.println(".button{background:#3364FF;border:none;color:white;padding:16px 40px;font-size:30px;margin:2px;cursor:pointer;}");
            client.println(".button2{background:#FF3333;} .disabled{background:gray;color:white;padding:16px 40px;font-size:30px;margin:2px;border:none;}");
            client.println("</style>");
            client.println("<script>function actualizarHora(){fetch('/hora').then(r=>r.text()).then(h=>{document.getElementById('hora').innerText=h;});}");
            client.println("setInterval(actualizarHora,1000);</script></head>");
            client.println("<body onload='actualizarHora()'><h1>ESP8266 Web Server</h1>");
            client.println("<p>Fecha actual: <strong>" + fechaActual + "</strong></p>");
            client.println("<p>Hora actual: <span id='hora'>" + horaActual + "</span></p>");

            client.println("<p>Luz De Afuera 1 - Estado " + (auto1 ? "Control Automático" : D1State) + "</p>");
            client.println(auto1
              ? "<p><button class='disabled' disabled>Control Automático</button></p>"
              : (D1State == "off"
                ? "<p><a href='/D1/on'><button class='button'>Encender</button></a></p>"
                : "<p><a href='/D1/off'><button class='button button2'>Apagar</button></a></p>"));

            client.println("<p>Luz De Afuera 2 - Estado " + (auto2 ? "Control Automático" : D2State) + "</p>");
            client.println(auto2
              ? "<p><button class='disabled' disabled>Control Automático</button></p>"
              : (D2State == "off"
                ? "<p><a href='/D2/on'><button class='button'>Encender</button></a></p>"
                : "<p><a href='/D2/off'><button class='button button2'>Apagar</button></a></p>"));

            client.println("<p>Luz Pieza - Estado " + D5State + "</p>");
            client.println(D5State == "off"
              ? "<p><a href='/D5/on'><button class='button'>Encender</button></a></p>"
              : "<p><a href='/D5/off'><button class='button button2'>Apagar</button></a></p>");

            client.println("<p>Luz LED - Estado " + D6State + "</p>");
            client.println(D6State == "off"
              ? "<p><a href='/D6/on'><button class='button'>Encender</button></a></p>"
              : "<p><a href='/D6/off'><button class='button button2'>Apagar</button></a></p>");

            // Formulario con horarios independientes
            client.println("<h2>Configurar Horarios Automáticos</h2>");
            client.println("<form action='/configurar' method='get'>");
            client.println("<h3>Relay 1:</h3>");
            client.println("Inicio: <input type='number' name='hi1' min='0' max='23' value='" + String(horaInicio1) + "'>:<input type='number' name='mi1' min='0' max='59' value='" + String(minutoInicio1) + "'><br>");
            client.println("Fin: <input type='number' name='hf1' min='0' max='23' value='" + String(horaFin1) + "'>:<input type='number' name='mf1' min='0' max='59' value='" + String(minutoFin1) + "'><br><br>");
            client.println("<input class='button' type='submit' name='guardar1' value='Guardar 1'>");

            // Mostrar mensaje "Se ha guardado exitosamente" si corresponde
            if (mostrarMensajeGuardado1 && millis() < mensajeGuardadoHasta1) {
              client.println("<p id='mensajeOK' style='color:lightgreen;font-weight:bold;'>✔ Se ha guardado exitosamente</p>");
              client.println("<script>setTimeout(()=>{document.getElementById('mensajeOK').style.display='none';},5000);</script>");
            }

            client.println("<h3>Relay 2:</h3>");
            client.println("Inicio: <input type='number' name='hi2' min='0' max='23' value='" + String(horaInicio2) + "'>:<input type='number' name='mi2' min='0' max='59' value='" + String(minutoInicio2) + "'><br>");
            client.println("Fin: <input type='number' name='hf2' min='0' max='23' value='" + String(horaFin2) + "'>:<input type='number' name='mf2' min='0' max='59' value='" + String(minutoFin2) + "'><br><br>");
            client.println("<input class='button' type='submit' name='guardar2' value='Guardar 2'>");

            // Mostrar mensaje "Se ha guardado exitosamente" si corresponde
            if (mostrarMensajeGuardado2 && millis() < mensajeGuardadoHasta2) {
              client.println("<p id='mensajeOK' style='color:lightgreen;font-weight:bold;'>✔ Se ha guardado exitosamente</p>");
              client.println("<script>setTimeout(()=>{document.getElementById('mensajeOK').style.display='none';},5000);</script>");
            }

            client.println("</form>");

            client.println("</body></html>");
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    header = "";
    client.stop();
  }
} 
