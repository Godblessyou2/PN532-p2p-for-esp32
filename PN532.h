/**
 * ============================================================
 *  PN532.h — PN532 NFC 控制器驱动库头文件
 * ============================================================
 *
 * 【文件作用】
 *   定义 PN532 芯片的完整驱动类，封装了所有 NFC 操作：
 *   - 基础配置（固件版本、SAM、GPIO、RF 场控制）
 *   - ISO14443A 读卡操作（被动目标检测、数据交换）
 *   - NFC P2P (DEP) 操作（发起者/目标模式）
 *   - Mifare Classic 操作（认证、读写、NDEF 格式化）
 *   - Mifare Ultralight 操作（页读写）
 *   - FeliCa 操作（Polling、读写、服务请求等）
 *
 * 【本项目使用的关键功能】
 *   - tgInitAsTarget(): 初始化为 NFC P2P 目标设备
 *   - inJumpForDEP():   以 DEP 协议发起 P2P 连接
 *   - inDataExchange(): 在已建立的 P2P 链路上交换数据
 *   - tgGetData():      目标模式下接收数据
 *   - tgSetData():      目标模式下发送数据
 *   - inRelease():      释放当前 NFC 链路
 *
 * 【依赖关系】
 *   PN532 → PN532Interface (抽象通信接口)
 *         → PN532_I2C      (I2C 传输层实现)
 *
 * ============================================================
 *
 * @file     PN532.h
 * @author   Adafruit Industries & Seeed Studio
 * @license  BSD
 */

#ifndef __PN532_H__
#define __PN532_H__

#include <stdint.h>
#include "PN532Interface.h"

/* ============================================================
 *  PN532 命令码定义
 *  每个命令码对应 PN532 芯片的一种操作
 * ============================================================ */

/* ---- 基础命令 ---- */
#define PN532_COMMAND_DIAGNOSE              (0x00)  // 诊断命令
#define PN532_COMMAND_GETFIRMWAREVERSION    (0x02)  // 获取固件版本
#define PN532_COMMAND_GETGENERALSTATUS      (0x04)  // 获取通用状态
#define PN532_COMMAND_READREGISTER          (0x06)  // 读寄存器
#define PN532_COMMAND_WRITEREGISTER         (0x08)  // 写寄存器
#define PN532_COMMAND_READGPIO              (0x0C)  // 读 GPIO 引脚
#define PN532_COMMAND_WRITEGPIO             (0x0E)  // 写 GPIO 引脚
#define PN532_COMMAND_SETSERIALBAUDRATE     (0x10)  // 设置串口波特率
#define PN532_COMMAND_SETPARAMETERS         (0x12)  // 设置参数
#define PN532_COMMAND_SAMCONFIGURATION      (0x14)  // SAM 配置
#define PN532_COMMAND_POWERDOWN             (0x16)  // 进入低功耗模式
#define PN532_COMMAND_RFCONFIGURATION       (0x32)  // RF 射频配置
#define PN532_COMMAND_RFREGULATIONTEST      (0x58)  // RF 调节测试

/* ---- 发起者 (Initiator) 命令 ---- */
#define PN532_COMMAND_INJUMPFORDEP          (0x56)  // DEP 跳转 (P2P 连接)
#define PN532_COMMAND_INJUMPFORPSL          (0x46)  // PSL 跳转
#define PN532_COMMAND_INLISTPASSIVETARGET   (0x4A)  // 列出被动目标
#define PN532_COMMAND_INATR                 (0x50)  // ATR 请求
#define PN532_COMMAND_INPSL                 (0x4E)  // PSL 请求
#define PN532_COMMAND_INDATAEXCHANGE        (0x40)  // 数据交换
#define PN532_COMMAND_INCOMMUNICATETHRU     (0x42)  // 直通通信
#define PN532_COMMAND_INDESELECT            (0x44)  // 取消选择
#define PN532_COMMAND_INRELEASE             (0x52)  // 释放目标
#define PN532_COMMAND_INSELECT              (0x54)  // 选择目标
#define PN532_COMMAND_INAUTOPOLL            (0x60)  // 自动轮询

