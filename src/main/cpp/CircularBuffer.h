//
// Created by samma on 2025-09-28, after the tutorial on Embedded Artistry.
//
#include <vector>
#include <iostream>

#ifndef RAYMOB_CIRCULARBUFFER_H
#define RAYMOB_CIRCULARBUFFER_H

#endif //RAYMOB_CIRCULARBUFFER_H

template <class T>
class CircularBuffer {
public:explicit CircularBuffer(size_t size) :
            buf_(std::unique_ptr<T[]>(new T[size])),
            max_size_(size){ }

    void put(T item)
    {
        buf_[head_] = item;

        if(full_)
        {
            tail_ = (tail_ + 1) % max_size_;
        }

        head_ = (head_ + 1) % max_size_;

        full_ = head_ == tail_;
    }

    T get(){
        if(empty())
        {
            return T();
        }

        //Read data and advance the tail (we now have a free space)
        auto val = buf_[tail_];
        full_ = false;
        tail_ = (tail_ + 1) % max_size_;

        return val;
    }

    void reset(){
        head_ = tail_;
        full_ = false;
    }

    bool empty() const{
        //if head and tail are equal, we are empty
        return (!full_ && (head_ == tail_));
    }

    bool full() const{
        //If tail is ahead the head by 1, we are full
        return full_;
    }

    size_t capacity() const
    {
        return max_size_;
    }

    size_t size() const{
        size_t size = max_size_;
        if(!full_){
            if(head_ >= tail_)
            {
                size = head_ - tail_;
            }
            else
            {
                size = max_size_ + head_ - tail_;
            }
        }
        return size;
    }

    int getHead() const{
        return head_;
    }

    int getTail() const{
        return tail_;
    }

private:
    std::unique_ptr<T[]> buf_;
    size_t head_ = 0;
    size_t tail_ = 0;
    const size_t max_size_;
    bool full_ = 0;
};