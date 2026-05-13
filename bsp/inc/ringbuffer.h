#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>

/* 缓冲区大小 */
#define RING_BUFFER_SIZE    1024


/**
 * @brief 环形缓冲区结构体
 * @note  内部保留 1 字节用于区分“满”和“空”，实际可用空间为 RING_BUFFER_SIZE - 1
 */
typedef struct {
    uint8_t  buffer[RING_BUFFER_SIZE];  /* 数据存储区 */
    volatile uint32_t head;             /* 读指针 */
    volatile uint32_t tail;             /* 写指针 */
} ring_buffer_t;

/* 初始化 */
void ring_buffer_init(ring_buffer_t *rb);

/* 单字节写入/读出 */
bool ring_buffer_write(ring_buffer_t *rb, uint8_t data);
bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data);

/* 批量写入/读出，返回实际操作字节数 */
uint32_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint32_t len);
uint32_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint32_t len);

/* 查询状态 */
uint32_t ring_buffer_available(ring_buffer_t *rb);   /* 可读字节数 */
uint32_t ring_buffer_free_space(ring_buffer_t *rb);  /* 剩余可用空间 */
bool ring_buffer_is_empty(ring_buffer_t *rb);
bool ring_buffer_is_full(ring_buffer_t *rb);

/* 清空缓冲区 */
void ring_buffer_clear(ring_buffer_t *rb);

#endif /* RING_BUFFER_H */