/* ---- 目标 (Target) 命令 ---- */
#define PN532_COMMAND_TGINITASTARGET        (0x8C)  // 初始化为目标
#define PN532_COMMAND_TGSETGENERALBYTES     (0x92)  // 设置通用字节
#define PN532_COMMAND_TGGETDATA             (0x86)  // 获取数据 (接收)
#define PN532_COMMAND_TGSETDATA             (0x8E)  // 设置数据 (发送)
#define PN532_COMMAND_TGSETMETADATA         (0x94)  // 设置元数据
#define PN532_COMMAND_TGGETINITIATORCOMMAND (0x88)  // 获取发起者命令
#define PN532_COMMAND_TGRESPONSETOINITIATOR (0x90)  // 响应发起者
#define PN532_COMMAND_TGGETTARGETSTATUS     (0x8A)  // 获取目标状态

/* ---- 响应码 ---- */
#define PN532_RESPONSE_INDATAEXCHANGE       (0x41)  // 数据交换响应
#define PN532_RESPONSE_INLISTPASSIVETARGET  (0x4B)  // 列出被动目标响应

/* ---- NFC 类型标识 ---- */
#define PN532_MIFARE_ISO14443A              (0x00)  // ISO14443A (Mifare)

/* ============================================================
 *  Mifare Classic 命令码
 * ============================================================ */
#define MIFARE_CMD_AUTH_A                   (0x60)  // 密钥 A 认证
#define MIFARE_CMD_AUTH_B                   (0x61)  // 密钥 B 认证
#define MIFARE_CMD_READ                     (0x30)  // 读数据块 (16字节)
#define MIFARE_CMD_WRITE                    (0xA0)  // 写数据块 (16字节)
#define MIFARE_CMD_WRITE_ULTRALIGHT         (0xA2)  // 写 Ultralight 页 (4字节)
#define MIFARE_CMD_TRANSFER                 (0xB0)  // 传输
#define MIFARE_CMD_DECREMENT                (0xC0)  // 递减
#define MIFARE_CMD_INCREMENT                (0xC1)  // 递增
#define MIFARE_CMD_STORE                    (0xC2)  // 存储

/* ============================================================
 *  FeliCa 命令码
 * ============================================================ */
#define FELICA_CMD_POLLING                  (0x00)  // 轮询
#define FELICA_CMD_REQUEST_SERVICE          (0x02)  // 请求服务
#define FELICA_CMD_REQUEST_RESPONSE         (0x04)  // 请求响应
#define FELICA_CMD_READ_WITHOUT_ENCRYPTION  (0x06)  // 无加密读取
#define FELICA_CMD_WRITE_WITHOUT_ENCRYPTION (0x08)  // 无加密写入
#define FELICA_CMD_REQUEST_SYSTEM_CODE      (0x0C)  // 请求系统码

/* ============================================================
 *  NDEF URI 前缀定义
 *  用于 NDEF 记录中的 URI 类型标识
 * ============================================================ */
