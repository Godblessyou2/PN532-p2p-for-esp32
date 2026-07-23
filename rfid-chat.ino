/**
 * ============================================================
 *  PN532 NFC P2P Chat — ESP32
 * ============================================================
 *
 * 【整体功能说明】
 *   通过两块 ESP32 + PN532 模块，利用 NFC P2P (DEP) 协议实现
 *   双向无线数据通信，类似一个 NFC "聊天" 应用。
 *
 * 【两种角色】
 *   1. INITIATOR（发起者）：主动发起连接，通过串口读取用户输入，
 *      发送给 Target，并显示 Target 的回复。
 *   2. TARGET（目标）：被动等待被连接，接收 Initiator 发来的数据，
 *      回复一个 ACK（包含自身 ID）。
 *
 * 【硬件连线】
 *   ESP32 GPIO8 → PN532 SDA
 *   ESP32 GPIO9 → PN532 SCL
 *   通信方式：I2C，速率 400kHz
 *
 * 【运行现象】
 *   ● 烧录时通过修改 ROLE_INITIATOR 宏来决定角色（1=发起者，0=目标）
 *   ● 启动后串口打印角色和随机 ID
 *   ● 将两块板子的 PN532 天线靠近（约 2~4cm）
 *   ● Initiator 端：在串口输入文字回车，即可发送给 Target
 *   ● Target 端：收到数据后自动回复 ACK:<本机ID>
 *   ● 如果通信断开，Initiator 会自动尝试重连
 *
 * ============================================================
 */

#include <Wire.h>
#include <esp_random.h>
#include "PN532_I2C.h"
#include <PN532.h>

// ======================== 配置区 ========================
#define ROLE_INITIATOR 1       // 1 = 发起者(Initiator), 0 = 目标(Target)
#define NFC_SDA        8       // PN532 I2C 数据引脚
#define NFC_SCL        9       // PN532 I2C 时钟引脚
#define I2C_SPEED      400000  // I2C 速率 400kHz
#define SERIAL_BAUD    115200  // 串口波特率
#define BUF_SIZE       96      // 收发缓冲区大小
#define LINE_SIZE      80      // 串口输入行缓冲区大小
// ========================================================

// ---- 全局对象 ----
PN532_I2C pn532_i2c(Wire);    // I2C 通信接口
PN532 nfc(pn532_i2c);         // PN532 NFC 控制器

// ---- 全局缓冲区 ----
uint8_t rxBuf[BUF_SIZE];      // 接收缓冲区
uint8_t txBuf[BUF_SIZE];      // 发送缓冲区
char    lineBuf[LINE_SIZE];   // 串口输入行缓冲
size_t  lineLen = 0;          // 当前行已读取长度

// ---- 全局状态 ----
uint32_t myId;                // 本机随机 ID（用于 ACK 标识）
bool     linkActive = false;  // NFC 链路是否已建立（仅 Initiator 使用）


// ==========================================================
//  工具函数 (Utilities)
// ==========================================================

/**
 * clearLine()
 *   清空串口行缓冲区，为下一行输入做准备。
 */
void clearLine() {
  memset(lineBuf, 0, sizeof(lineBuf));
  lineLen = 0;
}

/**
 * readSerialLine()
 *   从串口逐字符读取，直到遇到换行符 '\n'。
 *   - 忽略 '\r'（回车符）
 *   - 超过 LINE_SIZE 限制时自动截断
 *   @return true  — 已读到完整一行
 *           false — 还没读完（串口无数据或未遇到换行）
 */
bool readSerialLine() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      return (lineLen > 0);
    }

    if (lineLen < LINE_SIZE - 1)
      lineBuf[lineLen++] = c;
  }
  return false;
}

/**
 * resetLink()
 *   释放当前 NFC 链路，将 linkActive 标志置为 false。
 *   当通信失败时调用，触发重连机制。
 */
void resetLink() {
  nfc.inRelease();
  linkActive = false;
}


// ==========================================================
//  INITIATOR 模式（发起者）
// ==========================================================

/**
 * connectToTarget()
 *   使用 DEP（Data Exchange Protocol）被动模式发起连接。
 *   - 被动模式：由本地 PN532 产生射频场，与对端 Target 建立链路
 *   - baudrate=0x00 表示 106kbps（NFC-A / ISO14443A 速率）
 *   @return true  — 连接成功
 *           false — 连接失败（对端不在范围内或未就绪）
 */
bool connectToTarget() {
  if (nfc.inJumpForDEP(0x00, true)) {
    Serial.println("Connected to target");
    linkActive = true;
    return true;
  }
  return false;
}

