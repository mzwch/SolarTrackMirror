import serial
import time
from flask import Flask, request, jsonify, render_template
from threading import Thread
import math
from datetime import datetime, timedelta
from pytz import timezone
from skyfield.api import load, wgs84
import threading
import os
import sys
#为了接收ZeroW反馈的结果显示出来设置变量
import subprocess
import sqlite3

# 切换到脚本所在目录
os.chdir(os.path.dirname(os.path.abspath(__file__)))

# 设置串口通信
esp32_port = '/dev/serial0'  # 树莓派的串口设备
esp32_baudrate = 9600
ser = serial.Serial(esp32_port, esp32_baudrate, timeout=1)

# 创建 Flask 应用
app = Flask(__name__)

# 定义一些全局变量
sun_position = None  # 用于保存计算出的太阳方位和仰角
log_lines = []  # 用于保存日志信息，最多显示35行

last_jd = None
last_fw = None
gotime = 60 #定义太阳反射计算执行的循环时间
readgo = False
DB_PATH = "sun_log.db"

#定义两个变量用于控制是否自动进行太阳计算
tracking_thread = None
tracking_active = False

#创建一个全局线程锁
serial_lock = threading.Lock()



PARAM_FILE = 'params.txt'  # 存储参数的本地文件
PARAM_FILE2 = 'params2.txt'
def read_correction_factors():
    jd = [1.0] * 12
    fw = [1.0] * 12
    if os.path.exists(PARAM_FILE2):
        with open(PARAM_FILE2, 'r') as f:
            for line in f:
                if '=' in line:
                    k, v = line.strip().split('=', 1)
                    if k.startswith('jd_'):
                        idx = int(k[3:])
                        if 0 <= idx < 12:
                            jd[idx] = float(v)
                    elif k.startswith('fw_'):
                        idx = int(k[3:])
                        if 0 <= idx < 12:
                            fw[idx] = float(v)
    return jd, fw
jd_factors, fw_factors = read_correction_factors() #定义用于计算方位角度进行修改的参数




# 定义在ESP32上已有的全局变量
latitude = 33  # 纬度
longitude = 123  # 经度
target = "sun"  # 天文对象
JDALLDU = 0.0  # 实时的仰角位置值
FWALLDU = 0.0  # 实时的方位位置值
ref_azimuth = 180.0  # 反射出光线目标方位
ref_elevation = 0.0  # 反射出光线目标仰角
wifi_name = "defaultSSID"  # ZeroW需要连接的Wifi名称
wifi_pass = "defaultPASS"  # ZeroW需要连接的Wifi密码
timeutc = 0  # 当时UTC时间
GOGOlin = False  # 运动控制标志
raspi_ip = "0.0.0.0"
onlin = False  # 运动控制标志

# 初始化Skyfield天文计算库
bsp_path = 'de442.bsp'  # 指定使用的星历表文件路径(DE442是NASA的高精度行星/月球历表)
eph = load(bsp_path)    # 加载星历表数据文件


# 初始化时间系统
ts = load.timescale()  # 创建时间尺度对象，用于处理各种时间格式转换


# 读取本地的参数文件
def read_local_params():
    params = {}  # 创建一个字典用于存储参数
    if os.path.exists(PARAM_FILE):  # 检查文件是否存在
        with open(PARAM_FILE, 'r') as f:  # 以读取模式打开文件
            for line in f:  # 遍历文件中的每一行
                if '=' in line:  # 确保行内有"="，即键值对
                    k, v = line.strip().split('=', 1)  # 分割键和值，并去掉两端的空格
                    params[k] = v  # 存储到字典中
    return params  # 如果文件不存在，返回空字典
# 更新本地的参数文件
def write_local_params(params):
    with open(PARAM_FILE, 'w') as f:  # 以写入模式打开文件
        for k, v in params.items():  # 遍历字典中的每个键值对
            f.write(f"{k}={v}\n")  # 写入每一行，格式为键=值
            



def write_correction_factors(jd, fw):
    with open(PARAM_FILE2, 'w') as f:
        for i in range(12):
            f.write(f"jd_{i}={jd[i]}\n")
        for i in range(12):
            f.write(f"fw_{i}={fw[i]}\n")