#define NDEF_URIPREFIX_NONE                 (0x00)  // 无前缀
#define NDEF_URIPREFIX_HTTP_WWWDOT          (0x01)  // http://www.
#define NDEF_URIPREFIX_HTTPS_WWWDOT         (0x02)  // https://www.
#define NDEF_URIPREFIX_HTTP                 (0x03)  // http://
#define NDEF_URIPREFIX_HTTPS                (0x04)  // https://
#define NDEF_URIPREFIX_TEL                  (0x05)  // tel:
#define NDEF_URIPREFIX_MAILTO               (0x06)  // mailto:
#define NDEF_URIPREFIX_FTP_ANONAT           (0x07)  // ftp://anonymous@
#define NDEF_URIPREFIX_FTP_FTPDOT           (0x08)  // ftp://ftp.
#define NDEF_URIPREFIX_FTPS                 (0x09)  // ftps://
#define NDEF_URIPREFIX_SFTP                 (0x0A)  // sftp://
#define NDEF_URIPREFIX_SMB                  (0x0B)  // smb://
#define NDEF_URIPREFIX_NFS                  (0x0C)  // nfs://
#define NDEF_URIPREFIX_FTP                  (0x0D)  // ftp://
#define NDEF_URIPREFIX_DAV                  (0x0E)  // dav://
#define NDEF_URIPREFIX_NEWS                 (0x0F)  // news:
#define NDEF_URIPREFIX_TELNET               (0x10)  // telnet://
#define NDEF_URIPREFIX_IMAP                 (0x11)  // imap:
#define NDEF_URIPREFIX_RTSP                 (0x12)  // rtsp://
#define NDEF_URIPREFIX_URN                  (0x13)  // urn:
#define NDEF_URIPREFIX_POP                  (0x14)  // pop:
#define NDEF_URIPREFIX_SIP                  (0x15)  // sip:
#define NDEF_URIPREFIX_SIPS                 (0x16)  // sips:
#define NDEF_URIPREFIX_TFTP                 (0x17)  // tftp:
#define NDEF_URIPREFIX_BTSPP                (0x18)  // btspp://
#define NDEF_URIPREFIX_BTL2CAP              (0x19)  // btl2cap://
#define NDEF_URIPREFIX_BTGOEP               (0x1A)  // btgoep://
#define NDEF_URIPREFIX_TCPOBEX              (0x1B)  // tcpobex://
#define NDEF_URIPREFIX_IRDAOBEX             (0x1C)  // irdaobex://
#define NDEF_URIPREFIX_FILE                 (0x1D)  // file://
#define NDEF_URIPREFIX_URN_EPC_ID           (0x1E)  // urn:epc:id:
#define NDEF_URIPREFIX_URN_EPC_TAG          (0x1F)  // urn:epc:tag:
#define NDEF_URIPREFIX_URN_EPC_PAT          (0x20)  // urn:epc:pat:
#define NDEF_URIPREFIX_URN_EPC_RAW          (0x21)  // urn:epc:raw:
#define NDEF_URIPREFIX_URN_EPC              (0x22)  // urn:epc:
#define NDEF_URIPREFIX_URN_NFC              (0x23)  // urn:nfc:

/* ============================================================
 *  GPIO 引脚定义
 * ============================================================ */
#define PN532_GPIO_VALIDATIONBIT            (0x80)  // GPIO 验证位
#define PN532_GPIO_P30                      (0)     // P30 引脚 (可用作 GPIO)
#define PN532_GPIO_P31                      (1)     // P31 引脚 (可用作 GPIO)
#define PN532_GPIO_P32                      (2)     // P32 引脚 (保留，必须为1)
#define PN532_GPIO_P33                      (3)     // P33 引脚 (可用作 GPIO)
#define PN532_GPIO_P34                      (4)     // P34 引脚 (保留，必须为1)
#define PN532_GPIO_P35                      (5)     // P35 引脚 (可用作 GPIO)

/* ============================================================
 *  FeliCa 常量定义
 * ============================================================ */
#define FELICA_READ_MAX_SERVICE_NUM         16      // 最大读服务数
#define FELICA_READ_MAX_BLOCK_NUM           12      // 最大读块数 (典型 FeliCa 卡)
#define FELICA_WRITE_MAX_SERVICE_NUM        16      // 最大写服务数
#define FELICA_WRITE_MAX_BLOCK_NUM          10      // 最大写块数 (典型 FeliCa 卡)
#define FELICA_REQ_SERVICE_MAX_NODE_NUM     32      // 最大请求服务节点数

