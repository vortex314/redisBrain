# redisBrain
Central brain that gets sensor data from redis and pushes commands to actuators via redis 

# Rationale
In a robotic application you want to easily integrate the following elements
- sensor real-time data
- actuator control
- static configuration data
- collect time series data of sensors => feed Grafana
- collect logs of what happened to the system => feed Elasticsearch
- have a plan or map on what the robot needs to do

# Some code
```
auto steerAngle = redis.publisher<float>("dst/drive/steer/angle");
auto compassDirection = redis.subscriber<float>("src/navigation/compass/degrees") 
// CONTROL logic 
compassDirection >> [](const float &direction){
  auto delta = desiredDirection - direction;
  steerAngle.on(delta);
  };
  // RECORDING the data for posterity
compassDirection >> [](const float direction){
    redis.command({"TS.ADD","compassDirection","*",std::to_string(direction),"LABELS","node","compass","unit","degrees"});
    };
    
compassDirection >> [](const float& direction){
      redis.command({"XADD","..."});
    };

compassDirection >> redis.ts<float>("ts:lm:compass",{{"node","lawnmower"},{"object","compass"}});
compassDirection >> redis.stream("str:log");

localPosition >> [](const Location& relativeLocation ){
  auto absoluteLocation = relativeLocation + referenceLocation
  redis.geo(absoluteLocation );
}
```

