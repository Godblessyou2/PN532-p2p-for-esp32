/**
 * ============================================================
 *  PN532_I2C.cpp — PN532 I2C 通信接口实现
 * ============================================================
 *
 * 【文件作用】
 *   实现通过 I2C 总线与 PN532 芯片的底层通信协议。
 *   包括：命令帧构造与发送、ACK 帧验证、响应帧读取与解析。
 *
 * 【I2C 通信要点】
 *   - PN532 I2C 地址: 0x48 >> 1 = 0x24（7位地址）
 *   - I2C 最大单次传输: 32 字节（受 Wire 库限制）
 *   - 轮询机制: 通过读取 PN532 状态字节判断是否就绪
 *   - 状态字节 bit0: 1=就绪可读, 0=未就绪
 *
 * 【帧交互流程】
 *   1. 主机发送命令帧 → PN532
 *   2. PN532 返回 ACK 帧 (00 00 FF 00 FF 00)
 *   3. 主机轮询等待 PN532 就绪
 *   4. 主机读取响应帧
 *
 * ============================================================
 *
 * @modified picospuch
 */

#include "PN532_I2C.h"
#include "PN532_debug.h"
#include "Arduino.h"

/* PN532 的 I2C 地址 (0x48 右移1位 = 0x24，7位地址格式) */
#define PN532_I2C_ADDRESS       (0x48 >> 1)


/**
 * 构造函数 — 初始化 I2C 接口对象
 * @param wire  TwoWire 对象引用（通常是全局 Wire 对象）
 */
PN532_I2C::PN532_I2C(TwoWire &wire)
{
    _wire = &wire;
    command = 0;
}

/**
 * begin() — 初始化 I2C 总线
 *   调用 Wire.begin() 启动 I2C 主机模式
 */
void PN532_I2C::begin()
{
    _wire->begin();
}

/**
 * wakeup() — 唤醒 PN532 芯片
 *   I2C 模式下，PN532 上电后需要等待约 500ms 才能开始通信。
 *   这个延时确保芯片内部初始化完成。
 */
void PN532_I2C::wakeup()
{
    delay(500); // 等待 PN532 就绪
}

/**
 * writeCommand() — 向 PN532 发送命令帧
 *
 *   构造完整的 PN532 帧并通过 I2C 发送：
 *   [PREAMBLE][STARTCODE1][STARTCODE2][LEN][LCS][TFI][HEADER...][BODY...][DCS][POSTAMBLE]
 *
 *   发送后等待 PN532 返回 ACK 帧确认。
 *
 * @param header  命令头数据（第一个字节为命令码）
 * @param hlen    命令头长度
 * @param body    命令体数据（可选，默认为 NULL）
 * @param blen    命令体长度
 * @return   0    — 成功（收到有效 ACK）
 *           非0  — 失败
 */
int8_t PN532_I2C::writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    command = header[0];  // 保存命令码，用于后续验证响应

    /* 开始 I2C 传输（发送 PN532_I2C_ADDRESS 地址） */
    _wire->beginTransmission(PN532_I2C_ADDRESS);

    /* ---- 发送帧头 ---- */
    write(PN532_PREAMBLE);      // 前导字节: 0x00
    write(PN532_STARTCODE1);    // 起始码1:  0x00
    write(PN532_STARTCODE2);    // 起始码2:  0xFF

    /* ---- 发送长度字段 ---- */
    uint8_t length = hlen + blen + 1;   // 数据长度 = TFI(1) + header + body
    write(length);                       // LEN: 数据长度
    write(~length + 1);                  // LCS: 长度校验 (~LEN + 1)

    /* ---- 发送 TFI 标识 ---- */
    write(PN532_HOSTTOPN532);            // TFI: 0xD4 表示主机→PN532
    uint8_t sum = PN532_HOSTTOPN532;     // 累加校验和（从 TFI 开始）

    DMSG("write: ");

    /* ---- 发送命令头数据 ---- */
    for (uint8_t i = 0; i < hlen; i++) {
        if (write(header[i])) {
            sum += header[i];            // 累加到校验和

            DMSG_HEX(header[i]);
        } else {
            /* I2C 单次传输最大 32 字节，超出会失败 */
            DMSG("\nToo many data to send, I2C doesn't support such a big packet\n");
            return PN532_INVALID_FRAME;
        }
    }

    /* ---- 发送命令体数据 ---- */
    for (uint8_t i = 0; i < blen; i++) {
        if (write(body[i])) {
            sum += body[i];              // 累加到校验和

            DMSG_HEX(body[i]);
        } else {
            DMSG("\nToo many data to send, I2C doesn't support such a big packet\n");
            return PN532_INVALID_FRAME;
        }
    }

    /* ---- 发送帧尾 ---- */
    uint8_t checksum = ~sum + 1;         // DCS: 数据校验 (~sum + 1)
    write(checksum);
    write(PN532_POSTAMBLE);              // 后导字节: 0x00

    /* 结束 I2C 传输 */
    _wire->endTransmission();

    DMSG('\n');

    /* 等待并验证 ACK 帧 */
    return readAckFrame();
}