/**
 * class PN532 — PN532 NFC 控制器驱动类
 *
 *   封装了 PN532 芯片的所有功能，通过 PN532Interface
 *   通信接口与芯片交互。支持以下 NFC 操作：
 *
 *   【基础功能】
 *   - SAMConfig():              配置安全访问模块
 *   - getFirmwareVersion():     获取固件版本
 *   - readRegister/writeRegister(): 读写寄存器
 *   - readGPIO/writeGPIO():     读写 GPIO 引脚
 *   - setPassiveActivationRetries(): 设置被动激活重试次数
 *   - setRFField():             控制 RF 射频场
 *
 *   【NFC P2P (DEP) 功能 — 本项目核心】
 *   - inJumpForDEP():           以 DEP 协议发起 P2P 连接
 *   - tgInitAsTarget():         初始化为目标设备
 *   - inDataExchange():         数据交换 (发起者侧)
 *   - tgGetData():              接收数据 (目标侧)
 *   - tgSetData():              发送数据 (目标侧)
 *   - inRelease():              释放 NFC 链路
 *
 *   【ISO14443A 功能】
 *   - readPassiveTargetID():    读取被动目标 UID
 *   - inListPassiveTarget():    列出被动目标
 *
 *   【Mifare Classic 功能】
 *   - mifareclassic_AuthenticateBlock(): 认证块
 *   - mifareclassic_ReadDataBlock():     读数据块
 *   - mifareclassic_WriteDataBlock():    写数据块
 *   - mifareclassic_FormatNDEF():        格式化为 NDEF
 *   - mifareclassic_WriteNDEFURI():      写入 NDEF URI
 *
 *   【Mifare Ultralight 功能】
 *   - mifareultralight_ReadPage():       读页
 *   - mifareultralight_WritePage():      写页
 *
 *   【FeliCa 功能】
 *   - felica_Polling():                 轮询 FeliCa 卡
 *   - felica_SendCommand():             发送 FeliCa 命令
 *   - felica_RequestService():          请求服务
 *   - felica_RequestResponse():         请求响应
 *   - felica_ReadWithoutEncryption():   无加密读取
 *   - felica_WriteWithoutEncryption():  无加密写入
 *   - felica_RequestSystemCode():       请求系统码
 *   - felica_Release():                 释放 FeliCa 卡
 */
class PN532
{
public:
    /**
     * 构造函数
     * @param interface  PN532Interface 通信接口对象（如 PN532_I2C）
     */
    PN532(PN532Interface &interface);

    /**
     * begin() — 初始化 PN532
     *   调用通信接口的 begin() 和 wakeup() 完成硬件初始化
     */
    void begin(void);

    /* ============================================================
     *  基础功能函数
     * ============================================================ */

    /**
     * SAMConfig() — 配置 SAM（安全访问模块）
     *   设置 PN532 为正常工作模式，启用 IRQ 引脚
     * @return true  — 配置成功
     *         false — 配置失败
     */
    bool SAMConfig(void);

    /**
     * getFirmwareVersion() — 获取 PN532 固件版本
     * @return 32位版本信息 (IC版本 + 固件版本 + 功能支持标志)
     *         0 表示失败
     */
    uint32_t getFirmwareVersion(void);

    /**
     * readRegister() — 读取 PN532 寄存器
     * @param reg  16位寄存器地址
     * @return     寄存器值
     */
    uint32_t readRegister(uint16_t reg);

    /**
     * writeRegister() — 写入 PN532 寄存器
     * @param reg  16位寄存器地址
     * @param val  8位写入值
     * @return     0=失败, 1=成功
     */
    uint32_t writeRegister(uint16_t reg, uint8_t val);

    /**
     * writeGPIO() — 写入 GPIO 引脚状态
     * @param pinstate  引脚状态位图
     * @return true=成功, false=失败
     */
    bool writeGPIO(uint8_t pinstate);

    /**
     * readGPIO() — 读取 GPIO 引脚状态
     * @return  引脚状态位图
     */
    uint8_t readGPIO(void);

    /**
     * setPassiveActivationRetries() — 设置被动激活重试次数
     * @param maxRetries  最大重试次数 (0xFF=无限等待, 0x00~0xFE=指定次数)
     * @return true=成功, false=失败
     */
    bool setPassiveActivationRetries(uint8_t maxRetries);

    /**
     * setRFField() — 控制 RF 射频场
     * @param autoRFCA  0x00=不检查外部场, 0x02=激活前检查外部场
     * @param rFOnOff   0x00=关闭 RF 场, 0x01=开启 RF 场
     * @return true=成功, false=失败
     */
    bool setRFField(uint8_t autoRFCA, uint8_t rFOnOff);

