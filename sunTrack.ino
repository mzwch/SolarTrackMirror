
#include <AccelStepper.h>
#include <TimeLib.h>  // 添加TimeLib库以兼容SolarCalculator
#include <BluetoothSerial.h> 
#include <Preferences.h>  // 确保包含 Preferences 库

BluetoothSerial SerialBT; // 蓝牙通信实例
Preferences preferences;  // 声明 preferences 对象 用于存储停电后还可以读取当前电机位置信息的以及定位信息的
// 定义定时器间隔（以毫秒为单位）用于LOOP()循环中每隔固定时间执行一次写入操作
const unsigned long interval = 1UL * 60UL * 100UL; // 1分钟（1*60*1000毫）
unsigned long previousMillis = 0; // 上次函数执行的时间
const unsigned long interval2 = 1UL * 1UL * 200UL; // 0.2秒钟（1*1*200毫秒）
unsigned long previousMillis2 = 0; // 上次函数执行的时间
#define TX1_PIN 18           // 串口1的TX引脚（根据需要修改）
#define RX1_PIN 19           // 串口1的RX引脚（根据需要修改）
// 步进电机配置
#define AZIMUTH_STEPS_PER_REV 1000000   // 方位角电机每转脉冲数
#define ELEVATION_STEPS_PER_REV 1000000 // 仰角电机每转脉冲数

// 电机引脚定义（根据实际接线修改）
#define JDSTEP_PIN 33  // 定义用于脉冲信号的引脚
#define JDDIR_PIN 32   // 定义方向引脚

#define FWSTEP_PIN 25  // 定义用于脉冲信号的引脚
#define FWDIR_PIN 26   // 定义方向引脚

unsigned long lastReceivedTime = 0;  // 用于记录上次收到消息的时间
const unsigned long timeout = 5000;  // 超过5秒没有收到消息就认为断开连接
// 全局参数变量
float latitude = 33; //纬度
float longitude = 123; //经度
String target = "sun"; //天文对象
double JDALLDU = 0.0; //实时的仰角位置值
double FWALLDU = 0.0; // 实时的方位位置值
float ref_azimuth = 180.0; //反射出光线目标方位
float ref_elevation = 0.0; //反射出光线目标仰角
String wifi_name = "defaultSSID"; //ZeroW需要连接的Wifi名称
String wifi_pass = "defaultPASS"; //ZeroW需要连接的Wifi密码
String timeutc = ""; //当时UTC时间
bool GOGOlin = false;  // 是否自动控制标志
String raspi_ip = "0.0.0.0";
bool onlin = false; //是否ZeroW已经联上Wifi
bool linbut = false; //用于判断是否断线

// 步进电机对象
AccelStepper azimuthStepper(AccelStepper::DRIVER, FWSTEP_PIN, FWDIR_PIN);  // 方位电机设置使用电机库来控制
AccelStepper elevationStepper(AccelStepper::DRIVER, JDSTEP_PIN, JDDIR_PIN); // 仰角电机设置使用电机库来控制

void setup() {
  delay(5000);
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RX1_PIN, TX1_PIN);  // 初始化串口1，用于发送数据
  SerialBT.begin("Sun_WWW.NOW");  // 蓝牙名


  // 初始化步进电机
  azimuthStepper.setMaxSpeed(10000);
  azimuthStepper.setAcceleration(10000);
  elevationStepper.setMaxSpeed(10000);
  elevationStepper.setAcceleration(10000);
  loadParameters(); //执行一次读取机存储的参数值，读取到全局变量中

    // 设置电机初始位置为上次存放的位置
  azimuthStepper.setCurrentPosition((long)(FWALLDU / 360.0 * AZIMUTH_STEPS_PER_REV));
  elevationStepper.setCurrentPosition((long)(JDALLDU / 360.0 * ELEVATION_STEPS_PER_REV));
  Serial.print("开机读取电机位置为: 方位");
  Serial.print(FWALLDU,6);
  Serial.print("度。");
  Serial.print("仰角");
  Serial.print(JDALLDU,6);
  Serial.println("度。");
  Serial1.print("RE开机读取电机位置为: 方位");
  Serial1.print(FWALLDU,6);
  Serial1.print("度。");
  Serial1.print("仰角");
  Serial1.print(JDALLDU,6);
  Serial1.println("度。");
}

