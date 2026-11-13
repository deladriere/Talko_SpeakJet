#include "../src/banks.h"
#include "../lib/SpeakJet/src/speakjet.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

int main() {
  sj_chip *a=sj_new(), *b=sj_new();
  assert(a && b);
  unsigned selections=0;
  float pcm[64], other[64];sj_trace tags[64];
  for (auto bank : banks) for (size_t item=0;item<bank.count;++item) {
    sj_power_on(a);sj_power_on(b);
    Entry e=bank.entries ? bank.entries[item] : sound("native",bank.firstCode+item);
    auto feed=[&](sj_chip *s){
      if(e.bytes) for(unsigned i=0;i<e.count;++i) assert(sj_write_byte(s,e.bytes[i]));
      else assert(sj_write_byte(s,e.singleCode));
    };
    feed(a);feed(b);
    bool active=true;unsigned blocks=0;uint32_t previous=0;
    while(active && ++blocks<5000) {
      active=sj_render_tagged(a,pcm,tags,64);
      assert(sj_render(b,other,64)==active);
      assert(!std::memcmp(pcm,other,sizeof(pcm)));
      for (unsigned k=0;k<64;++k) {
        assert(std::isfinite(pcm[k]) && std::fabs(pcm[k])<1.5f);
        assert(tags[k].byte_offset>=previous);previous=tags[k].byte_offset;
        assert(tags[k].code==0 || (tags[k].code>=128 && tags[k].code<=254));
      }
    }
    assert(blocks<5000 && sj_buffered(a)==0);
    ++selections;
  }
  for(unsigned speed: {0u,60u,108u,114u,118u,127u}) {
    for(unsigned modifier: {7u,8u,14u,15u}) {
      sj_power_on(a);
      const uint8_t seq[]={21,uint8_t(speed),128,uint8_t(modifier),187,uint8_t(modifier),187,4,141,179,137,2,190,128};
      for(auto c:seq) assert(sj_write_byte(a,c));
      unsigned n=0;bool active=true;
      while(active && ++n<5000) {
        active=sj_render(a,pcm,64);
        for(auto x:pcm) assert(std::isfinite(x)&&std::fabs(x)<1.5f);
      }
      assert(n<5000 && sj_buffered(a)==0);
    }
  }
  sj_power_on(a);
  for(unsigned n=0;n<64;++n) assert(sj_write_byte(a,128));
  assert(!sj_write_byte(a,128));
  assert(sj_buffer_half_full(a));
  sj_power_on(a);assert(sj_buffered(a)==0);
  printf("%u bank selections, tagged/plain sample parity, 24 modifier/Speed streams and FIFO bounds OK\n",selections);
  sj_free(a);sj_free(b);
}