    /* ============================================================
     *  NFC P2P (DEP) 功能 — 本项目核心
     * ============================================================ */

    /**
     * tgInitAsTarget() — 初始化 PN532 为目标设备
     *   配置 PN532 为被动目标模式，等待发起者连接。
     *   内部包含 LLCP（逻辑链路控制协议）参数配置。
     *
     * @param timeout  等待超时时间 (毫秒), 0=不超时
     * @return   >0  — 成功（已就绪等待连接）
     *           =0  — 超时
     *           <0  — 失败
     */
    int8_t tgInitAsTarget(uint16_t timeout = 0);

    /**
     * tgInitAsTarget() — 自定义命令初始化为目标
     * @param command  自定义命令帧
     * @param len      命令帧长度
     * @param timeout  超时时间
     * @return   >0=成功, =0=超时, <0=失败
     */
    int8_t tgInitAsTarget(const uint8_t* command, const uint8_t len, const uint16_t timeout = 0);

    /**
     * tgGetData() — 目标模式下接收数据
     *   阻塞等待发起者发送数据
     * @param buf  接收缓冲区
     * @param len  缓冲区长度
     * @return   >0  — 接收到的数据长度
     *           <=0 — 失败
     */
    int16_t tgGetData(uint8_t *buf, uint8_t len);

    /**
     * tgSetData() — 目标模式下发送数据
     *   向发起者发送响应数据
     * @param header  数据头
     * @param hlen    头长度
     * @param body    数据体（可选）
     * @param blen    体长度
     * @return true=成功, false=失败
     */
    bool tgSetData(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);

    /**
     * inRelease() — 释放当前 NFC 链路
     * @param relevantTarget  目标编号 (0=释放所有)
     * @return  响应长度, 0=失败
     */
    int16_t inRelease(const uint8_t relevantTarget = 0);

    /**
     * inJumpForDEP() — 以 DEP 协议发起 P2P 连接
     *   被动模式：本地 PN532 产生射频场，与对端 Target 建立链路
     *
     * @param baudrate  速率: 0x00=106kbps, 0x01=212kbps, 0x02=424kbps
     * @param passive   true=被动模式, false=主动模式
     * @return true=连接成功, false=连接失败
     */
    bool inJumpForDEP(uint8_t baudrate = 0x00, bool passive = true);

    /* ============================================================
     *  ISO14443A 功能
     * ============================================================ */

    /**
     * inListPassiveTarget() — 列出被动目标
     *   PN532 作为读卡器，检测场内的被动目标
     * @return true=检测到目标, false=未检测到或失败
     */
    bool inListPassiveTarget();

    /**
     * readPassiveTargetID() — 读取被动目标 UID
     * @param cardbaudrate  卡波特率
     * @param uid           UID 输出缓冲区
     * @param uidLength     UID 长度输出
     * @param timeout       超时时间 (毫秒)
     * @return true=成功, false=失败
     */
    bool readPassiveTargetID(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout = 1000);

    /**
     * inDataExchange() — 与已连接目标交换数据
     *   发送数据并接收响应 (发起者侧使用)
     * @param send            发送数据
     * @param sendLength      发送长度
     * @param response        响应缓冲区
     * @param responseLength  响应长度 (输入为缓冲区大小，输出为实际长度)
     * @return true=成功, false=失败
     */
    bool inDataExchange(uint8_t *send, uint8_t sendLength, uint8_t *response, uint8_t *responseLength);

    /* ============================================================
     *  Mifare Classic 功能
     * ============================================================ */

    /**
     * mifareclassic_IsFirstBlock() — 判断是否为扇区首块
     *   小扇区 (0~127): 每4块一个扇区，块号%4==0 为首块
     *   大扇区 (128+):   每16块一个扇区，块号%16==0 为首块
     * @param uiBlock  块号
     * @return true=首块, false=非首块
     */
    bool mifareclassic_IsFirstBlock (uint32_t uiBlock);

