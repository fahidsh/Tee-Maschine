# MBED SERIAL DEBUGGER
I have created this library to help manage my Debugging messages 
in my Mbed Projects.

## Why?
The debugging messages are very useful during programming and 
debugging of mbed projects. But there are some reasons why they
are not ideal in Production use:
1. They consume a lot of memory (RAM), and unlike computers, 
   Micro-controllers boards like Arduino and STM don't have 
   mbed projects do not have a lot of RAM.
2. It may cause problems in the execution of the code, since
   usually, in production mode, Micro-controllers may be operated
   through battery. Which unlike computer doesn't have the
   Serial port. So, it might cause not ideal results.

It is of course possible to use the Serial printing during the 
programming and debugging without this or some other similar library 
but using this library just makes it much easier.

For example, you can have the printing enabled at the beginning of 
programming and then disable it when you are done. But later you 
discover a problem in your program, or you want to extend some 
functionality, with change of just one value your can turn printing
on and off.


## How to use this library
The library is used by first creating a macro by the name of 
`SHOW_SERIAL_DEBUG_MESSAGES`.

If the macro is defined, and set to `true`, the messages will be 
printed to Serial Console.

If the macro is not defined or is set to false, the messages will 
not be printed to Serial Console.

For example:
````c++
#define SHOW_SERIAL_DEBUG_MESSAGES true
#include "serial-debugger.h"
````

Now in your code, you can use the LOG macro to print the messages.

For example:
````c++
// print a INFO message
LOG("INFO message from line: %d", __LINE__);
````
### Supported Message Types
You can also specify the type of the message by using the
`MessageType` enum.

For example:
````c++
// print a DEBUG message
LOG(MessageType::DEBUG, "DEBUG message from line: %d", __LINE__);
// print a INFO message
LOG(MessageType::INFO, "INFO message from line: %d", __LINE__);
// print a WARNING message
LOG(MessageType::WARNING, "WARNING message from line: %d", __LINE__);
// print a ERROR message
LOG(MessageType::ERROR, "ERROR message from line: %d", __LINE__);
````
Following are the possible values of the MessageType enum:
| Value | Description |
|-|-|
|`MessageType::INFO`| INFO message |
|`MessageType::DEBUG`| DEBUG message |
|`MessageType::WARNING`| WARNING message |
|`MessageType::ERROR`| ERROR message |
|`MessageType::FETAL`| FETAL message |

### Default Message Type
Default type is `MessageType::INFO`, it is also used when 
`LOG("Message here")` is used without specifying the `MessageType`.

### Turning on/off the printing of messages
You can turn on/off the printing of messages by setting the 
value of `SHOW_SERIAL_DEBUG_MESSAGES` macro to `true` or `false`.

For example:
````c++
// turn on the printing of messages
#define SHOW_SERIAL_DEBUG_MESSAGES true
// turn off the printing of messages
#define SHOW_SERIAL_DEBUG_MESSAGES false
````

## Renaming the macro: LOG
LOG is a small befitting name for the function it has but since it 
is a very common name and there may be situation where you want to 
rename it to something else, you can rename the macro LOG to any 
other name you want. Just open the file `serial_debugger.h` and 
in the top area of file change the macro name `LOG` to the new name.

For example:
````c++
#if SHOW_SERIAL_DEBUG_MESSAGES
    #define LOG(...) MbedSerialDebugger::log(__VA_ARGS__)
#else
    #define LOG(...)
#endif
````

to
````c++
#if SHOW_SERIAL_DEBUG_MESSAGES
    #define SERIAL_LOG(...) MbedSerialDebugger::log(__VA_ARGS__)
#else
    #define SERIAL_LOG(...)
#endif
````
and now you can use the new macro `SERIAL_LOG` to print the messages.
For example:
````c++
// print a INFO message
SERIAL_LOG("INFO message from line: %d", __LINE__);
````

## Tested
This library has been tested on the following boards:
- NUCLEO-L152RE (Mbed-OS 6.x)

## Concept
If somehow this library isn't working for you, I recommend you to 
edit a little and play around with it, or just take the concept 
from it and create your own library.

This readme file is longer than the complete code of the library. 
So I am sure you will be able to understand it easily and improve 
on it.

## Possible Audience
This library may be useful for beginners into Micro-controller 
programming, but also for people who want to improve their
Micro-controller programming skills.

## Liability
This library is provided "as is" and without any warranty.
Please have look at the source code and decide for yourself
if it may be useful for you. If you have any questions, please
contact me at ...

## License
This library is licensed under the MIT license.