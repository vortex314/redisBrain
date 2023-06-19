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

# some thoughts 
To avoid too high traffic you want properties to be only communicated on change or on a max frequency
You don't want to limit the rate at the reciver
You want to choose the properties
The device should publisize teh properties
The properties should contain 
- key         : tree structure naming convention src/dst -- owner -- object -- property
- value       : primitive values ( int,float,string ) or complex ( JSON structure )
- updateTime  : last update in UTC
- desc        : text description of property
- mode        : read + write 
- expireTime  : auto delete of the property

Commands on this in memory dataset
- publish
-subscribe in different modes : all changes , on change , latest value every interval
- info : give all surrounding info and metadata

RedisJson seems a good solution , but it combines different technologies : RedisJson, Lua , PubSub 
The time interval triggering looks a challenge 


