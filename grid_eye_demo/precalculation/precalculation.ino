/*
  Visualizing the Panasonic Grid-EYE Sensor Data using Processing
  By: Nick Poole
  SparkFun Electronics
  Date: January 12th, 2018
  
  MIT License: Permission is hereby granted, free of charge, to any person obtaining a copy of this 
  software and associated documentation files (the "Software"), to deal in the Software without 
  restriction, including without limitation the rights to use, copy, modify, merge, publish, 
  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the 
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or 
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING 
  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  
  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14568
  
  This example is intended as a companion sketch to the Processing sketch found in the same folder.
  Once this code is running on your hardware, open the accompanying Processing sketch and run it 
  as well. The Processing sketch will receive the comma separated values generated by this code and
  use them to generate a thermal image. If you don't have Processing, you can download it here:
  https://processing.org/
  
  Hardware Connections:
  Attach the Qwiic Shield to your Arduino/Photon/ESP32 or other
  Plug the sensor onto the shield
*/

#include <Wire.h>
#include <SparkFun_GridEYE_Arduino_Library.h>
#include <cmath>


// OneM2M related
#include <WiFi.h>
#include "secret.h"

// CSE params
const char *host = "192.168.43.85";
const int httpPort = 8080;

// AE params
const int aePort = 80;
const char *origin = "Cae_nirmal";
///////////////////////////////////////////


WiFiServer server(aePort);

// Grid eye related 

GridEYE grideye;

const int NUM_FRAMES_PRECALCULATION = 250; 
// >= 258 Frames aren't supported because of stack overflow. (Found using binary search)
// So taking 250 to be safe.

int TotalActivePoints = 0; // Feature 1
int NumConnectedComponents = 0; // Feature 2
int sizeLargestComponent = 0; // Feature 3

float pre_calc_frames[NUM_FRAMES_PRECALCULATION+1][9][9];
float B[9][9]; // Background -- Mean of NUM_FRAMES_PRECALCULATION frames with no occupancy
float S[9][9]; // Standard deviation of each pixel in NUM_FRAMES_PRECALCULATION frames.
float M[9][9]; // Current thermal values
int F[9][9]; // Feature grid with active points

int visited[9][9]; // Used for DFS
int unique_components; // Number of connected components
int component_size[64];

void print_precalculate(int frame_num)
{
  Serial.print("Printing Frame: ");
  Serial.print(frame_num);
  Serial.println();
  
  for(int row = 0; row < 8; row++){
    for(int col = 0; col < 8; col++){
      Serial.print(pre_calc_frames[frame_num][row][col]);
      Serial.print(",");
    }
    Serial.println();
  }
  Serial.println("----------------------");
  Serial.println();
}

void calculate_background_frame()
{
  for(int row = 0; row < 8; row++){
    for(int col = 0; col < 8; col++){
      float cell_mean = 0;
      for(int frame_num = 0; frame_num < NUM_FRAMES_PRECALCULATION; frame_num++){
        cell_mean += pre_calc_frames[frame_num][row][col];
      }
      cell_mean = cell_mean / (float)NUM_FRAMES_PRECALCULATION;
      B[row][col] = cell_mean;
    }
  }

  Serial.println("\nPrinting background grid.");
  for(int row = 0; row < 8; row++){
    for(int col = 0; col < 8; col++){
      Serial.print(B[row][col]);
      Serial.print(",");
    }
    Serial.println();
  }
  Serial.println("----------------------");
  Serial.println();
}

void calculate_standard_deviation()
{
  for(int row = 0; row < 8; row++){
    for(int col = 0; col < 8; col++){
      float cell_std = 0.0;
      for(int frame_num = 0; frame_num < NUM_FRAMES_PRECALCULATION; frame_num++){
        cell_std += pow(pre_calc_frames[frame_num][row][col] - B[row][col], 2);
      }
      cell_std = cell_std / (float)NUM_FRAMES_PRECALCULATION;
      S[row][col] = sqrt(cell_std);
    }
  }

  Serial.println("\nPrinting std. grid.");
  for(int row = 0; row < 8; row++){
    for(int col = 0; col < 8; col++){
      Serial.print(S[row][col]);
      Serial.print(",");
    }
    Serial.println();
  }
  Serial.println("----------------------");
  Serial.println();
}

void print_current_thermal_values()
{
    Serial.println("Printing current thermal values");
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            Serial.print(M[row][col]);
            Serial.print(",");
        }
        Serial.println();
    }
    Serial.println("----------------------");
    Serial.println();
}

void calculate_feature_grid()
{

    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            if(M[row][col] - B[row][col] > 3.0 * S[row][col]){
                F[row][col] = 1;
            }else{
                F[row][col] = 0;
            }
        }
    }


    Serial.println("Printing current feature grid.");
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            Serial.print(F[row][col]);
            Serial.print(" ");
        }
        Serial.println();
    }
    Serial.println("----------------------");
    Serial.println();
}