/**
 * getResponseLength() — 获取 PN532 响应帧的数据长度
 *
 *   工作流程：
 *   1. 轮询读取 PN532 状态字节，等待 bit0 = 1（就绪）
 *   2. 读取帧头 (PREAMBLE + STARTCODE1 + STARTCODE2)
 *   3. 读取 LEN 字段获取数据长度
 *   4. 发送 NACK 帧请求重新读取完整响应
 *
 * @param buf     缓冲区
 * @param len     缓冲区长度
 * @param timeout 超时时间 (毫秒)
 * @return   >=0  — 响应数据长度
 *           <0   — 失败（超时或帧格式错误）
 */
int16_t PN532_I2C::getResponseLength(uint8_t buf[], uint8_t len, uint16_t timeout) {
    /* NACK 帧: 请求 PN532 重新发送上一次响应 */
    const uint8_t PN532_NACK[] = {0, 0, 0xFF, 0xFF, 0, 0};
    uint16_t time = 0;

    /* ---- 轮询等待 PN532 就绪 ---- */
    do {
        if (_wire->requestFrom(PN532_I2C_ADDRESS, 6)) {
            if (read() & 1) {  // 检查状态字节 bit0: 1=就绪
                break;         // PN532 已就绪
            }
        }

        delay(1);
        time++;
        if ((0 != timeout) && (time > timeout)) {
            return -1;         // 超时
        }
    } while (1);

    /* ---- 验证帧头 ---- */
    if (0x00 != read()      ||       // PREAMBLE:  0x00
            0x00 != read()  ||       // STARTCODE1: 0x00
            0xFF != read()           // STARTCODE2: 0xFF
        ) {

        return PN532_INVALID_FRAME;  // 帧头格式错误
    }

    /* ---- 读取数据长度 ---- */
    uint8_t length = read();

    /* ---- 发送 NACK 请求重新读取完整响应 ---- */
    _wire->beginTransmission(PN532_I2C_ADDRESS);
    for (uint16_t i = 0; i < sizeof(PN532_NACK); ++i) {
      write(PN532_NACK[i]);
    }
    _wire->endTransmission();

    return length;
}

/**
 * readResponse() — 读取 PN532 的完整响应帧
 *
 *   工作流程：
 *   1. 调用 getResponseLength() 获取响应长度
 *   2. 轮询等待 PN532 就绪
 *   3. 读取并验证完整帧格式
 *   4. 校验 TFI、命令码、数据校验和
 *   5. 返回纯数据部分（剥离帧头帧尾）
 *
 *   响应帧格式:
 *   [READY] [PREAMBLE][STARTCODE][LEN][LCS][TFI][CMD][DATA...][DCS][POSTAMBLE]
 *
 * @param buf     接收缓冲区
 * @param len     缓冲区最大长度
 * @param timeout 超时时间 (毫秒)
 * @return   >=0  — 响应数据长度（不含 TFI 和 CMD）
 *           <0   — 失败
 */