    /**
     * mifareclassic_IsTrailerBlock() — 判断是否为扇区尾块
     *   尾块存储密钥和访问控制位
     * @param uiBlock  块号
     * @return true=尾块, false=非尾块
     */
    bool mifareclassic_IsTrailerBlock (uint32_t uiBlock);

    /**
     * mifareclassic_AuthenticateBlock() — 认证 Mifare Classic 块
     *   使用密钥 A 或 B 对指定块进行认证
     * @param uid         卡 UID
     * @param uidLen      UID 长度 (Mifare Classic 为 4 字节)
     * @param blockNumber 块号 (1K卡: 0~63, 4K卡: 0~255)
     * @param keyNumber   密钥类型 (0=KEY_A, 1=KEY_B)
     * @param keyData     6字节密钥数据
     * @return 1=成功, 0=失败
     */
    uint8_t mifareclassic_AuthenticateBlock (uint8_t *uid, uint8_t uidLen, uint32_t blockNumber, uint8_t keyNumber, uint8_t *keyData);

    /**
     * mifareclassic_ReadDataBlock() — 读取 Mifare Classic 数据块
     *   读取指定块的 16 字节数据
     * @param blockNumber  块号
     * @param data         16字节输出缓冲区
     * @return 1=成功, 0=失败
     */
    uint8_t mifareclassic_ReadDataBlock (uint8_t blockNumber, uint8_t *data);

    /**
     * mifareclassic_WriteDataBlock() — 写入 Mifare Classic 数据块
     *   写入 16 字节数据到指定块
     * @param blockNumber  块号
     * @param data         16字节输入数据
     * @return 1=成功, 0=失败
     */
    uint8_t mifareclassic_WriteDataBlock (uint8_t blockNumber, uint8_t *data);

    /**
     * mifareclassic_FormatNDEF() — 格式化为 NDEF 格式
     *   将 Mifare Classic 卡格式化为 NFC Forum Tag
     * @return 1=成功, 0=失败
     */
    uint8_t mifareclassic_FormatNDEF (void);

    /**
     * mifareclassic_WriteNDEFURI() — 写入 NDEF URI 记录
     * @param sectorNumber   扇区号 (1~15)
     * @param uriIdentifier  URI 前缀标识
     * @param url            URI 文本 (最长38字符)
     * @return 1=成功, 0=失败
     */
    uint8_t mifareclassic_WriteNDEFURI (uint8_t sectorNumber, uint8_t uriIdentifier, const char *url);

    /* ============================================================
     *  Mifare Ultralight 功能
     * ============================================================ */

    /**
     * mifareultralight_ReadPage() — 读取 Ultralight 页
     *   读取指定页的 4 字节数据
     * @param page    页号 (0~63)
     * @param buffer  4字节输出缓冲区
     * @return 1=成功, 0=失败
     */
    uint8_t mifareultralight_ReadPage (uint8_t page, uint8_t *buffer);

    /**
     * mifareultralight_WritePage() — 写入 Ultralight 页
     *   写入 4 字节数据到指定页
     * @param page    页号 (0~63)
     * @param buffer  4字节输入数据
     * @return 1=成功, 0=失败
     */
    uint8_t mifareultralight_WritePage (uint8_t page, uint8_t *buffer);

    /* ============================================================
     *  FeliCa 功能
     * ============================================================ */

    /**
     * felica_Polling() — 轮询 FeliCa 卡
     *   检测场内的 FeliCa 卡并获取 IDm 和 PMm
     * @param systemCode         系统码 (0xFFFF=匹配所有)
     * @param requestCode        请求码 (0=无, 1=请求系统码, 2=请求通信性能)
     * @param idm                IDm 输出 (8字节)
     * @param pmm                PMm 输出 (8字节)
     * @param systemCodeResponse 系统码响应输出 (可选)
     * @param timeout            超时时间
     * @return 1=检测到卡, 0=未检测到, <0=错误
     */
    int8_t felica_Polling(uint16_t systemCode, uint8_t requestCode, uint8_t *idm, uint8_t *pmm, uint16_t *systemCodeResponse, uint16_t timeout=1000);

