/*
 Copyright (c) 2003-2015 Andy Little.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see http://www.gnu.org/licenses./
 */
#include "resources.hpp"
#include "config.hpp"
#include "frsky.hpp"
#include "aircraft.hpp"

#include "settings.hpp"

/*
 The FrSky ZAPP1 protocol

*/

namespace {

   namespace zapp1_impl{

   struct FrSky_msgID {
      static const char header_value =   0x5e;
      static const char escape_value =   0x5d;

      static const char Latitude     =    0x1; // deg10e7 to int32_t
      static const char Longtitude   =    0x2; // deg10e7 to int32_t
      static const char Altitude     =    0x8; // altitude in m 
   };

   template<typename T>
   struct create_message {
       create_message(char msg_id, T const & val);
       static const size_t length = sizeof(T) + 3;
       typedef char (&msg_buf_t)[length];
       msg_buf_t get() {
           return m_msg_buf;
       }
       create_message & operator = (T const & val);
       void assign( T const & val);
   private:
       typedef union {
           char m_array[sizeof(T)];
           T m_raw_value;
       } converter;
       char      m_msg_buf[length];
   };
    
   /*
    len is length of buf including checksum, but checksum is last byte in buf so
   last byte not included
   */
   template <char Len>
   static char FrSky_do_checksum(char * ar)
   {
       uint8_t sum = static_cast<uint8_t>(ar[0]);
       for ( size_t i = 1; i < (Len-1); ++i) {
           uint16_t const sum1 = sum  + static_cast<uint16_t>( static_cast<uint8_t>(ar[i]));
           sum += static_cast<uint8_t>(sum1 & 0xff)  + static_cast<uint8_t>((sum1 >> 8) && 0xff);
       }
       return static_cast<char>(sum);
   }

   template<typename T>
   void create_message<T>::assign( T const & val)
   {
       converter conv;
       conv.m_raw_value = val;
    
       for (size_t i = 0; i < sizeof(T); ++i) {
           m_msg_buf[2 + i] = conv.m_array[i];
       }
       m_msg_buf[length -1] = FrSky_do_checksum<length>(m_msg_buf);
   }
   // sort so lat and lon are uint32_t.. currently int32_t
   template<typename T>
   create_message<T>::create_message(char msg_id, T const & val)
   {
       m_msg_buf[0] = FrSky_msgID::header_value;
       m_msg_buf[1] = msg_id;
      
       this->assign(val);
   }
    
   template <typename T>
   create_message<T> & create_message<T>::operator = (T const & val)
   {
       this->assign(val);
       return *this;
   }
    
   create_message<int32_t> lat_msg(FrSky_msgID::Latitude,0);
   create_message<int32_t> lon_msg(FrSky_msgID::Longtitude,0);
   create_message<int32_t> alt_msg(FrSky_msgID::Altitude,0);

    /*
    angle is +- deg  - 180 to 180
    convert to angle of +- deg * 1e7
   */

   inline int32_t normalise_angle(quan::angle_<int32_t>::deg10e7 const & in)
   {
       int64_t v = in.numeric_value();
       if ( v >   1800000000LL){
                  v -= 3600000000LL;
         }else{
            if ( v < -1799999999LL){
                     v += 3600000000LL;
            }
         }
        return static_cast<int32_t>(v);
   }
   /* 
   max FrSky update rate is 1200 baud. If this is called at 50 Hz then
   works out that can send only 7 bytes over 3 cycles.
   so send 2, 2, then 3
    gives update rate for all variables of around 5 Hz

   or do one every 7 ms?

   The following functions are put in an array and called in rotation
     First func fro var does setup, others just put out some bytes
     called in 50 Hz loop so as not to exceed max data rate
   */
   // Latitude

