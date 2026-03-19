#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// RS232C 电平常量定义
#define RS232_LOGIC_1_VOLT (-12.0)  // 逻辑1（空闲/停止位）
#define RS232_LOGIC_0_VOLT (12.0)   // 逻辑0（起始位/数据0）
// 单字符帧结构：1空闲 + 1起始 + 7数据 + 1停止 = 10位/字符
#define FRAME_BITS_PER_CHAR (10)

/**
 * @brief RS232C编码：字符→电压序列
 * @param volts 输出电压数组（需提前分配内存）
 * @param volts_size 电压数组最大容量
 * @param msg 输入字符数组
 * @param size 输入字符个数
 * @return 实际编码的电压值个数（失败返回-1）
 */
int rs232c_encode(double *volts, int volts_size, const char *msg, int size) {
    // 参数合法性检查
    if (volts == NULL || msg == NULL || size <= 0 || volts_size <= 0) {
        printf("编码参数错误！\n");
        return -1;
    }
    // 计算需要的最小电压数组长度：字符数×单字符帧长度
    int required_size = size * FRAME_BITS_PER_CHAR;
    if (volts_size < required_size) {
        printf("电压数组容量不足，需要%d，实际%d\n", required_size, volts_size);
        return -1;
    }

    int volt_idx = 0;  // 电压数组索引
    // 遍历每个字符进行编码
    for (int i = 0; i < size; i++) {
        unsigned char ch = (unsigned char)msg[i];

        // 1. 填充空闲位（逻辑1，-12V）
        volts[volt_idx++] = RS232_LOGIC_1_VOLT;

        // 2. 填充起始位（逻辑0，+12V）
        volts[volt_idx++] = RS232_LOGIC_0_VOLT;

        // 3. 填充7位数据位（位小端序：先传b0，再b1~b6）
        for (int bit = 0; bit < 7; bit++) {
            // 提取第bit位（0~6），1则为-12V，0则为+12V
            int bit_val = (ch >> bit) & 0x01;
            volts[volt_idx++] = (bit_val == 1) ? RS232_LOGIC_1_VOLT : RS232_LOGIC_0_VOLT;
        }

        // 4. 填充停止位（逻辑1，-12V）
        volts[volt_idx++] = RS232_LOGIC_1_VOLT;
    }

    return volt_idx;  // 返回实际编码的电压数
}

/**
 * @brief RS232C解码：电压序列→字符
 * @param msg 输出字符数组（需提前分配内存）
 * @param size 字符数组最大容量
 * @param volts 输入电压数组
 * @param volts_size 电压数组长度
 * @return 实际解码的字符个数（失败返回-1）
 */
int rs232c_decode(char *msg, int size, const double *volts, int volts_size) {
    // 参数合法性检查
    if (msg == NULL || volts == NULL || size <= 0 || volts_size <= 0) {
        printf("解码参数错误！\n");
        return -1;
    }
    // 检查电压序列长度是否为帧长度的整数倍
    if (volts_size % FRAME_BITS_PER_CHAR != 0) {
        printf("电压序列长度非法，需为%d的整数倍\n", FRAME_BITS_PER_CHAR);
        return -1;
    }

    int char_idx = 0;  // 字符数组索引
    int total_chars = volts_size / FRAME_BITS_PER_CHAR;  // 可解码的字符数
    if (size < total_chars) {
        printf("字符数组容量不足，需要%d，实际%d\n", total_chars, size);
        return -1;
    }

    // 遍历每帧电压序列解码
    for (int i = 0; i < total_chars; i++) {
        // 计算当前字符帧的起始索引
        int frame_start = i * FRAME_BITS_PER_CHAR;
        unsigned char ch = 0;

        // 跳过空闲位（frame_start+0）和起始位（frame_start+1），直接处理数据位
        for (int bit = 0; bit < 7; bit++) {
            // 数据位起始索引：frame_start+2，按位小端序拼接
            int volt_pos = frame_start + 2 + bit;
            double volt = volts[volt_pos];
            
            // 电压判断：接近-12V为逻辑1，接近+12V为逻辑0（容错处理）
            if (volt < -3.0) {  // RS232逻辑1阈值：<-3V
                ch |= (1 << bit);
            }
            // 逻辑0无需处理（默认0）
        }

        // 验证停止位（frame_start+9），非必须但增强鲁棒性
        double stop_volt = volts[frame_start + 9];
        if (stop_volt > -3.0) {
            printf("第%d个字符停止位异常，仍尝试解码\n", i+1);
        }

        msg[char_idx++] = (char)ch;
    }
    // 字符串末尾加结束符
    msg[char_idx] = '\0';
    return char_idx;
}

// 测试函数
int main() {
    // 测试用例：输入字母'X'（ASCII 88 → 二进制 01011000）
    const char input_msg[] = "X";
    int input_size = strlen(input_msg);
    
    // 1. 分配电压数组（单字符需10位，这里预留足够空间）
    double volts[100] = {0};
    int encode_len = rs232c_encode(volts, 100, input_msg, input_size);
    if (encode_len <= 0) {
        return -1;
    }

    // 打印编码后的电压序列
    printf("=== 编码结果（字符'X'的电压序列）===\n");
    printf("帧结构：空闲位 → 起始位 → 7位数据位 → 停止位\n");
    printf("电压序列（单位V）：");
    for (int i = 0; i < encode_len; i++) {
        printf("%.0f ", volts[i]);
    }
    printf("\n\n");

    // 2. 解码电压序列
    char output_msg[100] = {0};
    int decode_len = rs232c_decode(output_msg, 100, volts, encode_len);
    if (decode_len <= 0) {
        return -1;
    }

    // 打印解码结果
    printf("=== 解码结果 ===\n");
    printf("解码字符：%s（ASCII码：%d）\n", output_msg, (unsigned char)output_msg[0]);

    return 0;
}
