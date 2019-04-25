#include "thread/channel.h"
#include "event/event.h"
#include "types.h"
#include "lib/err.h"
#include "lib/tinycthread/tinycthread.h"
#include "lib/vec/vec.h"
#include <math.h>

struct Channel {
  Ref ref;
  mtx_t lock;
  cnd_t cond;
  vec_t(Variant) messages;
  u64 sent;
  u64 received;
};

const usize sizeof_Channel = sizeof(Channel);

Channel* lovrChannelInit(Channel* channel) {
  vec_init(&channel->messages);
  mtx_init(&channel->lock, mtx_plain | mtx_timed);
  cnd_init(&channel->cond);
  return channel;
}

void lovrChannelDestroy(void* ref) {
  Channel* channel = ref;
  lovrChannelClear(channel);
  vec_deinit(&channel->messages);
  mtx_destroy(&channel->lock);
  cnd_destroy(&channel->cond);
}

bool lovrChannelPush(Channel* channel, Variant* variant, f64 timeout, u64* id) {
  mtx_lock(&channel->lock);

  if (channel->messages.length == INT32_MAX) {
    mtx_unlock(&channel->lock);
    lovrThrow("Channel cannot hold any more messages");
  }

  if (channel->messages.length == 0) {
    lovrRetain(channel);
  }

  vec_insert(&channel->messages, 0, *variant);
  *id = ++channel->sent;
  cnd_broadcast(&channel->cond);

  if (isnan(timeout) || timeout < 0.) {
    mtx_unlock(&channel->lock);
    return false;
  }

  while (channel->received < *id && timeout >= 0.) {
    if (isinf(timeout)) {
      cnd_wait(&channel->cond, &channel->lock);
    } else {
      struct timespec start;
      struct timespec until;
      struct timespec stop;
      timespec_get(&start, TIME_UTC);
      f64 whole, fraction;
      fraction = modf(timeout, &whole);
      until.tv_sec = start.tv_sec + (time_t) whole;
      until.tv_nsec = start.tv_nsec + (long) (fraction * 1e9 + .5);
      cnd_timedwait(&channel->cond, &channel->lock, &until);
      timespec_get(&stop, TIME_UTC);
      timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (f64) 1e9;
    }
  }

  bool read = channel->received >= *id;
  mtx_unlock(&channel->lock);
  return read;
}

bool lovrChannelPop(Channel* channel, Variant* variant, f64 timeout) {
  mtx_lock(&channel->lock);

  do {
    if (channel->messages.length > 0) {
      *variant = vec_pop(&channel->messages);
      if (channel->messages.length == 0) {
        lovrRelease(Channel, channel);
      }
      channel->received++;
      cnd_broadcast(&channel->cond);
      mtx_unlock(&channel->lock);
      return true;
    } else if (isnan(timeout) || timeout < 0.) {
      mtx_unlock(&channel->lock);
      return false;
    }

    if (isinf(timeout)) {
      cnd_wait(&channel->cond, &channel->lock);
    } else {
      struct timespec start;
      struct timespec until;
      struct timespec stop;
      timespec_get(&start, TIME_UTC);
      f64 whole, fraction;
      fraction = modf(timeout, &whole);
      until.tv_sec = start.tv_sec + (time_t) whole;
      until.tv_nsec = start.tv_nsec + (long) (fraction * 1e9 + .5);
      cnd_timedwait(&channel->cond, &channel->lock, &until);
      timespec_get(&stop, TIME_UTC);
      timeout -= (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / (f64) 1e9;
    }
  } while (true);
}

bool lovrChannelPeek(Channel* channel, Variant* variant) {
  mtx_lock(&channel->lock);

  if (channel->messages.length > 0) {
    *variant = vec_last(&channel->messages);
    mtx_unlock(&channel->lock);
    return true;
  }

  mtx_unlock(&channel->lock);
  return false;
}

void lovrChannelClear(Channel* channel) {
  mtx_lock(&channel->lock);
  for (i32 i = 0; i < channel->messages.length; i++) {
    lovrVariantDestroy(&channel->messages.data[i]);
  }
  channel->received = channel->sent;
  vec_clear(&channel->messages);
  cnd_broadcast(&channel->cond);
  mtx_unlock(&channel->lock);
}

i32 lovrChannelGetCount(Channel* channel) {
  mtx_lock(&channel->lock);
  i32 length = channel->messages.length;
  mtx_unlock(&channel->lock);
  return length;
}

bool lovrChannelHasRead(Channel* channel, u64 id) {
  mtx_lock(&channel->lock);
  bool received = channel->received >= id;
  mtx_unlock(&channel->lock);
  return received;
}
