#include <Redis.h>
#include <limero.h>
#include <stdint.h>

extern Log logger;

template <typename T>
T scale(T x, T x1, T x2, T y1, T y2) {
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

void joystickLogic(Redis &redis, Thread &workerThread) {
  TimerSource *timerWatchdog =
      new TimerSource(workerThread, 1000, true, "ticker");

  auto &pubWatchdog = redis.publisher<bool>("dst/hover/motor/watchdogReset");
  auto &redisBrainAlive = redis.publisher<bool>("src/redisBrain/system/alive");

  *timerWatchdog >> [&](const TimerMsg &) {
    pubWatchdog.on(true);
    redisBrainAlive.on(true);
  };

  redis.subscriber<int32_t>("src/joystick/axis/3") >>
      *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
        out = -scale(in, -32768, +32768, -1000, +1000);
        return true;
      }) >>
      redis.publisher<int32_t>("dst/hover/motor/speedTarget");

  redis.subscriber<int32_t>("src/joystick/axis/2") >>
      *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
        out = scale(in, -32768, +32768, -90, +90);
        return true;
      }) >>
      redis.publisher<int32_t>("dst/hover/motor/angleTarget");
}