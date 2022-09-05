#include <Redis.h>
#include <limero.h>
#include <stdint.h>

extern Log logger;

template <typename T>
T scale(T x, T x1, T x2, T y1, T y2) {
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

template <typename IN, typename OUT>
LambdaFlow<IN, OUT> &flow(std::function<bool(OUT &out, const IN &in)> f) {
  return *new LambdaFlow<IN, OUT>(f);
}

template <typename IN, typename OUT>
LambdaFlow<IN, OUT> &map(IN v1, OUT v2) {
  return *new LambdaFlow<IN, OUT>([v1, v2](OUT &out, const IN &in) {
    if (in == v1) {
      out = v2;
      return true;
    }
    return false;
  });
}

template <typename IN, typename OUT>
LambdaFlow<IN, OUT> &range(IN inMultiplier, IN x1, IN x2, OUT outMultiplier,
                           OUT y1, OUT y2) {
  return *new LambdaFlow<IN, OUT>([&](OUT &out, const IN &in) {
    out = outMultiplier * scale(inMultiplier * in, x1, x2, y1, y2);
    return true;
  });
}
void joystickLogic(Redis &r, Thread &workerThread) {
  TimerSource *timerWatchdog =
      new TimerSource(workerThread, 1000, true, "ticker");

  auto &pubWatchdog = r.publisher<bool>("dst/hover/motor/watchdogReset");
  auto &redisBrainAlive = r.publisher<bool>("src/redisBrain/system/alive");

  *timerWatchdog >> [&](const TimerMsg &) {
    pubWatchdog.on(true);
    redisBrainAlive.on(true);
  };
  /*
    r.subscriber<int32_t>("src/joystick/axis/3") >>
        *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
          out = -scale(in, -32768, +32768, -1000, +1000);
          return true;
        }) >>
        r.publisher<int32_t>("dst/hover/motor/speedTarget");

    r.subscriber<int32_t>("src/joystick/axis/2") >>
        *new LambdaFlow<int32_t, int32_t>([&](int32_t &out, const int32_t &in) {
          out = scale(-in, -32768, +32768, -90, +90);
          return true;
        }) >>
        r.publisher<int32_t>("dst/hover/motor/angleTarget");
  */

  r.subscriber<int32_t>("src/joystick/axis/3") >>
      range(1, -32768, +32768, -1000, +1000, -1) >>
      r.publisher<int32_t>("dst/hover/motor/speedTarget");

  r.subscriber<int32_t>("src/joystick/axis/2") >>
      range(-1, -32768, +32768, -90, +90, +1) >>
      r.publisher<int32_t>("dst/hover/motor/angleTarget");

  auto button_0 = r.subscriber<int>("src/joystick/button/0");
  auto button_1 = r.subscriber<int>("src/joystick/button/1");
  auto &pubPowerOn = r.publisher<int>("src/limero/powerOn");
  auto subPowerOn = r.subscriber<int>("src/limero/powerOn");

  button_0 >> map(1, 1) >> pubPowerOn;
  button_1 >> map(1, 0) >> pubPowerOn;
  subPowerOn >> map(1, 0) >> r.publisher<int>("dst/raspi/gpio9/value");
  subPowerOn >> map(1, 0) >> r.publisher<int>("dst/raspi/gpio15/value");
  subPowerOn >> map(0, 1) >> r.publisher<int>("dst/raspi/gpio9/value");
  subPowerOn >> map(0, 1) >> r.publisher<int>("dst/raspi/gpio15/value");

  auto collisonLeft = r.subscriber<int>("src/sideboard/sensor/left");
  auto collisonRight = r.subscriber<int>("src/sideboard/sensor/right");
  collisonLeft >> map(1, 0) >> pubPowerOn;
  collisonRight >> map(1, 0) >> pubPowerOn;
}