void loop() {
  delay(10);
    // 每次进入loop都模拟一次计数（你可以改成按键触发、传感器触发等）



  if (SerialBT.available()) {
    String btCmd = SerialBT.readStringUntil('\n');
    handleBluetoothCommand(btCmd);
    Serial.print("蓝牙接收到手机上来的指令：");
    Serial.println(btCmd);
  }
  if (Serial1.available()) { //监听来至串过来的信息是否有需要执行的指令
    double JDMOVangle = 0.0;
    double FWMOVangle = 0.0;
    double JDTOangle = 0.0;
    double FWTOangle = 0.0;
    linbut = true; //表示收到串口没断线
    String command = Serial1.readStringUntil('\n');
    command.trim();
    Serial.println("收到指令: " + command);
    //Serial1.println("收到指令: " + command);
    lastReceivedTime = millis();  // 重置计时器记录下每次收到串口信息的时间用于判断是否断连
    if (command == "GETALL"){
      loadParameters(); //执行一次读取机存储的参数值，读取到全局变量中
      // 构造逗号分隔的参数字符串，适合Python端解析
      String output = String(latitude, 6) + "," +
                      String(longitude, 6) + "," +
                      target + "," +
                      String(JDALLDU, 6) + "," +
                      String(FWALLDU, 6) + "," +
                      String(ref_azimuth, 2) + "," +
                      String(ref_elevation, 2) + "," +
                      wifi_name + "," +
                      wifi_pass + "," +
                      timeutc + "," +
                      (GOGOlin ? "1" : "0") + "," +
                      raspi_ip + "," +
                      (onlin ? "1" : "0");
      Serial1.println(output);
      //Serial.println("收到ZeroW读参要求，已发送信息");
      //Serial.println(output);
      return;
    }
    // 解析指令
    if (command.startsWith("GAITIME")){
      timeutc = command.substring(7); 
      Serial.print("接收到时间修改指令，时间为："); 
      Serial.println(timeutc); 
      preferences.begin("settings", false); 
      preferences.putString("timeutc", timeutc); 
      preferences.end();
      return;
    } // 关闭掉电存储的命名空间
    if (command.startsWith("JDMOVE")){
      JDMOVangle = command.substring(6).toDouble(); 
      gomove(JDMOVangle,"JDMOVE");
      return;
    }//Serial1.print("JD电机运行度数: ");Serial1.println(JDMOVangle,8);}//接收到让电机动作}
    if (command.startsWith("FWMOVE")){
      FWMOVangle = command.substring(6).toDouble(); 
      gomove(FWMOVangle,"FWMOVE");
      return;
      }//Serial1.print("FW电机运行度数: ");Serial1.println(FWMOVangle,8);}//接收到让电机动作}
    if (command.startsWith("JDMOTO")){
                          JDTOangle = command.substring(6).toDouble(); 
                                  if (JDTOangle <= 0) {
                                    Serial1.println("RE当前仰角低于地平线，保护电机不进行动作。");
                                    return;  // 直接退出，不执行电机动作
                                  }
                          gomoto(JDTOangle,"JDMOTO");
                          return;
                          }//Serial1.print("JD电机运行度数: ");Serial1.println(JDTOangle,8);}//接收到让电机动作}
    if (command.startsWith("FWMOTO")){
                          FWTOangle = command.substring(6).toDouble(); 
                                  if (FWTOangle >= 250) {
                                    Serial1.println("RE当前方位角过大，保护电机不进行动作。");
                                    return;  // 直接退出，不执行电机动作
                                  }
                          gomoto(FWTOangle,"FWMOTO");
                          return;
                          }//Serial1.print("FW电机运行度数: ");Serial1.println(FWTOangle,8);}//接收到让电机动作}
    if (command.startsWith("JDSET")){
                                          double JDSETangle = command.substring(5).toDouble(); 
                                          Serial.print("固定镜角度更新为: ");
                                          Serial.println(JDSETangle,8);
                                          Serial1.print("RE固定镜角度更新为: ");
                                          Serial1.println(JDSETangle,8);
                                          ref_elevation = JDSETangle;
                                          return;
                                          }
    if (command.startsWith("FWSET")){
                                          double FWSETangle = command.substring(5).toDouble(); 
                                          Serial.print("固定镜方位更新为: ");
                                          Serial.println(FWSETangle,8);
                                          Serial1.print("RE固定镜方位更新为: ");
                                          Serial1.println(FWSETangle,8);
                                          ref_azimuth = FWSETangle;
                                          return;
                                          }    
    if (command.startsWith("AUTOGO")) {

      GOGOlin = true;
      Serial1.println("RE开始自动运行");  // 完成后返回“GOMOVE DONE”消息
      saveParameters();
      return;
    }
    if (command.startsWith("STOPGO")) {

      GOGOlin = false;
      Serial1.println("RE停止自动运行");  // 完成后返回“GOMOVE DONE”消息
      saveParameters();
      return;
    }    

    if (command.startsWith("PARA")){
          int equalIndex = command.indexOf('=');
          if (equalIndex < 5) return; // 格式错误，忽略
          String key = command.substring(4, equalIndex); // 去掉 PARA
          String value = command.substring(equalIndex + 1);
          preferences.begin("settings", false);
          preferences.putString(key.c_str(), value);
          preferences.end();

      return;
    }

      if (command.startsWith("PARB")) {
          int equalIndex = command.indexOf('=');
          if (equalIndex < 5) return;  // 格式错误, 忽略

          String key = command.substring(4, equalIndex);  // 取出 key（应该是 "onlin"）
          String value = command.substring(equalIndex + 1);  // 取出值 "1"/"0"

          if (key == "onlin") {
              bool newState = (value == "1");
              onlin = newState;  // 更新内存变量

              preferences.begin("settings", false);
              preferences.putUInt("onlin", newState ? 1 : 0);  // 存入 NVS
              preferences.end();

              Serial.printf("[状态同步] 设置 onlin = %s\n", newState ? "true" : "false");
          }

          return;
      }








    if (command.startsWith("OPENTO")){
      return;
    }
  }
    // 检查是否超过5秒没有收到ZeroW的消息
  if (millis() - lastReceivedTime > timeout) {
      if (linbut == true){
      preferences.begin("settings", false);
      preferences.putUInt("onlin", 0);  // 存储 0 表示“离线”
      preferences.end();
      onlin = false;
      linbut = false;
      Serial.printf("这里测地标有顶替");
      }
  }




}
  String removeTrailingZeros(double num, int maxDecimals = 10) {
    String str = String(num, maxDecimals);
    if (str.indexOf('.') != -1) {
        while (str.endsWith("0")) str.remove(str.length() - 1);
        if (str.endsWith(".")) str.remove(str.length() - 1);
    }
    return str;
  }

