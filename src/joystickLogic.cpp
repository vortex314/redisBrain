#include <Redis.h>
#include <limero.h>
#include <stdint.h>

extern Log logger;

template <typename T>
T scale(T x, T x1, T x2, T y1, T y2) {
  return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

template <typename IN, typename OUT>
Flow<IN, OUT> &flow(std::function<bool(OUT &out, const IN &in)> f) {
  return *new Flow<IN, OUT>(f);
}

template <typename IN, typename OUT>
Flow<IN, OUT> &timeout(Thread &thread, int timeout_msec, OUT v) {
  TimerSource &timer =
      thread.createTimer(timeout_msec, true, true, "timeout_timer");
  auto &lf = *new Flow<IN, OUT>([&](OUT &out, const IN &in) {
    timer.start();
    return false;
  });
  timer >> [&, v](const TimerSource &tm) { lf.emit(v); };
  // lf.name((std::to_string(timeout_msec) + " msec timeout").c_str());
  return lf;
}

template <typename IN, typename OUT>
Flow<IN, OUT> &map(IN v1, OUT v2) {
  auto lf = new Flow<IN, OUT>([v1, v2](OUT &out, const IN &in) {
    if (in == v1) {
      out = v2;
      return true;
    }
    return false;
  });
  lf->name(("map(" + std::to_string(v1) + " -> " + std::to_string(v2) + ")")
               .c_str());
  return *lf;
}

template <typename T>
Flow<T, T> &when(ValueFlow<int>& stateFunction, const int stateValue) {
  auto lf = new Flow<T, T>([&stateFunction, stateValue](T &out, const T &in) {
    if (stateFunction() == stateValue) {
      out = in;
      return true;
    }
    return false;
  });
  lf->name(("when(" + std::to_string(stateValue) + ")").c_str());
  return *lf;
}

template <typename IN, typename OUT>
Flow<IN, OUT> &log(const char *s) {
  return *new Flow<IN, OUT>([s](OUT &out, const IN &in) {
    INFO("%s ", s);
    out = in;
    return true;
  });
}

template <typename IN, typename OUT>
Flow<IN, OUT> &range(IN inMultiplier, IN x1, IN x2, OUT outMultiplier, OUT y1,
                     OUT y2) {
  auto lf = new Flow<IN, OUT>([=](OUT &out, const IN &in) {
    out = outMultiplier * scale(inMultiplier * in, x1, x2, y1, y2);
    INFO("range %d,%d -> %d,%d for %d -> %d", x1, x2, y1, y2, in, out);
    return true;
  });
  lf->name(("range(" + std::to_string(x1) + "," + std::to_string(x2) + " -> " +
            std::to_string(y1) + "," + std::to_string(y2) + ")")
               .c_str());
  return *lf;
}
void joystickLogic(Redis &r, Thread &workerThread) {
  // STATE
  auto &power_on_state = *new ValueFlow<int>(workerThread);
  power_on_state = 0;
  power_on_state.name("power_on_state");
  r.subscriber<int>("dst/limero/powerOn") >> power_on_state >>
      r.publisher<int>("src/limero/powerOn");
  enum { REMOTE_CONTROL, SLEEP, LEARNING, CUTTING };
  auto &main_state = *new ValueFlow<int>(workerThread);
  main_state.name("main_state");
  main_state = REMOTE_CONTROL;
  auto &onRemote = when<int>(main_state, REMOTE_CONTROL);

  // INPUT

  auto &power_on_button = r.subscriber<int>("src/joystick/button/0");
  auto &power_off_button = r.subscriber<int>("src/joystick/button/1");
  auto &remote_on_button = r.subscriber<int>("src/joystick/button/2");

  auto &collison_left = r.subscriber<int>("src/sideboard/sensor/left");
  auto &collison_right = r.subscriber<int>("src/sideboard/sensor/right");
  auto &steer_joystick = r.subscriber<int>("src/joystick/axis/2");
  auto &speed_joystick = r.subscriber<int>("src/joystick/axis/3");
  auto &joystick_reader = r.subscriber<bool>("src/joystick/system/alive");

  // OUTPUT

  auto &pubWatchdog = r.publisher<bool>("dst/hover/motor/watchdogReset");
  auto &redisBrainAlive = r.publisher<bool>("src/redisBrain/system/alive");
  auto &hover_motor_speed = r.publisher<int>("dst/hover/motor/speedTarget");
  //  auto &hover_motor_steer = r.publisher<int>("dst/hover/motor/angleTarget");
  auto &hover_motor_steer = r.publisher<int>("dst/hover/motor/angleDelta");
  auto &power_relais_1 = r.publisher<int>("dst/raspi/gpio9/value");
  auto &power_relais_2 = r.publisher<int>("dst/raspi/gpio15/value");

  TimerSource &timerWatchdog =
      workerThread.createTimer(1000, true, true, "ticker");

  speed_joystick >> onRemote >> range(1, -32768, +32768, -1, -1000, +1000) >>
      hover_motor_speed;

  steer_joystick >> onRemote >> range(-1, -32768, +32768, +1, -90, +90) >>
      hover_motor_steer;

  remote_on_button >> map<int, int>(1, REMOTE_CONTROL) >> main_state;

  power_on_button >> onRemote >> map(1, 1) >> power_on_state;
  power_off_button >> onRemote >> map(1, 0) >> power_on_state;
  joystick_reader >> timeout<bool, int>(workerThread, 3000, 0) >>
      power_on_state;
  collison_left >> map(1, 0) >> power_on_state;
  collison_right >> map(1, 0) >> power_on_state;

  power_on_state >> [](int v) {
  //  INFO("power_on_state = %d via %s", v, logStack.toString().c_str());
  };

  power_on_state >> map(1, 0) >> power_relais_1;  // power on relais 1
  power_on_state >> map(1, 0) >> power_relais_2;  // power on relais 2
  power_on_state >> map(0, 1) >> power_relais_1;  // power off relais 1
  power_on_state >> map(0, 1) >> power_relais_2;  // power off relais 2

  timerWatchdog >> [&](const TimerSource &) {
    pubWatchdog.on(true);
    redisBrainAlive.on(true);
    power_on_state.on(power_on_state());
  };
  workerThread.run();
}