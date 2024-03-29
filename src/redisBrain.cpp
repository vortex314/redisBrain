#include <ConfigFile.h>
#include <Hardware.h>
#include <Log.h>
#include <limero.h>
#include <stdio.h>
#include <unistd.h>

#include "Redis.h"
Log logger;

std::string loadFile(const char *name);
bool loadConfig(JsonObject cfg, int argc, char **argv);
int timeOfDay();

extern void joystickLogic(Redis &, Thread &);
int timeOfDay();

int main(int argc, char **argv) {
  INFO("Loading configuration.");
  Json config;
  config["redis"]["host"] = "localhost";
  config["redis"]["port"] = 6379;
  configurator(config, argc, argv);
  Thread workerThread("worker");

  Redis redis(workerThread, config["redis"].as<JsonObject>());

  redis.connect();

  Json helloCmd(1024);
  helloCmd[0] = "hello";
  helloCmd[1] = "3";
  redis.request().on(helloCmd);

  redis.response() >> [&](const Json &json) {
    std::string s;
    serializeJson(json, s);
    INFO("Got response : %s", s.c_str());
  };

  //  joystickLogic(redis, workerThread);

  auto &ticker = workerThread.createTimer(300000, true, true, "ticker");
  auto &alive_ticker = workerThread.createTimer(1000, true, true, "alive_ticker");

  ValueFlow<int	> shake(workerThread);
  ;
  shake >> redis.publisher<int	>("dst/shaker1/shake/trigger");
  shake >> redis.publisher<int	>("dst/shaker2/shake/trigger");
  shake >> redis.publisher<int	>("dst/shaker3/shake/trigger");
  redis.subscriber<uint32_t>("dst/brain/shaker/interval") >>
      [&](const uint32_t &count) {
        INFO("Got interval %d", count);
        ticker.interval(count);
      };


  ticker >> [&shake](const TimerSource &) {
    int now = timeOfDay();
    if (now > 529 && now < 2200) {
      INFO("let's shake it %d ", now);
      shake.on(1);
    } else {
      INFO(" sleeping.... ");
    }
  };

  auto &alive_publisher= redis.publisher<int>("src/redisBrain/sys/alive");

  alive_ticker >> [&alive_publisher](const TimerSource&) {
	  const int x=1;
	  alive_publisher.on(x);
  };

  workerThread.run();
}

#include <sys/time.h>
int timeOfDay() {
  struct timeval tv;
  struct timezone tz;
  time_t t;
  struct tm *info;

  gettimeofday(&tv, NULL);
  t = tv.tv_sec;
  info = localtime(&t);
  return (info->tm_hour * 100) + info->tm_min;
}