/**
 * initiatorLoop() — 主循环中反复调用
 *   流程：
 *   1. 若链路未建立 → 尝试连接 Target（失败则 200ms 后重试）
 *   2. 若链路已建立 → 等待串口输入一行文字
 *   3. 将输入文字通过 inDataExchange 发送给 Target
 *   4. 打印发送内容 (TX) 和收到的回复 (RX)
 *   5. 若发送失败 → 释放链路，下次循环自动重连
 */
void initiatorLoop() {

  // 步骤 1：确保链路已建立
  if (!linkActive) {
    if (!connectToTarget()) {
      delay(200);       // 连接失败，稍等后重试
      return;
    }
  }

  // 步骤 2：等待串口输入
  if (!readSerialLine())
    return;

  // 步骤 3：准备并发送数据
  size_t len = strlen(lineBuf);
  if (len > BUF_SIZE)
    len = BUF_SIZE;
  memcpy(txBuf, lineBuf, len);

  uint8_t respLen = BUF_SIZE;
  bool ok = nfc.inDataExchange(txBuf, len, rxBuf, &respLen);

  // 步骤 4：处理发送结果
  if (!ok) {
    Serial.println("Send failed - reconnecting");
    resetLink();        // 释放链路，下次循环自动重连
  } else {
    // 显示已发送内容
    Serial.print("TX: ");
    Serial.write(txBuf, len);
    Serial.println();

    // 显示收到的回复
    if (respLen) {
      Serial.print("RX: ");
      Serial.write(rxBuf, respLen);
      Serial.println();
    }
  }

  // 步骤 5：清空行缓冲
  clearLine();
}


// ==========================================================
//  TARGET 模式（目标）
// ==========================================================

/**
 * armTarget()
 *   将 PN532 初始化为目标设备（被动等待连接）。
 *   - timeout=5000ms：等待 5 秒，若无连接则超时返回
 *   @return true  — 已就绪，等待 Initiator 连接
 *           false — 超时或初始化失败
 */
bool armTarget() {
  int8_t r = nfc.tgInitAsTarget(5000);
  if (r > 0) {
    Serial.println("Target ready");
    return true;
  }
  return false;
}

/**
 * targetLoop() — 主循环中反复调用
 *   流程：
 *   1. 若未就绪 → 初始化为目标设备（armTarget）
 *   2. 等待 Initiator 发送数据（tgGetData 阻塞等待）
 *   3. 收到数据 → 串口打印 (RX)，然后回复 ACK:<本机ID>
 *   4. 若回复失败或链路断开 → 重新 arm（下次循环自动重新初始化）
 */
void targetLoop() {

  static bool armed = false;

  // 步骤 1：确保已初始化为目标
  if (!armed) {
    armed = armTarget();
    if (!armed) {
      delay(200);       // 初始化失败，稍等后重试
      return;
    }
  }

  // 步骤 2：等待接收数据
  int16_t len = nfc.tgGetData(rxBuf, BUF_SIZE);

  if (len > 0) {
    // 步骤 3a：打印收到的数据
    Serial.print("RX: ");
    Serial.write(rxBuf, len);
    Serial.println();

    // 步骤 3b：构造并发送 ACK 回复
    char ack[32];
    int ackLen = snprintf(ack, sizeof(ack),
                          "ACK:%08lX", (unsigned long)myId);

    if (!nfc.tgSetData((uint8_t*)ack, ackLen)) {
      Serial.println("ACK failed - rearming");
      armed = false;    // 回复失败，需要重新初始化
    }

  } else if (len < 0) {
    // 步骤 4：链路断开，需要重新初始化
    armed = false;
  }
}


// ==========================================================
//  setup() — 系统初始化
// ==========================================================
void setup() {
  // 1. 初始化串口
  Serial.begin(SERIAL_BAUD);
  delay(200);

  // 2. 初始化 I2C 总线
  Wire.begin(NFC_SDA, NFC_SCL);
  Wire.setClock(I2C_SPEED);

  // 3. 初始化 PN532
  pn532_i2c.begin();
  nfc.begin();

  // 4. 配置 SAM（安全访问模块）→ 启用正常工作模式
  nfc.SAMConfig();

  // 5. 设置被动激活重试次数
  nfc.setPassiveActivationRetries(0x10);

  // 6. 生成本机随机 ID
  myId = esp_random();

  // 7. 打印启动信息
  Serial.println();
  Serial.println(ROLE_INITIATOR ? "MODE: INITIATOR" : "MODE: TARGET");
  Serial.printf("ID: %08lX\n", (unsigned long)myId);
}


// ==========================================================
//  loop() — 主循环
// ==========================================================
void loop() {
#if ROLE_INITIATOR
  initiatorLoop();    // 发起者模式：主动发数据
#else
  targetLoop();       // 目标模式：被动收数据并回复
#endif
}