# 解析ESP32发送过来的响应（逗号分隔）
def parse_response(line):
    try:
        keys = [
            "latitude", "longitude", "target", "JDALLDU", "FWALLDU", 
            "ref_azimuth", "ref_elevation", "wifi_name", "wifi_pass", 
            "timeutc", "GOGOlin", "raspi_ip", "onlin"
        ]  # 定义返回的参数顺序
        parts = line.strip().split(',')  # 将响应按逗号分隔成各个参数
        if len(parts) != len(keys):  # 检查返回的参数数量是否正确
            print(f"[ERROR] 参数数量不一致：{len(parts)} vs {len(keys)}")  # 如果不一致，输出错误信息
            return None  # 返回None，表示解析失败
        param_dict = dict(zip(keys, parts))  # 将参数名和对应的值合并成字典
        # 处理GOGOlin参数（将1或0转换为True或False）
        param_dict["GOGOlin"] = "True" if param_dict["GOGOlin"] == "1" else "False"
        param_dict["onlin"] = "True" if param_dict["onlin"] == "1" else "False"
        return param_dict  # 返回字典
    except UnicodeDecodeError:
        print("对原始数据解码出错")
        
        
# 判断 wlan0 是否处于 connected 状态
def is_wifi_connected():
    try:
        result = subprocess.run(["nmcli", "-t", "-f", "DEVICE,TYPE,STATE", "dev"], capture_output=True, text=True, check=True)
        for line in result.stdout.strip().split('\n'):
            if line.startswith("wlan0:wifi:connected"):
                return True
    except Exception as e:
        print(f"[WiFi检测异常] {e}")
    return False
    
    
