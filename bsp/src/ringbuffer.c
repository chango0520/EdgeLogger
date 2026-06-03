/**
 * @file    ringbuffer.c
 * @brief   无锁环形缓冲区（单生产者-单消费者模型）
 *
 * @note    适合中断 → 任务的数据传递场景。
 *          头/尾指针单调递增（uint32_t 计数器永不归零），
 *          不直接取模，数据写入时通过 % RING_BUFFER_SIZE 定位数组索引。
 *
 *          设计要点：
 *          - 保留 1 字节空间区分"满"和"空"
 *          - volatile 修饰 head/tail，防止编译器优化导致中断中不可见
 *          - 非线程安全！仅适用于单生产者 + 单消费者
 */

#include "ringbuffer.h"

/**
 * @brief  初始化环形缓冲区（头尾指针归零）
 */
void ring_buffer_init(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief  单字节写入
 * @retval true  = 写入成功
 * @retval false = 缓冲区已满
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
 * @brief  单字节读出
 * @param  data : [输出] 读出的字节
 * @retval true  = 读出成功
 * @retval false = 缓冲区为空
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
 * @brief  批量写入（尽可能多写，受剩余空间限制）
 * @param  data : 源数据指针
 * @param  len  : 期望写入长度
 * @return 实际写入的字节数（≤ len，0 表示满）
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
    uint32_t first_chunk = RING_BUFFER_SIZE - idx;  /* 从当前位置到数组末尾的长度 */

    if (len <= first_chunk) {
        /* 无需回绕：一次 memcpy 即可（用 for 循环代替 memcpy 保证可移植性） */
        for (uint32_t i = 0; i < len; i++) {
            rb->buffer[idx + i] = data[i];
        }
    } else {
        /* 需要回绕：先填满到末尾，再从开头继续 */
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
 * @brief  批量读出（尽可能多读，受已有数据的限制）
 * @param  data : 目标缓冲区
 * @param  len  : 期望读出长度
 * @return 实际读出的字节数（≤ len，0 表示空）
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
 * @brief  获取缓冲区中当前可读字节数
 * @note   由于 head 和 tail 为单调递增计数器且差值 ≤ RING_BUFFER_SIZE-1，
 *         tail - head 即为准确的可读字节数，无需取模。
 */
uint32_t ring_buffer_available(ring_buffer_t *rb)
{
    return rb->tail - rb->head;
}

/**
 * @brief  获取剩余可写入字节数
 * @note   保留 1 字节用于区分"满"和"空"，故 (size - 1 - available)
 */
uint32_t ring_buffer_free_space(ring_buffer_t *rb)
{
    return (RING_BUFFER_SIZE - 1) - ring_buffer_available(rb);
}

/**
 * @brief  检查缓冲区是否为空
 */
bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return (rb->head == rb->tail);
}

/**
 * @brief  检查缓冲区是否为满
 */
bool ring_buffer_is_full(ring_buffer_t *rb)
{
    return (ring_buffer_available(rb) == (RING_BUFFER_SIZE - 1));
}

/**
 * @brief  清空缓冲区（丢弃所有数据）
 * @note   直接将 head 对齐到 tail，无需逐字节清零
 */
void ring_buffer_clear(ring_buffer_t *rb)
{
    rb->head = rb->tail;
}
