#include "../lib/SpeakJet/src/speakjet.c"
#include <stdio.h>
#include <assert.h>

static void feed(sj_chip *s, const uint8_t *p, int n) {
  for (int i=0; i<n; ++i) assert(sj_write_byte(s,p[i]));
}

int main(void) {
  sj_chip *a=sj_new(), *b=sj_new();
  assert(a && b);
  int accepted=0, rejected=0;
  float x[8192], y[8192];
  for (int arrival=64; arrival<=512; arrival*=2) {
  for (int first=128;first<=199; first+=(arrival==64?1:4)) {
    for (int second=128;second<=199; second+=(arrival==64?1:4)) {
      sj_power_on(a); sj_power_on(b);
      uint8_t pair[]={first,second};
      feed(a,pair,2); feed(b,pair,1);
      sj_render(a,x,arrival); sj_render(b,y,arrival);
      feed(b,pair+1,1); sj_join_pending(b);
      if (b->joined) {
        ++accepted;
        if (memcmp(x,y,arrival*sizeof(float))) {
          fprintf(stderr,"past changed %d %d\n",first,second); return 1;
        }
        sj_render(a,x,8192); sj_render(b,y,8192);
        if (memcmp(x,y,sizeof(x))) {
          int at=0; while(at<8192 && x[at]==y[at]) ++at;
          fprintf(stderr,"future differs %d %d at %d: %g vs %g\n",first,second,at,x[at],y[at]);return 1;
        }
      } else ++rejected;
    }
  }
  }
  /* An arrival after release began leaves the current plan untouched. */
  sj_power_on(b); uint8_t iy=128; feed(b,&iy,1);
  sj_render(b,y,1);
  int until_release=b->stotal-b->release-b->stick+1;
  assert(until_release>0 && until_release<8192);
  sj_render(b,y,until_release);
  segment before[MAX_SEGMENTS]; memcpy(before,b->seg,sizeof(before));
  uint8_t ow=137; feed(b,&ow,1); sj_join_pending(b);
  assert(!b->joined && !memcmp(before,b->seg,sizeof(before)));
  /* Reset preserves the reusable planner and the immutable noise sweeps. */
  sj_chip *scratch=b->join_scratch;
  noise_sweep sweeps[SWEEP_COUNT]; memcpy(sweeps,b->sweeps,sizeof(sweeps));
  sj_power_on(b);
  assert(b->join_scratch==scratch && !memcmp(sweeps,b->sweeps,sizeof(sweeps)));
  /* Explicit Repeat's final copy must not be mistaken for a first play. */
  sj_power_on(b);
  const uint8_t repeat[]={26,2,128};feed(b,repeat,3);
  do { sj_render(b,y,1); } while (b->copies > 0);
  assert(b->sounding && b->repeated_command);
  memcpy(before,b->seg,sizeof(before));
  feed(b,&ow,1);sj_join_pending(b);
  assert(!b->joined && !memcmp(before,b->seg,sizeof(before)));
  sj_render(b,y,4096);
  for(int n=0;n<4096;++n) assert(isfinite(y[n]));
  /* Exact coefficient caching and caller block size must not change PCM. */
  for (int code=128; code<=254; ++code) {
    sj_power_on(a); sj_power_on(b);
    uint8_t sound=(uint8_t)code; feed(a,&sound,1);feed(b,&sound,1);
    sj_render(a,x,4096);
    for (int n=0;n<4096;++n) {
      b->noise_cached=false;
      sj_render(b,y+n,1);
    }
    assert(!memcmp(x,y,4096*sizeof(float)));
  }
  printf("Live pairs accepted=%d safely separate=%d; late arrival and reset OK\n",accepted,rejected);
  sj_free(a);sj_free(b);
  return 0;
}