def sync_and_track():
    global sun_position, tracking_active, latitude, longitude, target, JDALLDU, FWALLDU, ref_azimuth, ref_elevation, wifi_name, wifi_pass, timeutc, GOGOlin, raspi_ip, onlin, gotime
    global last_jd, last_fw, readgo
    last_sync_time = time.time()
    last_track_time = time.time()
    
    while True:
        # 1. 每2秒同步参数
        '''
        try:
            if time.time() - last_sync_time >= 2:
                with serial_lock:
                    ser.write(b'GETALL\n')  # 向ESP32请求所有参数
                    line = ser.readline().decode('utf-8', 'ignore').strip()  # 读取ESP32返回的参数
                if line:
                    new_params = parse_response(line)  # 解析响应
                    if new_params:
                        # 从本地获取上次已记录的 IP
                        old_params = read_local_params()
                        last_ip = old_params.get("raspi_ip", "")

                        # 当前系统实际 IP
                        try:
                            ip_result = subprocess.run(["hostname", "-I"], capture_output=True, text=True, check=True)
                            current_ip = ip_result.stdout.strip().split()[0]
                        except Exception as e:
                            current_ip = "0.0.0.0"

                        # 如果 IP 有变动才通知 ESP32
                        if current_ip != last_ip:
                            send_to_esp32(f"PARAraspi_ip={current_ip}")
                            print(f"[IP更新] IP地址变动: {last_ip} → {current_ip}")
                            old_params["raspi_ip"] = current_ip
                            write_local_params(old_params)  # 顺便更新本地记录      
                        
                        # 判断是否连接WiFi
                        wifi_connected = is_wifi_connected()

                        # 本地参数中的 onlin 是字符串，需要转换为布尔值比较
                        last_onlin = read_local_params().get("onlin", "False") == "True"
                        # 如果状态发生变化才同步给ESP32
                        if wifi_connected != last_onlin:
                            status_str = "1" if wifi_connected else "0"
                            send_to_esp32(f"PARBonlin={status_str}")
                            print(f"[WiFi状态] 状态变化: {last_onlin} → {wifi_connected}")

                            # 更新本地保存
                            old_params["onlin"] = "True" if wifi_connected else "False"
                            write_local_params(old_params)
    
    
                        # 如果新参数与本地参数不一致，更新

                        if new_params != read_local_params():

                            print("参数变化，更新本地参数和ESP32 NVS")
                            # 更新本地参数文件
                            
                            write_local_params(new_params)
                            
                            if wifi_name != new_params["wifi_name"] or wifi_pass != new_params["wifi_pass"]:
                                print(f"Wi-Fi信息发生变化，连接到新的Wi-Fi: {new_params['wifi_name']}")
                                wifi_name = new_params["wifi_name"]
                                wifi_pass = new_params["wifi_pass"]
                                # 连接新Wi-Fi网络（调用连接Wi-Fi的函数）
                                connect_to_wifi(wifi_name, wifi_pass)
                            # 更新全局变量
                            try:
                                latitude = float(new_params["latitude"])
                                longitude = float(new_params["longitude"])
                                target = new_params["target"]
                                JDALLDU = float(new_params["JDALLDU"])
                                FWALLDU = float(new_params["FWALLDU"])
                                ref_azimuth = float(new_params["ref_azimuth"])
                                ref_elevation = float(new_params["ref_elevation"])
                                wifi_name = new_params["wifi_name"]
                                wifi_pass = new_params["wifi_pass"]
                                timeutc = new_params["timeutc"]
                                GOGOlin = new_params["GOGOlin"] == "True"
                                raspi_ip = new_params["raspi_ip"]
                                tracking_active = GOGOlin
                                onlin = new_params["onlin"] == "True"
                            except ValueError:
                                print("解析读取的ESP32数据出错可能是转码失败")





                last_sync_time = time.time()  # 更新同步时间
        except UnicodeDecodeError:
            print("[读取参数时] 无法解码串口数据，跳过此数据")
        '''
        # 2. 每10秒钟计算太阳位置并控制反射
        try:
            if tracking_active and time.time() - last_track_time >= gotime:
                # 设置本地时区(上海时区，UTC+8)
                local_tz = timezone('Asia/Shanghai')
                sun = eph[target]
                earth = eph['earth']
                observer = earth + wgs84.latlon(latitude, longitude)

                # 获取“当前时间”和“前10秒时间”的两个观测时间
                now_local = datetime.now(local_tz)
                now_utc = now_local.astimezone(timezone('UTC'))
                t_now = ts.from_datetime(now_utc)

                past_local = now_local - timedelta(seconds=gotime)
                past_utc = past_local.astimezone(timezone('UTC'))
                t_past = ts.from_datetime(past_utc)

                # 计算当前太阳方向
                astrometric_now = observer.at(t_now).observe(sun).apparent()
                alt_now, az_now, _ = astrometric_now.altaz()

                # 计算前一个时间点太阳方向
                astrometric_past = observer.at(t_past).observe(sun).apparent()
                alt_past, az_past, _ = astrometric_past.altaz()

                # 当前时刻入射方位/仰角
                az1_now = az_now.degrees
                el1_now = alt_now.degrees

                # 上一时刻入射方位/仰角
                az1_past = az_past.degrees
                el1_past = alt_past.degrees

                # 目标反射方向（固定）
                az2 = ref_azimuth
                el2 = ref_elevation

                # 计算当前和过去的镜面角度（光线平分方向）
                az_bis_now, el_bis_now = calculate_bisector_azimuth_elevation(az1_now, el1_now, az2, el2)
                az_bis_past, el_bis_past = calculate_bisector_azimuth_elevation(az1_past, el1_past, az2, el2)

                # 获取当前小时(从早上6点算起)，用来选取修正因子
                hour_index = now_local.hour - 6
                if 0 <= hour_index < 12:
                    fw_factor = fw_factors[hour_index]
                    jd_factor = jd_factors[hour_index]
                else:
                    fw_factor = 1.0
                    jd_factor = 1.0

                if el_bis_now <= 0 and not readgo:   
                    print(f"[镜面控制] 当前仰角 {el_bis_now:.8f}° 小于等于 0，跳过发送ESP32控制指令。")
                    send_to_esp32("JDMOTO90.00")
                    readgo = True        
                    last_jd = None
                    last_fw = None                    
                elif el_bis_now > 0:
                    # 根据修正模型计算最终输出的镜面角度
                    delta_fw = (az_bis_now - az_bis_past) * fw_factor #计算出新时间和上一次时间的增加量并乘以设置的系数
                    delta_jd = (el_bis_now - el_bis_past) * jd_factor #计算出新时间和上一次时间的增加量并乘以设置的系数
                    

                    if last_fw is None or last_jd is None:
                        adj_fw = az_bis_now
                        adj_jd = el_bis_now
                        print(f"重新开始，当前方位为: {adj_fw:.8f}°，当前角度为: {adj_jd:.8f}°")
                    else:
                        adj_fw = last_fw + delta_fw #需要设置的角度就是之前的角度加上已经经过系数计算后的增量值
                        adj_jd = last_jd + delta_jd
                    # 发送控制命令给 ESP32（仰角需大于0才发送）
                    if adj_jd > 0:
                        last_fw = adj_fw
                        last_jd = adj_jd

                        # 更新全局sun_position变量 这里好像也没啥用的，不用看
                        sun_position = {
                            "sun_azimuth": az1_now,
                            "sun_elevation": el1_now,
                            "mirror_azimuth": adj_fw,
                            "mirror_elevation": adj_jd
                        }
             
                        # 打印调试信息
                        # 计算差值，避免重复
                        az_diff = adj_fw - az_bis_now
                        el_diff = adj_jd - el_bis_now

                        # 打印
                        print(f"时间: {now_local.strftime('%Y-%m-%d %H`%M`%S')}")
                        print(f"太阳方位角: {az1_now:.8f}°，太阳高度角: {el1_now:.8f}°")
                        print(f"镜子方位角: {adj_fw:.8f}°，镜子仰角: {adj_jd:.8f}°（已修正）")
                        print(f"没修正的镜子方位角: {az_bis_now:.8f}°，镜子仰角: {el_bis_now:.8f}°（没修正）")
                        print(f"修正的方位减没修正的: {az_diff:.8f}°，修正的仰角减去没修正的：{el_diff:.8f}")
                        print(f"使用的修正系数:方位为 {fw_factor:.8f}，仰角为: {jd_factor:.8f}")
                        
                        # 连接数据库并插入
                        conn = sqlite3.connect(DB_PATH)
                        cursor = conn.cursor()

                        cursor.execute('''
                        INSERT INTO log (
                            time,
                            sun_az,
                            sun_el,
                            mirror_az,
                            mirror_el,
                            mirror_az_raw,
                            mirror_el_raw,
                            az_diff,
                            el_diff,
                            fw_factor,
                            jd_factor
                        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                        ''', (
                            now_local.strftime('%Y-%m-%d %H:%M:%S'),
                            az1_now,
                            el1_now,
                            adj_fw,
                            adj_jd,
                            az_bis_now,
                            el_bis_now,
                            az_diff,
                            el_diff,
                            fw_factor,
                            jd_factor
                        ))

                        conn.commit()
                        cursor.close()
                        conn.close()
                        
                        send_to_esp32(f"GAITIME{now_local.strftime('%Y-%m-%d %H`%M`%S')}")
                        send_to_esp32(f"FWMOTO{adj_fw:.8f}")
                        send_to_esp32(f"JDMOTO{adj_jd:.8f}")
                        print("------------------------------------------------------------------------")
                        readgo = False

                # 更新时间戳
                last_track_time = time.time()

            time.sleep(0.1)  # 避免CPU占用过高
        except UnicodeDecodeError:
            print("[太阳控制输出] 出错，跳过此数据")




