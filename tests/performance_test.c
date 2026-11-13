#include "../lib/SpeakJet/src/speakjet.c"
#include <assert.h>
#include <stdio.h>
static unsigned duration(sj_chip *s,unsigned pitch,unsigned rate) {
  sj_power_on(s);sj_set_performance(s,pitch,rate);assert(sj_write_byte(s,128));
  float x;unsigned n=0;
  while(sj_render(s,&x,1)){assert(isfinite(x));assert(++n<20000);}
  return n;
}
int main(void) {
  sj_chip *s=sj_new();assert(s);
  unsigned normal=duration(s,256,256),slow=duration(s,256,64),fast=duration(s,256,1024);
  assert(slow>normal*3.9 && slow<normal*4.1);
  assert(fast>normal*.24 && fast<normal*.26);
  assert(duration(s,128,256)==normal && duration(s,512,256)==normal);
  float x[4096];sj_power_on(s);sj_write_byte(s,128);sj_render(s,x,64);int low=s->period;
  sj_set_performance(s,512,256);sj_render(s,x,256);assert(s->period<low*.55);
  /* At slow speed a live next allophone still uses the native join plan. */
  sj_chip *b=sj_new();assert(b);sj_power_on(s);
  sj_set_performance(s,256,64);sj_set_performance(b,256,64);
  sj_write_byte(s,128);sj_write_byte(s,137);sj_write_byte(b,128);
  float y[4096];sj_render(s,x,512);sj_render(b,y,512);assert(!memcmp(x,y,512*sizeof(float)));
  sj_write_byte(b,137);sj_join_pending(b);assert(b->joined);
  sj_render(s,x,4096);sj_render(b,y,4096);assert(!memcmp(x,y,sizeof(x)));
  sj_power_on(s);for(int n=0;n<32;++n)assert(sj_write_byte(s,128));
  assert(!sj_buffer_half_full(s));assert(sj_write_byte(s,128));assert(sj_buffer_half_full(s));
  sj_free(s);sj_free(b);
  printf("Live pitch independent of duration; quarter/four-times rate, slow native joins and half-full flow control OK\n");
}
