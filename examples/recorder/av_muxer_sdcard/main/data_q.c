/* Data queue utility to buffer data

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "media_lib_os.h"
#include "data_q.h"

#define DATA_Q_ALLOC_HEAD_SIZE   (4)
#define DATA_Q_DATA_ARRIVE_BITS  (1)
#define DATA_Q_DATA_CONSUME_BITS (2)
#define DATA_Q_USER_FREE_BITS    (4)

#define _SET_BITS(group, bit)    media_lib_event_group_set_bits((media_lib_event_grp_handle_t) group, bit)
// need manual clear bits
#define _WAIT_BITS(group, bit)                                                                           \
    media_lib_event_group_wait_bits((media_lib_event_grp_handle_t) group, bit, MEDIA_LIB_MAX_LOCK_TIME); \
    media_lib_event_group_clr_bits(group, bit)

#define _MUTEX_LOCK(mutex)   media_lib_mutex_lock((media_lib_mutex_handle_t) mutex, MEDIA_LIB_MAX_LOCK_TIME)
#define _MUTEX_UNLOCK(mutex) media_lib_mutex_unlock((media_lib_mutex_handle_t) mutex)

static int data_q_release_user(data_q_t* q)
{
    _SET_BITS(q->event, DATA_Q_USER_FREE_BITS);
    return 0;
}

static int data_q_notify_data(data_q_t* q)
{
    _SET_BITS(q->event, DATA_Q_DATA_ARRIVE_BITS);
    return 0;
}

static int data_q_wait_data(data_q_t* q)
{
    q->user++;
    _MUTEX_UNLOCK(q->lock);
    _WAIT_BITS(q->event, DATA_Q_DATA_ARRIVE_BITS);
    _MUTEX_LOCK(q->lock);
    int ret = (q->quit) ? -1 : 0;
    q->user--;
    data_q_release_user(q);
    return ret;
}

static int data_q_data_consumed(data_q_t* q)
{
    _SET_BITS(q->event, DATA_Q_DATA_CONSUME_BITS);
    return 0;
}

static int data_q_wait_consume(data_q_t* q)
{
    q->user++;
    _MUTEX_UNLOCK(q->lock);
    _WAIT_BITS(q->event, DATA_Q_DATA_CONSUME_BITS);
    _MUTEX_LOCK(q->lock);
    int ret = (q->quit) ? -1 : 0;
    q->user--;
    data_q_release_user(q);
    return ret;
}

static int data_q_wait_user(data_q_t* q)
{
    _MUTEX_UNLOCK(q->lock);
    _WAIT_BITS(q->event, DATA_Q_USER_FREE_BITS);
    _MUTEX_LOCK(q->lock);
    return 0;
}

data_q_t* data_q_init(int size)
{
    data_q_t* q = media_lib_calloc(1, size);
    if (q == NULL) {
        return NULL;
    }
    q->buffer = media_lib_malloc(size);
    media_lib_mutex_create(&q->lock);
    media_lib_event_group_create(&q->event);
    if (q->buffer == NULL || q->lock == NULL || q->event == NULL) {
        data_q_deinit(q);
        return NULL;
    }
    q->size = size;
    return q;
}

void data_q_deinit(data_q_t* q)
{
    if (q) {
        if (q->lock) {
            _MUTEX_LOCK(q->lock);
            q->quit = 1;
            // send quit message to let user quit
            data_q_notify_data(q);
            data_q_data_consumed(q);
            while (q->user) {
                data_q_wait_user(q);
            }
            _MUTEX_UNLOCK(q->lock);
            media_lib_mutex_destroy((media_lib_mutex_handle_t) q->lock);
        }
        if (q->event) {
            media_lib_event_group_destroy((media_lib_mutex_handle_t) q->event);
        }
        if (q->buffer) {
            media_lib_free(q->buffer);
        }
        media_lib_free(q);
    }
}

/*   case 1:  [0...rp...wp...size]
 *   case 2:  [0...wp...rp...fillend...size]
 *   special case: wp == rp have fill_end buffer full else empty
 */
