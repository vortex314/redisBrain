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
redis.subscriber<float>("src/navigation/compass/degrees") >> [](const float &direction){
  auto delta = desiredDirection - direction;
  steerAngle.on(delta);
  };
```