def connect_to_wifi(wifi_name, wifi_pass):
    print(f"尝试连接到 Wi-Fi: {wifi_name}")

    if wifi_pass.strip() == "":
        # 开放网络：不需要密码
        cmd = [
            "nmcli", "device", "wifi", "connect",
            wifi_name,
            "hidden", "yes",
            "ifname", "wlan0"
        ]
    else:
        # 有密码的网络
        cmd = [
            "nmcli", "device", "wifi", "connect",
            wifi_name,
            "password", wifi_pass,
            "hidden", "yes",
            "ifname", "wlan0"
        ]

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        time.sleep(1)
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        print("连接成功！")
        print(result.stdout)
    except subprocess.CalledProcessError as e:
        print("连接失败！")
        print(e.stderr)
    
    
    


def calculate_bisector_azimuth_elevation(azimuth1, elevation1, azimuth2, elevation2): #这是用于计算入射线和反射线的的中平分线坐标的
    # 转为弧度
    az1_rad = math.radians(azimuth1)
    el1_rad = math.radians(elevation1)
    az2_rad = math.radians(azimuth2)
    el2_rad = math.radians(elevation2)

    # 方向向量计算
    v1 = [math.cos(el1_rad) * math.sin(az1_rad), math.cos(el1_rad) * math.cos(az1_rad), math.sin(el1_rad)]
    v2 = [math.cos(el2_rad) * math.sin(az2_rad), math.cos(el2_rad) * math.cos(az2_rad), math.sin(el2_rad)]

    # 向量相加得到角平分方向
    vm = [v1[i] + v2[i] for i in range(3)]
    # 单位化
    mag = math.sqrt(sum(c**2 for c in vm))
    vm_unit = [c / mag for c in vm]

    # 仰角
    elevation = math.degrees(math.asin(vm_unit[2]))
    # 真方位角（从北起顺时针）
    azimuth = math.degrees(math.atan2(vm_unit[0], vm_unit[1]))
    if azimuth < 0:
        azimuth += 360  # 保证方位角在[0,360]度之间

    return azimuth, elevation



