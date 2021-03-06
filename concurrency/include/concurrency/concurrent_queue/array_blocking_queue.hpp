#ifndef __ARRAY_BLOCKING_QUEUE_HPP__
#define __ARRAY_BLOCKING_QUEUE_HPP__

#include <mutex>
#include <chrono>
#include <condition_variable>
#include <array>

#include "concurrency/concurrent_queue/concurrent_queue.hpp"

/**
 * @brief 底层由数组实现的队列
 * 
 * @tparam T 元素种类
 * @tparam N 队列容量, 默认容量std::numeric_limits<std::size_t>::max()
 */
template <typename T, std::size_t N = std::numeric_limits<std::size_t>::max()>
class ArrayBlockingQueue
{
private:
    std::array<T, N> _queue;
    std::mutex _mutex;
    std::condition_variable _not_full;
    std::condition_variable _not_empty;
    std::size_t _put_idx = 0;
    std::size_t _take_idx = 0;
    std::size_t _count = 0;

public:
    typedef T value_type;
    ArrayBlockingQueue();
    ~ArrayBlockingQueue();
    ArrayBlockingQueue(const ArrayBlockingQueue &) = delete;
    ArrayBlockingQueue &operator=(const ArrayBlockingQueue &) = delete;
    /**
     * @brief 阻塞地将元素的放入队列
     */
    virtual void push(T &&ele);
    /**
     * @brief 阻塞地将元素的放入队列
     */
    virtual void push(const T &ele);
    /**
     * @brief 将元素的放入队列, 立即返回
     * @param T 入队元素
     * @return bool 入队是否成功
     */
    virtual bool try_push(T &&ele);
    /**
     * @brief 将元素的放入队列, 立即返回
     * @param T 入队元素
     * @return bool 入队是否成功
     */
    virtual bool try_push(const T &ele);

    /**
     * @brief 等待地将元素放进队列
     * 
     * @tparam Rep 刻度数的算术类型
     * @tparam Period 滴答周期
     * @param ele 入队元素
     * @param wait_duration 等待时间
     * @return true 入队成功
     * @return false 入队失败
     */
    template <class Rep, class Period>
    bool wait_push(T &&ele, const std::chrono::duration<Rep, Period> &wait_duration);

    /**
     * @brief 等待地将元素放进队列
     * 
     * @tparam Rep 刻度数的算术类型
     * @tparam Period 滴答周期
     * @param ele 入队元素
     * @param wait_duration 等待时间
     * @return true 入队成功
     * @return false 入队失败
     */
    template <class Rep, class Period>
    bool wait_push(const T &ele, const std::chrono::duration<Rep, Period> &wait_duration);

    /**
     * @brief 将元素弹出队列
     */
    virtual T pop();
    /**
     * @brief try_pop 弹出队列元素, 立即返回
     *
     * @param ele 弹出元素赋值对象
     */
    virtual bool try_pop(T &ele);

    /**
     * @brief 等待地将元素弹出队列
     * 
     * @tparam Rep 刻度数的算术类型
     * @tparam Period 滴答周期
     * @param ele 弹出元素赋值对象
     * @param wait_duration 等待时间
     * @return true 入队成功
     * @return false 入队失败
     */
    template <class Rep, class Period>
    bool wait_pop(T &ele, const std::chrono::duration<Rep, Period> &wait_duration);

    /**
     * @brief 返回队列大小
     */
    virtual std::size_t size();
    /**
     * @brief 返回队列容量
     */
    virtual std::size_t cap();
    /**
     * @brief 返回队列大小
     */
    virtual bool empty();

private:
    /**
     * @brief 队列已满
     */
    bool full();
    /**
     * @brief FIFO插入元素
     */
    void insert(T &&ele);
    /**
     * @brief FIFO插入元素
     */
    void insert(const T &ele);
    /**
     * @brief FIFO删除元素
     */
    T remove();
};

template <typename T, std::size_t N>
ArrayBlockingQueue<T, N>::ArrayBlockingQueue()
{
}

template <typename T, std::size_t N>
ArrayBlockingQueue<T, N>::~ArrayBlockingQueue()
{
}

template <typename T, std::size_t N>
void ArrayBlockingQueue<T, N>::push(T &&ele)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _not_full.wait(lock, [this]()
                       { return !full(); });
        this->insert(std::forward<T>(ele));
    }

    _not_empty.notify_one();
}