void DFS(int row, int col, int component_num)
{
    if(row < 0 || row > 7 || col < 0 || col > 7){
        return;
    }
    if(visited[row][col] != 0 || F[row][col] == 0){
        return;
    }
    visited[row][col] = component_num;
    DFS(row-1, col-1, component_num);
    DFS(row-1, col, component_num);
    DFS(row-1, col+1, component_num);
    DFS(row, col-1, component_num);
    DFS(row, col+1, component_num);
    DFS(row+1, col-1, component_num);
    DFS(row+1, col, component_num);
    DFS(row+1, col+1, component_num);
}

void DFS_init()
{
    unique_components = 0;
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            visited[row][col] = 0;
        }
    }
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            if(visited[row][col] == 0 && F[row][col] == 1){
                DFS(row, col, ++unique_components);
            }
        }
    }
}

void find_size_largest_component()
{
    for(int i = 0; i < 64; i++){
        component_size[i] = 0;
    }
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            component_size[visited[row][col]]++;
        }
    }
    sizeLargestComponent = 0;
    for(int i = 1; i < 64; i++){ // Avoid zero
        if(component_size[i] > sizeLargestComponent){
            sizeLargestComponent = component_size[i];
        }
    }
}

void calculate_features()
{
    Serial.println("Calculating and printing extracted features.");
    // Feature 1 - Total active points.
    TotalActivePoints = 0;
    for(int row = 0; row < 8; row++){
        for(int col = 0; col < 8; col++){
            TotalActivePoints += F[row][col];
        }
    }
    Serial.print("Feature 1 - Total active points: ");
    Serial.println(TotalActivePoints);

    // Feature 2 - Number of connected components
    // Using DFS
    DFS_init();
    NumConnectedComponents = unique_components;

    // Feature 3 - Size of largest component
    find_size_largest_component();

    Serial.print("Feature 2 - Number of connected components: ");
    Serial.println(NumConnectedComponents);
    Serial.print("Feature 3 - Size of largest connected component(s): ");
    Serial.println(sizeLargestComponent);

}

void setup() {

  // Start your preferred I2C object 
  Wire.begin();
  // Library assumes "Wire" for I2C but you can pass something else with begin() if you like
  grideye.begin();
  // Pour a bowl of serial
  Serial.begin(115200);

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
  String resulat = send("/server", 2, "{\"m2m:ae\":{\"rn\":\"nirmal\",\"api\":\"nirmal.esw.com\",\"rr\":\"true\",\"poa\":[\"http://" + WiFi.localIP().toString() + ":" + aePort + "\"]}}");

  if (resulat == "HTTP/1.1 201 Created")
  {
    // Create Container resource
    send("/server/nirmal", 3, "{\"m2m:cnt\":{\"rn\":\"grid_eye\"}}");
    // Create ContentInstance resource
    send("/server/nirmal/grid_eye", 4, "{\"m2m:cin\":{\"con\":\"0\"}}");
    // Create Subscription resource
    send("/server/nirmal/grid_eye", 23, "{\"m2m:sub\":{\"rn\":\"grid_eye_sub\",\"nu\":[\"Cae_nirmal\"],\"nct\":1}}");
  }
  else
  {
    Serial.println();
    Serial.println("Error: ");
    Serial.println(resulat);
  }

  // Grid eye related
  for(int calc_iter = 0; calc_iter < NUM_FRAMES_PRECALCULATION; calc_iter++)
  {
    int row = 0;
    int col = 0;
    for(unsigned char i = 0; i < 64; i++){
      row = i/8;
      col = i%8;
      pre_calc_frames[calc_iter][row][col] = grideye.getPixelTemperature(i);
    }
    delay(100);
  }
  Serial.println("Preprocessing complete");
  for(int i = 0; i < 10; i++)
  {
    print_precalculate(i);
  }
  calculate_background_frame(); // Calculating B.
  calculate_standard_deviation(); // Calculating S.
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
  Serial.println("Pushing data to OM2M");
  Serial.println("**************************************");
  // Format to send: Feature1,Feature2,Feature3
  delay(500);
  String data = String() + "{\"m2m:cin\":{\"con\":\"" + TotalActivePoints + "," + NumConnectedComponents + "," + sizeLargestComponent + "\"}}";
  send("/server/nirmal/grid_eye", 4, data);
}

void loop() {
  delay(1000);
  // Print the temperature value of each pixel in floating point degrees Celsius
  // separated by commas 
//   int j = 0;
//   for(unsigned char i = 0; i < 64; i++){
//     if(i % 8 == 0){
//       Serial.println();
//     }
//     Serial.print(grideye.getPixelTemperature(i));
//     Serial.print(",");
//   } 

    int row = 0;
    int col = 0;
    for(unsigned char i = 0; i < 64; i++){
        row = i/8;
        col = i%8;
        M[row][col] = grideye.getPixelTemperature(i);
    }

    print_current_thermal_values();

    calculate_feature_grid();

    calculate_features();


  // End each frame with a linefeed
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.println();
  Serial.println("**************************************");
  push(); // Pushing data to OM2M
  // Give Processing time to chew
  delay(100); // This is okay delay

  // Extra delay for testing
//   delay(20000);

}
