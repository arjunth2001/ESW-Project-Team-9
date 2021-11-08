
#include <WiFi.h>
#include <HTTPClient.h>
const char *ssid = "Arjun";
const char *password = "4872375050123";
String cse_ip = "esw-onem2m.iiit.ac.in";
String server = "https://" + cse_ip + "/~/in-cse/in-name/Team-9";

String send(String url, int ty, String rep)
{

  HTTPClient http;
  http.begin(url);
  http.addHeader("X-M2M-Origin", "rTI9uxe@NN:7rn5sOIaYf");
  http.addHeader("Content-Type", "application/json;ty=" + String(ty));
  //http.addHeader("Content-Length", "100");
  http.addHeader("Connection", "close");
  int code = http.POST(rep);
  http.end();
  Serial.print(code);
  delay(300);
  return (String(code));
}
void setup()
{
  Serial.begin(9600);  // setup Serial Monitor to display information
  pinMode(12, INPUT);  // Input from sensor
  pinMode(15, OUTPUT); // OUTPUT to alarm or LED
  delay(10);

  Serial.println();
  Serial.println();

  // Connect to WIFI network
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.persistent(false);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void push()
{
  int motion = digitalRead(12);
  if (motion)
  {
    Serial.println("Motion detected");
    digitalWrite(15, HIGH);
  }
  else
  {
    Serial.println("===nothing moves");
    digitalWrite(15, LOW);
  }
  delay(500);
  String data = String() + "{\"m2m:cin\":{\"con\":\"" + motion + "\"}}";
  Serial.println(send(server + "/pir/", 4, data));
}
void loop()
{
  push();
}
