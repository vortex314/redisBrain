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
LambdaFlow<IN, OUT> &timeout(Thread &thread, int to, OUT v) {
  TimerSource &timer = *new TimerSource(thread, 1000, true, "timer");
  auto &lf = *new LambdaFlow<IN, OUT>([&](OUT &out, const IN &in) {
    timer.reset();
    return false;
  });
  timer >> [&](const TimerMsg &tm) { lf.emit(v); };
  return lf;
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
LambdaFlow<IN, OUT> *range(IN inMultiplier, IN x1, IN x2, OUT outMultiplier,
                           OUT y1, OUT y2) {
  return new LambdaFlow<IN, OUT>([=](OUT &out, const IN &in) {
    out = outMultiplier * scale(inMultiplier * in, x1, x2, y1, y2);
    INFO("range %d,%d -> %d,%d for %d -> %d", x1, x2, y1, y2, in, out);
    return true;
  });
}
void joystickLogic(Redis &r, Thread &workerThread) {
  // STATE
  auto &power_on_state = *new ValueFlow<int>(0);
  r.subscriber<int>("dst/limero/powerOn") >> power_on_state >>
      r.publisher<int>("src/limero/powerOn");
  // INPUT
  auto &power_on_button = r.subscriber<int>("src/joystick/button/0");
  auto &power_off_button = r.subscriber<int>("src/joystick/button/1");
  auto &collison_left = r.subscriber<int>("src/sideboard/sensor/left");
  auto &collison_right = r.subscriber<int>("src/sideboard/sensor/right");
  auto &steer_joystick = r.subscriber<int>("src/joystick/axis/2");
  auto &speed_joystick = r.subscriber<int>("src/joystick/axis/3");
  auto &joystick_reader = r.subscriber<bool>("src/joystick/system/alive");

  // OUTPUT

  auto &pubWatchdog = r.publisher<bool>("dst/hover/motor/watchdogReset");
  auto &redisBrainAlive = r.publisher<bool>("src/redisBrain/system/alive");
  auto &hover_motor_speed = r.publisher<int>("dst/hover/motor/speedTarget");
  auto &hover_motor_steer = r.publisher<int>("dst/hover/motor/angleTarget");
  auto &power_relais_1 = r.publisher<int>("dst/raspi/gpio9/value");
  auto &power_relais_2 = r.publisher<int>("dst/raspi/gpio15/value");

  TimerSource *timerWatchdog =
      new TimerSource(workerThread, 1000, true, "ticker");

  speed_joystick >> range(1, -32768, +32768, -1, -1000, +1000) >>
      hover_motor_speed;

  steer_joystick >> range(-1, -32768, +32768, +1, -90, +90) >>
      hover_motor_steer;

  power_on_button >> map(1, 1) >> power_on_state;
  power_off_button >> map(1, 0) >> power_on_state;
  joystick_reader >> timeout<bool,int>(workerThread, 3000, 0) >> power_on_state;
  collison_left >> map(1, 0) >> power_on_state;
  collison_right >> map(1, 0) >> power_on_state;

  power_on_state >> map(1, 0) >> power_relais_1;  // power on relais 1
  power_on_state >> map(1, 0) >> power_relais_2;  // power on relais 2
  power_on_state >> map(0, 1) >> power_relais_1;  // power off relais 1
  power_on_state >> map(0, 1) >> power_relais_2;  // power off relais 2

  *timerWatchdog >> [&](const TimerMsg &) {
    pubWatchdog.on(true);
    redisBrainAlive.on(true);
    power_on_state.request();
  };
  workerThread.run();
}