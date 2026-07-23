/**
 * ============================================================
 *  PN532_I2C.h — PN532 I2C 通信接口实现
 * ============================================================
 *
 * 【文件作用】
 *   实现 PN532Interface 基类，通过 I2C 总线与 PN532 芯片通信。
 *   这是本项目使用的具体传输层实现。
 *
 * 【I2C 地址】
 *   PN532 的 I2C 地址为 0x48（7位地址），在代码中右移1位得到 0x24。
 *   实际通信时 Arduino Wire 库会自动处理地址位。
 *
 * 【依赖库】
 *   - Wire.h: Arduino I2C 通信库
 *   - PN532Interface.h: PN532 通信接口抽象基类
 *
 * ============================================================
 *
 * @modified picospuch
 */

#ifndef __PN532_I2C_H__
#define __PN532_I2C_H__

#include <Wire.h>
#include "PN532Interface.h"

/**
 * class PN532_I2C — PN532 I2C 通信接口
 *
 *   继承 PN532Interface，通过 I2C 总线实现与 PN532 的通信。
 *   封装了帧构造、ACK 校验、响应读取等底层操作。
 */
class PN532_I2C : public PN532Interface {
public:
    /**
     * 构造函数
     * @param wire  TwoWire 对象引用（通常是 Wire 或 Wire1）
     */
    PN532_I2C(TwoWire &wire);

    /**
     * begin() — 初始化 I2C 总线
     */
    void begin();

    /**
     * wakeup() — 唤醒 PN532
     *   I2C 模式下需要等待 500ms 让芯片就绪
     */
    void wakeup();

    /**
     * writeCommand() — 发送 PN532 命令帧
     *   构造完整的 PN532 帧（前导码+数据+校验），通过 I2C 发送，
     *   然后等待并验证 ACK 帧。
     *
     * @param header  命令头（含命令码）
     * @param hlen    命令头长度
     * @param body    命令体（可选）
     * @param blen    命令体长度
     * @return   0    — 成功
     *           非0  — 失败
     */
    virtual int8_t writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);

    /**
     * readResponse() — 读取 PN532 响应帧
     *   等待 PN532 就绪，读取响应帧，验证帧格式和校验，
     *   返回纯数据部分。
     *
     * @param buf     接收缓冲区
     * @param len     缓冲区最大长度
     * @param timeout 超时时间 (毫秒)
     * @return   >=0  — 数据长度
     *           <0   — 失败
     */
    int16_t readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);

private:
    TwoWire* _wire;       // I2C 总线对象指针
    uint8_t command;      // 当前发送的命令码（用于验证响应命令码 = command+1）

    /**
     * readAckFrame() — 读取并验证 ACK 帧
     *   ACK 帧固定为 6 字节: 00 00 FF 00 FF 00
     * @return   0    — ACK 有效
     *           非0  — ACK 无效或超时
     */
    int8_t readAckFrame();

    /**
     * getResponseLength() — 获取响应帧的数据长度
     *   先轮询 PN532 状态寄存器等待就绪，然后读取帧头获取长度。
     *   读取后发送 NACK 请求重新读取完整响应。
     *
     * @param buf     缓冲区
     * @param len     缓冲区长度
     * @param timeout 超时时间
     * @return   >=0  — 响应数据长度
     *           <0   — 失败
     */
    int16_t getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout);

    /**
     * write() — I2C 写一个字节
     *   兼容不同版本 Arduino IDE 的 Wire 库 API
     */
    inline uint8_t write(uint8_t data) {
        #if ARDUINO >= 100
            return _wire->write(data);
        #else
            return _wire->send(data);
        #endif
    }

    /**
     * read() — I2C 读一个字节
     *   兼容不同版本 Arduino IDE 的 Wire 库 API
     */
    inline uint8_t read() {
        #if ARDUINO >= 100
            return _wire->read();
        #else
            return _wire->receive();
        #endif
    }
};

#endif
