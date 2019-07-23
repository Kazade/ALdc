ALdc is a port of MojoAL to the Dreamcast. It's been achieved by porting the necessary parts of SDL2 and then writing a custom SDL audio device which sends the stream in to the Kallistios snd_stream code. It supports core OpenAL 1.1 and seems to work really well!

This library (along with GLdc) is part of the Simulant engine project and was necessary to get Simulant's audio functionality working on the Dreamcast.

ALdc inherits the zlib licensing of upstream MojoAL and SDL 2 (GLdc is MIT, Simulant is LGPL/proprietary) which means you can legally ship it compiled into Dreamcast binaries.

Huge thanks to Ryan C. Gordon for MojoAL, and special thanks to mrneo240 for solving the buffering issues I was having, as well as his streaming sample contribution. Finally big shout out to everyone on the Simulant Discord who helped test on various emulators!