template <typename T, std::size_t N>
void ArrayBlockingQueue<T, N>::push(const T &ele)
{
    {
        std::unique_lock<std::mutex> lock(_mutex);
        _not_full.wait(lock, [this]()
                       { return !full(); });
        this->insert(ele);
    }

    _not_empty.notify_one();
}

template <typename T, std::size_t N>
bool ArrayBlockingQueue<T, N>::try_push(T &&ele)
{
    if (_mutex.try_lock() && !full())
    {
        this->insert(std::forward<T>(ele));
        _mutex.unlock();
        _not_empty.notify_one();
        return true;
    }

    return false;
}

template <typename T, std::size_t N>
bool ArrayBlockingQueue<T, N>::try_push(const T &ele)
{
    if (_mutex.try_lock())
    {
        if (full())
        {
            _mutex.unlock();
            return false;
        }
        this->insert(ele);
        _mutex.unlock();
        _not_empty.notify_one();
        return true;
    }

    return false;
}

template <typename T, std::size_t N>
template <class Rep, class Period>
bool ArrayBlockingQueue<T, N>::wait_push(T &&ele, const std::chrono::duration<Rep, Period> &wait_duration)
{
    bool res;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        res = _not_full.wait_for(lock, wait_duration, [this]()
                                 { return !full(); });
        if (res)
        {
            this->insert(ele);
            _not_empty.notify_one();
        }
    }

    return res;
}

template <typename T, std::size_t N>
template <class Rep, class Period>
bool ArrayBlockingQueue<T, N>::wait_push(const T &ele, const std::chrono::duration<Rep, Period> &wait_duration)
{
    bool res;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        res = _not_full.wait_for(lock, wait_duration, [this]()
                                 { return !full(); });
        if (res)
        {
            this->insert(ele);
            _not_empty.notify_one();
        }
    }

    return res;
}

template <typename T, std::size_t N>
T ArrayBlockingQueue<T, N>::pop()
{
    std::unique_lock<std::mutex> lock(_mutex);
    _not_empty.wait(lock, [this]()
                    { return !empty(); });

    auto ele = remove();
    _not_full.notify_one();

    return ele;
}

template <typename T, std::size_t N>
bool ArrayBlockingQueue<T, N>::try_pop(T &ele)
{
    if (_mutex.try_lock())
    {
        if (empty())
        {
            _mutex.unlock();
            return false;
        }
        ele = remove();
        _mutex.unlock();
        _not_full.notify_one();
        return true;
    }

    return false;
}

template <typename T, std::size_t N>
template <class Rep, class Period>
bool ArrayBlockingQueue<T, N>::wait_pop(T &ele, const std::chrono::duration<Rep, Period> &wait_duration)
{
    bool res;
    {
        std::unique_lock<std::mutex> lock(_mutex);
        res = _not_empty.wait_for(lock, wait_duration, [this]()
                                  { return !empty(); });
        if (res)
        {
            ele = remove();
            _not_full.notify_one();
        }
    }

    return res;
}

template <typename T, std::size_t N>
std::size_t ArrayBlockingQueue<T, N>::size()
{
    return this->cap() - _count;
}

template <typename T, std::size_t N>
std::size_t ArrayBlockingQueue<T, N>::cap()
{
    return _queue.max_size();
}

template <typename T, std::size_t N>
bool ArrayBlockingQueue<T, N>::empty()
{
    return _count == 0;
}

template <typename T, std::size_t N>
bool ArrayBlockingQueue<T, N>::full()
{
    return _count == this->cap();
}

template <typename T, std::size_t N>
void ArrayBlockingQueue<T, N>::insert(T &&ele)
{
    _queue[_put_idx] = std::forward<T>(ele);
    if (++_put_idx == cap())
    {
        _put_idx = 0;
    }
    _count++;
}

template <typename T, std::size_t N>
void ArrayBlockingQueue<T, N>::insert(const T &ele)
{
    _queue[_put_idx] = ele;
    if (++_put_idx == cap())
    {
        _put_idx = 0;
    }
    _count++;
}

template <typename T, std::size_t N>
T ArrayBlockingQueue<T, N>::remove()
{
    auto ele = std::move(_queue[_take_idx]);
    if (++_take_idx == cap())
    {
        _take_idx = 0;
    }
    _count--;
    return ele;
}

#endif /* __ARRAY_BLOCKING_QUEUE_HPP__ */
