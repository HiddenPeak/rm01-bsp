/**
 * @file test_crc_debug.c
 * @brief CRC算法测试程序，用于调试XSP16数据校验
 */

#include <stdio.h>
#include <stdint.h>

// 测试数据：FF 14 2D (前3字节)，期望CRC：DB
uint8_t test_data[] = {0xFF, 0x14, 0x2D};
uint8_t expected_crc = 0xDB;

// 当前使用的CRC8算法（多项式0x07）
uint8_t crc8_current(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF;
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

// CRC8算法变种1：多项式0x31 (Dallas/Maxim)
uint8_t crc8_maxim(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0x8C;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// CRC8算法变种2：多项式0x1D
uint8_t crc8_1d(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x1D;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// CRC8算法变种3：多项式0x39
uint8_t crc8_39(const uint8_t *data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x39;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// CRC8算法变种4：多项式0x9B (SAE J1850)
uint8_t crc8_j1850(const uint8_t *data, size_t length) {
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x1D;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc ^ 0xFF;
}

// 简单校验和
uint8_t simple_checksum(const uint8_t *data, size_t length) {
    uint8_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }
    return sum;
}

// XOR校验
uint8_t xor_checksum(const uint8_t *data, size_t length) {
    uint8_t xor = 0;
    for (size_t i = 0; i < length; i++) {
        xor ^= data[i];
    }
    return xor;
}

// 反向XOR校验
uint8_t reverse_xor_checksum(const uint8_t *data, size_t length) {
    uint8_t xor = 0xFF;
    for (size_t i = 0; i < length; i++) {
        xor ^= data[i];
    }
    return xor;
}

int main(void) {
    printf("XSP16 CRC算法测试\n");
    printf("测试数据: FF 14 2D\n");
    printf("期望CRC: 0x%02X (DB)\n\n", expected_crc);
    
    uint8_t result;
    
    // 测试各种算法
    result = crc8_current(test_data, 3);
    printf("当前CRC8(0x07): 0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = crc8_maxim(test_data, 3);
    printf("CRC8-Maxim:     0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = crc8_1d(test_data, 3);
    printf("CRC8(0x1D):     0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = crc8_39(test_data, 3);
    printf("CRC8(0x39):     0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = crc8_j1850(test_data, 3);
    printf("CRC8-J1850:     0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = simple_checksum(test_data, 3);
    printf("简单校验和:      0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = xor_checksum(test_data, 3);
    printf("XOR校验:        0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    result = reverse_xor_checksum(test_data, 3);
    printf("反向XOR校验:    0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    // 计算期望值的反向算法
    printf("\n分析期望值0xDB:\n");
    printf("0xDB = %d (十进制)\n", expected_crc);
    printf("0xDB = 11011011 (二进制)\n");
    
    // 测试是否只是字节序问题
    uint8_t reversed_data[] = {0x2D, 0x14, 0xFF};
    result = crc8_current(reversed_data, 3);
    printf("\n反向数据测试(2D 14 FF): 0x%02X %s\n", result, (result == expected_crc) ? "✓" : "✗");
    
    return 0;
}
