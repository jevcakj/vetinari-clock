#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <time.h>

#include "index.h"
#include "nastaveni.h"

typedef struct{
  ulong stime;
  ulong etime;
  int max_diff;
  char stimeS[6];
  char etimeS[6];
  int min;
  int sec;
  bool speed;
} time_range_t;

typedef struct{
  double config_time;
  size_t range_count;
  time_range_t ranges[10];
} config_t;

const char *ssid = "Jan's Galaxy A52s 5G";
const char *password = "1udij194";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char *ntpServer1 = "0.cz.pool.ntp.org";
const char *ntpServer2 = "0.europe.pool.ntp.org";
const char *ntpServer3 = "time.google.com";
const long gmtOffset = 3600;
const int daylightOffset = 3600;

config_t config;
time_range_t compl_ranges[10];
const char *date_format = "%Y-%m-%dT%H:%M:%S";
time_t real_time;
ulong range_end_millis;
time_t virt_time;
bool current_speed = 0;

//  WebSocket
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
    ws.onEvent(onEvent);
  server.addHandler(&ws);
}

// HTTP
String processor(const String &var) {
  Serial.println(var);
  if(var =="DATETIME"){
    char buff[20];
    time_t curr;
    time(&curr);
    curr += config.config_time;
    strftime(buff, 20, date_format, gmtime(&curr));
    Serial.print("Index time: ");
    Serial.println(buff);
    return buff;
  }
  return String();
}

ulong parseTime(const char* time){
  config;
  int h, m;
  sscanf(time, "%d:%d", &h, &m);
  return h * 3600 + m * 60;
}

void handle_data(AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, data, len);
  if(error){
    req->send(400, "text/html", "Invalid JSON");
  }
  const char* timeS = doc["time"];
  int year, month, day, hour, minute, second;
  time_t now;
  time(&now);
  Serial.println(timeS);
  sscanf(timeS, "%d-%d-%dT%d:%d", &year, &month, &day, &hour, &minute);
  struct tm time_tm{
    .tm_sec = localtime(&now)->tm_sec,
    .tm_min = minute,
    .tm_hour = hour,
    .tm_mday = day,
    .tm_mon = month - 1,
    .tm_year = year - 1900
  };
  // rozdil od aktualniho casu
  config.config_time = (double)mktime(&time_tm) - (double)now;

  JsonArray ranges = doc["ranges"];
  config.range_count = ranges.size();
  for(size_t i = 0; i < config.range_count; i++){
    JsonObject range = ranges[i];

    const char *stimeS = range["stime"];
    ulong stime = parseTime(stimeS);
    const char *etimeS = range["etime"];
    ulong etime = parseTime(etimeS);
    const char *minS = range["min"];
    const char *secS = range["sec"];
    const char *speedS = range["speed"];
    
    time_range_t *c_range = &config.ranges[i];
    strcpy(c_range->stimeS, stimeS);
    strcpy(c_range->etimeS, etimeS);
    c_range->min = atoi(minS);
    c_range->sec = atoi(secS);
    c_range->speed = atoi(speedS) == 1;

    c_range->stime = stime;
    c_range->etime = etime;
    c_range->max_diff = c_range->min * 60 + c_range->sec;

    // vytvorit vyrovnavaci range
    compl_ranges[i].stime = c_range->etime;
    ulong compl_range_len = c_range->speed == 1 ? c_range->max_diff * 2 : c_range->max_diff / 2 ;
    compl_ranges[i].etime = c_range->etime + compl_range_len;
    compl_ranges[i].min = c_range->min > 1 ? c_range->min / 2 : 1;
    compl_ranges[i].sec = 0;
    compl_ranges[i].max_diff = compl_ranges[i].min * 60;
    compl_ranges[i].speed = c_range->speed == 0;
  }

  req->send(200);
}

void handle_config(AsyncWebServerRequest *req){
  JsonDocument doc;
  char buff[17];
  time_t curr;
  time(&curr);
  curr += config.config_time;
  strftime(buff, 17, "%Y-%m-%dT%H:%M", gmtime(&curr));
  doc["time"] = buff;

  JsonArray ranges = doc.createNestedArray("ranges");
  for(size_t i = 0; i < config.range_count; i++){
    JsonObject range = ranges.createNestedObject();
    range["stime"] = config.ranges[i].stimeS;
    range["etime"] = config.ranges[i].etimeS;
    range["min"] = String(config.ranges[i].min);
    range["sec"] = String(config.ranges[i].sec);
    range["speed"] = String(config.ranges[i].speed ? 1 : 0);
  }
  String res;
  serializeJson(doc, res);

  req->send(200, "application/json", res);
}