def send_to_esp32(command):
    with serial_lock:
        ser.write(f"{command}\n".encode())
        print(f"发送到 ESP32: {command}")

# 后端接口

@app.route('/')
def index():
    return render_template('index.html', jd_factors=jd_factors, fw_factors=fw_factors)


@app.route('/send', methods=['POST'])
def send_command():
    global tracking_thread, tracking_active
    global last_jd, last_fw

    data = request.json
    command_type = data.get('type')
    value = data.get('value')

    if command_type == "JDMOVE":
        send_to_esp32(f"JDMOVE{value}")
    elif command_type == "FWMOVE":
        send_to_esp32(f"FWMOVE{value}")
    elif command_type == "START_AUTO":
        if not tracking_active:
            print("启动自动追踪")
            last_jd = None
            last_fw = None
            tracking_active = True # 设置为True，开始自动追踪
            send_to_esp32("AUTOGO\n")
        else:
            print("追踪已在运行中，无需重复启动")
    elif command_type == "STOPTO":
        print("停止自动追踪")
        last_jd = None
        last_fw = None
        tracking_active = False  # 线程会在下一轮 sleep 后自行停止
        send_to_esp32("STOPGO\n")

    return jsonify({"status": "success"}), 200

@app.route('/update_factors', methods=['POST'])
def update_factors():
    global jd_factors, fw_factors
    jd_factors = [float(request.form.get(f"jd_{i}", 1.0)) for i in range(12)]
    fw_factors = [float(request.form.get(f"fw_{i}", 1.0)) for i in range(12)]
    write_correction_factors(jd_factors, fw_factors)
    return "参数已更新"

@app.route('/adjust', methods=['POST'])
def adjust():
    global last_fw, last_jd
    data = request.json
    direction = data.get("direction")
    step = float(data.get("step", 0.1))

    if last_fw is None:
        last_fw = 0.0
    if last_jd is None:
        last_jd = 0.0

    if direction == "UP":
        last_jd += step
    elif direction == "DOWN":
        last_jd -= step
    elif direction == "LEFT":
        last_fw -= step
    elif direction == "RIGHT":
        last_fw += step

    # 限制仰角不能为负
    if last_jd < 0:
        last_jd = 0.0

    # 发送新值给 ESP32
    print("------------------------------------------------------------------------")
    send_to_esp32(f"FWMOTO{last_fw:.8f}")
    send_to_esp32(f"JDMOTO{last_jd:.8f}")
    print("------------------------------------------------------------------------")

    return jsonify({"status": "adjusted", "fw": last_fw, "jd": last_jd})


@app.route('/recv', methods=['GET'])
def recv_message():
    # 获取 ESP32 返回的信息
    lines = []
    try:
        with serial_lock:
            while ser.in_waiting:
                line = ser.readline().decode('utf-8', 'ignore').strip()  # 读取ESP32返回的参数
                if line and "RE" in line:  # 只保留包含"RE"的内容
                    # 去掉 "RE"
                    cleaned_line = line.replace("RE", "", 1).strip()
                    # 加上当前日期时间
                    timestamp = datetime.now().strftime("%m-%d %H:%M:%S")
                    final_line = f"{timestamp} {cleaned_line}"
                    lines.append(final_line)
                    if len(lines) > 200:
                        lines = lines[-200:]
    except UnicodeDecodeError:
        print("[连续读取] 出错，跳过此数据")
    # 返回最新的消息（最多返回35行）
    return jsonify({"lines": lines[-35:]})

if __name__ == '__main__':
    # 启动同步线程
    threading.Thread(target=sync_and_track, daemon=True).start()
    # 添加参数 suppress Werkzeug 日志
    import logging
    log = logging.getLogger('werkzeug')
    log.setLevel(logging.ERROR)  # 或者 logging.CRITICAL 来彻底屏蔽



    app.run(host='0.0.0.0', port=80)