// 初始化存储
void loadParameters() {
  preferences.begin("settings", true);
  latitude = preferences.getFloat("latitude", latitude);
  longitude = preferences.getFloat("longitude", longitude);
  target = preferences.getString("target", target);
  JDALLDU = preferences.getDouble("JDALLDU", JDALLDU);
  FWALLDU = preferences.getDouble("FWALLDU", FWALLDU);
  ref_azimuth = preferences.getFloat("ref_azimuth", ref_azimuth);
  ref_elevation = preferences.getFloat("ref_elevation", ref_elevation);
  wifi_name = preferences.getString("wifi_name", wifi_name);
  wifi_pass = preferences.getString("wifi_pass", wifi_pass);
  timeutc = preferences.getString("timeutc", timeutc);
  GOGOlin = preferences.getUInt("GOGOlin", 0) != 0; // 默认false
  raspi_ip = preferences.getString("raspi_ip", raspi_ip);
  onlin = preferences.getUInt("onlin", 0) != 0; // 默认false
  preferences.end();
}

//存储参数值的函数
void saveParameters() {
  preferences.begin("settings", false);
  preferences.putFloat("latitude", latitude);
  preferences.putFloat("longitude", longitude);
  preferences.putString("target", target);
  preferences.putDouble("JDALLDU", JDALLDU);
  preferences.putDouble("FWALLDU", FWALLDU);
  preferences.putFloat("ref_azimuth", ref_azimuth);
  preferences.putFloat("ref_elevation", ref_elevation);
  preferences.putString("wifi_name", wifi_name);
  preferences.putString("wifi_pass", wifi_pass);
  preferences.putString("timeutc", timeutc);
  preferences.putUInt("GOGOlin", GOGOlin ? 1 : 0); // 布尔转uint8存储
  preferences.putString("raspi_ip", raspi_ip);
  preferences.putUInt("onlin", onlin ? 1 : 0); // 布尔转uint8存储
  preferences.end();
}
// 蓝牙收到指令命令后的处理
void handleBluetoothCommand(String cmd) {
  cmd.trim(); // 移除命令前后的空格和换行符
  if (cmd == "GETALL") {
    loadParameters(); //执行一次读取机存储的参数值，读取到全局变量中

     // 将所有参数拼接成一个字符串并发送回手机
    String output = "latitude:" + String(latitude, 6) + "\n" +
                    "longitude:" + String(longitude, 6) + "\n" +
                    "target:" + target + "\n" +
                    "JDALLDU:" + String(JDALLDU,6) + "\n" +
                    "FWALLDU:" + String(FWALLDU,6) + "\n" +
                    "ref_azimuth:" + String(ref_azimuth) + "\n" +
                    "ref_elevation:" + String(ref_elevation) + "\n" +
                    "wifi_name:" + wifi_name + "\n" +
                    "wifi_pass:" + wifi_pass + "\n" +
                    "timeutc:" + timeutc + "\n" +
                    "raspi_ip:" + raspi_ip + "\n" +
                    "GOGOlin:" + (GOGOlin ? 1 : 0) + "\n" +
                    "onlin:" + (onlin ? 1 : 0);
    SerialBT.println(output);
    Serial.println("已向蓝牙发送信息");
    Serial.print("时间已发给APP的：");
    Serial.println(output);
    return;
  }

// 如果接收到的命令是 "SETPARAMS:" 开头
  if (cmd.startsWith("SETPARAMS:")) {
    // 命令格式示例：SETPARAMS:latitude=23.45,longitude=113.6,target=sun,...
    cmd = cmd.substring(10);  // 去掉命令的 "SETPARAMS:" 前缀
    int idx;
    // 逐个解析参数对（例如 latitude=23.45）
    while ((idx = cmd.indexOf(',')) != -1) {
      String pair = cmd.substring(0, idx);  // 获取单个参数对
      applyParam(pair);  // 应用该参数
      cmd = cmd.substring(idx + 1);  // 截取剩余部分
    }
    applyParam(cmd);  // 处理最后一个参数
    saveParameters();  // 保存修改后的参数=
    SerialBT.println("ESP32反馈完成更新");  // 向手机发送参数已更新的提示
    return;
  }

  // 如果接收到的命令是 "JDMOVE:" 开头,下面是接收蓝牙来的电机控制指令
  if (cmd.startsWith("JDMOVE:")) {
    double val = cmd.substring(7).toDouble();  // 获取电机移动值
    gomove(val,"JDMOVE");  // 调用 gomove() 函数控制仰角电机
    SerialBT.print("仰角电机移动：");  // 完成后返回“GOMOVE DONE”消息
    SerialBT.println(val,6);  // 完成后返回“GOMOVE DONE”消息
    return;
  }
  if (cmd.startsWith("FWMOVE:")) {
    double val = cmd.substring(7).toDouble();  // 获取电机移动值
    gomove(val,"FWMOVE");  // 调用 gomove() 函数控制仰角电机
    SerialBT.print("方位电机移动：");  // 完成后返回“GOMOVE DONE”消息
    SerialBT.println(val,6);  // 完成后返回“GOMOVE DONE”消息
    return;
  }
  if (cmd.startsWith("JDMOTO:")) {
    double val = cmd.substring(7).toDouble();  // 获取电机移动值
    gomoto(val,"JDMOTO");  // 调用 gomoto() 函数控制仰角电机
    SerialBT.print("仰角电机定位到：");  // 完成后返回“GOMOVE DONE”消息
    SerialBT.println(val,6);  // 完成后返回“GOMOVE DONE”消息
    return;
  }
  if (cmd.startsWith("FWMOTO:")) {
    double val = cmd.substring(7).toDouble();  // 获取电机移动值
    gomoto(val,"FWMOTO");  // 调用 gomoto() 函数控制仰角电机
    SerialBT.print("方位电机定位到：");  // 完成后返回“GOMOVE DONE”消息
    SerialBT.println(val,6);  // 完成后返回“GOMOVE DONE”消息
    return;
  }
  if (cmd.startsWith("AUTOGO")) {

    GOGOlin = true;
    SerialBT.println("开始自动运行");  // 完成后返回“GOMOVE DONE”消息
    saveParameters();
    return;
  }
  if (cmd.startsWith("STOPGO")) {

    GOGOlin = false;
    SerialBT.println("停止自动运行");  // 完成后返回“GOMOVE DONE”消息
    saveParameters();
    return;
  }


  // 如果命令无法识别
  SerialBT.println("不明错误的指令");  // 返回“UNKNOWN COMMAND”提示
}
// 应用参数对把蓝牙读取取出一个个字符组转化成对应的参数写入全局变量
void applyParam(String pair) {
  int eq = pair.indexOf('=');
  if (eq == -1) return;
  String key = pair.substring(0, eq);
  String val = pair.substring(eq + 1);
  if (key == "wifi_pass") wifi_pass = val;
  if (val.length() == 0) return;  // 忽略空值

  if (key == "latitude") latitude = val.toFloat();
  else if (key == "longitude") longitude = val.toFloat();
  else if (key == "target") target = val;
  else if (key == "JDALLDU") JDALLDU = val.toDouble();
  else if (key == "FWALLDU") FWALLDU = val.toDouble();
  else if (key == "ref_azimuth") ref_azimuth = val.toFloat();
  else if (key == "ref_elevation") ref_elevation = val.toFloat();
  else if (key == "wifi_name") wifi_name = val;
  else if (key == "wifi_pass") wifi_pass = val;
  else if (key == "timeutc") timeutc = val;
}


