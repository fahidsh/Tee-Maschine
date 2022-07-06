//
// Created by fahid on 28.06.2022.
//

#ifndef MBED_SERIAL_DEBUGGER_H
#define MBED_SERIAL_DEBUGGER_H

#if SHOW_SERIAL_DEBUG_MESSAGES
    #define LOG(...) MbedSerialDebugger::log(__VA_ARGS__)
#else
    #define LOG(...)
#endif

enum class MessageType {
    INFO,
    DEBUG,
    LOG,
    WARNING,
    ERROR,
    FETAL
};

class MbedSerialDebugger {
public:
    static void log(const char *message_with_args="", ...);
    static void log(MessageType type, const char *message_with_args="", ...);
private:
    static void print_message(MessageType type = MessageType::INFO, const char *message = "");
};


#endif //MBED_SERIAL_DEBUGGER_H