static int data_q_get_available(data_q_t* q)
{
    if (q->wp > q->rp) {
        return q->size - q->wp;
    }
    if (q->wp == q->rp) {
        if (q->fill_end) {
            return 0;
        }
        return q->size - q->wp;
    }
    return q->rp - q->wp;
}

static bool data_q_have_data(data_q_t* q)
{
    if (q->wp == q->rp && q->fill_end == 0) {
        return false;
    }
    return true;
}

void* data_q_get_buffer(data_q_t* q, int size)
{
    int avail = 0;
    size += DATA_Q_ALLOC_HEAD_SIZE;
    if (q == NULL || size > q->size) {
        return NULL;
    }
    _MUTEX_LOCK(q->lock);
    while (1) {
        avail = data_q_get_available(q);
        // size not enough
        if (avail < size && q->fill_end == 0) {
            if (q->wp == q->rp) {
                q->wp = q->rp = 0;
            }
            q->fill_end = q->wp;
            q->wp = 0;
            avail = data_q_get_available(q);
        }
        if (avail >= size) {
            uint8_t* buffer = (uint8_t*) q->buffer + q->wp;
            q->user++;
            _MUTEX_UNLOCK(q->lock);
            return buffer + DATA_Q_ALLOC_HEAD_SIZE;
        }
        int ret = data_q_wait_consume(q);
        if (ret != 0) {
            _MUTEX_UNLOCK(q->lock);
            break;
        }
    }
    return NULL;
}

int data_q_send_buffer(data_q_t* q, int size)
{
    int ret = -1;
    if (q == NULL) {
        return -1;
    }
    size += DATA_Q_ALLOC_HEAD_SIZE;
    _MUTEX_LOCK(q->lock);
    if (data_q_get_available(q) >= size) {
        uint8_t* buffer = (uint8_t*) q->buffer + q->wp;
        *((int*) buffer) = size;
        q->wp += size;
        q->user--;
        data_q_notify_data(q);
        data_q_release_user(q);
        _MUTEX_UNLOCK(q->lock);
        ret = 0;
    } else {
        _MUTEX_UNLOCK(q->lock);
    }
    return ret;
}

int data_q_read_lock(data_q_t* q, void** buffer, int* size)
{
    int ret = -1;
    _MUTEX_LOCK(q->lock);
    while (1) {
        if (data_q_have_data(q) == false) {
            if (data_q_wait_data(q) != 0) {
                _MUTEX_UNLOCK(q->lock);
                return -1;
            }
            continue;
        }
        uint8_t* data_buffer = (uint8_t*) q->buffer + q->rp;
        int data_size = *((int*) data_buffer);
        *buffer = data_buffer + DATA_Q_ALLOC_HEAD_SIZE;
        *size = data_size - DATA_Q_ALLOC_HEAD_SIZE;
        q->user++;
        _MUTEX_UNLOCK(q->lock);
        ret = 0;
        break;
    }
    return ret;
}

int data_q_read_unlock(data_q_t* q)
{
    int ret = -1;
    if (q) {
        _MUTEX_LOCK(q->lock);
        uint8_t* buffer = (uint8_t*) q->buffer + q->rp;
        int size = *((int*) buffer);
        q->rp += size;
        if (q->fill_end && q->rp >= q->fill_end) {
            q->fill_end = 0;
            q->rp = 0;
        }
        q->user--;
        data_q_data_consumed(q);
        data_q_release_user(q);
        _MUTEX_UNLOCK(q->lock);
        return 0;
    }
    return ret;
}

int data_q_query(data_q_t* q, int* q_num, int* q_size)
{
    if (q) {
        _MUTEX_LOCK(q->lock);
        *q_num = *q_size = 0;
        if (data_q_have_data(q)) {
            int rp = q->rp;
            int ring = q->fill_end;
            while (rp != q->wp || ring) {
                int size = *(int*) (q->buffer + rp);
                rp += size;
                if (ring && rp == ring) {
                    ring = 0;
                    rp = 0;
                }
                (*q_num)++;
                (*q_size) += size - DATA_Q_ALLOC_HEAD_SIZE;
            }
        }
        _MUTEX_UNLOCK(q->lock);
    }
    return 0;
}
