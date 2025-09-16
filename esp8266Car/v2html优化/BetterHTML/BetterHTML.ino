#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncTCP.h>
// --- 新增开始 ---
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
// --- 新增结束 ---


// 使用宏定义，定义ESP8266引脚(D8,D7,D6,D5,D4,D3)，它们用于控制小车的电机和方向。
#define ENA 14      // L298N: ENA -> ESP8266:D5
#define ENB 12      // L298N :ENB -> ESP8266:D6
#define IN_1 15     // L298N: IN1 -> ESP8266:D8
#define IN_2 13     // L298N: IN2 -> ESP8266:D7
#define IN_3 2      // L298N: IN3 -> ESP8266:D4
#define IN_4 0      // L298N: IN4 -> ESP8266:D3

// --- 新增开始 ---
// 创建 PCA9685 驱动对象
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// 定义舵机参数
#define SERVOMIN  150 // 0度时的脉冲长度
#define SERVOMAX  600 // 180度时的脉冲长度
#define SERVO_FREQ 50 // 舵机频率
// --- 新增结束 ---

void handleDirection(String dir, int speedCar);
// 定义WiFi的用户名和密码，PARAM_dir参数
// const char* ssid = "FS_ROBOT";
// const char* password = "hqyj12345678";
const char* ssid = "iPhone11pro";
const char* password = "asdqwedsa";
const char* PARAM_dir = "dir";

// 创建一个AsyncWebServer 80端口，可以通过该端口访问服务器
AsyncWebServer server(80);

// 定义一个HTML页面，其中包含了小车控制的相关控件，这里使用HTML创建UI界面，JavaScript建立调用函数
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<link rel="shortcut icon" href="#" />
  <title>WiFi 遥控小车</title>
  <meta charset="UTF-8">
  <meta http-equiv="X-UA-Compatible" content="IE=edge">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
        body {
            background-color: rgb(0,0,0);
            color:rgb(254,220,0);
            font-family: Arial;
            text-align: center;
            /* 禁止在移动设备上长按选择文本或弹出菜单 */
            -webkit-user-select: none;
            -moz-user-select: none;
            -ms-user-select: none;
            user-select: none;
        }
        h2{
            font-size: 5.0rem;
        }
        button {
            text-align: center;
            height: auto;
            font-size: 10px;
            margin: 10px;
            padding: 20px 40px;
            border-radius: 5px;
            background-color: rgb(254,220,0);
            color: rgb(0,0,0);
            border: none;
            cursor: pointer;
            transition: all 0.2s ease-in-out;          
        }
        .parent {
            display: flex;
            justify-content: center;
            align-items: center;
        }
        button:active {
            background-color: rgb(212, 29, 29);
        }
        .slider{
        	-webkit-appearance: none;
            width: 50%;
            height: 15px;
            border-radius: 5px;
            background: #d3d3d3;
            outline: none;
            opacity: 0.7;
            -webkit-transition: .2s;
            transition: opacity .2s;
            margin-top: 10px;
        }
        .slider:hover{ opacity: 1;}
        .slider::-webkit-slider-thumb{
        	-webkit-appearance: none;
            appearance: none;
            width: 25px;
            height: 25px;
            border-radius: 50%;
            background: rgb(254, 220, 0);
            cursor: pointer;
        }
    </style>