   inline int16_t esc_write_sp(char * buf, int16_t len, bool start_of_frame)
   {

      int16_t pos = 0;
      int16_t count = 0;
      if(start_of_frame){
       frsky_tx_rx_task::put(FrSky_msgID::header_value);
        ++pos;
        ++count;
      }
      for( ; pos < len; ++pos){
          char ch = buf[pos];
          if ( (ch == FrSky_msgID::header_value) || (ch == FrSky_msgID::escape_value) ){
            frsky_tx_rx_task::put(FrSky_msgID::escape_value);
            frsky_tx_rx_task::put(ch ^ 0x60);
            count += 2;
          }else{
             frsky_tx_rx_task::put(ch);
             ++count;
          }
      }
      return count;
   }

   //return actual num of chars written including escapes
   int16_t update_lat_msg1()
   {
       the_aircraft.mutex_acquire();
         quan::angle_<int32_t>::deg10e7 temp =  the_aircraft.location.gps_lat;
       the_aircraft.mutex_release();
       lat_msg = normalise_angle(temp);
       return esc_write_sp(lat_msg.get(), 2, true);
   }
   int16_t update_lat_msg2()
   {
       return esc_write_sp(lat_msg.get() + 2, 2, false);
   }
   int16_t update_lat_msg3()
   {
      return esc_write_sp(lat_msg.get() + 4, 3, false);
   }

   //longtitude
   int16_t update_lon_msg1()
   {
      the_aircraft.mutex_acquire();
         quan::angle_<int32_t>::deg10e7 temp =  the_aircraft.location.gps_lon;
      the_aircraft.mutex_release();
      lon_msg = normalise_angle(temp);
      return esc_write_sp(lon_msg.get(), 2, true);
   }

   int16_t update_lon_msg2()
   {
      return esc_write_sp(lon_msg.get() + 2, 2, false);
   }
   int16_t update_lon_msg3()
   {
      return esc_write_sp(lon_msg.get() + 4, 3, false);
   }

   int16_t update_alt_msg1()
   {
      if ( settings::altitude_src == settings::altitude_t::gps_alt){
         the_aircraft.mutex_acquire();
            quan::length_<int32_t>::mm temp = the_aircraft.location.gps_alt;
         the_aircraft.mutex_release();
         alt_msg = temp.numeric_value()/1000;
      }else{
         the_aircraft.mutex_acquire();
            quan::length_<float>::m temp = the_aircraft.baro_alt;
         the_aircraft.mutex_release();
         alt_msg = static_cast<int32_t>(temp.numeric_value());
      }
      return esc_write_sp(alt_msg.get(),2,true);
   }

   int16_t update_alt_msg2()
   {
      return esc_write_sp(alt_msg.get() + 2, 2, false);
   }
   int16_t update_alt_msg3()
   {
      return esc_write_sp(alt_msg.get() + 4, 3,false);
   }

   typedef int16_t(*msgfun)();
    
   msgfun msgfuns[] = {
       update_lat_msg1,
       update_lat_msg2,
       update_lat_msg3,
       update_lon_msg1,
       update_lon_msg2,
       update_lon_msg3,
       update_alt_msg1,
       update_alt_msg2,
       update_alt_msg3,
   };

  void send_message()
  {
    static uint8_t idx = 0;
    static uint8_t byte_idx = 0;
    static int16_t write_count = 0;
    
    while (write_count <= 0 ){ // ready for more comms
       // call current fun
       write_count += msgfuns[idx]();
       //and update to next fun
       idx = (idx + 1) % (sizeof(msgfuns)/sizeof(msgfun));
    }
    // number of bytes allowed is 7  per 60 msec ( each call happens every 20 msec)
   // so split the bytes into 2,2,3 allowed each time.
    byte_idx = (byte_idx + 1 ) % 3; 
    int const bytes_gone = (byte_idx == 2)?3:2;
    write_count -= bytes_gone;
    // cap the read ahead...
    if( write_count < -64){
       write_count = -64;
    }
  }
 } // zapp1_impl

}//namespace

namespace zapp1{

   // call once per millisec
   void frsky_send_message()
   {
      zapp1_impl::send_message();
   }
}