int16_t PN532_I2C::readResponse(uint8_t buf[], uint8_t len, uint16_t timeout)
{
    uint16_t time = 0;
    uint8_t length;

    /* 步骤1: 获取响应帧长度 */
    length = getResponseLength(buf, len, timeout);

    /* 步骤2: 轮询等待 PN532 就绪，准备读取完整响应 */
    do {
        if (_wire->requestFrom(PN532_I2C_ADDRESS, 6 + length + 2)) {
            if (read() & 1) {  // 检查状态字节 bit0: 1=就绪
                break;         // PN532 已就绪
            }
        }

        delay(1);
        time++;
        if ((0 != timeout) && (time > timeout)) {
            return -1;         // 超时
        }
    } while (1);

    /* ---- 验证帧头 ---- */
    if (0x00 != read()      ||       // PREAMBLE:  0x00
            0x00 != read()  ||       // STARTCODE1: 0x00
            0xFF != read()           // STARTCODE2: 0xFF
        ) {

        return PN532_INVALID_FRAME;  // 帧头格式错误
    }

    /* ---- 读取并验证长度字段 ---- */
    length = read();

    if (0 != (uint8_t)(length + read())) {   // LEN + LCS 应等于 0
        return PN532_INVALID_FRAME;          // 长度校验失败
    }

    /* ---- 验证 TFI 和命令码 ---- */
    uint8_t cmd = command + 1;               // 响应命令码 = 发送命令码 + 1
    if (PN532_PN532TOHOST != read() || (cmd) != read()) {
        return PN532_INVALID_FRAME;          // TFI 或命令码不匹配
    }

    /* ---- 读取数据部分 ---- */
    length -= 2;                             // 减去 TFI 和 CMD 两个字节
    if (length > len) {
        return PN532_NO_SPACE;               // 缓冲区空间不足
    }

    DMSG("read:  ");
    DMSG_HEX(cmd);

    uint8_t sum = PN532_PN532TOHOST + cmd;   // 校验和累加（TFI + CMD）
    for (uint8_t i = 0; i < length; i++) {
        buf[i] = read();                     // 读取数据字节
        sum += buf[i];                       // 累加校验和

        DMSG_HEX(buf[i]);
    }
    DMSG('\n');

    /* ---- 验证数据校验和 ---- */
    uint8_t checksum = read();
    if (0 != (uint8_t)(sum + checksum)) {
        DMSG("checksum is not ok\n");
        return PN532_INVALID_FRAME;          // 校验和错误
    }

    read();         // 读取 POSTAMBLE (0x00)

    return length;  // 返回数据长度
}

/**
 * readAckFrame() — 读取并验证 ACK 帧
 *
 *   ACK 帧格式固定为 6 字节: 00 00 FF 00 FF 00
 *   PN532 在收到命令帧后会返回 ACK 表示已接收。
 *
 * @return   0    — ACK 有效
 *           非0  — ACK 无效或超时
 */
int8_t PN532_I2C::readAckFrame()
{
    /* ACK 帧的固定内容 */
    const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};
    uint8_t ackBuf[sizeof(PN532_ACK)];

    DMSG("wait for ack at : ");
    DMSG(millis());
    DMSG('\n');

    uint16_t time = 0;

    /* ---- 轮询等待 PN532 就绪 ---- */
    do {
        if (_wire->requestFrom(PN532_I2C_ADDRESS,  sizeof(PN532_ACK) + 1)) {
            if (read() & 1) {  // 检查状态字节 bit0: 1=就绪
                break;         // PN532 已就绪
            }
        }

        delay(1);
        time++;
        if (time > PN532_ACK_WAIT_TIME) {
            DMSG("Time out when waiting for ACK\n");
            return PN532_TIMEOUT;  // 等待 ACK 超时
        }
    } while (1);

    DMSG("ready at : ");
    DMSG(millis());
    DMSG('\n');

    /* ---- 读取 ACK 帧内容 ---- */
    for (uint8_t i = 0; i < sizeof(PN532_ACK); i++) {
        ackBuf[i] = read();
    }

    /* ---- 验证 ACK 帧是否匹配 ---- */
    if (memcmp(ackBuf, PN532_ACK, sizeof(PN532_ACK))) {
        DMSG("Invalid ACK\n");
        return PN532_INVALID_ACK;  // ACK 帧内容不匹配
    }

    return 0;  // ACK 有效
}
