/*license*/
#pragma once

namespace lexer::internal {
    constexpr auto test_text = R"a(
fmt QUICPacket: 
   form :b1
   if form:
      :LongPacket
   else:
      :OneRTTPacket
   

fmt LongPacket:
   fixed :b1
   long_packet_type :b2
   reserved :b2
   packet_number_length :b2
   version :u32
   

fmt ConnectionID:
   id :[]byte
   fn encode():
        pass      


fmt Varint:
   value :u64
   fn decode(input):
      p = input[0]
      value = match p&0xC0 >> 6:
         0 => input.u8() ~ msb(2)
         1 => input.u16() ~ msb(2)
         2 => input.u32() ~ msb(2)
         3 => input.u64() ~ msb(2)

   fn encode(output):
      match value:
         ..0x40 => output.u8(value.u8())
         ..0x4000 => output.u16(value.u16() | msb(2,1))
         ..0x40000000 => output.u32(value.u32() | msb(2,2))
         ..0x4000000000000000 => output.u64(value | msb(2,3))
         _ => error("too large number")

)a";

}
