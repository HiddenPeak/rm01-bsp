#include <stdio.h>
#include <stdint.h>

// 当前使用的CRC8算法
uint8_t calculate_crc8_current(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // 多项式0x07
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// 常用的CRC8-Dallas/Maxim算法
uint8_t calculate_crc8_dallas(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;  // 多项式0x31 (reversed)
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// CRC8算法（多项式0x07，初始值0x00）
uint8_t calculate_crc8_simple(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// CRC8算法（多项式0x31）
uint8_t calculate_crc8_poly31(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// 简单的校验和算法
uint8_t calculate_checksum(const uint8_t *data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// XOR校验算法
uint8_t calculate_xor_checksum(const uint8_t *data, size_t length) {
    uint8_t xor_result = 0;
    for (size_t i = 0; i < length; i++) {
        xor_result ^= data[i];
    }
    return xor_result;
}

int main() {
    // 测试数据：FF 14 2D，期望CRC：DB
    uint8_t test_data[] = {0xFF, 0x14, 0x2D};
    uint8_t expected_crc = 0xDB;
    
    printf("测试数据: FF 14 2D\n");
    printf("期望CRC: 0x%02X\n\n", expected_crc);
    
    printf("不同CRC算法测试结果：\n");
    printf("当前算法(0x07,init=0xFF): 0x%02X\n", calculate_crc8_current(test_data, 3));
    printf("Dallas/Maxim算法:         0x%02X\n", calculate_crc8_dallas(test_data, 3));
    printf("简单CRC8(0x07,init=0x00): 0x%02X\n", calculate_crc8_simple(test_data, 3));
    printf("CRC8多项式0x31:           0x%02X\n", calculate_crc8_poly31(test_data, 3));
    printf("简单校验和:               0x%02X\n", calculate_checksum(test_data, 3));
    printf("XOR校验:                  0x%02X\n", calculate_xor_checksum(test_data, 3));
    
    // 测试只校验电压和电流（不包括包头）
    uint8_t data_only[] = {0x14, 0x2D};
    printf("\n只校验电压和电流 (14 2D)：\n");
    printf("当前算法(0x07,init=0xFF): 0x%02X\n", calculate_crc8_current(data_only, 2));
    printf("Dallas/Maxim算法:         0x%02X\n", calculate_crc8_dallas(data_only, 2));
    printf("简单CRC8(0x07,init=0x00): 0x%02X\n", calculate_crc8_simple(data_only, 2));
    printf("CRC8多项式0x31:           0x%02X\n", calculate_crc8_poly31(data_only, 2));
    printf("简单校验和:               0x%02X\n", calculate_checksum(data_only, 2));
    printf("XOR校验:                  0x%02X\n", calculate_xor_checksum(data_only, 2));
    
    return 0;
}
