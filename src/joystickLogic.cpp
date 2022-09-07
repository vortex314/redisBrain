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
      INFO("map %d -> %d for %d -> %d", v1, v2, in, out);
      return true;
    }
    return false;
  });
}

template <typename IN, typename OUT>
LambdaFlow<IN, OUT> &timeout(Thread &thread, int timeout_msec, OUT v1) {
  TimerSource &ts = *new TimerSource(thread, timeout_msec, true);

  return *new LambdaFlow<IN, OUT>(
      [v1, ts](OUT &out, const IN &in) { return false; });
}

template <typename IN, typename OUT>
LambdaFlow<IN, OUT> *range(IN inMultiplier, IN x1, IN x2, OUT outMultiplier,
                           OUT y1, OUT y2) {
  return new LambdaFlow<IN, OUT>([=](OUT &out, const IN &in) {
    out = outMultiplier * scale(inMultiplier * in, x1, x2, y1, y2);
    INFO("range %d,%d -> %d,%d for %d -> %d", x1, x2, y1, y2, in, out);
    return true;
  });
}
void joystickLogic(Redis &r, Thread &workerThread) {
  TimerSource *timerWatchdog =
      new TimerSource(workerThread, 1000, true, "ticker");

  auto &pubWatchdog = r.publisher<bool>("dst/hover/motor/watchdogReset");
  auto &redisBrainAlive = r.publisher<bool>("src/redisBrain/system/alive");

  r.subscriber<int32_t>("src/joystick/axis/3") >>
      range(1, -32768, +32768, -1, -1000, +1000) >>
      r.publisher<int32_t>("dst/hover/motor/speedTarget");

  r.subscriber<int32_t>("src/joystick/axis/2") >>
      range(-1, -32768, +32768, +1, -90, +90) >>
      r.publisher<int32_t>("dst/hover/motor/angleTarget");

  auto &button_0 = r.subscriber<int>("src/joystick/button/0");
  auto &button_1 = r.subscriber<int>("src/joystick/button/1");
  auto &joystickAlive = r.subscriber<bool>("src/joystick/system/alive")

                            auto &powerOn = *new ValueFlow<int>(0);
  r.subscriber<int>("dst/limero/powerOn") >> powerOn >>
      r.publisher<int>("src/limero/powerOn");

  joystickAlive >> timeout(0) >> powerOn;
  button_0 >> map(1, 1) >> powerOn;
  button_1 >> map(1, 0) >> powerOn;
  powerOn >> map(1, 0) >> r.publisher<int>("dst/raspi/gpio9/value");
  powerOn >> map(1, 0) >> r.publisher<int>("dst/raspi/gpio15/value");
  powerOn >> map(0, 1) >> r.publisher<int>("dst/raspi/gpio9/value");
  powerOn >> map(0, 1) >> r.publisher<int>("dst/raspi/gpio15/value");

  r.subscriber<int>("src/sideboard/sensor/left") >> map(1, 0) >> powerOn;
  r.subscriber<int>("src/sideboard/sensor/right") >> map(1, 0) >> powerOn;

  auto &pubSrcPowerOn = r.publisher<int>("src/limero/powerOn");

  *timerWatchdog >> [&](const TimerMsg &) {
    pubWatchdog.on(true);
    redisBrainAlive.on(true);
    powerOn.request();
  };
  workerThread.run();
}