#include <hls_stream.h>
#include <stdint.h>
// Probe the 2018.3 scheduler with the completion test before the stream reads.
void timer_probe(hls::stream<uint8_t>& a,hls::stream<uint8_t>& b,
    hls::stream<uint8_t>& c,hls::stream<uint32_t>& out) {
    bool da=false,db=false,dc=false;uint32_t clocks=0;
    for(;;) {
#pragma HLS PIPELINE II=1
        if(da && db && dc) break;
        uint8_t token;
        if(!da && a.read_nb(token))da=true;
        if(!db && b.read_nb(token))db=true;
        if(!dc && c.read_nb(token))dc=true;
        ++clocks;
    }
    out.write(clocks);
}
