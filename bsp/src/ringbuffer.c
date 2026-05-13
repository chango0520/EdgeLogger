#include "ringbuffer.h"

/**
 * @brief 初始化环形缓冲区
 */
void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief 单字节写入
 * @retval true 成功，false 缓冲区满
 */
bool ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
    if (ring_buffer_is_full(rb)) {
        return false;
    }
    uint32_t idx = rb->tail % RING_BUFFER_SIZE;
    rb->buffer[idx] = data;
    rb->tail++;
    return true;
}

/**
 * @brief 单字节读出
 * @retval true 成功，false 缓冲区空
 */
bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
{
    if (ring_buffer_is_empty(rb)) {
        return false;
    }
    uint32_t idx = rb->head % RING_BUFFER_SIZE;
    *data = rb->buffer[idx];
    rb->head++;
    return true;
}

/**
 * @brief 批量写入
 * @param data 源数据指针
 * @param len  期望写入长度
 * @return 实际写入的字节数
 */
uint32_t ring_buffer_write_multi(ring_buffer_t *rb, const uint8_t *data, uint32_t len)
{
    uint32_t free = ring_buffer_free_space(rb);
    if (len > free) {
        len = free;   /* 只写入实际可用的空间 */
    }
    if (len == 0) {
        return 0;
    }

    uint32_t idx = rb->tail % RING_BUFFER_SIZE;
    uint32_t first_chunk = RING_BUFFER_SIZE - idx;  /* 到数组末尾的长度 */

    if (len <= first_chunk) {
        /* 只需一段拷贝 */
        for (uint32_t i = 0; i < len; i++) {
            rb->buffer[idx + i] = data[i];
        }
    } else {
        /* 需要两段拷贝：先拷到末尾，再从开头继续 */
        uint32_t second_chunk = len - first_chunk;
        for (uint32_t i = 0; i < first_chunk; i++) {
            rb->buffer[idx + i] = data[i];
        }
        for (uint32_t i = 0; i < second_chunk; i++) {
            rb->buffer[i] = data[first_chunk + i];
        }
    }

    rb->tail += len;
    return len;
}

/**
 * @brief 批量读出
 * @param data 目标缓冲区
 * @param len  期望读出长度
 * @return 实际读出的字节数
 */
uint32_t ring_buffer_read_multi(ring_buffer_t *rb, uint8_t *data, uint32_t len)
{
    uint32_t avail = ring_buffer_available(rb);
    if (len > avail) {
        len = avail;
    }
    if (len == 0) {
        return 0;
    }

    uint32_t idx = rb->head % RING_BUFFER_SIZE;
    uint32_t first_chunk = RING_BUFFER_SIZE - idx;

    if (len <= first_chunk) {
        for (uint32_t i = 0; i < len; i++) {
            data[i] = rb->buffer[idx + i];
        }
    } else {
        uint32_t second_chunk = len - first_chunk;
        for (uint32_t i = 0; i < first_chunk; i++) {
            data[i] = rb->buffer[idx + i];
        }
        for (uint32_t i = 0; i < second_chunk; i++) {
            data[first_chunk + i] = rb->buffer[i];
        }
    }

    rb->head += len;
    return len;
}

/**
 * @brief 获取可读字节数
 */
uint32_t ring_buffer_available(ring_buffer_t *rb)
{
    return (uint32_t)(rb->tail - rb->head) % RING_BUFFER_SIZE;
}

/**
 * @brief 获取剩余可用空间（保留 1 字节）
 */
uint32_t ring_buffer_free_space(ring_buffer_t *rb)
{
    return (RING_BUFFER_SIZE - 1) - ring_buffer_available(rb);
}

/**
 * @brief 检查是否为空
 */
bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return (rb->head == rb->tail);
}

/**
 * @brief 检查是否为满
 */
bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return (ring_buffer_available(rb) == (RING_BUFFER_SIZE - 1));
}

/**
 * @brief 清空缓冲区
 */
void ring_buffer_clear(ring_buffer_t *rb)
{
    rb->head = rb->tail;  /* 直接丢弃所有数据 */
}
