#include <ConfigFile.h>
#include <Hardware.h>
#include <Log.h>
#include <limero.h>
#include <stdio.h>
#include <unistd.h>

#include "Redis.h"
Log logger;

bool loadConfig(JsonObject cfg, int argc, char **argv);

extern void joystickLogic(Redis &, Thread &);
extern void treeShaker(Redis &, Thread &);

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
    if (json.is<JsonArray>() && json[0] != "pmessage") {
      INFO("Got response %s", s.c_str());
    }
  };

  joystickLogic(redis, workerThread);
//  treeShaker(redis, workerThread);
}
