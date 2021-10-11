#include <WiFi.h>
const char *ssid = "Dlink3";
const char *password = "4872375050123";

// CSE params
const char *host = "192.168.0.2";
const int httpPort = 8080;

// AE params
const int aePort = 80;
const char *origin = "Cae_arjun";
///////////////////////////////////////////

WiFiServer server(aePort);

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

  // Start HTTP server
  server.begin();
  Serial.println("Server started");

  // Create AE resource
  String resulat = send("/server", 2, "{\"m2m:ae\":{\"rn\":\"arjun\",\"api\":\"arjun.esw.com\",\"rr\":\"true\",\"poa\":[\"http://" + WiFi.localIP().toString() + ":" + aePort + "\"]}}");

  if (resulat == "HTTP/1.1 201 Created")
  {
    // Create Container resource
    send("/server/arjun", 3, "{\"m2m:cnt\":{\"rn\":\"pir\"}}");

    // Create ContentInstance resource
    send("/server/arjun/pir", 4, "{\"m2m:cin\":{\"con\":\"0\"}}");
    // Create Subscription resource
    send("/server/arjun/pir", 23, "{\"m2m:sub\":{\"rn\":\"pir_sub\",\"nu\":[\"Cae_arjun\"],\"nct\":1}}");
  }
  else
  {
    Serial.println();
    Serial.println("Error: ");
    Serial.println(resulat);
  }
}
String send(String url, int ty, String rep)
{

  // Connect to the CSE address
  Serial.print("connecting to ");
  Serial.println(host);

  WiFiClient client;

  if (!client.connect(host, httpPort))
  {
    Serial.println("connection failed");
    return "error";
  }

  // prepare the HTTP request
  String req = String() + "POST " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "X-M2M-Origin: " + origin + "\r\n" +
               "Content-Type: application/json;ty=" + ty + "\r\n" +
               "Content-Length: " + rep.length() + "\r\n"
                                                   "Connection: close\r\n\n" +
               rep;

  Serial.println(req + "\n");

  // Send the HTTP request
  client.print(req);

  unsigned long timeout = millis();
  while (client.available() == 0)
  {
    if (millis() - timeout > 5000)
    {
      Serial.println(">>> Client Timeout !");
      client.stop();
      return "error";
    }
  }

  // Read the HTTP response
  String res = "";
  if (client.available())
  {
    res = client.readStringUntil('\r');
    Serial.print(res);
  }
  while (client.available())
  {
    String line = client.readStringUntil('\r');
    Serial.print(line);
  }

  Serial.println();
  Serial.println("closing connection");
  Serial.println();
  return res;
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
  send("/server/arjun/pir", 4, data);
}
void loop()
{
  //push();
}