    /**
     * felica_SendCommand() — 发送 FeliCa 命令
     * @param command        命令数据
     * @param commandlength  命令长度
     * @param response       响应缓冲区
     * @param responseLength 响应长度输出
     * @return 1=成功, <0=错误
     */
    int8_t felica_SendCommand (const uint8_t * command, uint8_t commandlength, uint8_t * response, uint8_t * responseLength);

    /**
     * felica_RequestService() — 请求 FeliCa 服务
     * @param numNode       节点数
     * @param nodeCodeList  节点码列表 (大端序)
     * @param keyVersions   密钥版本输出
     * @return 1=成功, <0=错误
     */
    int8_t felica_RequestService(uint8_t numNode, uint16_t *nodeCodeList, uint16_t *keyVersions) ;

    /**
     * felica_RequestResponse() — 请求 FeliCa 响应
     * @param mode  当前模式输出
     * @return 1=成功, <0=错误
     */
    int8_t felica_RequestResponse(uint8_t *mode);

    /**
     * felica_ReadWithoutEncryption() — 无加密读取 FeliCa 数据
     * @param numService       服务数
     * @param serviceCodeList  服务码列表
     * @param numBlock         块数
     * @param blockList        块列表
     * @param blockData        块数据输出
     * @return 1=成功, <0=错误
     */
    int8_t felica_ReadWithoutEncryption (uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16]);

    /**
     * felica_WriteWithoutEncryption() — 无加密写入 FeliCa 数据
     * @param numService       服务数
     * @param serviceCodeList  服务码列表
     * @param numBlock         块数
     * @param blockList        块列表
     * @param blockData        块数据输入
     * @return 1=成功, <0=错误
     */
    int8_t felica_WriteWithoutEncryption (uint8_t numService, const uint16_t *serviceCodeList, uint8_t numBlock, const uint16_t *blockList, uint8_t blockData[][16]);

    /**
     * felica_RequestSystemCode() — 请求 FeliCa 系统码
     * @param numSystemCode   系统码数量输出
     * @param systemCodeList  系统码列表输出
     * @return 1=成功, <0=错误
     */
    int8_t felica_RequestSystemCode(uint8_t *numSystemCode, uint16_t *systemCodeList);

    /**
     * felica_Release() — 释放 FeliCa 卡
     * @return 1=成功, <0=错误
     */
    int8_t felica_Release();

    /* ============================================================
     *  辅助打印函数
     * ============================================================ */

    /**
     * PrintHex() — 以十六进制格式打印数据
     *   输出格式: " XX XX XX XX"
     * @param data     数据指针
     * @param numBytes 数据长度
     */
    static void PrintHex(const uint8_t *data, const uint32_t numBytes);

    /**
     * PrintHexChar() — 以十六进制+字符格式打印数据
     *   输出格式: " XX XX XX XX    ...."
     * @param data     数据指针
     * @param numBytes 数据长度
     */
    static void PrintHexChar(const uint8_t *pbtData, const uint32_t numBytes);

    /**
     * getBuffer() — 获取内部缓冲区指针
     *   用于直接操作 PN532 命令缓冲区
     * @param len  输出缓冲区可用长度
     * @return     缓冲区指针
     */
    uint8_t *getBuffer(uint8_t *len) {
        *len = sizeof(pn532_packetbuffer) - 4;
        return pn532_packetbuffer;
    };

private:
    uint8_t _uid[7];              // ISO14443A UID 缓存
    uint8_t _uidLen;              // UID 长度
    uint8_t _key[6];              // Mifare Classic 密钥缓存
    uint8_t inListedTag;          // 当前已列出的目标编号
    uint8_t _felicaIDm[8];       // FeliCa IDm (NFCID2) 缓存
    uint8_t _felicaPMm[8];       // FeliCa PMm (PAD) 缓存

    uint8_t pn532_packetbuffer[64];  // PN532 命令/响应包缓冲区

    PN532Interface *_interface;   // 通信接口指针
};

#endif
