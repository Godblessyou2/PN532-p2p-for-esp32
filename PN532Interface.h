/**
 * ============================================================
 *  PN532Interface.h — PN532 通信接口抽象基类
 * ============================================================
 *
 * 【文件作用】
 *   定义 PN532 与主控芯片之间的通信接口抽象层。
 *   所有具体的传输方式（I2C、SPI、UART）都需要继承此基类
 *   并实现 writeCommand() 和 readResponse() 两个纯虚函数。
 *
 * 【PN532 帧格式】
 *   PN532 通信采用固定帧格式：
 *   [PREAMBLE][STARTCODE1][STARTCODE2][LEN][LCS][TFI][DATA...][DCS][POSTAMBLE]
 *   - PREAMBLE:  0x00 (前导字节)
 *   - STARTCODE: 0x00 0xFF (固定起始码)
 *   - LEN:       数据长度 (TFI + DATA 的字节数)
 *   - LCS:       长度校验 (~LEN + 1)
 *   - TFI:       传输方向标识
 *                0xD4 = 主机 → PN532 (Host to PN532)
 *                0xD5 = PN532 → 主机 (PN532 to Host)
 *   - DATA:      命令/响应数据
 *   - DCS:       数据校验 (~sum(TFI+DATA) + 1)
 *   - POSTAMBLE: 0x00 (后导字节)
 *
 * ============================================================
 */

#ifndef __PN532_INTERFACE_H__
#define __PN532_INTERFACE_H__

#include <stdint.h>

/* ---- PN532 帧格式常量 ---- */
#define PN532_PREAMBLE                (0x00)   // 前导字节
#define PN532_STARTCODE1              (0x00)   // 起始码第1字节
#define PN532_STARTCODE2              (0xFF)   // 起始码第2字节
#define PN532_POSTAMBLE               (0x00)   // 后导字节

#define PN532_HOSTTOPN532             (0xD4)   // TFI: 主机 → PN532
#define PN532_PN532TOHOST             (0xD5)   // TFI: PN532 → 主机

#define PN532_ACK_WAIT_TIME           (10)     // ACK 等待超时时间 (毫秒)

/* ---- 错误码定义 ---- */
#define PN532_INVALID_ACK             (-1)     // ACK 帧无效
#define PN532_TIMEOUT                 (-2)     // 通信超时
#define PN532_INVALID_FRAME           (-3)     // 帧格式错误 (起始码/校验失败)
#define PN532_NO_SPACE                (-4)     // 接收缓冲区空间不足

/**
 * REVERSE_BITS_ORDER(b) — 位序反转宏
 *   将一个字节的高低位互换，用于某些特殊协议处理。
 *   例如: 0b11001010 → 0b01010011
 */
#define REVERSE_BITS_ORDER(b)         b = (b & 0xF0) >> 4 | (b & 0x0F) << 4; \
                                      b = (b & 0xCC) >> 2 | (b & 0x33) << 2; \
                                      b = (b & 0xAA) >> 1 | (b & 0x55) << 1

/**
 * class PN532Interface — PN532 通信接口抽象基类
 *
 *   所有传输层（I2C/SPI/UART）必须实现以下两个纯虚函数：
 *   - writeCommand(): 发送命令帧并等待 ACK
 *   - readResponse(): 读取响应帧并剥离帧头帧尾
 */
class PN532Interface
{
public:
    /**
     * begin() — 初始化通信接口（如初始化 I2C/SPI 总线）
     */
    virtual void begin() = 0;

    /**
     * wakeup() — 唤醒 PN532 芯片
     *   某些传输方式需要特殊唤醒序列
     */
    virtual void wakeup() = 0;

    /**
     * writeCommand() — 向 PN532 发送命令帧
     *
     *   构造完整的 PN532 帧并发送，然后等待 ACK 响应。
     *
     * @param header  命令头数据（包含命令码）
     * @param hlen    命令头长度
     * @param body    命令体数据（可选）
     * @param blen    命令体长度
     * @return   0    — 发送成功（收到有效 ACK）
     *           非0  — 发送失败
     */
    virtual int8_t writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0) = 0;

    /**
     * readResponse() — 读取 PN532 的响应帧
     *
     *   等待 PN532 返回响应，解析帧格式，剥离帧头帧尾，
     *   将纯数据部分存入 buf。
     *
     * @param buf     接收缓冲区
     * @param len     缓冲区最大长度
     * @param timeout 超时时间 (毫秒)，0 表示不超时
     * @return   >=0  — 响应数据长度（不含帧头帧尾）
     *           <0   — 读取失败（超时/帧错误/空间不足）
     */
    virtual int16_t readResponse(uint8_t buf[], uint8_t len, uint16_t timeout = 1000) = 0;
};

#endif
