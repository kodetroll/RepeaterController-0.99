                ARDUINO Repeater Controller V0.99

                     (C) 2012 KB4OID LABS 

           - A Division of Kodetroll Heavy Industries -

This is an Arduino sketch that will implement a rather sketchy (see what I 
did there?) minimalist repeater controller using a simple interface (such 
as a sound card interface). It will keep track of ID time and generate a 
CWID of the specified callsign. Can be configured for negative or positive 
logic input and output. See the README.txt file for more details (including
hookup diagrams).

Thats all...~Steve>, KB4OID (kb4oid@kb4oid.org)

Notes:
This sketch requires the Arduino Time Library, available from:

http://playground.arduino.cc/Code/Time

The Time.h library (as does any using PROGMEM) will only work with pre-1.5.7 
versions of the Arduino IDE. This is due to the requirement that current
versions of the IDE enforce PROGMEM type declarations to be const.

If you want to use the Time.h library with newer IDE versions, ping me for
the changes that need to be made to the library. 

License Info: 
This code (C) 2012 KB4OID Labs - A division of Kodetroll Heavy Industries 
All rights reserved, but otherwise free to use for personal use. 
No warranty expressed or implied. 
This code is for educational or personal use only.