void notify() {
  ws.textAll("a");
}

//  Time
ulong seconds_of_day(time_t now){
  struct tm time_tm;
  localtime_r(&now, &time_tm);
  return time_tm.tm_hour * 3600 + time_tm.tm_min * 60 + time_tm.tm_sec;
}

time_range_t* get_current_range(time_t now){
  ulong secs = seconds_of_day(now);
  for(size_t i = 0; i <config.range_count; i++){
    time_range_t* range = &config.ranges[i];
    if(secs > range->stime && secs < range->etime){
      return range;
    }
    time_range_t* compl_range = &compl_ranges[i];
    if(secs > compl_range->stime && secs < compl_range->etime){
      return compl_range;
    }
  }
  return nullptr;
}

int handle_classic(ulong start_millis){
  struct tm real_tm;
  localtime_r(&real_time, &real_tm);

  struct tm virt_tm;
  localtime_r(&virt_time, &virt_tm);

  if(virt_tm.tm_sec == 0){
    range_end_millis = start_millis + 60000;
  }

  // muj virtualni cas
  ulong ms_remaining = range_end_millis - start_millis;
  ulong tics_remaining = 60 - virt_tm.tm_sec;
  ulong tick_avg = ms_remaining / tics_remaining;
  // kolik zbyva prumerne na kazdy tik do konce minuty
  ulong bound = ms_remaining / (60 - real_tm.tm_sec) > 100 ? 500 : 100;
  int tick_ms = virt_tm.tm_sec == 59 ? ms_remaining : random(tick_avg - bound, tick_avg + bound);
  return tick_ms;
}

int handle_range(ulong start_millis, time_range_t* range){
  int tick_ms = 1000;
  struct tm real_tm;
  localtime_r(&real_time, &real_tm);

  struct tm virt_tm;
  localtime_r(&virt_time, &virt_tm);

  if(seconds_of_day(real_time) == range->stime){
    range_end_millis = start_millis + (range->etime - range->stime) * 1000;
  }

  if(current_speed){
    //faster
    tick_ms = random(500, 1100);
  }
  else{
    // slower
    tick_ms = random(900, 1500);
  }

  if( virt_time < real_time - range->max_diff ){
      //jsem v case pozadu, musim pridat
      current_speed = true;
    }
    else if(virt_time > real_time + range->max_diff){
      // jsem v case napred, musim zpomalit
      current_speed = false;
    }
    else if(virt_time > real_time - (range->max_diff * 9)/10 && virt_time < real_time + (range->max_diff * 9)/10){
      current_speed = range->speed == 1;
    }
    return tick_ms;
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  Serial.println(WiFi.localIP());

  configTime(gmtOffset, daylightOffset, ntpServer1, ntpServer2, ntpServer3 );

  struct tm loc_time;
  while(!getLocalTime(&loc_time)){
    Serial.println("Waiting for ntp");
    delay(1000);
  }

  config.config_time = 0;
  config.range_count = 0;

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html, processor);
  });
  server.on("/nastaveni.html", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", nastaveni_html, processor);
  });
  server.on("/config", handle_config);
  server.on(
    "/data", 
    HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    handle_data
  );

  server.begin();
  real_time = time(NULL);
  virt_time = time(NULL);
  struct tm time_tm;
  localtime_r(&real_time, &time_tm);
  range_end_millis = millis() + (60 - time_tm.tm_sec) * 1000;
  Serial.println("Setup finished");
}

void loop() {
  ulong start_millis = millis();
  
  notify();

  real_time = time(NULL);
  virt_time++;

  time_range_t *range = get_current_range(real_time);
  int tick_ms = 1000;
  if(range){
    tick_ms = handle_range(start_millis, range);
  }
  else{
    tick_ms = handle_classic(start_millis);
  }
  delay(tick_ms - (millis() - start_millis));
}