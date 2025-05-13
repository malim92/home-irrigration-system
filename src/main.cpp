#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#define RELAY_PIN 4

using AsyncClient = AsyncClientClass;

FirebaseApp app;
RealtimeDatabase db;

WiFiClientSecure sslClient1, sslClient2;
AsyncClient asyncClient1(sslClient1), asyncClient2(sslClient2);

UserAuth *userAuthPtr = nullptr;
AsyncResult dbResult;
bool setupComplete = false;
AsyncResult authResult;
bool lastRelayState = false;

// Firebase auth and config values
String apiKey, userEmail, userPassword, dbUrl, ssid, password;

// Read config from SPIFFS
bool loadConfig()
{
  File file = SPIFFS.open("/config.json");
  if (!file || file.size() == 0)
  {
    Serial.println("Failed to open file or file is empty!");
    // return;
  }

  file.seek(0); // Go back to start of file

  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, file);
  if (err)
  {
    Serial.print("❌ JSON parse error: ");
    Serial.println(err.c_str());
  }

  Serial.println("✅ JSON parsed successfully!");

  ssid = doc["wifi"]["ssid"].as<String>();
  password = doc["wifi"]["password"].as<String>();
  apiKey = doc["firebase"]["api_key"].as<String>();
  userEmail = doc["firebase"]["email"].as<String>();
  userPassword = doc["firebase"]["password"].as<String>();
  dbUrl = doc["firebase"]["database_url"].as<String>();

  return true;
}

void processData(AsyncResult &res)
{
  if (!res.isResult())
    return;

  if (res.isError())
  {
    Firebase.printf("❌ [%s] %s (code %d)\n", res.uid().c_str(), res.error().message().c_str(), res.error().code());
  }
  else if (res.available())
  {
    Firebase.printf("✅ [%s] Payload: %s\n", res.uid().c_str(), res.c_str());

    if (res.path() == "/control/relay")
    {
      // bool newState = res.to<bool>();
      bool newState = true;
      digitalWrite(RELAY_PIN, newState ? HIGH : LOW);
      lastRelayState = newState;
      Serial.printf("🔄 Relay updated to: %s\n", newState ? "ON" : "OFF");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  if (!SPIFFS.begin(true))
  {
    Serial.println("⚠️ SPIFFS mount failed");
    return;
  }

  if (!loadConfig())
  {
    Serial.println("⚠️ Config load failed");
    return;
  }

  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("🔌 Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.printf("\n✅ Connected! IP: %s\n\n", WiFi.localIP().toString().c_str());

  sslClient1.setInsecure();
  sslClient2.setInsecure();
  sslClient1.setTimeout(1000);
  sslClient2.setTimeout(1000);

  userAuthPtr = new UserAuth(apiKey.c_str(), userEmail.c_str(), userPassword.c_str());

  initializeApp(asyncClient1, app, getAuth(*userAuthPtr), processData, "AUTH");
  app.getApp<RealtimeDatabase>(db);
  db.url(dbUrl.c_str());
}

void loop()
{
  app.loop();
  if (app.ready() && !setupComplete)
  {
    setupComplete = true;
    Serial.println("✅ Firebase app ready!");

    // Initial value pull
    db.get(asyncClient1, "/control/relay", processData, false, "INIT_READ");

    // Start listening to changes
    // db.stream<FirebaseVariant>(asyncClient1, "/control/relay", processData, "RELAY_STREAM");
  }
  else
  {
    Serial.println("App Not Ready.");
  }

  delay(500);
  processData(dbResult);
}