</head>
<body>
  <h2>NodeMCU小车控制</h2>
  <p>舵机控制</p>
  <div>
  <label for="horizontalServoSlider">水平舵机：</label>
  <input type="range" min="0" max="180" value="90" class="slider" id="horizontalServoSlider">
  </div>
  <div>
  <label for="verticalServoSlider">竖直舵机： </label>
  <input type="range" min="0" max="180" value="90" class="slider" id="verticalServoSlider">
  </div>
  <p>小车速度控制</p>
  <label for="speedSlider">速度控制器: </label>
  <input type="range" min="0" max="255" value="150" class="slider" id="speedSlider">
  <div class="parent">
    <button class="button" id="F">前进</button>
  </div>
  <div class="parent">
    <button class="button" id="L">左转</button>
    <button class="button" id="S">停止</button>
    <button class="button" id="R">右转</button>
  </div>
  <div class="parent">
    <button class="button" id="B">后退</button>
  </div>
  <script>
     let speedCar = 150;
     let speedSlider  = document.getElementById("speedSlider");
     speedSlider.value = speedCar;
     
     speedSlider.oninput = function(){
     	speedCar = this.value;
        console.log("Speed set to: " + speedCar);
     };
     
     function sendCommand(direction){
     	let xhr = new XMLHttpRequest();
        xhr.open("GET", "/direction?dir=" + direction + "&speedCar=" +speedCar, true);
        xhr.send();
        console.log("Send command: " + direction +" with speed: " + speedCar);
     }
     
    


    let horizontalSlider = document.getElementById("horizontalServoSlider");
    let verticalSlider = document.getElementById("verticalServoSlider");

    function sendServoCommand(servo, angle){
        let xhr = new XMLHttpRequest();
        xhr.open("GET", "/servo?servo=" + servo + "&angle=" + angle, true);
        xhr.send();
        console.log("Send servo command: " + servo + " to angle: " + angle);
    }

    horizontalSlider.oninput = function(){
        sendServoCommand('H', this.value);
    }
    verticalSlider.oninput = function(){
        sendServoCommand('V', this.value);
    }


    // --- 新的小车移动控制逻辑 ---
    /**
     * @brief 为按钮添加 "按住即走，松开即停" 的事件监听
     * @param {string} buttonId 按钮的HTML ID
     * @param {string} direction 按下时要发送的方向指令
     */
     function addHoldAndReleaseListener(buttonId, direction){
     	const button = document.getElementById(buttonId);
        
        // 鼠标按下或触摸开始事件
        const startAction = (e) => {
        	e.preventDefault();	// 防止移动端长按触发其他事件
            sendCommand(direction);
        };
        
        // 鼠标松开或触摸结束事件
        const stopAction = (e) => {
        	e.preventDefault();
            sendCommand('S');	// 松开时发送停止指令
        };
        
        button.addEventListener('mousedown', startAction);
        button.addEventListener('mouseup', stopAction);
        button.addEventListener('mouseleave', stopAction);	// 鼠标移除按钮也算松开
        
        button.addEventListener('touchstart', startAction, {passive: false});
        button.addEventListener('touchend', stopAction);
        button.addEventListener('touchcancel', stopAction);
     }
     
     // 为每个方向按钮绑定事件
     addHoldAndReleaseListener('F', 'F');
     addHoldAndReleaseListener('B', 'B');
     addHoldAndReleaseListener('L', 'L');
     addHoldAndReleaseListener('R', 'R');
     
     // 停止按钮是特殊的，它只需要一个点击事件
     document.getElementById('S').addEventListener('click', () => {
     	sendCommand('S');
     });
     
  </script>
</body>
</html>
)rawliteral";


// --- 新增开始 ---
/**
 * @brief 将指定舵机转到特定角度
 * @param channel 舵机编号 (0-15)
 * @param angle 角度 (0-180)
 */
void setServoAngle(uint8_t channel, int angle) {
  // 确保角度在0-180之间
  angle = constrain(angle, 0, 180); 
  // 使用 map() 函数将 0-180 度的角度线性映射到 SERVOMIN-SERVOMAX 的脉冲计数值
  int pulselen = map(angle, 0, 180, SERVOMIN, SERVOMAX);
  pwm.setPWM(channel, 0, pulselen);
}
// --- 新增结束 ---

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);  
  pinMode(IN_1, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_4, OUTPUT); 

  // --- 新增开始 ---
  // 初始化 I2C 总线，并指定 ESP8266 的 SDA 和 SCL 引脚
  Wire.begin(D2, D1); // D2(GPIO4) is SDA, D1(GPIO5) is SCL
  
  // 初始化 PCA9685
  pwm.begin();
  pwm.setPWMFreq(SERVO_FREQ);

  // 设置舵机初始位置为90度（中间位置）
  setServoAngle(0, 90); // 水平舵机
  setServoAngle(1, 90); // 竖直舵机
  Serial.println("Servos initialized to 90 degrees.");
  // --- 新增结束 ---


  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){
    delay(1000);
    Serial.println("Connecting to WiFi...");

  }
  Serial.println(WiFi.localIP());

  // 设置服务器路由，使其能够响应对根路径/的HTTP GET请求，并返回HTML页面。
 server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
   request->send_P(200, "text/html", index_html);
   Serial.print("get success!");
  }); 

  // 在服务器对象上设置了一个名为“direction”的HTTP GET路由，该路由通过request对象的getParam函数来获取speedCar和dir参数的值，
