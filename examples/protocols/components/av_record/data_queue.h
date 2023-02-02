/* Data queue utility to buffer data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _DATA_Q_H
#define _DATA_Q_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Struct for data queue
 *        Data queue works like a queue, you can receive the exact size of data as you send previously.
 *        It allows you to get continuous buffer so that no need to care ring back issue.
 *        It adds a fill_end member to record fifo write end position before ring back.
 */
typedef struct {
    void *buffer;     /*!< Buffer for queue */
    int   size;       /*!< Buffer size */
    int   fill_end;   /*!< Buffer write position before ring back */
    int   wp;         /*!< Write pointer */
    int   rp;         /*!< Read pointer */
    int   user;       /*!< Buffer reference by reader or writer */
    int   quit;       /*!< Buffer quit flag */
    void *lock;       /*!< Protect lock */
    void *write_lock; /*!< Write lock to let only one writer at same time */
    void *event;      /*!< Event group to wake up reader or writer */
} data_queue_t;

/**
 * @brief         Initialize data queue
 *
 * @param         size: Buffer size
 * @return        - NULL: Fail to initialize queue
 *                - Others: Data queue instance
 */
data_queue_t *data_queue_init(int size);

/**
 * @brief         Wakeup thread which wait on queue data
 *
 * @param         q: Data queue instance
 */
void data_queue_wakeup(data_queue_t *q);

/**
 * @brief         Deinitialize data queue
 *
 * @param         q: Data queue instance
 */
void data_queue_deinit(data_queue_t *q);

/**
 * @brief         Get continuous buffer from data queue
 *
 * @param         q: Data queue instance
 * @param         size: Buffer size want to get
 * @return        - NULL: Fail to get buffer
 *                - Others: Buffer data
 */
void *data_queue_get_buffer(data_queue_t *q, int size);

/**
 * @brief         Send data into data queue
 *
 * @param         q: Data queue instance
 * @param         size: Buffer size want to get
 * @return        - 0: On success
 *                - Others: Fail to send buffer
 */
int data_queue_send_buffer(data_queue_t *q, int size);

/**
 * @brief         Read data from data queue, and add reference count
 *
 * @param         q: Data queue instance
 * @param[out]    buffer: Buffer in front of queue, this buffer is always valid before call `data_queue_read_unlock`
 * @param[out]    size: Buffer size in front of queue
 * @return        - 0: On success
 *                - Others: Fail to read buffer
 */
int data_queue_read_lock(data_queue_t *q, void **buffer, int *size);

/**
 * @brief         Release data be read and decrease reference count
 *
 * @param         q: Data queue instance
 * @param[out]    buffer: Buffer in front of queue
 * @param[out]    size: Buffer size in front of queue
 * @return        - 0: On success
 *                - Others: Fail to read buffer
 */
int data_queue_read_unlock(data_queue_t *q);

/**
 * @brief         Query data queue information
 *
 * @param         q: Data queue instance
 * @param[out]    q_num: Data block number in queue
 * @param[out]    q_size: Total data size kept in queue
 * @return        - 0: On success
 *                - Others: Fail to query
 */
int data_queue_query(data_queue_t *q, int *q_num, int *q_size);

/**
 * @brief         Query available data size
 *
 * @param         q: Data queue instance
 * @return        Available size for write queue
 */
int data_queue_get_available(data_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif
