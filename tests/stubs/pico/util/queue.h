#pragma once
#include <deque>
#include <vector>
#include <cstring>
struct queue_t {size_t element=0,capacity=0;std::deque<std::vector<uint8_t>> items;};
inline void queue_init(queue_t *q,size_t element,size_t capacity){q->element=element;q->capacity=capacity;q->items.clear();}
inline bool queue_try_add(queue_t *q,const void *value){if(q->items.size()>=q->capacity)return false;auto *p=static_cast<const uint8_t*>(value);q->items.emplace_back(p,p+q->element);return true;}
inline bool queue_try_peek(queue_t *q,void *value){if(q->items.empty())return false;std::memcpy(value,q->items.front().data(),q->element);return true;}
inline bool queue_try_remove(queue_t *q,void *value){if(!queue_try_peek(q,value))return false;q->items.pop_front();return true;}
inline unsigned queue_get_level(queue_t *q){return q->items.size();}
inline bool queue_is_empty(queue_t *q){return q->items.empty();}
