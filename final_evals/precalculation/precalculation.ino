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
#include "HTTPClient.h"

// Timestamp
#include "time.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;

// OneM2M related
#include <WiFi.h>
#include "secret.h"
const String cse_ip = "esw-onem2m.iiit.ac.in";
const String server = "https://" + cse_ip + "/~/in-cse/in-name/Team-9";

// Features
const int FEATURE_HISTORY_NUM = 3;
int global_feature1 = 0;
int global_feature2 = 0;
int global_feature3 = 0;
int feature_history[FEATURE_HISTORY_NUM][3];

// Encryption
#include "AES.h"
#include "Base64.h"
#include "mbedtls/md.h"

AES aes;

char b64data[200];
byte cipher[1000];
byte iv[N_BLOCK];
byte key[] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
byte my_iv[N_BLOCK] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

uint8_t getrnd()
{
  uint8_t really_random = *(volatile uint8_t *)0x3FF20E44;
  return really_random;
}

// Generate a random initialization vector
void gen_iv(byte *iv)
{
  for (int i = 0; i < N_BLOCK; i++)
  {
    iv[i] = (byte)getrnd();
  }
}

String encrypt(String msg)
{
  aes.set_key(key, sizeof(key));
  gen_iv(my_iv);
  base64_encode(b64data, (char *)my_iv, N_BLOCK);
  Serial.println(" IV b64: " + String(b64data));
  int b64len = base64_encode(b64data, (char *)msg.c_str(), msg.length());
  aes.do_aes_encrypt((byte *)b64data, b64len, cipher, key, 128, my_iv);
  base64_encode(b64data, (char *)cipher, aes.get_size());
  Serial.println("Encrypted data in base64: " + String(b64data));
  return String(b64data);
}

String hash(String payload0)
{
  char *key = "ESWTEAM9";
  int mainmsg_len = payload0.length() + 1;
  char payload[mainmsg_len];
  payload0.toCharArray(payload, mainmsg_len);
  byte hmacResult[32];

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
  Serial.println(payload0.length());
  const size_t payloadLength = strlen(payload);
  const size_t keyLength = strlen(key);

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *)key, keyLength);
  mbedtls_md_hmac_update(&ctx, (const unsigned char *)payload, payloadLength);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  Serial.print("Hash: ");
  String final_hash = "";

  for (int i = 0; i < sizeof(hmacResult); i++)
  {
    char str[3];
    sprintf(str, "%02x", (int)hmacResult[i]);
    final_hash += str;
  }
  Serial.print(final_hash);
  return final_hash;
}

// // Timestamp
// const char* ntpServer = "pool.ntp.org";
// const long  gmtOffset_sec = 19800;   //Replace with your GMT offset (seconds)
// const int   daylightOffset_sec = 0;  //Replace with your daylight offset (seconds)

// ESP-Cam communication

const char *esp_cam_capture_address = "http://192.168.43.100/capture";

int unique_id = 10000000; // Starts from 10000

String uint64ToString(uint64_t input)
{
  String result = "";
  uint8_t base = 10;

  do
  {
    char c = input % base;
    input /= base;

    if (c < 10)
      c += '0';
    else
      c += 'A' - 10;
    result = c + result;
  } while (input);
  return result;
}
// WiFiClient client;
HTTPClient http;

// Grid eye related

GridEYE grideye;

const int NUM_FRAMES_PRECALCULATION = 250;
// >= 258 Frames aren't supported because of stack overflow. (Found using binary search)
// So taking 250 to be safe.

int TotalActivePoints = 0;      // Feature 1
int NumConnectedComponents = 0; // Feature 2
int sizeLargestComponent = 0;   // Feature 3

const int MAT_DIM = 8;
float pre_calc_frames[NUM_FRAMES_PRECALCULATION + 1][MAT_DIM][MAT_DIM];
float B[MAT_DIM][MAT_DIM]; // Background -- Mean of NUM_FRAMES_PRECALCULATION frames with no occupancy
float S[MAT_DIM][MAT_DIM]; // Standard deviation of each pixel in NUM_FRAMES_PRECALCULATION frames.
float M[MAT_DIM][MAT_DIM]; // Current thermal values
int F[MAT_DIM][MAT_DIM];   // Feature grid with active points

int visited[9][9];     // Used for DFS
int unique_components; // Number of connected components
int component_size[64];

void print_precalculate(int frame_num)
{
  Serial.print("Printing Frame: ");
  Serial.print(frame_num);
  Serial.println();

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      float cell_mean = 0;
      for (int frame_num = 0; frame_num < NUM_FRAMES_PRECALCULATION; frame_num++)
      {
        cell_mean += pre_calc_frames[frame_num][row][col];
      }
      cell_mean = cell_mean / (float)NUM_FRAMES_PRECALCULATION;
      B[row][col] = cell_mean;
    }
  }

  Serial.println("\nPrinting background grid.");
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      float cell_std = 0.0;
      for (int frame_num = 0; frame_num < NUM_FRAMES_PRECALCULATION; frame_num++)
      {
        cell_std += pow(pre_calc_frames[frame_num][row][col] - B[row][col], 2);
      }
      cell_std = cell_std / (float)NUM_FRAMES_PRECALCULATION;
      S[row][col] = sqrt(cell_std);
    }
  }

  Serial.println("\nPrinting std. grid.");
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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

  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      if (M[row][col] - B[row][col] > 3.0 * S[row][col])
      {
        F[row][col] = 1;
      }
      else
      {
        F[row][col] = 0;
      }
    }
  }

  Serial.println("Printing current feature grid.");
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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
  if (row < 0 || row > 7 || col < 0 || col > 7)
  {
    return;
  }
  if (visited[row][col] != 0 || F[row][col] == 0)
  {
    return;
  }
  visited[row][col] = component_num;
  DFS(row - 1, col - 1, component_num);
  DFS(row - 1, col, component_num);
  DFS(row - 1, col + 1, component_num);
  DFS(row, col - 1, component_num);
  DFS(row, col + 1, component_num);
  DFS(row + 1, col - 1, component_num);
  DFS(row + 1, col, component_num);
  DFS(row + 1, col + 1, component_num);
}