// 分别表示小车的速度和方向。
server.on("/direction", HTTP_GET, [](AsyncWebServerRequest *request) {
  int speedCar =0;
  String dir = ""; 

// 如果获取成功，则分别将speedCar和dir打印到串口中，并通过ENA、ENB、IN_1、IN_2、IN_3和IN_4等引脚控制小车的运动。

  if(request->hasParam("speedCar")){
    speedCar = request->getParam("speedCar")->value().toInt();
    Serial.print("get speedCar: ");Serial.println(speedCar);
    }
       if(request->hasParam("dir")){
       dir = request->getParam("dir")->value().c_str();
       Serial.print("get dir: ");Serial.println(dir);
       }
    
    handleDirection(dir,speedCar);

     request->send(200, "text/html", "Received direction parameter: " + dir + "Recive Car Speed: " + speedCar);
   
  });

  // --- 新增开始 ---
  // 新增舵机控制的服务器路由
  server.on("/servo", HTTP_GET, [](AsyncWebServerRequest *request) {
    String servo_id;
    int angle = 90; // 默认角度

    // 检查是否存在 "servo" 参数
    if (request->hasParam("servo")) {
        servo_id = request->getParam("servo")->value();
    }
    // 检查是否存在 "angle" 参数
    if (request->hasParam("angle")) {
        angle = request->getParam("angle")->value().toInt();
    }

    // 根据 servo_id 控制对应的舵机
    if (servo_id == "H") {
        Serial.printf("Received Horizontal Servo Angle: %d\n", angle);
        setServoAngle(0, angle); // 通道0控制水平舵机
    } else if (servo_id == "V") {
        Serial.printf("Received Vertical Servo Angle: %d\n", angle);
        setServoAngle(1, angle); // 通道1控制竖直舵机
    }

    request->send(200, "text/plain", "OK"); // 向客户端发送一个简单的确认
  });
  // --- 新增结束 ---

  server.begin();
}


void handleDirection(String dir, int speedCar){
    if (dir == "F") {
      Serial.println("Moving forward...");
      
      digitalWrite(IN_1, LOW);
      digitalWrite(IN_2, HIGH);
      analogWrite(ENA, speedCar);

      digitalWrite(IN_3, LOW);
      digitalWrite(IN_4, HIGH);
      analogWrite(ENB, speedCar);
      }


      else if(dir =="L"){
      Serial.println("Moving goLeft...");
      digitalWrite(IN_1, LOW);
      digitalWrite(IN_2, HIGH);
      analogWrite(ENA, speedCar);

      digitalWrite(IN_3, HIGH);
      digitalWrite(IN_4, LOW);
      analogWrite(ENB, speedCar);
      }
       else if(dir =="S"){
      Serial.println("Moving stopRobot...");
      digitalWrite(IN_1, LOW);
      digitalWrite(IN_2, LOW);
      analogWrite(ENA, speedCar);

      digitalWrite(IN_3, LOW);
      digitalWrite(IN_4, LOW);
      analogWrite(ENB, speedCar);
      }
       else if(dir =="R"){
      Serial.println("Moving goRight...");
       digitalWrite(IN_1, HIGH);
      digitalWrite(IN_2, LOW);
      analogWrite(ENA, speedCar);

      digitalWrite(IN_3, LOW);
      digitalWrite(IN_4, HIGH);
      analogWrite(ENB, speedCar);
      }


      else if(dir =="B"){
       Serial.println("Moving goBack...");
      digitalWrite(IN_1, HIGH);
      digitalWrite(IN_2, LOW);
      analogWrite(ENA, speedCar);

      digitalWrite(IN_3, HIGH);
      digitalWrite(IN_4, LOW);
      analogWrite(ENB, speedCar);
        
      }
      
}

void loop() {
  // put your main code here, to run repeatedly:

}
