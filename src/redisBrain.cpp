#include <Config.h>
#include <Hardware.h>
#include <Log.h>
#include <limero.h>
#include <stdio.h>
#include <unistd.h>

#include "Redis.h"
Log logger;

std::string loadFile(const char *name);
bool loadConfig(JsonObject cfg, int argc, char **argv);

int main(int argc, char **argv) {
  INFO("Start %s", argv[0]);
  DynamicJsonDocument config(10240);
  loadConfig(config.to<JsonObject>(), argc, argv);
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
    //  INFO("Got response : %s", s.c_str());
  };

  redis.subscriber<int>("src/x/y/z") >> redis.publisher<int>("dst/x/y/z");
  redis.subscriber<std::string>("src/x/y/a") >>
      redis.publisher<std::string>("dst/x/y/a");

  workerThread.run();
}

void deepMerge(JsonVariant dst, JsonVariant src) {
  if (src.is<JsonObject>()) {
    for (auto kvp : src.as<JsonObject>()) {
      deepMerge(dst.getOrAddMember(kvp.key()), kvp.value());
    }
  } else {
    dst.set(src);
  }
}

bool loadConfig(JsonObject cfg, int argc, char **argv) {
  // override args
  int c;
  while ((c = getopt(argc, argv, "f:v")) != -1) switch (c) {
      case 'f': {
        std::string s = loadFile(optarg);
        DynamicJsonDocument doc(10240);
        deserializeJson(doc, s);
        deepMerge(cfg, doc);
        break;
      }
      case 'v': {
        logger.setLevel(Log::L_DEBUG);
        break;
      }
      case '?':
        printf("Usage %s -v -f <configFile.json>", argv[0]);
        break;
      default:
        WARN("Usage %s -v -f <configFile.json> ", argv[0]);
        abort();
    }
  std::string s;
  serializeJson(cfg, s);
  INFO("config:%s", s.c_str());
  return true;
};