void DFS_init()
{
  unique_components = 0;
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      visited[row][col] = 0;
    }
  }
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      if (visited[row][col] == 0 && F[row][col] == 1)
      {
        DFS(row, col, ++unique_components);
      }
    }
  }
}

void find_size_largest_component()
{
  for (int i = 0; i < 64; i++)
  {
    component_size[i] = 0;
  }
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
      component_size[visited[row][col]]++;
    }
  }
  sizeLargestComponent = 0;
  for (int i = 1; i < 64; i++)
  { // Avoid zero
    if (component_size[i] > sizeLargestComponent)
    {
      sizeLargestComponent = component_size[i];
    }
  }
}

void calculate_features()
{
  Serial.println("Calculating and printing extracted features.");
  // Feature 1 - Total active points.
  TotalActivePoints = 0;
  for (int row = 0; row < 8; row++)
  {
    for (int col = 0; col < 8; col++)
    {
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

String grid_in_string()
{
  String grid_string = "0000000000000000000000000000000000000000000000000000000000000000";
  for (int i = 0; i < 64; i++)
  {
    int row = i / 8;
    int col = i % 8;
    grid_string[i] = F[row][col] == 1 ? '1' : '0';
  }
  return grid_string;
}

void setup()
{

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

  // Fetching time from web. Indian Standard Time.
  timeClient.begin();
  timeClient.setTimeOffset(19800);

  // Grid eye related
  for (int calc_iter = 0; calc_iter < NUM_FRAMES_PRECALCULATION; calc_iter++)
  {
    int row = 0;
    int col = 0;
    for (unsigned char i = 0; i < 64; i++)
    {
      row = i / 8;
      col = i % 8;
      pre_calc_frames[calc_iter][row][col] = grideye.getPixelTemperature(i);
    }
    delay(100);
  }
  Serial.println("Preprocessing complete");
  for (int i = 0; i < 10; i++)
  {
    print_precalculate(i);
  }
  calculate_background_frame();   // Calculating B.
  calculate_standard_deviation(); // Calculating S.
}

String send(String url, int ty, String rep)
{
  Serial.println("TO SEND");
  Serial.println(rep);
  HTTPClient http;
  http.begin(url);
  http.addHeader("X-M2M-Origin", "rTI9uxe@NN:7rn5sOIaYf");
  http.addHeader("Content-Type", "application/json;ty=" + String(ty));
  //http.addHeader("Content-Length", "100");
  http.addHeader("Connection", "close");
  int code = http.POST(rep);
  http.end();
  // Serial.print(code);
  // delay(300);
  return (String(code));
}

void push()
{
  Serial.println("Pushing data to OM2M");
  Serial.println("**************************************");

  // Timestamp
  while (!timeClient.update())
  {
    timeClient.forceUpdate();
  }
  String time_stamp = timeClient.getFormattedDate();
  String grid_string = grid_in_string();
  String info_to_send = String() + unique_id + "," + TotalActivePoints + "," + NumConnectedComponents + "," + sizeLargestComponent + "," + grid_string + "," + time_stamp;

  // info_to_send = encrypt(info_to_send);
  String info_hash = hash(info_to_send);
  info_to_send = encrypt(info_to_send);
  info_to_send = info_to_send + "," + info_hash;
  // Format to send: Unique_ID,Feature1,Feature2,Feature3,TimeStamp
  // delay(500);
  String data = String() + "{\"m2m:cin\":{\"con\":\"" + info_to_send + "\"}}";
  Serial.println(send(server + "/testing/", 4, data));
}

void loop()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected in while.");
    Serial.print("Local IP: http://");
    Serial.print(WiFi.localIP());
    Serial.println("' to connect");
  }
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

  // Sending request to espcam server to take image and save it with name unique_id
  WiFiClient client;
  // http.begin(client, esp_cam_capture_address);
  // int httpResponseCode = http.POST(uint64ToString(unique_id));
  // Serial.print("HTTP Response code: ");
  // Serial.println(httpResponseCode);

  // // Free resources
  // http.end();
  global_feature1 = 0;
  global_feature2 = 0;
  global_feature3 = 0;
  for (int features_i = 0; features_i < FEATURE_HISTORY_NUM; features_i++)
  {
    int row = 0;
    int col = 0;
    for (unsigned char i = 0; i < 64; i++)
    {
      row = i / 8;
      col = i % 8;
      M[row][col] = grideye.getPixelTemperature(i);
    }

    // print_current_thermal_values();

    calculate_feature_grid();

    calculate_features();
    feature_history[features_i][0] = TotalActivePoints;
    feature_history[features_i][1] = unique_components;
    feature_history[features_i][2] = sizeLargestComponent;
    int update_global_feature = 0;
    if(sizeLargestComponent > global_feature3){
      update_global_feature = 1;
    }
    if(sizeLargestComponent == global_feature3){
      if(TotalActivePoints > global_feature1){
        update_global_feature = 1;
      }else if(TotalActivePoints == global_feature1){
        if(unique_components < global_feature2){
          update_global_feature = 1;
        }
      }
    }
  }

  // End each frame with a linefeed
  Serial.println();
  Serial.println("-------------------------------------");
  Serial.println();
  Serial.println("**************************************");
  push();      // Pushing data to OM2M
  delay(3000); // Delay of 5 seconds to detect occupancy
  unique_id++;
}