void gomove(double degrees, String motorName) {
    long pulses = 0; // 用于存储计算出的脉冲数
    if (motorName == "JDMOVE") { // 仰角电机
        // 根据角度计算需要的脉冲数
        pulses = (long)round(degrees * (ELEVATION_STEPS_PER_REV / 360.0));
        // 使用步进电机库移动电机
        int eledat = elevationStepper.currentPosition();
        elevationStepper.move(pulses); // 移动指定脉冲数        
        while (elevationStepper.distanceToGo() != 0) {
            elevationStepper.run(); // 自动加速/减速运行
        }
        elevationStepper.setCurrentPosition(eledat);

        Serial.print("仰角电机已移动 ");
        Serial.print(degrees,8);
        Serial.print(" 度。");
        Serial.print(pulses);
        Serial.print(" 步。");
        Serial.print("当前仰角位置（度）：");
        Serial.println((double)elevationStepper.currentPosition()/ELEVATION_STEPS_PER_REV*360.0,6);

        Serial1.print("RE-JDMOVE");
        Serial1.print(removeTrailingZeros(degrees));
        Serial1.print("#");
        Serial1.print("仰角电机已移动 ");
        Serial1.print(degrees,8);
        Serial1.print(" 度,");
        Serial1.print(pulses);
        Serial1.print(" 步,");
        Serial1.print("当前仰角位置（度）：");
        Serial1.println((double)elevationStepper.currentPosition()/ELEVATION_STEPS_PER_REV*360.0,8);
    } else if (motorName == "FWMOVE") { // 方位电机
        // 根据角度计算需要的脉冲数
        pulses = (long)round(degrees * (AZIMUTH_STEPS_PER_REV / 360.0));
        // 使用步进电机库移动电机
        int azidat = azimuthStepper.currentPosition();
        azimuthStepper.move(pulses); // 移动指定脉冲数        
        while (azimuthStepper.distanceToGo() != 0) {
            azimuthStepper.run(); // 自动加速/减速运行
        }
        azimuthStepper.setCurrentPosition(azidat);

        Serial.print("方位电机已移动 ");
        Serial.print(degrees,8);
        Serial.print(" 度,");
        Serial.print(pulses);
        Serial.print(" 步,");
        Serial.print("当前方位位置（度）：");
        Serial.println((double)azimuthStepper.currentPosition()/AZIMUTH_STEPS_PER_REV*360.0,8);

        Serial1.print("RE-FWMOVE");
        Serial1.print(removeTrailingZeros(degrees));
        Serial1.print("#");
        Serial1.print("方位电机已移动 ");
        Serial1.print(degrees,8);
        Serial1.print(" 度,");
        Serial1.print(pulses);
        Serial1.print(" 步,");
        Serial1.print("当前方位位置（度）：");
        Serial1.println((double)azimuthStepper.currentPosition()/AZIMUTH_STEPS_PER_REV*360.0,8);

    } else {
        // 如果电机名称无效
        Serial.println("无效的电机名称，请输入 'JDMOTO' 或 'FWMOTO'。");
    }

}
void gomoto(double degrees, String motorName) {

      if (motorName == "FWMOTO") { // 方位电机
        long oldsteps = azimuthStepper.currentPosition();
        long azimuth_steps = (long)(degrees / 360.0 * AZIMUTH_STEPS_PER_REV);   
        azimuthStepper.moveTo(azimuth_steps);
        // 执行步进电机运动
        while (azimuthStepper.distanceToGo() != 0) {
            azimuthStepper.run();
        }
        FWALLDU = (double)azimuthStepper.currentPosition()/AZIMUTH_STEPS_PER_REV*360.0;
        //把当前镜子方向的电机脉冲值写入机存中
        preferences.begin("settings", false);
        preferences.putDouble("FWALLDU", FWALLDU);
        preferences.end(); // 关闭掉电存储的命名空间
        //Serial1.print("FWMOTO");
        //Serial1.print(removeTrailingZeros(degrees));
        //Serial1.print("#");
        Serial1.print("RE方位动: ");
        Serial1.print(((double)(azimuth_steps-oldsteps)/AZIMUTH_STEPS_PER_REV*360.0),6);
        Serial1.print(" 度,");
        Serial1.print((long)(azimuth_steps-oldsteps));
        Serial1.print(" 步,");
        Serial1.print("当前位（度）：");
        Serial1.print((double)azimuthStepper.currentPosition()/AZIMUTH_STEPS_PER_REV*360.0,6);
        Serial.print("方位动: ");
        Serial.print(((double)(azimuth_steps-oldsteps)/AZIMUTH_STEPS_PER_REV*360.0),6);
        Serial.print(" 度,");
        Serial.print((long)(azimuth_steps-oldsteps));
        Serial.print(" 步,");
        Serial.print("输入的位置（度）：");
        Serial.print(degrees,6);
        Serial.print("当前位（度）：");
        Serial.println((double)azimuthStepper.currentPosition()/AZIMUTH_STEPS_PER_REV*360.0,6);
      }

      if (motorName == "JDMOTO") { // 仰角电机
        if (degrees < 0) {
          Serial1.println("RE当前仰角低于地平线，保护电机不进行动作。");
          return;  // 直接退出，不执行电机动作
        }
        long oldsteps = elevationStepper.currentPosition();
        long elevation_steps = (long)(degrees / 360.0 * ELEVATION_STEPS_PER_REV);
        elevationStepper.moveTo(elevation_steps);
        // 执行步进电机运动
        while (elevationStepper.distanceToGo() != 0) {
            elevationStepper.run();
        }
        JDALLDU = (double)elevationStepper.currentPosition()/ELEVATION_STEPS_PER_REV*360.0;
        
        //把当前镜子方向的电机脉冲值写入机存中
        preferences.begin("settings", false);
        preferences.putDouble("JDALLDU", JDALLDU);
        preferences.end(); // 关闭掉电存储的命名空间

        //Serial1.print("JDMOTO");
        //Serial1.print(removeTrailingZeros(degrees));
        //Serial1.print("#");
        Serial1.print("，仰角动 ");
        Serial1.print(((double)(elevation_steps-oldsteps)/ELEVATION_STEPS_PER_REV*360.0),6);
        Serial1.print(" 度,");
        Serial1.print((long)(elevation_steps-oldsteps));
        Serial1.print(" 步,");
        Serial1.print("当前仰（度）：");
        Serial1.println((double)elevationStepper.currentPosition()/ELEVATION_STEPS_PER_REV*360.0,6);
        Serial.print(".仰角动 ");
        Serial.print(((double)(elevation_steps-oldsteps)/ELEVATION_STEPS_PER_REV*360.0),6);
        Serial.print(" 度,");
        Serial.print((long)(elevation_steps-oldsteps));
        Serial.print(" 步,");
        Serial.print("当前仰（度）：");
        Serial.println((double)elevationStepper.currentPosition()/ELEVATION_STEPS_PER_REV*360.0,6);
      }
      }


