
#ifndef _lifo_hpp
#define _lifo_hpp

#define LOCK_TYPE_MUTEX 1
#define LOCK_TYPE_CAS_FENCE 2
#define LOCK_TYPE_CAS 3
#define LOCK_TYPE LOCK_TYPE_CAS_FENCE

#if LOCK_TYPE == LOCK_TYPE_MUTEX
#include <pthread.h>
#else
#include <atomic>
#endif

template <typename T>
class AtomicLifo
{
private:
    T **data;
    int top;
    int size;
#if LOCK_TYPE == LOCK_TYPE_MUTEX
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#elif LOCK_TYPE == LOCK_TYPE_CAS_FENCE
    int critical = 0;
#endif
public:
    AtomicLifo(int size)
    {
        data = new T *[size];
        top = -1;
        this->size = size;
    }

    ~AtomicLifo()
    {
        delete data;
    }

#if LOCK_TYPE == LOCK_TYPE_MUTEX
    bool push(T *item)
    {

        if (pthread_mutex_trylock(&mutex))
        {
            return false;
        }
        int t = top;
        int newt = t + 1;
        if (newt >= size)
        {
            pthread_mutex_unlock(&mutex);
            return false;
        }
        top = newt;
        data[newt] = item;
        pthread_mutex_unlock(&mutex);
        return true;
    }

    T *pop()
    {

        if (pthread_mutex_trylock(&mutex))
        {
            return nullptr;
        }
        int t = top;
        T *item;

        if (t < 0)
        {
            pthread_mutex_unlock(&mutex);
            return nullptr;
        }
        // int newt = t-1;
        top = t - 1;
        item = data[t];
        pthread_mutex_unlock(&mutex);
        return item;
    }

#elif LOCK_TYPE == LOCK_TYPE_CAS_FENCE
    bool push(T *item)
    {
        int OPEN = 0;
        if (!__atomic_compare_exchange_n(&critical, &OPEN, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        {
            return false;
        }
        int t = top;
        int newt = t + 1;
        if (newt >= size)
        {
            __atomic_store_n(&critical, 0, __ATOMIC_SEQ_CST);
            return false;
        }
        top = newt;
        data[newt] = item;
        __atomic_store_n(&critical, 0, __ATOMIC_SEQ_CST);
        return true;
    }

    T *pop()
    {
        int OPEN = 0;
        if (!__atomic_compare_exchange_n(&critical, &OPEN, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        {
            return nullptr;
        }
        int t = top;
        T *item;

        if (t < 0)
        {
            __atomic_store_n(&critical, 0, __ATOMIC_SEQ_CST);
            return nullptr;
        }
        top = t - 1;
        item = data[t];
        __atomic_store_n(&critical, 0, __ATOMIC_SEQ_CST);
        return item;
    }
#elif LOCK_TYPE == LOCK_TYPE_CAS
    bool push(T *item)
    {
        int t = top;
        int newt = t + 1;

        if (newt >= size)
        {
            return false;
        }
        if (!__atomic_compare_exchange(&top, &t, &newt, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        {
            return false;
        }
        data[newt] = item;
        return true;
    }

    T *pop()
    {
        int t = top;
        T *item;

        if (t < 0)
        {
            return nullptr;
        }
        int newt = t - 1;
        if (!__atomic_compare_exchange(&top, &t, &newt, 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))
        {
            return nullptr;
        }
        item = data[t];

        return item;
    }
#endif

    int get_size()
    {
        return top + 1;
    }
};

#endif //_lifo_hpp